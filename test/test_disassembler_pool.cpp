#include <catch2/catch_test_macros.hpp>

#include "lib/disassemblerpool.h"
#include "lib/solvethread.h"
#include "lib/puzzle.h"
#include "lib/problem.h"
#include "lib/assembler.h"
#include "lib/assembler_0.h"
#include "lib/assembly.h"
#include "lib/disassembly.h"
#include "lib/solution.h"
#include "lib/gridtype.h"
#include "lib/bt_assert.h"
#include "tools/gzstream.h"
#include "tools/xml.h"

#include <chrono>
#include <thread>
#include <vector>
#include <memory>
#include <string>
#include <sstream>
#include <cstdlib>
#include <atomic>

namespace {

#ifdef _WIN32
#include <stdlib.h>
void set_env_var(const char * name, const char * value) {
  if (value) _putenv_s(name, value);
  else _putenv_s(name, "");
}
#else
#include <cstdlib>
void set_env_var(const char * name, const char * value) {
  if (value) setenv(name, value, 1);
  else unsetenv(name);
}
#endif

struct ScopedEnv {
  std::string name;
  bool hadValue;
  std::string oldValue;

  ScopedEnv(const char * var, const char * val) : name(var) {
    const char * existing = std::getenv(var);
    if (existing) {
      hadValue = true;
      oldValue = existing;
    } else {
      hadValue = false;
    }
    set_env_var(var, val);
  }

  ~ScopedEnv() {
    set_env_var(name.c_str(), hadValue ? oldValue.c_str() : nullptr);
  }
};

std::unique_ptr<puzzle_c> loadPuzzle(const char * path) {
  auto str = openGzFile(path);
  if (!str) return nullptr;
  xmlParser_c pars(*str);
  return std::make_unique<puzzle_c>(pars);
}

/* Collects a handful of real assemblies from a solved problem to feed a
 * disassembler pool with. Every assembly here has placementCount() > 1 (the
 * puzzle has multiple pieces), so each one triggers a real disassemble()
 * call and real wall-clock cost -- unlike the default-constructed
 * assembly_c(gt) used elsewhere in this file, which has 0 placements and
 * never reaches disassemble() at all.
 */
std::vector<std::unique_ptr<assembly_c>> collectSomeAssemblies(const problem_c & problem, size_t limit) {
  std::vector<std::unique_ptr<assembly_c>> found;

  class Collect : public assembler_cb {
  public:
    std::vector<std::unique_ptr<assembly_c>> * out;
    size_t limit;
    bool assembly(std::unique_ptr<assembly_c> a) override {
      if (out->size() < limit) out->push_back(std::move(a));
      return true;
    }
  } cb;
  cb.out = &found;
  cb.limit = limit;

  assembler_0_c assm(problem);
  assm.setNumThreads(1);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
  assm.assemble(&cb);

  return found;
}

} // namespace

TEST_CASE("disassembler pool: equivalence with sequential disassembly", "[disasm][pool][equivalence]") {
  /* Solve Pelikan Burr twice through solveThread_c:
   * 1. Once with BURRTOOLS_NO_DISASM_POOL=1 (sequential fallback)
   * 2. Once with multi-threaded disassembler pool (4 threads)
   * Assert identical assembly count, solution count, and solution movesText.
   */
  struct SolveData {
    unsigned long assemblies = 0;
    unsigned long solutions = 0;
    std::vector<std::string> moves;
  };

  auto runSolve = [](bool noPool) -> SolveData {
    ScopedEnv envPool("BURRTOOLS_NO_DISASM_POOL", noPool ? "1" : nullptr);
    ScopedEnv envThreads("BURRTOOLS_THREADS", "4");

    auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
    REQUIRE(p != nullptr);
    problem_c * problem = p->getProblem(0);
    REQUIRE(problem != nullptr);
    problem->removeAllSolutions();

    int par = solveThread_c::PAR_DISASSM |
              solveThread_c::PAR_KEEP_ROTATIONS |
              solveThread_c::PAR_KEEP_MIRROR;

    solveThread_c st(*problem, par);
    REQUIRE(st.start());

    while (st.isRunning()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (st.currentAction() == solveThread_c::ACT_ASSERT) {
      FAIL(std::string("solveThread threw assert: ") + st.getAssertException().what());
    }

    SolveData data;
    data.assemblies = problem->getNumAssemblies();
    data.solutions = problem->getNumSolutions();

    for (unsigned int i = 0; i < problem->getNumberOfSavedSolutions(); i++) {
      const solution_c * s = problem->getSavedSolution(i);
      if (s && s->getDisassembly()) {
        data.moves.push_back(s->getDisassembly()->movesText());
      }
    }

    return data;
  };

  SolveData seqData = runSolve(true);
  SolveData poolData = runSolve(false);

  CHECK(seqData.assemblies == 12);
  CHECK(seqData.solutions == 1);
  REQUIRE(seqData.moves.size() == 1);
  CHECK(seqData.moves[0] == "98.2.4.2");

  CHECK(poolData.assemblies == seqData.assemblies);
  CHECK(poolData.solutions == seqData.solutions);
  REQUIRE(poolData.moves.size() == seqData.moves.size());
  for (size_t i = 0; i < seqData.moves.size(); i++) {
    CHECK(poolData.moves[i] == seqData.moves[i]);
  }
}

TEST_CASE("disassembler pool: unit test with backpressure and sequence ordering", "[disasm][pool][unit]") {
  auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  // Get a valid assembly from saved solutions if any, or create a dummy assembly
  const gridType_c * gt = problem->getPuzzle().getGridType();
  REQUIRE(gt != nullptr);

  std::vector<uint64_t> receivedSeqs;
  std::mutex cbMutex;

  {
    disassemblerPool_c pool(
      *problem,
      4,
      [&](uint64_t seqNo, std::unique_ptr<assembly_c> a, std::unique_ptr<separation_c> s) {
        (void)a;
        (void)s;
        std::lock_guard<std::mutex> lock(cbMutex);
        receivedSeqs.push_back(seqNo);
      }
    );

    // Submit 100 assemblies (greater than max_reorder_size = 64) to exercise permit throttling, reorder-window backpressure, and sequence ordering
    const unsigned int SUBMIT_COUNT = 100;
    for (unsigned int i = 0; i < SUBMIT_COUNT; i++) {
      auto assm = std::make_unique<assembly_c>(gt);
      pool.submit(std::move(assm));
    }

    pool.finish();
  }

  REQUIRE(receivedSeqs.size() == 100);
  for (size_t i = 0; i < receivedSeqs.size(); i++) {
    CHECK(receivedSeqs[i] == i);
  }
}

TEST_CASE("disassembler pool: requestStop salvages the backlog without loss", "[disasm][pool][salvage]") {
  /* Gated merger => deterministic backpressure: with the callback blocked,
   * the reorder buffer fills, workers park, the queue fills, and the
   * submitter blocks. requestStop() must then salvage the queued assemblies
   * (merger skips their sequence numbers via dropped_) so that
   * delivered + salvaged == submitted exactly: nothing lost, nothing twice.
   */
  auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);
  const gridType_c * gt = problem->getPuzzle().getGridType();
  REQUIRE(gt != nullptr);

  std::atomic<bool> gate{false};
  std::vector<uint64_t> receivedSeqs;
  std::mutex cbMutex;

  disassemblerPool_c pool(
    *problem,
    2,
    [&](uint64_t seqNo, std::unique_ptr<assembly_c> a, std::unique_ptr<separation_c> s) {
      (void)a;
      (void)s;
      while (!gate.load(std::memory_order_acquire))
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      std::lock_guard<std::mutex> lock(cbMutex);
      receivedSeqs.push_back(seqNo);
    }
  );

  static constexpr unsigned int SUBMIT_COUNT = 200;
  std::atomic<unsigned int> attempted{0};
  std::thread submitter([&]() {
    for (unsigned int i = 0; i < SUBMIT_COUNT; i++) {
      auto assm = std::make_unique<assembly_c>(gt);
      attempted.fetch_add(1, std::memory_order_relaxed);
      if (!pool.submit(std::move(assm)))
        break; // rejected after stop: the assembly is salvaged in the pool
    }
  });

  // The submitter outruns 2 workers by orders of magnitude (at most
  // 64 filed + 64 queued + 2 in flight fit with the merger gated), so it
  // is blocked on backpressure long before this sleep ends.
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  pool.requestStop();
  gate.store(true, std::memory_order_release);
  submitter.join();
  pool.finish();
  std::vector<std::unique_ptr<assembly_c>> salvaged = pool.takeSalvaged();

  // Exact accounting: every submitted assembly was either delivered or
  // salvaged (submit rejections land in the salvage, never dropped).
  REQUIRE(receivedSeqs.size() + salvaged.size() == attempted.load());
  // Delivered sequence numbers are strictly increasing and unique (the
  // merger still emits in submit order, skipping dropped sequences).
  for (size_t i = 1; i < receivedSeqs.size(); i++) {
    CHECK(receivedSeqs[i] > receivedSeqs[i - 1]);
  }
  // The submitter provably hit backpressure, so there was backlog to salvage.
  CHECK(!salvaged.empty());
  for (const auto &a : salvaged) {
    CHECK(a != nullptr);
  }
}

TEST_CASE("problem: stashed assemblies survive save and reload", "[stash][roundtrip]") {
  auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);
  const gridType_c * gt = problem->getPuzzle().getGridType();
  REQUIRE(gt != nullptr);

  // Build valid (all non-placed) assemblies: the loader requires exactly
  // one placement entry per part.
  unsigned int pieces = 0;
  for (unsigned int i = 0; i < problem->getNumberOfParts(); i++)
    pieces += problem->getPartMaximum(i);
  REQUIRE(pieces > 0);

  std::vector<std::unique_ptr<assembly_c>> v;
  for (int k = 0; k < 3; k++) {
    auto a = std::make_unique<assembly_c>(gt);
    for (unsigned int i = 0; i < pieces; i++)
      a->addNonPlacement();
    v.push_back(std::move(a));
  }
  problem->stashAssemblies(std::move(v));

  std::ostringstream saved;
  {
    xmlWriter_c xml(saved);
    p->save(xml);
  }
  REQUIRE(saved.str().size() > 0);

  std::istringstream reloaded(saved.str());
  xmlParser_c pars(reloaded);
  puzzle_c restored(pars);

  problem_c * restoredProblem = restored.getProblem(0);
  REQUIRE(restoredProblem != nullptr);
  std::vector<std::unique_ptr<assembly_c>> back = restoredProblem->takeStashedAssemblies();
  REQUIRE(back.size() == 3);
  for (const auto &a : back) {
    CHECK(a != nullptr);
  }
}

TEST_CASE("disassembler pool: lifecycle abort without hang or leaks", "[disasm][pool][lifecycle]") {
  auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);
  const gridType_c * gt = problem->getPuzzle().getGridType();

  SECTION("abort without finish") {
    disassemblerPool_c pool(
      *problem,
      4,
      [](uint64_t, std::unique_ptr<assembly_c>, std::unique_ptr<separation_c>) {}
    );

    for (int i = 0; i < 20; i++) {
      pool.submit(std::make_unique<assembly_c>(gt));
    }

    pool.abort();
    CHECK(pool.isAborted());
  }

  SECTION("double abort") {
    disassemblerPool_c pool(
      *problem,
      4,
      [](uint64_t, std::unique_ptr<assembly_c>, std::unique_ptr<separation_c>) {}
    );

    pool.abort();
    pool.abort();
    CHECK(pool.isAborted());
  }

  SECTION("finish then abort") {
    disassemblerPool_c pool(
      *problem,
      2,
      [](uint64_t, std::unique_ptr<assembly_c>, std::unique_ptr<separation_c>) {}
    );

    pool.finish();
    pool.abort();
  }

  SECTION("destruction with pending work") {
    {
      disassemblerPool_c pool(
        *problem,
        4,
        [](uint64_t, std::unique_ptr<assembly_c>, std::unique_ptr<separation_c>) {}
      );

      for (int i = 0; i < 30; i++) {
        pool.submit(std::make_unique<assembly_c>(gt));
      }
      // Destructor invokes abort()
    }
  }
}

TEST_CASE("disassembler pool: exception in callback propagates cleanly to finish()", "[disasm][pool][exception]") {
  auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);
  const gridType_c * gt = problem->getPuzzle().getGridType();

  disassemblerPool_c pool(
    *problem,
    2,
    [](uint64_t seqNo, std::unique_ptr<assembly_c>, std::unique_ptr<separation_c>) {
      if (seqNo == 2) {
        throw std::runtime_error("simulated callback error");
      }
    }
  );

  for (int i = 0; i < 10; i++) {
    pool.submit(std::make_unique<assembly_c>(gt));
  }

  CHECK_THROWS_AS(pool.finish(), std::runtime_error);
}

TEST_CASE("disassembler pool: interactive solve thread lifecycle with pause, progress, and continue", "[disasm][pool][solvethread]") {
  SECTION("assembler 0 pause during search, poll progress, and continue to completion") {
    auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
    REQUIRE(p != nullptr);
    problem_c * problem = p->getProblem(0);
    REQUIRE(problem != nullptr);
    problem->removeAllSolutions();

    int par = solveThread_c::PAR_DISASSM |
              solveThread_c::PAR_KEEP_ROTATIONS |
              solveThread_c::PAR_KEEP_MIRROR;

    unsigned long first_phase_assemblies = 0;
    unsigned int final_act1 = solveThread_c::ACT_PAUSING;
    {
      solveThread_c st1(*problem, par);
      REQUIRE(st1.start(false));

      // Poll metrics during execution and pause as soon as work has begun
      while (st1.isRunning()) {
        unsigned int act = st1.currentAction();
        st1.currentActionParameter();
        st1.getTime();

        if (act == solveThread_c::ACT_ASSEMBLING ||
            act == solveThread_c::ACT_DISASSEMBLING ||
            (problem->numAssembliesKnown() && problem->getNumAssemblies() > 0)) {
          auto stop_start = std::chrono::steady_clock::now();
          st1.stop();
          while (st1.isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
          }
          auto stop_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - stop_start
          ).count();
          CHECK(stop_ms < 2000);
          break;
        }
        std::this_thread::yield();
      }

      CHECK(st1.stopped());
      final_act1 = st1.currentAction();
      CHECK((final_act1 == solveThread_c::ACT_PAUSING || final_act1 == solveThread_c::ACT_FINISHED));
      if (problem->numAssembliesKnown()) {
        first_phase_assemblies = problem->getNumAssemblies();
      }
    }

    // Branch on whether the thread actually finished searching, not on
    // assembly count: the search can find all 12 assemblies in the window
    // between the poll loop noticing work has started and stop() taking
    // effect, which leaves the thread paused (SS_SOLVING) rather than
    // finished (SS_SOLVED) even though every assembly has been found.
    if (final_act1 != solveThread_c::ACT_FINISHED) {
      CHECK(problem->getSolveState() == SS_SOLVING);

      solveThread_c st2(*problem, par);
      REQUIRE(st2.start(false));

      while (st2.isRunning()) {
        st2.currentAction();
        st2.currentActionParameter();
        st2.getTime();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }

      CHECK(st2.stopped());
      CHECK(st2.currentAction() == solveThread_c::ACT_FINISHED);
      CHECK(problem->getSolveState() == SS_SOLVED);
      CHECK(problem->getNumAssemblies() == 12);
      CHECK(problem->getNumSolutions() == 1);
    } else {
      CHECK(problem->getSolveState() == SS_SOLVED);
      CHECK(first_phase_assemblies == 12);
      CHECK(problem->getNumSolutions() == 1);
    }
  }

  SECTION("assembler 1 pause during search, poll progress, and continue to completion") {
    auto p = loadPuzzle("examples/CubeInCage.xmpuzzle");
    REQUIRE(p != nullptr);
    problem_c * problem = p->getProblem(0);
    REQUIRE(problem != nullptr);
    problem->removeAllSolutions();

    int par = solveThread_c::PAR_KEEP_ROTATIONS |
              solveThread_c::PAR_KEEP_MIRROR;

    unsigned long first_phase_assemblies = 0;
    unsigned int final_act1 = solveThread_c::ACT_PAUSING;
    {
      solveThread_c st1(*problem, par);
      REQUIRE(st1.start(false));

      while (st1.isRunning()) {
        unsigned int act = st1.currentAction();
        st1.currentActionParameter();
        st1.getTime();

        if (act == solveThread_c::ACT_ASSEMBLING ||
            (problem->numAssembliesKnown() && problem->getNumAssemblies() > 0)) {
          st1.stop();
          break;
        }
        std::this_thread::yield();
      }

      while (st1.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }

      CHECK(st1.stopped());
      final_act1 = st1.currentAction();
      CHECK((final_act1 == solveThread_c::ACT_PAUSING || final_act1 == solveThread_c::ACT_FINISHED));
      if (problem->numAssembliesKnown()) {
        first_phase_assemblies = problem->getNumAssemblies();
      }
    }

    // Branch on whether the thread actually finished searching, not on
    // assembly count: the search can find all 96 assemblies in the window
    // between the poll loop noticing work has started and stop() taking
    // effect, which leaves the thread paused (SS_SOLVING) rather than
    // finished (SS_SOLVED) even though every assembly has been found.
    if (final_act1 != solveThread_c::ACT_FINISHED) {
      CHECK(problem->getSolveState() == SS_SOLVING);

      solveThread_c st2(*problem, par);
      REQUIRE(st2.start(false));

      while (st2.isRunning()) {
        st2.currentAction();
        st2.currentActionParameter();
        st2.getTime();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }

      CHECK(st2.stopped());
      CHECK(st2.currentAction() == solveThread_c::ACT_FINISHED);
      CHECK(problem->getSolveState() == SS_SOLVED);
      CHECK(problem->getNumAssemblies() == 96);
    } else {
      CHECK(problem->getSolveState() == SS_SOLVED);
      CHECK(first_phase_assemblies == 96);
    }
  }
}

TEST_CASE("solveThread_c: explicit thread count reaches the assembler", "[solvethread][threads]") {
  auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);
  problem->removeAllSolutions();

  int par = solveThread_c::PAR_KEEP_ROTATIONS | solveThread_c::PAR_KEEP_MIRROR;

  solveThread_c st(*problem, par, 2);
  REQUIRE(st.start());

  while (st.isRunning()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  if (st.currentAction() == solveThread_c::ACT_ASSERT) {
    FAIL(std::string("solveThread threw assert: ") + st.getAssertException().what());
  }

  REQUIRE(problem->getAssembler() != nullptr);
  CHECK(problem->getAssembler()->getNumThreads() == 2);
}

/* The progress model weights disassembly against assembly by measured seconds,
 * so the pool has to report how much it has done and what it cost -- on both
 * of its internal code paths: the multi-threaded worker/merger pipeline, and
 * the single-threaded "inline" path submit() takes when the pool collapses to
 * one thread (num_threads <= 1, or BURRTOOLS_NO_DISASM_POOL is set). Both
 * paths run real disassemble() work, so both must advance the counters and
 * the cost identically as far as a consumer of completedCount()/
 * submittedCount() is concerned.
 */
TEST_CASE("disassembler pool reports counts and accumulated cost",
          "[disasm][pool][progress]") {
  auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  SECTION("threaded pool (num_threads > 1)") {
    auto found = collectSomeAssemblies(*problem, 5);
    REQUIRE_FALSE(found.empty());

    disassemblerPool_c pool(*problem, 2, nullptr);
    const unsigned long n = found.size();
    for (auto & a : found) pool.submit(std::move(a));
    pool.finish();

    CHECK(pool.submittedCount() == n);
    CHECK(pool.completedCount() == n);
    // Real disassemble() work ran on every submitted assembly (all have
    // placementCount() > 1, from a 12-piece puzzle), so measured wall time
    // must be strictly positive -- an untouched or deleted timing block
    // would leave this at exactly 0.0.
    CHECK(pool.accumulatedCostSeconds() > 0.0);
  }

  SECTION("inline pool (num_threads == 1, submit() runs disassemble() synchronously)") {
    auto found = collectSomeAssemblies(*problem, 5);
    REQUIRE_FALSE(found.empty());

    disassemblerPool_c pool(*problem, 1, nullptr);
    const unsigned long n = found.size();
    for (auto & a : found) pool.submit(std::move(a));
    pool.finish();

    CHECK(pool.submittedCount() == n);
    CHECK(pool.completedCount() == n);
    CHECK(pool.accumulatedCostSeconds() > 0.0);
  }
}
