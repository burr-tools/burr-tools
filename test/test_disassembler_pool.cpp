#include <catch2/catch_test_macros.hpp>

#include "lib/disassemblerpool.h"
#include "lib/solvethread.h"
#include "lib/puzzle.h"
#include "lib/problem.h"
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
      unsigned int final_act1 = st1.currentAction();
      CHECK((final_act1 == solveThread_c::ACT_PAUSING || final_act1 == solveThread_c::ACT_FINISHED));
      if (problem->numAssembliesKnown()) {
        first_phase_assemblies = problem->getNumAssemblies();
      }
    }

    // If it paused before finding all assemblies, continue to completion
    if (first_phase_assemblies < 12) {
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
      unsigned int final_act1 = st1.currentAction();
      CHECK((final_act1 == solveThread_c::ACT_PAUSING || final_act1 == solveThread_c::ACT_FINISHED));
      if (problem->numAssembliesKnown()) {
        first_phase_assemblies = problem->getNumAssemblies();
      }
    }

    // If it paused before finding all assemblies, continue to completion
    if (first_phase_assemblies < 96) {
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

