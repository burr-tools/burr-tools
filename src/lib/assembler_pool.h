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
#include <iterator>
#include <mutex>
#include <stop_token>
#include <vector>

#include "bt_assert.h"
#include "thread_budget.h"

/**
 * Dynamic, non-terminating task queue for assembly search subtrees.
 *
 * A worker whose pop finds an empty queue waits until either new work arrives
 * (a sibling splits a task, see the per-engine split helpers), all work is
   * done (global quiescence: queue empty and no worker active), or the search
   * is stopped via the run or jthread stop tokens (this pool deliberately
   * has no stop flag of its own, so pushes stay accepted while stopping).
   * Workers therefore stay alive for the whole search and
 * remain available when a sibling blocked in disassemblerPool_c::submit()
 * wakes up again.
 *
 * Threading contract:
 * - All queue/counter state is guarded by `mtx`. `active_workers` and
 *   `waiting_workers` are plain unsigned ints under that mutex -- deliberately
 *   NOT atomics, so the "queue empty && nobody active" quiescence predicate is
 *   evaluated atomically with the wait.
 * - Coordination happens only at task boundaries (pop / task_done / push).
 *   The only thing the exact-cover node loop touches is wants_work(), a
 *   relaxed atomic flag; a worker that acts on it donates with an ordinary
 *   push.
 * - Every successful pop_task() MUST be paired with exactly one finishTask(),
 *   even when the task body throws or the search aborts mid-task. The worker
 *   loops use try/catch for this. (task_done() alone is only correct when no
 *   budget is set; finishTask() releases the budget token first.)
 *   [SPEC-POOL-1, model-checked in spec/AssemblyPool.tla]
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
    for (auto &t : initial_tasks)
      queue.push_back(std::move(t));
    updateHunger();
  }

  /// Attach a shared budget (nullable). Must be called with no workers
  /// running; the pointer must outlive the search.
  void setBudget(ThreadBudget *budget) { budget_ = budget; }

  /**
   * Pop a task. Returns true with out_task set (caller now owns one unit of
   * `active_workers` and must call finishTask() exactly once), or false when
   * the worker should terminate: global quiescence (queue empty and no worker
   * active), jthread stop, or the run's stop token.
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
      updateHunger();
      auto pred = [&] {
        return !queue.empty() || active_workers == 0 ||
               runStop.stop_requested() || st.stop_requested();
      };
      cv.wait(lock, st, pred);
      waiting_workers--;
      updateHunger();

      if (runStop.stop_requested() || st.stop_requested()) {
        releaseBudget();
        return false;
      }
      if (queue.empty()) {
        // active_workers == 0 is the only way to reach here with an empty
        // queue: global quiescence.
        releaseBudget();
        return false;
      }
      if (budget_ != nullptr && !ThreadBudget::holdsHere(budget_) &&
          !budget_->tryAcquire()) {
        // Budget exhausted: park token-free (releasing the pool lock first,
        // so no lock ordering issues) until a token frees or we must stop.
        lock.unlock();
        bool got = budget_->acquire([&] {
          return runStop.stop_requested() || st.stop_requested();
        }, st);
        if (!got)
          return false;
        // Re-check under the pool lock before waiting with the token: a
        // sibling may have consumed the remaining tasks while this thread
        // was parked on the budget. Waiting on an empty queue while holding
        // a token would starve token-gated consumers (e.g. disassemblers);
        // in that case return the token and wait token-free instead.
        // (Reaching here implies the token is freshly acquired: the check
        // above guarantees this thread held none on entry.)
        lock.lock();
        if (queue.empty() || runStop.stop_requested() || st.stop_requested()) {
          releaseBudget();
          if (runStop.stop_requested() || st.stop_requested())
            return false;
          if (queue.empty() && active_workers == 0)
            return false;
          continue;
        }
      }
      out_task = std::move(queue.front());
      queue.pop_front();
      updateHunger();
      active_workers++;
#ifndef NDEBUG
      dbg_pops_.fetch_add(1, std::memory_order_relaxed);
#endif
      return true;
    }
  }

  /// Mark the task from pop_task() as finished: return its budget token (if
  /// any) first, then record progress. Broadcasts when the last active
  /// worker drains an empty queue so quiescent waiters can terminate.
  void finishTask() {
#ifndef NDEBUG
    // Pairing tripwire (SPEC-POOL-1, live): finishing more often than
    // popping is always a bug. End-of-run equality is pinned in
    // test_thread_protocol.cpp; this catches double-finish live. The check
    // and the increment run under the pool mutex: dbg_pops_ is bumped under
    // it, so a lock-free relaxed read here could still observe a stale pop
    // count and throw spuriously on weakly ordered hardware (arm64).
    {
      std::lock_guard<std::mutex> lock(mtx);
      bt_assert(dbg_finishes_.load(std::memory_order_relaxed) <
                dbg_pops_.load(std::memory_order_relaxed));
      dbg_finishes_.fetch_add(1, std::memory_order_relaxed);
    }
#endif
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

  /// Push dynamically split child tasks. Returns the number accepted
  /// (0 only for empty input). Pushes stay accepted while stopping: a
  /// worker interrupted mid-task re-queues it here for in-session resume,
  /// so a pool-level stop flag would refuse exactly the pushes the resume
  /// depends on -- this pool deliberately has no requestStop()/abort();
  /// stop arrives via the run and jthread stop tokens instead. Wakes
  /// waiters when tasks were accepted.
  size_t push_tasks(std::vector<TaskType> new_tasks) {
    size_t accepted = 0;
    {
      std::lock_guard<std::mutex> lock(mtx);
      if (new_tasks.empty())
        return 0;
      for (auto &t : new_tasks)
        queue.push_back(std::move(t));
      accepted = new_tasks.size();
      updateHunger();
    }
    if (accepted > 0)
      cv.notify_all();
    return accepted;
  }

  /// Push dynamically split children to the FRONT of the queue, keeping
  /// their order (the next pop takes the first of them). Workers pop from
  /// the front, so a split subtree is explored depth-first: the queue then
  /// stays at about workers x depth x fan-out tasks; breadth-first order
  /// grows it towards every node of the split depth (see
  /// design/2026-10-16-divide-at-stop.md). The old, shallow tasks drift to the back and are taken
  /// when the front runs dry -- exactly when workers starve, so the split
  /// heuristic breaks them up; dynamic splitting, not queue order, is what
  /// bounds the tail. Returns the number accepted; wakes waiters.
  size_t push_tasks_front(std::vector<TaskType> new_tasks) {
    size_t accepted = 0;
    {
      std::lock_guard<std::mutex> lock(mtx);
      if (new_tasks.empty())
        return 0;
      queue.insert(queue.begin(), std::make_move_iterator(new_tasks.begin()),
                   std::make_move_iterator(new_tasks.end()));
      accepted = new_tasks.size();
      updateHunger();
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

  /// True while more workers wait for a task than the queue holds: the
  /// "hungry" signal for donation (a running task hands part of its
  /// unexplored territory to the pool, see assembler_0's donate()). A
  /// relaxed load of a flag kept in step with the guarded counters, so
  /// workers may poll it from inside the search loop without touching the
  /// mutex. It is only a hint: a stale read costs at most one needless
  /// donation or a few hundred nodes of delay, never correctness --
  /// donated work is ordinary pool work either way.
  bool wants_work() const {
    return hungry_.load(std::memory_order_relaxed);
  }

  /// Take the hunger signal for one donation: true for exactly one caller
  /// per raise, which then must push. Clearing the flag keeps the other
  /// running workers from donating for the same waiter; the donor's
  /// push_tasks() re-derives it, so a second waiter raises it again. Call
  /// only once there is something to give -- a claim that is not followed
  /// by a push leaves the waiter unannounced until the next pool event.
  bool claim_work() {
    bool expected = true;
    return hungry_.compare_exchange_strong(expected, false,
                                           std::memory_order_relaxed);
  }

  /// Number of queued (not yet popped) tasks. Lock-protected; task granularity.
  size_t queued() const {
    std::lock_guard<std::mutex> lock(mtx);
    return queue.size();
  }

#ifndef NDEBUG
  /* Debug-only pop/finish accounting (SPEC-POOL-1, pinned end-of-run in
   * test_thread_protocol.cpp). Compiled out under NDEBUG: per the PR #113
   * lesson these tripwires gate nothing in release builds. Read after
   * joining workers (or accept relaxed staleness). */
  unsigned long debugPops() const { return dbg_pops_.load(std::memory_order_relaxed); }
  unsigned long debugFinishes() const { return dbg_finishes_.load(std::memory_order_relaxed); }
  unsigned long debugUnpaired() const { return debugPops() - debugFinishes(); }
#endif

  /// Wake all waiters to re-check the predicate; changes no state.
  /// Paired with ThreadBudget::notify() in a stop_callback so parked workers
  /// observe stop promptly, while in-flight retry pushes and drain() keep
  /// working for in-session resume.
  void notify() {
    cv.notify_all();
  }

  /**
   * Move all still-queued tasks out for an in-session resume. Active
   * (half-searched) tasks are NOT included: the worker re-queues the
   * unexplored remainder of its current task when it notices the abort
   * (divided pieces or written-back live stacks), so the continue
   * re-walks nothing.
   */
  std::vector<TaskType> drain() {
    std::vector<TaskType> rest;
    std::lock_guard<std::mutex> lock(mtx);
    rest.reserve(queue.size());
    while (!queue.empty()) {
      rest.push_back(std::move(queue.front()));
      queue.pop_front();
    }
    updateHunger();
    return rest;
  }

private:
  // Call with mtx held, after every change of waiting_workers or queue.
  // Stores only on a change: workers poll the flag's cache line from the
  // search loop, and every pop and push passes through here.
  void updateHunger() {
    const bool hungry = waiting_workers > queue.size();
    if (hungry_.load(std::memory_order_relaxed) != hungry)
      hungry_.store(hungry, std::memory_order_relaxed);
  }

  void releaseBudget() {
    if (budget_ != nullptr)
      budget_->release(); // no-op unless this thread holds one
  }

  mutable std::mutex mtx;
  std::condition_variable_any cv;
  std::deque<TaskType> queue;
  unsigned int active_workers{0};  // guarded by mtx (see class comment)
  unsigned int waiting_workers{0}; // guarded by mtx (see class comment)
  ThreadBudget *budget_{nullptr};  // Set via setBudget() before workers start; null = uncapped (checked per pop).
#ifndef NDEBUG
  std::atomic<unsigned long> dbg_pops_{0};
  std::atomic<unsigned long> dbg_finishes_{0};
#endif
  // Mirror of waiting_workers > queue.size(), see wants_work(). Last and on
  // its own cache line: the search loops poll it, and the mutex and
  // counters above change on every pop and push.
  alignas(64) std::atomic<bool> hungry_{false};
};

#endif // __ASSEMBLER_POOL_H__
