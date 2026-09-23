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
#ifndef __THREAD_BUDGET_H__
#define __THREAD_BUDGET_H__

#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <stop_token>

/**
 * Kill switch (benchmarking only): with BURRTOOLS_NO_BUDGET=1 every budget
 * pointer is treated as null and both pools behave exactly as without any
 * cap, so capped vs uncapped can be A/B measured from the same build.
 */
inline bool threadBudgetEnabled() {
  static const bool enabled = std::getenv("BURRTOOLS_NO_BUDGET") == nullptr;
  return enabled;
}

/**
 * Concurrency architecture note (the one place that states the whole
 * picture; other comments point here instead of at external documents).
 *
 * A solve runs a two-tier pipeline with at most total() threads *working*
 * at once, where total() is the disassembly pool size:
 *
 * - Tier 1 (assembly) searches exact-cover subtrees from a
 *   non-terminating AssemblyTaskPool (assembler_pool.h): workers that run
 *   out of tasks wait instead of dying, and only all-idle + empty-queue
 *   (global quiescence) ends the search. A popping worker splits its task
 *   one level deeper when the prefix is shallow or siblings starve.
 *   Interrupted runs are resumable in-session (requeued tasks +
 *   emittedSignatures dedup) but never across save files.
 * - Tier 2 (disassembly) pulls found assemblies off a bounded queue with
 *   dedicated workers and a merger thread that re-emits results strictly
 *   in submit order. finish() drains and joins; abort() discards.
 * - The shared ThreadBudget below caps working threads: an assembler
 *   holds one token across a whole subtree task, a disassembler one per
 *   job. Submit pacing is deliberately absent -- the bounded queue stays
 *   a real buffer -- so pipeline overlap survives.
 *
 * Load-bearing deadlock rule: a token is held only across actual
 * searching/disassembling. Every wait (empty/full queues, reorder
 * buffer, budget exhaustion itself) happens token-free -- note in
 * particular submit's queue-full yield and the checked-out job held in
 * hand (never requeued) across pickup parks. Every such wait's progress
 * condition then depends only on token holders, which always progress,
 * or on terminal flags. Releasing wakes a waiter; shutdowns wake all.
 *
  * Per-thread holdings ride a function-local thread_local pointer (see
  * heldBudget below): at most one token per thread (true by construction
  * -- one task at a time per thread in every worker loop here), so
  * take/return pair up without threading flags through signatures.
  * A pointer, not a boolean: if several budgets ever coexist, release()
  * only returns a token to the budget that granted it.
 *
  * History and measurements live in
  * design/2026-09-22-assembly-work-stealing.md; the contract lives here.
  */
class ThreadBudget {
public:
  explicit ThreadBudget(unsigned int total) : available_(total), total_(total) {}

  ThreadBudget(const ThreadBudget &) = delete;
  ThreadBudget &operator=(const ThreadBudget &) = delete;

  unsigned int total() const { return total_; }

  /** Take a token if one is free. Leaf: budget mutex only. Re-entrant for
   * this budget (already holding reads as success without consuming
   * another token, so a double-take can never leak a permit); refuses
   * while holding a *different* budget's token, since one thread must
   * never hold two tokens at once. */
  bool tryAcquire() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (heldBudget() == this)
      return true;
    if (heldBudget() != nullptr || shutdown_ || available_ == 0)
      return false;
    available_--;
    heldBudget() = this;
    return true;
  }

  /**
   * Take a token, blocking until one is free or isTerminal() (or the
   * jthread stop, or budget shutdown). Returns true holding a token.
   */
  template <typename Pred>
  bool acquire(Pred isTerminal, std::stop_token st = {}) {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      if (heldBudget() == this)
        return true;
      if (heldBudget() != nullptr)
        return false;
    }
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, st, [&] {
      return shutdown_ || available_ > 0 || isTerminal() || st.stop_requested();
    });
    if (shutdown_ || isTerminal() || st.stop_requested()) {
      // Hand off: a token may be free while this waiter exits on terminal
      // grounds without consuming it. Without a wake, a sibling parked on
      // the same CV would sleep despite progress being possible (or despite
      // its own terminal predicate already being true). shutdown() already
      // broadcasts, but run-stop terminal has no broadcaster.
      bool handOff = (available_ > 0);
      lock.unlock();
      if (handOff)
        cv_.notify_one();
      return false;
    }
    if (available_ == 0)
      return false;
    available_--;
    heldBudget() = this;
    return true;
  }

  /** Return a held token; no-op unless this thread holds one from *this*
   * budget (a token from another budget is left alone). Wakes one waiter. */
  void release() {
    bool notify = false;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      if (heldBudget() != this)
        return;
      heldBudget() = nullptr;
      // No clamp needed: every increment pairs with a prior take, and takes
      // can't exceed the initial total while the holding protocol holds.
      available_++;
      notify = true;
    }
    // notify_one, not all: exactly one waiter can consume one freed token,
    // so waking the whole herd (up to 256 threads) per completion would be
    // pure spurious-wakeup burn under sustained pressure. A woken waiter
    // whose predicate fails re-waits; every release still wakes at least
    // one, and shutdown() below keeps notify_all so terminal exits can't
    // strand anyone.
    if (notify)
      cv_.notify_one();
  }

  /** Wake all parkers to re-check their predicates; consumes nothing and
   * changes no state. Needed to deliver stop promptly: terminal flags appear
   * only in acquire() predicates, so firing a stop source without a wake
   * would leave parkers asleep until the next release(). */
  void notify() {
    cv_.notify_all();
  }

  /** Terminal broadcast: wake all parkers; further takes fail. */
  void shutdown() {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      shutdown_ = true;
    }
    cv_.notify_all();
  }

  static bool holdsHere(const ThreadBudget *b) { return heldBudget() == b; }

private:
  // NOTE: per-thread holding as a function-local static, NOT an
  // `inline thread_local` static data member: Apple's linker rejects the
  // latter with duplicate 'thread-local wrapper routine' symbols when the
  // header is included in multiple translation units, while function-local
  // thread_locals of (implicitly inline) member functions merge correctly
  // on every toolchain we target. Keep it this way.
  static const ThreadBudget *&heldBudget() {
    static thread_local const ThreadBudget *held = nullptr;
    return held;
  }

  mutable std::mutex mtx_;
  std::condition_variable_any cv_;
  unsigned int available_;
  const unsigned int total_;
  bool shutdown_{false}; // guarded by mtx_
};

/**
 * RAII token for straight-line scopes (serial search paths). Takes on
 * construction (blocking), returns on destruction. With a null budget this
 * is a no-op and holds() is true (search freely). The token selects which
 * run's cancellation the wait honors -- pass the current run token.
 */
class BudgetGuard {
public:
  explicit BudgetGuard(ThreadBudget *budget,
                       std::stop_token stop = {})
      : budget_(budget) {
    if (budget_ != nullptr) {
      holds_ = budget_->acquire([stop] { return stop.stop_requested(); }, stop);
    } else {
      holds_ = true;
    }
  }

  ~BudgetGuard() {
    if (budget_ != nullptr && holds_)
      budget_->release();
  }

  bool holds() const { return holds_; }

  BudgetGuard(const BudgetGuard &) = delete;
  BudgetGuard &operator=(const BudgetGuard &) = delete;

private:
  ThreadBudget *budget_;
  bool holds_{false};
};

#endif // __THREAD_BUDGET_H__
