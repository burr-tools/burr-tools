#include "py_solution_iterator.h"

#include "lib/puzzle.h"
#include "lib/problem.h"
#include "lib/assembler.h"
#include "lib/assembly.h"
#include "lib/disassembler.h"
#include "lib/disassembler_0.h"
#include "lib/disassembly.h"
#include "lib/gridtype.h"
#include "lib/voxel.h"

#include <pybind11/pybind11.h>
#include <chrono>
#include <stdexcept>

namespace py = pybind11;

SolutionIterator::SolutionIterator(std::shared_ptr<puzzle_c> puz,
                                   unsigned int problem_idx,
                                   bool disassemble,
                                   bool reduce,
                                   bool keep_rotations,
                                   bool keep_mirror)
  : puzzle(std::move(puz)),
    problem_idx(problem_idx),
    disassemble(disassemble),
    reduce(reduce),
    keep_rotations(keep_rotations),
    keep_mirror(keep_mirror)
{
  if (!puzzle) {
    throw std::invalid_argument("Puzzle pointer is null");
  }
  if (problem_idx >= puzzle->getNumberOfProblems()) {
    throw std::out_of_range("Problem index out of range");
  }

  worker_thread = std::thread(&SolutionIterator::worker_run, this);
}

SolutionIterator::~SolutionIterator() {
  stop();
}

unsigned long SolutionIterator::get_iterations() const {
  assembler_c * a = active_assm.load(std::memory_order_acquire);
  if (a) {
    return a->getIterations();
  }
  return iterations.load(std::memory_order_relaxed);
}

void SolutionIterator::stop() {
  bool expected = false;
  if (!stop_requested.compare_exchange_strong(expected, true)) {
    return;
  }
  assembler_c * a = active_assm.load(std::memory_order_acquire);
  if (a) {
    a->stop();
  }
  {
    std::lock_guard<std::mutex> lock(queue_mutex);
    cv_can_push.notify_all();
    cv_can_pop.notify_all();
  }
  if (worker_thread.joinable()) {
    worker_thread.join();
  }
}

void SolutionIterator::push_item(QueueItem && item) {
  std::unique_lock<std::mutex> lock(queue_mutex);
  cv_can_push.wait(lock, [this]() {
    return queue.size() < MAX_QUEUE_SIZE || stop_requested.load();
  });
  if (stop_requested.load()) {
    return;
  }
  queue.push_back(std::move(item));
  cv_can_pop.notify_one();
}

PySolution SolutionIterator::next() {
  while (true) {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      if (!queue.empty()) {
        QueueItem item = std::move(queue.front());
        queue.pop_front();
        cv_can_push.notify_one();

        if (item.type == QueueItem::ITEM_SOLUTION) {
          return item.solution;
        } else if (item.type == QueueItem::ITEM_FINISHED) {
          finished.store(true);
          throw py::stop_iteration();
        } else {
          finished.store(true);
          throw std::runtime_error(item.error_message);
        }
      }

      if (finished.load()) {
        throw py::stop_iteration();
      }
      if (stop_requested.load()) {
        throw py::stop_iteration();
      }

      {
        py::gil_scoped_release release;
        cv_can_pop.wait_for(lock, std::chrono::milliseconds(50));
      }
    }

    if (PyErr_CheckSignals() != 0) {
      stop();
      throw py::error_already_set();
    }
  }
}

void SolutionIterator::worker_run() {
  try {
    if (stop_requested.load(std::memory_order_acquire)) {
      return;
    }

    problem_c * problem = puzzle->getProblem(problem_idx);
    const gridType_c * gt = problem->getPuzzle().getGridType();

    for (unsigned int i = 0; i < puzzle->getNumberOfShapes(); i++) {
      puzzle->getShape(i)->initHotspot();
    }

    assm = gt->findAssembler(*problem);
    if (!assm) {
      push_item(QueueItem{QueueItem::ITEM_ERROR, {}, "Failed to create assembler for problem"});
      return;
    }

    active_assm.store(assm.get(), std::memory_order_release);
    if (stop_requested.load(std::memory_order_acquire)) {
      assm->stop();
      active_assm.store(nullptr, std::memory_order_release);
      return;
    }

    assembler_c::errState err = assm->createMatrix(keep_mirror, keep_rotations, false);
    if (err != assembler_c::ERR_NONE) {
      active_assm.store(nullptr, std::memory_order_release);
      std::string msg = std::string("Matrix creation error in assembler: ") + assembler_c::getErrorMessage(err);
      push_item(QueueItem{QueueItem::ITEM_ERROR, {}, msg});
      return;
    }

    if (stop_requested.load(std::memory_order_acquire)) {
      active_assm.store(nullptr, std::memory_order_release);
      return;
    }

    if (reduce) {
      assm->reduce();
    }

    if (stop_requested.load(std::memory_order_acquire)) {
      active_assm.store(nullptr, std::memory_order_release);
      return;
    }

    if (disassemble && (gt->getCapabilities() & gridType_c::CAP_DISASSEMBLE)) {
      disasm = std::make_unique<disassembler_0_c>(*problem);
    }

    unsigned int asm_count{0};
    unsigned int sol_count{0};

    assm->assemble([&](std::unique_ptr<assembly_c> a) -> bool {
      if (stop_requested.load(std::memory_order_relaxed)) {
        return false;
      }

      asm_count++;

      PySolution sol;
      sol.assembly_number = asm_count;
      sol.has_disassembly = false;
      sol.total_moves = 0;
      sol.level = 0;

      for (unsigned int i = 0; i < a->placementCount(); i++) {
        PyPlacement p;
        p.piece_id = i;
        p.is_placed = a->isPlaced(i);
        if (p.is_placed) {
          p.x = a->getX(i);
          p.y = a->getY(i);
          p.z = a->getZ(i);
          p.transformation = a->getTransformation(i);
        } else {
          p.x = p.y = p.z = 0;
          p.transformation = 0xff;
        }
        sol.placements.push_back(p);
      }

      if (disassemble && disasm) {
        auto da = disasm->disassemble(a.get());
        if (da) {
          sol_count++;
          sol.solution_number = sol_count;
          sol.has_disassembly = true;
          sol.total_moves = da->sumMoves();
          sol.level = da->getMoves();
          sol.moves_text = da->movesText();
        } else {
          return !stop_requested.load(std::memory_order_relaxed);
        }
      } else {
        sol.solution_number = asm_count;
      }

      push_item(QueueItem{QueueItem::ITEM_SOLUTION, std::move(sol), ""});
      return !stop_requested.load(std::memory_order_relaxed);
    });

    iterations.store(assm->getIterations(), std::memory_order_relaxed);
    active_assm.store(nullptr, std::memory_order_release);

    push_item(QueueItem{QueueItem::ITEM_FINISHED, {}, ""});
  } catch (const std::exception & e) {
    active_assm.store(nullptr, std::memory_order_release);
    push_item(QueueItem{QueueItem::ITEM_ERROR, {}, e.what()});
  } catch (...) {
    active_assm.store(nullptr, std::memory_order_release);
    push_item(QueueItem{QueueItem::ITEM_ERROR, {}, "Unknown C++ exception occurred during solving"});
  }
}
