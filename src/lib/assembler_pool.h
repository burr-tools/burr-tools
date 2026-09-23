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

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef __ASSEMBLER_POOL_H__
#define __ASSEMBLER_POOL_H__

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stop_token>
#include <vector>

#include "thread_budget.h"

/**
 * Dynamic, non-terminating task queue for assembly search subtrees.
 *
 * A worker whose pop finds an empty queue waits until either new work arrives
 * (a sibling splits a task, see the per-engine split helpers), all work is
 * done (global quiescence: queue empty and no worker active), or the search
 * is stopped/aborted. Workers therefore stay alive for the whole search and
 * remain available when a sibling blocked in disassemblerPool_c::submit()
 * wakes up again.
 *
 * Threading contract:
 * - All queue/counter state is guarded by `mtx`. `active_workers` and
 *   `waiting_workers` are plain unsigned ints under that mutex -- deliberately
 *   NOT atomics, so the "queue empty && nobody active" quiescence predicate is
 *   evaluated atomically with the wait.
 * - Coordination happens only at task boundaries (pop / task_done / push).
 *   Nothing here runs inside the exact-cover node loop.
 * - Every successful pop_task() MUST be paired with exactly one finishTask(),
 *   even when the task body throws or the search aborts mid-task. The worker
 *   loops use try/catch for this. (task_done() alone is only correct when no
 *   budget is set; finishTask() releases the budget token first.)
 * - Progress accounting (totalTasks/completedTasks on assembler_c) stays with
 *   the caller: push_tasks() reports how many tasks it accepted via its return
 *   value so the caller can bump totalTasks; completedTasks is bumped next to
 *   finishTask(). The pool itself keeps no progress counters.
 * - With a budget set (ThreadBudget, shared with the disassembly pool),
 *   popping also reserves one budget token, held across the task and returned
 *   by finishTask(). Reservation happens inside the pool lock when a task is
 *   available; when the budget is exhausted the thread parks token-free on
 *   the budget CV (never holding pool state across the park), so no lock
 *   ordering issues arise. Threads that find nothing to do never hold
 *   tokens: quiescence/terminal exits release before returning false.
 */
template <typename TaskType>
class AssemblyTaskPool {
public:
  AssemblyTaskPool() = default;

  // no copying; pools are shared by reference between worker threads
  AssemblyTaskPool(const AssemblyTaskPool &) = delete;
  AssemblyTaskPool &operator=(const AssemblyTaskPool &) = delete;

  /// Seed the pool before workers start. Must be called with no workers
  /// running (typically right after task generation, before spawning threads).
  void seed(std::vector<TaskType> initial_tasks) {
    std::lock_guard<std::mutex> lock(mtx);
    queue.clear();
    active_workers = 0;
    waiting_workers = 0;
    stop_requested = false;
    for (auto &t : initial_tasks)
      queue.push_back(std::move(t));
  }

  /// Attach a shared budget (nullable). Must be called with no workers
  /// running; the pointer must outlive the search.
  void setBudget(ThreadBudget *budget) { budget_ = budget; }

  /**
   * Pop a task. Returns true with out_task set (caller now owns one unit of
   * `active_workers` and must call finishTask() exactly once), or false when
   * the worker should terminate: global quiescence (queue empty and no worker
   * active), pool stop, jthread stop, or the run's stop token.
   *
   * With a budget set, a successful pop also holds one budget token for this
   * thread (returned by finishTask()). Budget exhaustion parks the thread
   * token-free; quiescence/terminal exits never leak a token.
   */
  bool pop_task(TaskType &out_task, std::stop_token runStop,
                std::stop_token st = {}) {
    while (true) {
      std::unique_lock<std::mutex> lock(mtx);
      waiting_workers++;
      auto pred = [&] {
        return !queue.empty() || active_workers == 0 || stop_requested.load() ||
               runStop.stop_requested() || st.stop_requested();
      };
      cv.wait(lock, st, pred);
      waiting_workers--;

      if (stop_requested.load() || runStop.stop_requested() ||
          st.stop_requested()) {
        releaseBudget();
        return false;
      }
      if (queue.empty()) {
        // active_workers == 0 is the only way to reach here with an empty
        // queue: global quiescence.
        releaseBudget();
        return false;
      }
      if (budget_ != nullptr && !ThreadBudget::holdsHere() &&
          !budget_->tryAcquire()) {
        // Budget exhausted: park token-free (releasing the pool lock first,
        // so no lock ordering issues) until a token frees or we must stop.
        lock.unlock();
        bool got = budget_->acquire([&] {
          return stop_requested.load() || runStop.stop_requested() ||
                 st.stop_requested();
        }, st);
        if (!got)
          return false;
        continue;
      }
      out_task = std::move(queue.front());
      queue.pop_front();
      active_workers++;
      return true;
    }
  }

  /// Mark the task from pop_task() as finished: return its budget token (if
  /// any) first, then record progress. Broadcasts when the last active
  /// worker drains an empty queue so quiescent waiters can terminate.
  void finishTask() {
    releaseBudget();
    task_done();
  }

  /// Mark the task from pop_task() as finished. Only correct without a
  /// budget (or after the token was returned another way); prefer
  /// finishTask().
  void task_done() {
    std::lock_guard<std::mutex> lock(mtx);
    if (active_workers > 0)
      active_workers--;
    if (queue.empty() && active_workers == 0)
      cv.notify_all();
  }

  /// Push dynamically split child tasks. Returns the number accepted (0 after
  /// stop/abort). Wakes waiters when tasks were accepted.
  size_t push_tasks(std::vector<TaskType> new_tasks) {
    size_t accepted = 0;
    {
      std::lock_guard<std::mutex> lock(mtx);
      if (stop_requested || new_tasks.empty())
        return 0;
      for (auto &t : new_tasks)
        queue.push_back(std::move(t));
      accepted = new_tasks.size();
    }
    if (accepted > 0)
      cv.notify_all();
    return accepted;
  }

  /// True while at least one worker is waiting for work. Used by the
  /// split heuristic: a popping thread splits its task only when siblings
  /// are actually starving. Lock-protected read; called at task granularity.
  bool has_waiting_workers() const {
    std::lock_guard<std::mutex> lock(mtx);
    return waiting_workers > 0;
  }

  /// Number of queued (not yet popped) tasks. Lock-protected; task granularity.
  size_t queued() const {
    std::lock_guard<std::mutex> lock(mtx);
    return queue.size();
  }

  /// Wake all waiters to re-check the predicate; changes no state.
  /// Paired with ThreadBudget::notify() in a stop_callback so parked workers
  /// observe stop promptly, while in-flight retry pushes and drain() keep
  /// working for in-session resume (unlike requestStop() below).
  void notify() {
    cv.notify_all();
  }

  /// Wake all waiters; subsequent pop_task() calls return false once the
  /// queue drains. In-flight tasks still run to their next task_done().
  void requestStop() {
    {
      std::lock_guard<std::mutex> lock(mtx);
      stop_requested = true;
    }
    cv.notify_all();
  }

  /// requestStop() plus discard of all queued (never-started) work.
  void abort() {
    {
      std::lock_guard<std::mutex> lock(mtx);
      stop_requested = true;
      queue.clear();
    }
    cv.notify_all();
  }

  /**
   * Move all still-queued tasks out for an in-session resume. Active
   * (half-searched) tasks are NOT included: the worker re-queues its current
   * task when it notices the abort, so the continue re-searches it from
   * scratch with already-reported assemblies suppressed via the
   * emittedSignatures dedup under callbackMutex.
   */
  std::vector<TaskType> drain() {
    std::vector<TaskType> rest;
    std::lock_guard<std::mutex> lock(mtx);
    rest.reserve(queue.size());
    while (!queue.empty()) {
      rest.push_back(std::move(queue.front()));
      queue.pop_front();
    }
    return rest;
  }

private:
  void releaseBudget() {
    if (budget_ != nullptr)
      budget_->release(); // no-op unless this thread holds one
  }

  mutable std::mutex mtx;
  std::condition_variable_any cv;
  std::deque<TaskType> queue;
  unsigned int active_workers{0};  // guarded by mtx (see class comment)
  unsigned int waiting_workers{0}; // guarded by mtx (see class comment)
  std::atomic<bool> stop_requested{false};
  ThreadBudget *budget_{nullptr};  // Set via setBudget() before workers start; null = uncapped (checked per pop).
};

#endif // __ASSEMBLER_POOL_H__
