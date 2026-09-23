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
#include "thread_budget.h"
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

  // Throttling: submitters pace via the bounded queue, workers via shared
  // budget tokens (see the concurrency architecture note in thread_budget.h).

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

// Salvage helper: the assembly was not enqueued (terminal state), so keep
// it for re-submission on the next run instead of losing it. The caller
// must hold queue_mutex. No sequence number is assigned, so the merger
// never expects this assembly and no dropped_ entry is needed.
static void salvageAssembly(std::vector<std::unique_ptr<assembly_c>> &salvaged,
                            std::unique_ptr<assembly_c> a) {
  if (a)
    salvaged.push_back(std::move(a));
}

bool disassemblerPool_c::submit(std::unique_ptr<assembly_c> a) {
  if (is_inline) {
    if (aborted.load(std::memory_order_relaxed) || finished.load(std::memory_order_relaxed) ||
        stop_requested.load(std::memory_order_relaxed)) {
      std::lock_guard<std::mutex> qlock(queue_mutex);
      salvageAssembly(salvaged_, std::move(a));
      return false;
    }
    std::lock_guard<std::mutex> lock(inline_mutex);
    uint64_t seq = next_submit_seq++;
    std::unique_ptr<separation_c> s;
    if (a && a->placementCount() > 1) {
      s = inline_dis->disassemble(a.get());
    }
    if (on_result) {
      on_result(seq, std::move(a), std::move(s));
    }
    return true;
  }

  std::unique_lock<std::mutex> lock(queue_mutex);
  auto terminal = [this]() {
    return aborted.load(std::memory_order_relaxed) ||
           finished.load(std::memory_order_relaxed) ||
           stop_requested.load(std::memory_order_relaxed);
  };
  // Re-check under the lock: requestStop() sets the flag and then salvages
  // the queue under this same mutex, so anything submitted after that point
  // must not land in the queue behind the salvage.
  if (terminal()) {
    salvageAssembly(salvaged_, std::move(a));
    return false;
  }
  auto hasSpace = [this]() {
    return (work_queue.size() < max_queue_size &&
            (next_submit_seq.load(std::memory_order_relaxed) - next_merge_seq.load(std::memory_order_relaxed)) < max_reorder_size);
  };

  // Wait for queue space WITHOUT holding our search token: the drain we wait
  // for runs on tokens, so waiting while holding one could deadlock (all
  // tokens held by queue-blocked submitters, none left to drain with).
  // Only threads that actually hold a token yield it here (hadToken): a
  // token-free submitter must not acquire one it never had, or the token
  // would leak (available_ permanently decreases).
  const bool hadToken = (budget_ != nullptr) && ThreadBudget::holdsHere(budget_);
  while (!hasSpace() && !terminal()) {
    if (hadToken)
      budget_->release();
    cv_producer.wait(lock, [&]() { return hasSpace() || terminal(); });
    if (terminal()) {
      salvageAssembly(salvaged_, std::move(a));
      return false;
    }
    if (hadToken &&
        !budget_->acquire([&]() { return terminal(); })) {
      salvageAssembly(salvaged_, std::move(a));
      return false;
    }
  }

  if (terminal()) {
    salvageAssembly(salvaged_, std::move(a));
    return false;
  }

  uint64_t seq = next_submit_seq++;
  work_queue.push(Task{seq, std::move(a)});
  cv_worker.notify_one();
  return true;
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

      // Budget gate (see the concurrency architecture note in
      // thread_budget.h): hold one shared token across this
      // job so searching + disassembling threads never exceed the budget.
      // The checked-out job stays in hand across a park (no requeue churn:
      // requeueing would ping-pong the job through the queue while firing
      // spurious producer wakes that let submitters steal the freed token
      // back before any disassembler gets it). Terminal wakes requeue the
      // job and exit. Park WITHOUT finished/stop_requested in the
      // predicate: finish() and requestStop() both require workers to keep
      // draining (finish joins them first), so a parked pickup must persist
      // until it can proceed; only abort and jthread teardown exit here.
      if (budget_ != nullptr && !budget_->tryAcquire()) {
        bool got = budget_->acquire([this, st]() {
          return aborted.load(std::memory_order_relaxed) || st.stop_requested();
        }, st);
        if (!got) {
          std::lock_guard<std::mutex> qlock(queue_mutex);
          work_queue.push(std::move(task));
          return;
        }
      }

      std::unique_ptr<separation_c> sep;
      if (task.assembly && task.assembly->placementCount() > 1 && !st.stop_requested() && !aborted.load(std::memory_order_relaxed)) {
        sep = dis.disassemble(task.assembly.get());
      }

      // Job done: return the budget token (potentially waking a parked
      // assembler or disassembler) before filing the result for the merger.
      if (budget_ != nullptr)
        budget_->release();

      {
        std::unique_lock<std::mutex> lock(result_mutex);
        bool ok = cv_reorder.wait(lock, st, [this]() {
          return reorder_buffer.size() < max_reorder_size || aborted.load(std::memory_order_relaxed);
        });

        if (!ok || st.stop_requested() || aborted.load(std::memory_order_relaxed)) {
          // Token was already returned above; release() here would be a
          // no-op via the per-thread holdings flag. Do not re-release (a
          // counting release would inflate available_ beyond total_).
          return;
        }

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
    if (budget_ != nullptr) {
      budget_->release(); // drop any token held across the throwing job
      budget_->shutdown();
    }
    {
      std::lock_guard<std::mutex> qlock(queue_mutex);
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
                 dropped_.count(next_merge_seq.load(std::memory_order_relaxed)) > 0 ||
                 aborted.load(std::memory_order_relaxed) ||
                 (finished.load(std::memory_order_relaxed) && next_merge_seq.load(std::memory_order_relaxed) == next_submit_seq.load(std::memory_order_relaxed));
        });

        if (!ok || st.stop_requested() || aborted.load(std::memory_order_relaxed))
          return;

        // A sequence discarded by requestStop(): salvaged for the next run,
        // so no result will ever arrive. Skip it without a callback.
        if (dropped_.count(next_merge_seq.load(std::memory_order_relaxed)) > 0) {
          dropped_.erase(next_merge_seq.load(std::memory_order_relaxed));
          next_merge_seq.fetch_add(1, std::memory_order_release);
          lock.unlock();
          cv_producer.notify_one();
          continue;
        }

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
    if (budget_ != nullptr) {
      budget_->release(); // drop any token held across the throwing job
      budget_->shutdown();
    }
    {
      std::lock_guard<std::mutex> qlock(queue_mutex);
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

  // Stop means stop: queued (never-started) assemblies are moved to the
  // salvage store instead of being disassembled, so stop returns promptly
  // instead of draining the backlog. In-flight jobs run to completion and
  // already-filed results are still delivered by the merger. The caller
  // moves the salvage to the problem after finish(), which re-submits it on
  // the next run -- without that, the assembler's emitted-signatures dedup
  // would suppress the dropped assemblies forever on pause/continue.
  // Collect the discarded sequence numbers first: they were assigned at
  // submit time, so the merger would wait for their results forever unless
  // told to skip them (see dropped_).
  std::vector<uint64_t> discardedSeqs;
  {
    std::lock_guard<std::mutex> qlock(queue_mutex);
    while (!work_queue.empty()) {
      discardedSeqs.push_back(work_queue.front().seqNo);
      salvaged_.push_back(std::move(work_queue.front().assembly));
      work_queue.pop();
    }
    cv_producer.notify_all();
  }
  if (!discardedSeqs.empty()) {
    std::lock_guard<std::mutex> rlock(result_mutex);
    for (uint64_t seq : discardedSeqs)
      dropped_.insert(seq);
    cv_merger.notify_all();
  }
  if (budget_ != nullptr)
    budget_->notify();
}

std::vector<std::unique_ptr<assembly_c>> disassemblerPool_c::takeSalvaged() {
  std::lock_guard<std::mutex> qlock(queue_mutex);
  std::vector<std::unique_ptr<assembly_c>> out;
  out.swap(salvaged_);
  return out;
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
    salvaged_.clear();
    cv_worker.notify_all();
    cv_producer.notify_all();
  }
  if (budget_ != nullptr)
    budget_->shutdown();

  {
    std::lock_guard<std::mutex> rlock(result_mutex);
    reorder_buffer.clear();
    dropped_.clear();
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
