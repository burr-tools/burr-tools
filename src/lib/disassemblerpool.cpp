/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "disassemblerpool.h"
#include "disassembler_0.h"
#include "assembly.h"
#include "disassembly.h"
#include "problem.h"
#include "puzzle.h"
#include "gridtype.h"

#include <cstdlib>
#include <algorithm>

disassemblerPool_c::disassemblerPool_c(
  const problem_c & puz,
  unsigned int requested_threads,
  ResultCallback cb
) : puzzle(puz),
    num_threads(requested_threads),
    on_result(std::move(cb))
{
  // Pre-warm lazy caches on the constructor thread before any worker threads start.
  // gridType_c::getSymmetries() lazily initializes a mutable pointer without internal locks.
  if (puzzle.getPuzzle().getGridType()) {
    puzzle.getPuzzle().getGridType()->getSymmetries();
  }

  if (std::getenv("BURRTOOLS_NO_DISASM_POOL") != nullptr) {
    is_inline = true;
    inline_dis = std::make_unique<disassembler_0_c>(puzzle);
    return;
  }

  if (num_threads == 0) {
    if (const char * env = std::getenv("BURRTOOLS_THREADS")) {
      int t = std::atoi(env);
      if (t > 0) num_threads = static_cast<unsigned int>(t);
    }
    if (num_threads == 0) {
      num_threads = std::thread::hardware_concurrency();
      if (num_threads == 0) num_threads = 1;
    }
  }

  // Sanity cap on worker threads to avoid resource exhaustion
  num_threads = std::min(num_threads, 256u);
  max_queue_size = std::max<size_t>(64, num_threads);
  max_reorder_size = std::max<size_t>(64, num_threads * 2);

  // Seed one permit per disassembler worker thread. This asynchronous N-permit buffer
  // ensures all N disassemblers can stay saturated even when only a single assembler
  // worker is finding assemblies (preventing pipeline starvation under subtree skew).
  // submit() throttles the assembler once all N disassemblers are already busy.
  available_disassembly_permits = num_threads;

  if (num_threads == 1) {
    is_inline = true;
    inline_dis = std::make_unique<disassembler_0_c>(puzzle);
    return;
  }

  // Start worker threads
  workers.reserve(num_threads);
  for (unsigned int i = 0; i < num_threads; ++i) {
    workers.emplace_back([this](std::stop_token st) { worker_loop(st); });
  }

  // Start merger thread
  merger = std::jthread([this](std::stop_token st) { merger_loop(st); });
}

disassemblerPool_c::~disassemblerPool_c() {
  abort();
}

void disassemblerPool_c::check_exception() {
  std::lock_guard<std::mutex> lock(exception_mutex);
  if (worker_exception) {
    std::exception_ptr ex = worker_exception;
    worker_exception = nullptr;
    std::rethrow_exception(ex);
  }
}

void disassemblerPool_c::submit(std::unique_ptr<assembly_c> a) {
  if (aborted.load(std::memory_order_relaxed) || finished.load(std::memory_order_relaxed))
    return;

  if (is_inline) {
    std::lock_guard<std::mutex> lock(inline_mutex);
    uint64_t seq = next_submit_seq++;
    std::unique_ptr<separation_c> s;
    if (a && a->placementCount() > 1) {
      s = inline_dis->disassemble(a.get());
    }
    if (on_result) {
      on_result(seq, std::move(a), std::move(s));
    }
    return;
  }

  std::unique_lock<std::mutex> lock(queue_mutex);
  cv_producer.wait(lock, [this]() {
    return ((work_queue.size() < max_queue_size &&
            (next_submit_seq.load(std::memory_order_relaxed) - next_merge_seq.load(std::memory_order_relaxed)) < max_reorder_size) ||
            aborted.load(std::memory_order_relaxed));
  });

  if (aborted.load(std::memory_order_relaxed))
    return;

  uint64_t seq = next_submit_seq++;
  work_queue.push(Task{seq, std::move(a)});
  cv_worker.notify_one();

  // Cooperative token handoff:
  // Yield the current assembler thread's CPU slot until a disassembler completes
  // or the solve finishes / aborts / stops.
  cv_assembler.wait(lock, [this]() {
    return available_disassembly_permits > 0 ||
           aborted.load(std::memory_order_relaxed) ||
           finished.load(std::memory_order_relaxed) ||
           stop_requested.load(std::memory_order_relaxed);
  });

  if (available_disassembly_permits > 0) {
    available_disassembly_permits--;
  }
}

void disassemblerPool_c::worker_loop(std::stop_token st) {
  try {
    disassembler_0_c dis(puzzle);

    while (!st.stop_requested()) {
      Task task;
      {
        std::unique_lock<std::mutex> lock(queue_mutex);
        bool ok = cv_worker.wait(lock, st, [this]() {
          return !work_queue.empty() || finished.load(std::memory_order_relaxed) || aborted.load(std::memory_order_relaxed);
        });

        if (!ok || st.stop_requested() || aborted.load(std::memory_order_relaxed))
          return;

        if (work_queue.empty()) {
          if (finished.load(std::memory_order_relaxed))
            return;
          continue;
        }

        task = std::move(work_queue.front());
        work_queue.pop();
        cv_producer.notify_one();
      }

      std::unique_ptr<separation_c> sep;
      if (task.assembly && task.assembly->placementCount() > 1 && !st.stop_requested() && !aborted.load(std::memory_order_relaxed)) {
        sep = dis.disassemble(task.assembly.get());
      }

      // Return compute slot permit to waiting assembler thread
      {
        std::lock_guard<std::mutex> qlock(queue_mutex);
        available_disassembly_permits++;
        cv_assembler.notify_one();
      }

      {
        std::unique_lock<std::mutex> lock(result_mutex);
        bool ok = cv_reorder.wait(lock, st, [this]() {
          return reorder_buffer.size() < max_reorder_size || aborted.load(std::memory_order_relaxed);
        });

        if (!ok || st.stop_requested() || aborted.load(std::memory_order_relaxed))
          return;

        reorder_buffer.emplace(task.seqNo, Result{std::move(task.assembly), std::move(sep)});
        cv_merger.notify_one();
      }
    }
  } catch (...) {
    {
      std::lock_guard<std::mutex> lock(exception_mutex);
      if (!worker_exception) {
        worker_exception = std::current_exception();
      }
    }
    aborted.store(true, std::memory_order_release);
    {
      std::lock_guard<std::mutex> qlock(queue_mutex);
      available_disassembly_permits += num_threads;
      cv_assembler.notify_all();
      cv_worker.notify_all();
      cv_producer.notify_all();
    }
    {
      std::lock_guard<std::mutex> rlock(result_mutex);
      cv_merger.notify_all();
      cv_reorder.notify_all();
    }
    return;
  }
}

void disassemblerPool_c::merger_loop(std::stop_token st) {
  try {
    while (!st.stop_requested()) {
      Result res;
      uint64_t seq = 0;
      {
        std::unique_lock<std::mutex> lock(result_mutex);
        bool ok = cv_merger.wait(lock, st, [this]() {
          return reorder_buffer.find(next_merge_seq.load(std::memory_order_relaxed)) != reorder_buffer.end() ||
                 aborted.load(std::memory_order_relaxed) ||
                 (finished.load(std::memory_order_relaxed) && next_merge_seq.load(std::memory_order_relaxed) == next_submit_seq.load(std::memory_order_relaxed));
        });

        if (!ok || st.stop_requested() || aborted.load(std::memory_order_relaxed))
          return;

        auto it = reorder_buffer.find(next_merge_seq.load(std::memory_order_relaxed));
        if (it != reorder_buffer.end()) {
          seq = it->first;
          res = std::move(it->second);
          reorder_buffer.erase(it);
          next_merge_seq.fetch_add(1, std::memory_order_release);
          cv_reorder.notify_one();
        } else if (finished.load(std::memory_order_relaxed) && next_merge_seq.load(std::memory_order_relaxed) == next_submit_seq.load(std::memory_order_relaxed)) {
          return;
        } else {
          continue;
        }
      }

      cv_producer.notify_one();

      if (on_result) {
        on_result(seq, std::move(res.assembly), std::move(res.separation));
      }
    }
  } catch (...) {
    {
      std::lock_guard<std::mutex> lock(exception_mutex);
      if (!worker_exception) {
        worker_exception = std::current_exception();
      }
    }
    aborted.store(true, std::memory_order_release);
    {
      std::lock_guard<std::mutex> qlock(queue_mutex);
      available_disassembly_permits += num_threads;
      cv_assembler.notify_all();
      cv_worker.notify_all();
      cv_producer.notify_all();
    }
    {
      std::lock_guard<std::mutex> rlock(result_mutex);
      cv_merger.notify_all();
      cv_reorder.notify_all();
    }
    return;
  }
}

void disassemblerPool_c::finish() {
  if (is_inline)
    return;

  std::lock_guard<std::mutex> lock(lifecycle_mutex);
  if (finished.load(std::memory_order_relaxed)) {
    check_exception();
    return;
  }

  if (aborted.load(std::memory_order_relaxed)) {
    for (auto &w : workers) {
      if (w.joinable() && w.get_id() != std::this_thread::get_id())
        w.join();
    }
    workers.clear();
    if (merger.joinable() && merger.get_id() != std::this_thread::get_id())
      merger.join();
    check_exception();
    return;
  }

  {
    std::lock_guard<std::mutex> qlock(queue_mutex);
    finished.store(true, std::memory_order_release);
    cv_assembler.notify_all();
    cv_worker.notify_all();
  }

  for (auto &w : workers) {
    if (w.joinable())
      w.join();
  }
  workers.clear();

  {
    std::lock_guard<std::mutex> rlock(result_mutex);
    cv_merger.notify_all();
  }
  if (merger.joinable())
    merger.join();

  check_exception();
}

void disassemblerPool_c::requestStop() {
  stop_requested.store(true, std::memory_order_release);
  if (is_inline)
    return;

  // Wake any assembler thread blocked in submit(), but do not abort workers or discard
  // the reorder buffer so already-queued tasks are processed when finish() is called.
  {
    std::lock_guard<std::mutex> qlock(queue_mutex);
    cv_assembler.notify_all();
    cv_producer.notify_all();
  }
}

void disassemblerPool_c::abort() {
  if (is_inline)
    return;

  std::lock_guard<std::mutex> lock(lifecycle_mutex);
  aborted.store(true, std::memory_order_release);

  for (auto &w : workers) {
    w.request_stop();
  }
  merger.request_stop();

  {
    std::lock_guard<std::mutex> qlock(queue_mutex);
    while (!work_queue.empty()) work_queue.pop();
    available_disassembly_permits += num_threads;
    cv_assembler.notify_all();
    cv_worker.notify_all();
    cv_producer.notify_all();
  }

  {
    std::lock_guard<std::mutex> rlock(result_mutex);
    reorder_buffer.clear();
    cv_merger.notify_all();
    cv_reorder.notify_all();
  }

  for (auto &w : workers) {
    if (w.joinable() && w.get_id() != std::this_thread::get_id())
      w.join();
  }
  workers.clear();

  if (merger.joinable() && merger.get_id() != std::this_thread::get_id())
    merger.join();
}
