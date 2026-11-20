#include "py_solution_iterator.h"

#include "lib/assembly.h"
#include "lib/disassembly.h"
#include "lib/gridtype.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/solver/solversession.h"
#include "lib/solver/solverspec.h"
#include "lib/voxel.h"

#include <pybind11/pybind11.h>
#include <chrono>
#include <stdexcept>
#include <string>

namespace py = pybind11;

SolutionIterator::SolutionIterator(std::shared_ptr<puzzle_c> puz,
                                   unsigned int problem_idx,
                                   bool disassemble,
                                   bool reduce,
                                   bool keep_rotations,
                                   unsigned int threads,
                                   rotationMode_e rotation_mode)
  : puzzle_(std::move(puz))
{
  if (!puzzle_) {
    throw std::invalid_argument("Puzzle pointer is null");
  }
  if (problem_idx >= puzzle_->getNumberOfProblems()) {
    throw std::out_of_range("Problem index out of range");
  }

  problem_c * problem = puzzle_->getProblem(problem_idx);
  const gridType_c * gt = problem->getPuzzle().getGridType();

  // Every front end normalises shape hotspots before solving (burrTxt,
  // burrTxt2 and the GUI all call initHotspot() on every shape): hotspot
  // coordinates feed placement output, and a stored non-canonical hotspot
  // would make this path disagree with the rest.
  for (unsigned int i = 0; i < puzzle_->getNumberOfShapes(); i++) {
    puzzle_->getShape(i)->initHotspot();
  }

  solverConfig_t config;
  if (threads > 0) {
    config.threads = threads;
  }
  config.rotation.mode = rotation_mode;
  const solverSettings_t settings = config.resolve();

  const bool canDisassemble = (gt->getCapabilities() & gridType_c::CAP_DISASSEMBLE) != 0;
  disassemble_ = disassemble && canDisassemble;

  solverOptions_t opts;
  opts.reduce            = reduce;
  opts.disassemble       = disassemble_;
  opts.keepRotations     = keep_rotations;
  opts.completeRotations = false;
  opts.rotationMoves     = disassemble_ && (rotation_mode != ROTATION_OFF);
  opts.countOnly         = false;
  opts.queueCapacity     = 4;
  opts.settings          = settings;

  session_ = std::make_unique<solverSession_c>(makeSolverSpec(*problem), std::move(opts));
  if (!session_->start()) {
    finished_.store(true, std::memory_order_release);
    throw std::runtime_error("Failed to start solver session");
  }
}

SolutionIterator::~SolutionIterator() {
  stop();
}

unsigned long SolutionIterator::get_iterations() const {
  if (session_) {
    unsigned long cur = session_->getIterations();
    if (cur > 0)
      return cur;
  }
  return last_iterations_.load(std::memory_order_relaxed);
}

void SolutionIterator::stop() {
  stopped_.store(true, std::memory_order_release);
  if (session_) {
    session_->abort();
    last_iterations_.store(session_->getIterations(), std::memory_order_relaxed);
  }
}

PySolution SolutionIterator::next() {
  while (true) {
    if (finished_.load(std::memory_order_acquire) ||
        stopped_.load(std::memory_order_acquire)) {
      throw py::stop_iteration();
    }

    std::optional<solverResult_t> item;
    {
      py::gil_scoped_release release;
      item = session_->popFor(std::chrono::milliseconds(50));
    }

    if (!item) {
      if (PyErr_CheckSignals() != 0) {
        stop();
        throw py::error_already_set();
      }

      const solverState_e st = session_->sessionState();
      if (st == solverState_e::Running) {
        continue;
      }

      // One final non-blocking pop in case an item arrived right before
      // the session transitioned out of Running.
      item = session_->popFor(std::chrono::milliseconds(0));
      if (!item) {
        session_->join();
        last_iterations_.store(session_->getIterations(), std::memory_order_relaxed);
        finished_.store(true, std::memory_order_release);

        if (st == solverState_e::Error) {
          if (session_->exception())
            throw std::runtime_error(session_->exceptionMessage());
          if (session_->currentAction() == solverSession_c::ACT_ASSERT) {
            const auto & ex = session_->getAssertException();
            throw std::runtime_error(ex.expr ? ex.expr : "Internal solver assertion");
          }
          auto info = session_->runInfo();
          const assembler_c::errState err = info ? info->error : assembler_c::ERR_NONE;
          if (err == assembler_c::ERR_PUZZLE_UNHANDABLE)
            throw std::runtime_error("Failed to create assembler for problem");
          std::string msg = std::string("Matrix creation error in assembler: ") +
                            assembler_c::getErrorMessage(err);
          throw std::runtime_error(msg);
        }

        throw py::stop_iteration();
      }
    }

    if (stopped_.load(std::memory_order_acquire)) {
      throw py::stop_iteration();
    }

    asm_count_++;
    const assembly_c * a = item->assembly.get();
    const separation_c * da = item->separation.get();

    if (disassemble_ && !da) {
      // Assembly did not disassemble; skip to the next result.
      continue;
    }

    PySolution sol;
    sol.assembly_number = asm_count_;
    sol.has_disassembly = false;
    sol.total_moves = 0;
    sol.level = 0;

    sol.placements.reserve(a->placementCount());
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

    if (disassemble_) {
      sol_count_++;
      sol.solution_number = sol_count_;
      sol.has_disassembly = true;
      sol.total_moves = da->sumMoves();
      sol.level = da->getMoves();
      sol.moves_text = da->movesText();
    } else {
      sol.solution_number = asm_count_;
    }

    return sol;
  }
}
