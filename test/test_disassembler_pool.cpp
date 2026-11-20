#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"
#include "lib/disassemblerpool.h"
#include "lib/problemcollector.h"
#include "lib/solver/solversession.h"
#include "lib/puzzle.h"
#include "lib/problem.h"
#include "lib/assembler.h"
#include "lib/assembler_1.h"
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
#include <mutex>
#include <algorithm>

namespace {

using bttest::ScopedSettings;

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

  assembler_1_c assm(makeSolverSpec(problem), bttest::settings());
  assm.setNumThreads(1);
  REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);
  assm.assemble(&cb);

  return found;
}

} // namespace

TEST_CASE("disassembler pool: equivalence with sequential disassembly", "[disasm][pool][equivalence]") {
  /* Solve Pelikan Burr twice through solverSession_c + problemCollector_c:
   * 1. Once with BURRTOOLS_DISASM_POOL=0 (sequential fallback)
   * 2. Once with multi-threaded disassembler pool (4 threads)
   * Assert identical assembly count, solution count, and solution movesText.
   */
  struct SolveData {
    unsigned long assemblies = 0;
    unsigned long solutions = 0;
    std::vector<std::string> moves;
  };

  auto runSolve = [](bool noPool) -> SolveData {
    ScopedSettings envPool;
    envPool->disassemblerPool = !noPool;
    ScopedSettings envThreads;
    envThreads->threads = 4;

    auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
    REQUIRE(p != nullptr);
    problem_c * problem = p->getProblem(0);
    REQUIRE(problem != nullptr);
    problem->removeAllSolutions();

    solverOptions_t opts;
    opts.reduce = false;
    opts.disassemble = true;
    opts.keepRotations = true;
    opts.settings = bttest::settings();

    problemCollectorOptions_t copts;
    copts.disassemble = true;
    problemCollector_c collector(*problem, copts);

    solverSession_c s(makeSolverSpec(*problem), opts);
    s.setPreparedHook([problem]() { problem->beginSolving(); });
    REQUIRE(s.start());

    collector.consumeAll(s);
    s.join();

    if (s.currentAction() == solverSession_c::ACT_ASSERT) {
      FAIL(std::string("solverSession threw assert: ") + s.getAssertException().what());
    }
    REQUIRE(s.sessionState() == solverState_e::Finished);
    problem->finishedSolving();

    SolveData data;
    data.assemblies = problem->getNumAssemblies();
    data.solutions = problem->getNumSolutions();

    for (unsigned int i = 0; i < problem->getNumberOfSavedSolutions(); i++) {
      const solution_c * sol = problem->getSavedSolution(i);
      if (sol && sol->getDisassembly()) {
        data.moves.push_back(sol->getDisassembly()->movesText());
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

TEST_CASE("disassembler pool: unit test with backpressure and unique sequence numbers", "[disasm][pool][unit]") {
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
      makeSolverSpec(*problem),
      bttest::withThreads(4),
      [&](uint64_t seqNo, std::unique_ptr<assembly_c> a, std::unique_ptr<separation_c> s) {
        (void)a;
        (void)s;
        std::lock_guard<std::mutex> lock(cbMutex);
        receivedSeqs.push_back(seqNo);
      }
    );

    // Submit 100 assemblies (more than the queue's 64) to exercise queue
    // backpressure and the sequence numbering
    const unsigned int SUBMIT_COUNT = 100;
    for (unsigned int i = 0; i < SUBMIT_COUNT; i++) {
      auto assm = std::make_unique<assembly_c>(gt);
      pool.submit(std::move(assm));
    }

    pool.finish();
  }

  // Delivered in any order, but each number 0..99 exactly once.
  REQUIRE(receivedSeqs.size() == 100);
  std::sort(receivedSeqs.begin(), receivedSeqs.end());
  for (size_t i = 0; i < receivedSeqs.size(); i++) {
    CHECK(receivedSeqs[i] == i);
  }
}

TEST_CASE("disassembler pool: requestStop salvages the backlog without loss", "[disasm][pool][salvage]") {
  /* Gated callback => deterministic backpressure: with the callback
   * blocked, every worker sits in it, the queue fills, and the submitter
   * blocks. requestStop() must then salvage the queued assemblies so that
   * delivered + salvaged == submitted exactly: nothing lost, nothing twice,
   * and the delivered sequence numbers stay gap-free (the salvage is a
   * suffix of the submit order).
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
    makeSolverSpec(*problem),
    bttest::withThreads(2),
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
  // 64 queued + 2 in flight fit with the callback gated), so it
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
  // Delivered sequence numbers are unique and gap-free: exactly
  // 0 .. delivered - 1, in any order.
  std::sort(receivedSeqs.begin(), receivedSeqs.end());
  for (size_t i = 0; i < receivedSeqs.size(); i++) {
    CHECK(receivedSeqs[i] == i);
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
      makeSolverSpec(*problem),
      bttest::withThreads(4),
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
      makeSolverSpec(*problem),
      bttest::withThreads(4),
      [](uint64_t, std::unique_ptr<assembly_c>, std::unique_ptr<separation_c>) {}
    );

    pool.abort();
    pool.abort();
    CHECK(pool.isAborted());
  }

  SECTION("finish then abort") {
    disassemblerPool_c pool(
      makeSolverSpec(*problem),
      bttest::withThreads(2),
      [](uint64_t, std::unique_ptr<assembly_c>, std::unique_ptr<separation_c>) {}
    );

    pool.finish();
    pool.abort();
  }

  SECTION("destruction with pending work") {
    {
      disassemblerPool_c pool(
        makeSolverSpec(*problem),
        bttest::withThreads(4),
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
    makeSolverSpec(*problem),
    bttest::withThreads(2),
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

TEST_CASE("disassembler pool: interactive solver session lifecycle with pause, progress, and continue", "[disasm][pool][session]") {
  SECTION("exact-once problem: pause during search, poll progress, and continue to completion") {
    auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
    REQUIRE(p != nullptr);
    problem_c * problem = p->getProblem(0);
    REQUIRE(problem != nullptr);
    problem->removeAllSolutions();

    auto spec = makeSolverSpec(*problem);
    solverOptions_t opts;
    opts.reduce = false;
    opts.disassemble = true;
    opts.keepRotations = true;
    opts.settings = bttest::settings();

    problemCollectorOptions_t copts;
    copts.disassemble = true;

    problemCollector_c collector(*problem, copts);

    solverState_e finalState1 = solverState_e::Idle;
    std::optional<resumeData_t> saved;
    {
      solverSession_c s1(spec, opts);
      s1.setPreparedHook([problem]() { problem->beginSolving(); });
      REQUIRE(s1.start(false));

      // Poll metrics during execution and pause as soon as work has begun
      while (s1.sessionState() == solverState_e::Running) {
        unsigned int act = s1.currentAction();
        s1.currentActionParameter();
        s1.getProgress();
        collector.drainFrom(s1);

        if (act == solverSession_c::ACT_ASSEMBLING ||
            act == solverSession_c::ACT_DISASSEMBLING ||
            s1.assembliesFound() > 0) {
          auto stop_start = std::chrono::steady_clock::now();
          s1.pause();
          auto stop_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - stop_start
          ).count();
          CHECK(stop_ms < 2000);
          break;
        }
        std::this_thread::yield();
      }
      s1.join();
      collector.drainFrom(s1);

      finalState1 = s1.sessionState();
      CHECK((finalState1 == solverState_e::Paused || finalState1 == solverState_e::Finished));
      if (finalState1 == solverState_e::Paused) {
        CHECK(s1.currentAction() == solverSession_c::ACT_PAUSING);
        saved = s1.state();
        problem->setResumeData(saved->clone());
      } else {
        CHECK(s1.currentAction() == solverSession_c::ACT_FINISHED);
        problem->finishedSolving();
      }
    }

    // Branch on whether the session actually finished searching, not on
    // assembly count: the search can find all 12 assemblies in the window
    // between the poll loop noticing work has started and pause() taking
    // effect, which leaves the session Paused (SS_SOLVING) rather than
    // Finished (SS_SOLVED) even though every assembly has been found.
    if (finalState1 != solverState_e::Finished) {
      CHECK(problem->getSolveState() == SS_SOLVING);
      REQUIRE(saved.has_value());

      solverSession_c s2(spec, opts);
      s2.setResume(std::move(*saved));
      REQUIRE(s2.start(false));

      while (auto r = s2.pop()) {
        s2.currentAction();
        s2.currentActionParameter();
        s2.getProgress();
        collector.consume(std::move(*r));
      }
      s2.join();

      CHECK(s2.sessionState() == solverState_e::Finished);
      CHECK(s2.currentAction() == solverSession_c::ACT_FINISHED);
      problem->finishedSolving();
      CHECK(problem->getSolveState() == SS_SOLVED);
      CHECK(problem->getNumAssemblies() == 12);
      CHECK(problem->getNumSolutions() == 1);
    } else {
      CHECK(problem->getSolveState() == SS_SOLVED);
      CHECK(problem->getNumAssemblies() == 12);
      CHECK(problem->getNumSolutions() == 1);
    }
  }

  SECTION("assembler 1 pause during search, poll progress, and continue to completion") {
    auto p = loadPuzzle("examples/CubeInCage.xmpuzzle");
    REQUIRE(p != nullptr);
    problem_c * problem = p->getProblem(0);
    REQUIRE(problem != nullptr);
    problem->removeAllSolutions();

    auto spec = makeSolverSpec(*problem);
    solverOptions_t opts;
    opts.reduce = false;
    opts.disassemble = false;
    opts.keepRotations = true;
    opts.settings = bttest::settings();

    problemCollectorOptions_t copts;
    copts.disassemble = false;

    problemCollector_c collector(*problem, copts);

    solverState_e finalState1 = solverState_e::Idle;
    std::optional<resumeData_t> saved;
    {
      solverSession_c s1(spec, opts);
      s1.setPreparedHook([problem]() { problem->beginSolving(); });
      REQUIRE(s1.start(false));

      while (s1.sessionState() == solverState_e::Running) {
        unsigned int act = s1.currentAction();
        s1.currentActionParameter();
        s1.getProgress();
        collector.drainFrom(s1);

        if (act == solverSession_c::ACT_ASSEMBLING ||
            s1.assembliesFound() > 0) {
          s1.pause();
          break;
        }
        std::this_thread::yield();
      }
      s1.join();
      collector.drainFrom(s1);

      finalState1 = s1.sessionState();
      CHECK((finalState1 == solverState_e::Paused || finalState1 == solverState_e::Finished));
      if (finalState1 == solverState_e::Paused) {
        CHECK(s1.currentAction() == solverSession_c::ACT_PAUSING);
        saved = s1.state();
        problem->setResumeData(saved->clone());
      } else {
        CHECK(s1.currentAction() == solverSession_c::ACT_FINISHED);
        problem->finishedSolving();
      }
    }

    // Branch on whether the session actually finished searching, not on
    // assembly count: the search can find all 96 assemblies in the window
    // between the poll loop noticing work has started and pause() taking
    // effect, which leaves the session Paused (SS_SOLVING) rather than
    // Finished (SS_SOLVED) even though every assembly has been found.
    if (finalState1 != solverState_e::Finished) {
      CHECK(problem->getSolveState() == SS_SOLVING);
      REQUIRE(saved.has_value());

      solverSession_c s2(spec, opts);
      s2.setResume(std::move(*saved));
      REQUIRE(s2.start(false));

      while (auto r = s2.pop()) {
        s2.currentAction();
        s2.currentActionParameter();
        s2.getProgress();
        collector.consume(std::move(*r));
      }
      s2.join();

      CHECK(s2.sessionState() == solverState_e::Finished);
      CHECK(s2.currentAction() == solverSession_c::ACT_FINISHED);
      problem->finishedSolving();
      CHECK(problem->getSolveState() == SS_SOLVED);
      CHECK(problem->getNumAssemblies() == 96);
    } else {
      CHECK(problem->getSolveState() == SS_SOLVED);
      CHECK(problem->getNumAssemblies() == 96);
    }
  }
}

namespace {

/* Stops the solve between the assembler completing its search and the
 * disassembly pool draining. The first assembly found is held back and only
 * handed to the pool after requestPause(), so the pool salvages it instead of
 * disassembling it: the run ends with the search complete but an assembly
 * still undisassembled, which is the state a GUI Stop lands in when it
 * arrives just as a fast search finishes.
 */
class stopAfterSearchInterceptor_c : public assembler_cb {
  solverSession_c & session_;
  std::mutex heldMutex_;
  std::unique_ptr<assembly_c> held_;

public:
  explicit stopAfterSearchInterceptor_c(solverSession_c & s) : session_(s) {
    session_.setAssemblyInterceptor(this);
    session_.setAssemblyPhaseEndedHook([this]() { onAssemblyPhaseEnded(); });
  }

  bool assembly(std::unique_ptr<assembly_c> a) override {
    {
      std::lock_guard<std::mutex> lock(heldMutex_);
      if (!held_) {
        held_ = std::move(a);
        return true;
      }
    }
    return session_.submitAssemblyForTest(std::move(a));
  }

  ThreadBudget * threadBudget(void) override {
    return session_.threadBudgetForTest();
  }

  void onAssemblyPhaseEnded(void) {
    session_.requestPause();
    std::lock_guard<std::mutex> lock(heldMutex_);
    if (held_)
      session_.submitAssemblyForTest(std::move(held_));
  }
};

}

TEST_CASE("solverSession_c: a stop after the search completes stays resumable until every assembly is disassembled", "[disasm][pool][session]") {
  auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);
  problem->removeAllSolutions();

  auto spec = makeSolverSpec(*problem);
  solverOptions_t opts;
  opts.reduce = false;
  opts.disassemble = true;
  opts.keepRotations = true;
  opts.settings = bttest::settings();

  problemCollectorOptions_t copts;
  copts.disassemble = true;

  problemCollector_c collector(*problem, copts);
  std::optional<resumeData_t> saved;
  {
    solverSession_c s1(spec, opts);
    stopAfterSearchInterceptor_c interceptor(s1);
    s1.setPreparedHook([problem]() { problem->beginSolving(); });
    REQUIRE(s1.start(false));

    collector.consumeAll(s1);
    s1.join();

    // The search is complete, but the held-back assembly was salvaged
    // rather than disassembled, so the solve is paused, not finished.
    REQUIRE(s1.assemblerForTest() != nullptr);
    CHECK(s1.assemblerForTest()->getFinished() >= 1);
    CHECK(s1.sessionState() == solverState_e::Paused);
    CHECK(s1.currentAction() == solverSession_c::ACT_PAUSING);
    saved = s1.state();
    CHECK_FALSE(saved->pending.empty());
    problem->setResumeData(saved->clone());
    CHECK(problem->getSolveState() == SS_SOLVING);
  }

  // Continue disassembles the pending assembly and only then finishes.
  solverSession_c s2(spec, opts);
  s2.setResume(std::move(*saved));
  REQUIRE(s2.start(false));
  collector.consumeAll(s2);
  s2.join();

  CHECK(s2.sessionState() == solverState_e::Finished);
  CHECK(s2.currentAction() == solverSession_c::ACT_FINISHED);
  problem->finishedSolving();
  CHECK(problem->getSolveState() == SS_SOLVED);
  CHECK(problem->getNumAssemblies() == 12);
  CHECK(problem->getNumSolutions() == 1);
}

TEST_CASE("solverSession_c: lifecycle guard invariants", "[session][lifecycle]") {
  // LIFECYCLE-1..4 from design/2026-10-13-solver-lifecycle.md, asserted as
  // post-mortem checks on real runs. Deterministic by construction: only
  // terminal states are inspected, no timing windows.
  auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);
  problem->removeAllSolutions();

  // LIFECYCLE-4: a fresh problem is startable.
  CHECK(problem->getSolveState() == SS_UNSOLVED);
  CHECK(problem->canStartSolving());

  auto spec = makeSolverSpec(*problem);
  solverOptions_t opts;
  opts.reduce = false;
  opts.disassemble = true;
  opts.keepRotations = true;
  opts.settings = bttest::settings();

  problemCollectorOptions_t copts;
  copts.disassemble = true;
  problemCollector_c collector(*problem, copts);

  solverSession_c s(spec, opts);
  s.setPreparedHook([problem]() { problem->beginSolving(); });
  REQUIRE(s.start(false));
  collector.consumeAll(s);
  s.join();
  REQUIRE(s.sessionState() == solverState_e::Finished);
  REQUIRE(s.currentAction() == solverSession_c::ACT_FINISHED);
  problem->finishedSolving();

  // LIFECYCLE-1: FINISHED implies solved, search complete, nothing stashed.
  CHECK(problem->getSolveState() == SS_SOLVED);
  REQUIRE(s.assemblerForTest() != nullptr);
  CHECK(s.assemblerForTest()->getFinished() >= 1);
  auto stashed = problem->takeStashedAssemblies();
  CHECK(stashed.empty());

  // LIFECYCLE-4: a solved problem must be reset before restarting.
  CHECK(!problem->canStartSolving());

  // LIFECYCLE-3: pause() and abort() are no-ops on terminal states.
  s.pause();
  CHECK(s.sessionState() == solverState_e::Finished);
  CHECK(s.currentAction() == solverSession_c::ACT_FINISHED);
  s.abort();
  CHECK(s.sessionState() == solverState_e::Finished);
  CHECK(s.currentAction() == solverSession_c::ACT_FINISHED);

  // LIFECYCLE-4: reset restores startability; LIFECYCLE-2 is covered by the
  // pause-branch checks in the lifecycle tests above (Paused implies
  // SS_SOLVING with resume data present).
  problem->removeAllSolutions();
  CHECK(problem->getSolveState() == SS_UNSOLVED);
  CHECK(problem->canStartSolving());
}

TEST_CASE("solverSession_c: lifecycle misuse is rejected loudly in all build modes", "[session][lifecycle]") {
  // LIFECYCLE-4 enforcement: misuse fails loud unconditionally. Double-start,
  // reuse after finish, and state() outside Paused throw on the caller; an
  // already-solved problem refuses beginSolving()/setAssembler() without
  // clobbering its stored solutions.
  auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);
  problem->removeAllSolutions();

  auto spec = makeSolverSpec(*problem);
  solverOptions_t opts;
  opts.reduce = false;
  opts.disassemble = true;
  opts.keepRotations = true;
  opts.settings = bttest::settings();

  problemCollectorOptions_t copts;
  copts.disassemble = true;
  problemCollector_c collector(*problem, copts);

  solverSession_c s(spec, opts);
  s.setPreparedHook([problem]() { problem->beginSolving(); });
  REQUIRE(s.start(false));

  // Double-start while running throws instead of clobbering the worker.
  CHECK_THROWS_AS(s.start(false), assert_exception);

  collector.consumeAll(s);
  s.join();
  REQUIRE(s.sessionState() == solverState_e::Finished);
  REQUIRE(s.currentAction() == solverSession_c::ACT_FINISHED);
  problem->finishedSolving();

  // Reuse after finish and state() outside Paused throw.
  CHECK_THROWS_AS(s.start(false), assert_exception);
  CHECK_THROWS_AS(s.state(), assert_exception);

  // A solved problem refuses starting without reset and stays intact.
  CHECK_FALSE(problem->canStartSolving());
  CHECK_THROWS_AS(problem->beginSolving(), assert_exception);
  CHECK(problem->getSolveState() == SS_SOLVED);
  CHECK(problem->getNumAssemblies() == 12);
  CHECK(problem->getNumSolutions() == 1);

  // After the reset the problem solves cleanly with a new session.
  problem->removeAllSolutions();
  problemCollector_c collector2(*problem, copts);
  solverSession_c resumed(spec, opts);
  resumed.setPreparedHook([problem]() { problem->beginSolving(); });
  REQUIRE(resumed.start(false));
  collector2.consumeAll(resumed);
  resumed.join();
  CHECK(resumed.sessionState() == solverState_e::Finished);
  CHECK(resumed.currentAction() == solverSession_c::ACT_FINISHED);
  problem->finishedSolving();
  CHECK(problem->getSolveState() == SS_SOLVED);
  CHECK(problem->getNumAssemblies() == 12);
}

TEST_CASE("solverSession_c: explicit thread count reaches the assembler", "[session][threads]") {
  auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);
  problem->removeAllSolutions();

  solverOptions_t opts;
  opts.reduce = false;
  opts.disassemble = false;
  opts.keepRotations = true;
  opts.settings = bttest::withThreads(2);

  solverSession_c s(makeSolverSpec(*problem), opts);
  REQUIRE(s.start(false));
  while (s.pop()) {}
  s.join();

  if (s.currentAction() == solverSession_c::ACT_ASSERT) {
    FAIL(std::string("solverSession threw assert: ") + s.getAssertException().what());
  }

  REQUIRE(s.sessionState() == solverState_e::Finished);
  REQUIRE(s.assemblerForTest() != nullptr);
  CHECK(s.assemblerForTest()->getNumThreads() == 2);
}

/* The progress model weights disassembly against assembly by measured seconds,
 * so the pool has to report how much it has done and what it cost -- on both
 * of its internal code paths: the multi-threaded worker pipeline, and
 * the single-threaded "inline" path submit() takes when the pool collapses to
 * one thread (num_threads <= 1, or BURRTOOLS_DISASM_POOL=0). Both
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

    disassemblerPool_c pool(makeSolverSpec(*problem), bttest::withThreads(2), nullptr);
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

    disassemblerPool_c pool(makeSolverSpec(*problem), bttest::withThreads(1), nullptr);
    const unsigned long n = found.size();
    for (auto & a : found) pool.submit(std::move(a));
    pool.finish();

    CHECK(pool.submittedCount() == n);
    CHECK(pool.completedCount() == n);
    CHECK(pool.accumulatedCostSeconds() > 0.0);
  }
}
