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
 * Shared cap on concurrently *working* solver threads (design section 6.3).
 *
 * One ThreadBudget object is shared by the assembly task pool(s) and the
 * disassembly pool of a single solve. An assembler holds one token across a
 * whole subtree task (many submits, never per-submit pacing); a
 * disassembler holds one per disassembly job. Searching + disassembling
 * threads therefore never exceed total().
 *
 * Anti-deadlock contract, load-bearing: a token is held only across actual
 * searching/disassembling. Every wait (empty/full queues, reorder buffer,
 * budget exhaustion itself) happens token-free, and every such wait's
 * progress condition depends only on token holders (which always progress)
 * or terminal flags. Releasing always broadcasts.
 *
 * Per-thread holdings are tracked in t_holds: at most one token per thread
 * (true by construction -- no worker holds across tasks), so take and
 * return pair up even across the submit yield path without threading flags
 * through call signatures. One task at a time per thread is assumed;
 * every worker loop in this codebase satisfies it.
 */
class ThreadBudget {
public:
  explicit ThreadBudget(unsigned int total) : available_(total), total_(total) {}

  ThreadBudget(const ThreadBudget &) = delete;
  ThreadBudget &operator=(const ThreadBudget &) = delete;

  unsigned int total() const { return total_; }

  /** Take a token if one is free. Leaf: budget mutex only. */
  bool tryAcquire() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (shutdown_ || available_ == 0)
      return false;
    available_--;
    t_holds = true;
    return true;
  }

  /**
   * Take a token, blocking until one is free or isTerminal() (or the
   * jthread stop, or budget shutdown). Returns true holding a token.
   */
  template <typename Pred>
  bool acquire(Pred isTerminal, std::stop_token st = {}) {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, st, [&] {
      return shutdown_ || available_ > 0 || isTerminal() || st.stop_requested();
    });
    if (shutdown_ || isTerminal() || st.stop_requested())
      return false;
    if (available_ == 0)
      return false;
    available_--;
    t_holds = true;
    return true;
  }

  /** Return a held token; no-op unless this thread holds one. Wakes one waiter. */
  void release() {
    bool notify = false;
    {
      std::lock_guard<std::mutex> lock(mtx_);
      if (!t_holds)
        return;
      t_holds = false;
      // No clamp needed: every increment pairs with a prior take, and takes
      // can't exceed the initial total while the flag protocol holds.
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

  /** Terminal broadcast: wake all parkers; further takes fail. */
  void shutdown() {
    {
      std::lock_guard<std::mutex> lock(mtx_);
      shutdown_ = true;
    }
    cv_.notify_all();
  }

  static bool holdsHere() { return t_holds; }

private:
  mutable std::mutex mtx_;
  std::condition_variable_any cv_;
  unsigned int available_;
  const unsigned int total_;
  bool shutdown_{false}; // guarded by mtx_

  static thread_local bool t_holds;
};

inline thread_local bool ThreadBudget::t_holds = false;

/**
 * RAII token for straight-line scopes (serial search paths). Takes on
 * construction (blocking), returns on destruction. With a null budget this
 * is a no-op and holds() is true (search freely).
 */
class BudgetGuard {
public:
  explicit BudgetGuard(ThreadBudget *budget,
                       const std::atomic<bool> *abort = nullptr)
      : budget_(budget) {
    if (budget_ != nullptr) {
      holds_ = budget_->acquire(
          [&] { return abort != nullptr && abort->load(std::memory_order_relaxed); });
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
