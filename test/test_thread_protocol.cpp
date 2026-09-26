#include <catch2/catch_test_macros.hpp>

#include "lib/thread_budget.h"
#include "lib/assembler_pool.h"

#include <atomic>
#include <chrono>
#include <stop_token>
#include <thread>
#include <vector>

#ifndef NDEBUG

/* Debug-only accounting for the threading protocol (SPEC-BUDGET-1,
 * SPEC-POOL-1): ThreadBudget take/return balance and AssemblyTaskPool
 * pop/finish pairing, checked live by tripwires in the headers and
 * end-of-run here. The counters compile out under NDEBUG, so this file
 * is a no-op pass in release builds (see below); per the PR #113 lesson
 * these checks gate nothing where assertions are off.
 *
 * Worker threads never use REQUIRE/CHECK directly (a Catch2 assertion
 * throwing off-thread would terminate); they record violations in
 * atomics that the main thread asserts on after joining. */

TEST_CASE("ThreadBudget balances takes and returns under contention", "[thread][protocol]") {
  ThreadBudget budget(2);

  constexpr int kThreads = 6;
  constexpr int kIters = 200;
  std::atomic<int> inside{0};
  std::atomic<bool> violated{false};

  std::vector<std::jthread> workers;
  for (int i = 0; i < kThreads; ++i) {
    workers.emplace_back([&](std::stop_token st) {
      for (int n = 0; n < kIters; ++n) {
        if (!budget.acquire([] { return false; }, st))
          return;  // only on jthread teardown; terminal is never set here
        if (!ThreadBudget::holdsHere(&budget))
          violated.store(true);
        int cur = inside.fetch_add(1) + 1;
        if (cur > 2)
          violated.store(true);  // more holders than tokens: the N-active bound
        inside.fetch_sub(1);
        budget.release();
      }
    });
  }
  for (auto &w : workers)
    w.join();  // join, never destruction: the loop must run all iterations

  CHECK(!violated.load());
  CHECK(budget.debugOutstanding() == 0);
  CHECK(budget.debugTakes() == budget.debugReturns());
  CHECK(budget.debugTakes() == static_cast<unsigned long>(kThreads * kIters));
}

TEST_CASE("ThreadBudget re-entrant take consumes no extra token", "[thread][protocol]") {
  ThreadBudget budget(1);

  CHECK(budget.tryAcquire());
  CHECK(budget.tryAcquire());  // re-entrant: success without consuming
  CHECK(budget.debugTakes() == 1);
  CHECK(!ThreadBudget::holdsHere(nullptr));
  CHECK(ThreadBudget::holdsHere(&budget));
  budget.release();
  CHECK(budget.debugOutstanding() == 0);
  CHECK(budget.debugTakes() == budget.debugReturns());
}

TEST_CASE("ThreadBudget shutdown wakes parked acquirers", "[thread][protocol]") {
  ThreadBudget budget(1);
  REQUIRE(budget.tryAcquire());  // main thread holds the only token

  std::atomic<bool> waiterReturned{false};
  std::atomic<bool> waiterGot{true};
  std::jthread waiter([&](std::stop_token st) {
    bool got = budget.acquire([] { return false; }, st);
    waiterGot.store(got);
    waiterReturned.store(true);
  });

  // The waiter parks: no token free, terminal never set. Give it a
  // scheduling quantum to reach the wait so the shutdown broadcast path
  // (not the immediate-fail path) is the one exercised. Either path must
  // return false; only the parked path proves the wakeup works.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  budget.shutdown();
  waiter.join();

  CHECK(waiterReturned.load());
  CHECK(!waiterGot.load());
  budget.release();  // in-flight holders can still return after shutdown
  CHECK(budget.debugOutstanding() == 0);
  CHECK(budget.debugTakes() == budget.debugReturns());
}

TEST_CASE("AssemblyTaskPool pairs every pop with a finish across splits", "[thread][protocol]") {
  AssemblyTaskPool<int> pool;
  pool.seed({1, 2, 3, 4});

  std::atomic<int> sum{0};
  std::atomic_flag splitDone = ATOMIC_FLAG_INIT;

  std::vector<std::jthread> workers;
  for (int i = 0; i < 3; ++i) {
    workers.emplace_back([&](std::stop_token st) {
      int task = 0;
      std::stop_token noStop;
      while (pool.pop_task(task, noStop, st)) {
        try {
          // Exactly one dynamic split per run, by whoever pops first:
          // quiescence (empty queue, nobody active) is unreachable before
          // it, so the run cannot end early and the test is deterministic.
          if (!splitDone.test_and_set())
            pool.push_tasks({5, 6});
          sum.fetch_add(task);
        } catch (...) {
          pool.finishTask();
          throw;
        }
        pool.finishTask();
      }
    });
  }
  for (auto &w : workers)
    w.join();

  CHECK(sum.load() == 1 + 2 + 3 + 4 + 5 + 6);
  CHECK(pool.debugPops() == 6);
  CHECK(pool.debugPops() == pool.debugFinishes());
  CHECK(pool.debugUnpaired() == 0);
}

TEST_CASE("AssemblyTaskPool stop preserves pairing and loses no task", "[thread][protocol]") {
  AssemblyTaskPool<int> pool;
  std::vector<int> seed(100);
  for (int i = 0; i < 100; ++i)
    seed[static_cast<size_t>(i)] = i;
  pool.seed(std::move(seed));

  std::atomic<int> finishedCount{0};

  std::vector<std::jthread> workers;
  for (int i = 0; i < 3; ++i) {
    workers.emplace_back([&](std::stop_token st) {
      int task = 0;
      std::stop_token noStop;
      while (pool.pop_task(task, noStop, st)) {
        (void)task;
        try {
          if (pool.debugPops() >= 5)
            pool.requestStop();
        } catch (...) {
          pool.finishTask();
          throw;
        }
        pool.finishTask();
        finishedCount.fetch_add(1);
      }
      // NOTE: no pairing check here -- pops minus finishes is nonzero
      // whenever a sibling is mid-task. Equality holds only once every
      // worker has terminated (quiescence), asserted below after joining.
    });
  }
  for (auto &w : workers)
    w.join();

  // Every pop paired, and every task is either finished or still queued
  // for drain()/resume: the stop-time conservation law.
  CHECK(pool.debugUnpaired() == 0);
  CHECK(pool.debugPops() == pool.debugFinishes());
  CHECK(finishedCount.load() + static_cast<int>(pool.drain().size()) == 100);
}

#else

TEST_CASE("thread protocol accounting is debug-only", "[thread][protocol]") {
  // The take/return and pop/finish counters compile out under NDEBUG, so
  // there is nothing to pin in release builds. This placeholder keeps the
  // case list stable across build types.
  SUCCEED();
}

#endif
