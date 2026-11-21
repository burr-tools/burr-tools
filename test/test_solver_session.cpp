/* Tests of solverSession_c (src/lib/solver/solversession.h):
 * - bounded pull queue (pop, drain, backpressure, onWake)
 * - assembly-only, disassembly across thread counts, countOnly
 * - in-place pause() -> state() -> resume() AND cross-session setResume()
 *   at every stop point (mid-search, full queue, pool seed, pool drained,
 *   assembly phase ended), verifying nothing lost and nothing repeated on
 *   both bit-parallel and Dancing Cells engines
 * - resume validation error policy and abort() in every state
 * - B4's check: chained pause -> resume -> pause at many points (pop counts,
 *   every firing of the pool hooks, right after resume() while pending
 *   assemblies are re-submitted, during preparation), with and without
 *   disassembly, seq uniqueness and gap-freeness, both resume paths against
 *   an uninterrupted run. Result order is unspecified: only the set and
 *   count of assemblies and solutions are compared
 * - abort() racing the end-of-phase decision and start()'s validation,
 *   pending assemblies continued without disassembly, exceptions
 */
#include <catch2/catch_test_macros.hpp>

#include "lib/solver/solversession.h"
#include "lib/solver/solverspec.h"
#include "lib/assembler_1.h"
#include "lib/problem.h"
#include "lib/problemcollector.h"
#include "lib/puzzle.h"
#include "lib/solution.h"
#include "tools/xml.h"
#include "test_helpers.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

std::unique_ptr<puzzle_c> loadPuzzle(const char * path) {
  auto p = puzzle_c::load(path);
  REQUIRE(p != nullptr);
  return p;
}

std::string fingerprint(const assembly_c & a) {
  std::string s;
  for (unsigned int i = 0; i < a.placementCount(); i++) {
    s += std::to_string(i);
    if (a.isPlaced(i)) {
      s += ":" + std::to_string(a.getX(i)) + "," + std::to_string(a.getY(i)) +
           "," + std::to_string(a.getZ(i)) + "," +
           std::to_string(static_cast<unsigned int>(a.getTransformation(i))) + ";";
    } else {
      s += ":-;";
    }
  }
  return s;
}

struct Collected {
  std::multiset<std::string> assemblies;
  std::multiset<std::string> solutions;
  std::vector<uint64_t> seqs;
};

void recordResult(Collected & out, const solverResult_t & r) {
  REQUIRE(r.assembly != nullptr);
  std::string fp = fingerprint(*r.assembly);
  out.assemblies.insert(fp);
  if (r.assembly->placementCount() <= 1 || r.separation != nullptr)
    out.solutions.insert(fp);
  out.seqs.push_back(r.seq);
}

Collected runToCompletion(std::shared_ptr<const solverSpec_t> spec, solverOptions_t opts) {
  solverSession_c session(std::move(spec), std::move(opts));
  REQUIRE(session.start());
  Collected out;
  while (auto r = session.pop())
    recordResult(out, *r);
  session.join();
  REQUIRE(session.sessionState() == solverState_e::Finished);
  auto info = session.runInfo();
  REQUIRE(info.has_value());
  CHECK(info->complete);
  CHECK(info->error == assembler_c::ERR_NONE);
  CHECK(info->assembliesFound == out.assemblies.size());
  if (session.solutionsFound() > 0)
    CHECK(info->solutionsFound == out.solutions.size());
  return out;
}

} // namespace

TEST_CASE("solverSession_c: assembly-only, disassembly across thread counts, countOnly, backpressure",
          "[session][parallel]") {
  auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  auto spec = makeSolverSpec(*p->getProblem(0));

  SECTION("assembly-only with tiny queueCapacity and onWake") {
    solverOptions_t opts;
    opts.disassemble = false;
    opts.queueCapacity = 2;
    opts.settings = bttest::withThreads(4);

    std::atomic<unsigned int> wakes{0};
    solverSession_c session(spec, opts, [&]() { wakes.fetch_add(1, std::memory_order_relaxed); });
    REQUIRE(session.start());

    Collected out;
    while (auto r = session.pop()) {
      CHECK(r->separation == nullptr);
      recordResult(out, *r);
    }
    session.join();
    CHECK(session.sessionState() == solverState_e::Finished);
    CHECK(out.assemblies.size() == 12);
    CHECK(session.assembliesFound() == 12);
    CHECK(session.solutionsFound() == 0);
    CHECK(session.getProgress() == 1.0f);
    CHECK(wakes.load(std::memory_order_relaxed) >= 1);
  }

  SECTION("disassembly with 4 threads and with 1 produces the exact same multisets") {
    solverOptions_t parallelOpts;
    parallelOpts.disassemble = true;
    parallelOpts.queueCapacity = 4;
    parallelOpts.settings = bttest::withThreads(4);
    Collected parallel = runToCompletion(spec, parallelOpts);
    CHECK(parallel.assemblies.size() == 12);
    CHECK(parallel.solutions.size() == 1);
    // seq: unique and gap-free, in whatever order the results arrived
    std::vector<uint64_t> sorted = parallel.seqs;
    std::sort(sorted.begin(), sorted.end());
    for (size_t i = 0; i < sorted.size(); i++)
      CHECK(sorted[i] == i);

    solverOptions_t serialOpts = parallelOpts;
    serialOpts.settings = bttest::withThreads(1);
    Collected serial = runToCompletion(spec, serialOpts);
    CHECK(serial.assemblies == parallel.assemblies);
    CHECK(serial.solutions == parallel.solutions);
  }

  SECTION("countOnly enqueues nothing and keeps exact counts") {
    for (bool disasm : {false, true}) {
      solverOptions_t opts;
      opts.disassemble = disasm;
      opts.countOnly = true;
      opts.settings = bttest::withThreads(4);

      solverSession_c session(spec, opts);
      REQUIRE(session.start());
      CHECK_FALSE(session.pop().has_value());
      session.join();
      REQUIRE(session.sessionState() == solverState_e::Finished);
      CHECK(session.assembliesFound() == 12);
      CHECK(session.solutionsFound() == (disasm ? 1u : 0u));
    }
  }
}

TEST_CASE("solverSession_c: stopAfterPreparation pauses cleanly, state() and resume() complete",
          "[session][resume]") {
  auto p = loadPuzzle("examples/CubeInCage.xmpuzzle");
  auto spec = makeSolverSpec(*p->getProblem(0));

  solverOptions_t opts;
  opts.disassemble = false;
  opts.settings = bttest::withThreads(2);
  Collected baseline = runToCompletion(spec, opts);
  REQUIRE(baseline.assemblies.size() == 96);

  solverSession_c session(spec, opts);
  REQUIRE(session.start(/*stopAfterPreparation=*/true));
  session.join();
  REQUIRE(session.sessionState() == solverState_e::Paused);

  resumeData_t saved = session.state();
  CHECK(saved.engine == "2.2");
  CHECK(!saved.state.empty());
  CHECK(saved.pending.empty());

  // Branch A: in-place resume
  REQUIRE(session.resume());
  Collected inPlace;
  while (auto r = session.pop())
    recordResult(inPlace, *r);
  session.join();
  REQUIRE(session.sessionState() == solverState_e::Finished);
  CHECK(inPlace.assemblies == baseline.assemblies);

  // Branch B: fresh session from saved state()
  solverSession_c fresh(spec, opts);
  fresh.setResume(std::move(saved));
  REQUIRE(fresh.start());
  Collected fromSaved;
  while (auto r = fresh.pop())
    recordResult(fromSaved, *r);
  fresh.join();
  REQUIRE(fresh.sessionState() == solverState_e::Finished);
  CHECK(fromSaved.assemblies == baseline.assemblies);
}

TEST_CASE("solverSession_c: pause, state, in-place resume and cross-session resume lose and repeat nothing",
          "[session][parallel][resume][save]") {
  struct Case {
    const char * name;
    const char * path;
    bool bitParallel;
    bool disassemble;
  };
  const Case cases[] = {
    { "CubeInCage bit-parallel asm",      "examples/CubeInCage.xmpuzzle",      true,  false },
    { "CubeInCage Dancing Cells asm",     "examples/CubeInCage.xmpuzzle",      false, false },
    { "PiecesOfEight bit-parallel asm",   "examples/PiecesOfEight.xmpuzzle",   true,  false },
    { "PiecesOfEight Dancing Cells asm",  "examples/PiecesOfEight.xmpuzzle",   false, false },
    { "PelikanBurr bit-parallel disasm",  "examples/PelikanBurr.xmpuzzle",     true,  true  },
    { "PelikanBurr Dancing Cells disasm", "examples/PelikanBurr.xmpuzzle",     false, true  },
  };

  for (const auto & c : cases) {
    INFO(c.name);
    auto p = loadPuzzle(c.path);
    auto spec = makeSolverSpec(*p->getProblem(0));

    solverOptions_t opts;
    opts.disassemble = c.disassemble;
    opts.queueCapacity = 2; // small cap to exercise backpressure + soft-cap lift on pause()
    opts.settings = bttest::withThreads(4);
    opts.settings.bitParallel = c.bitParallel;

    Collected baseline = runToCompletion(spec, opts);
    REQUIRE(baseline.assemblies.size() > 3);

    // Helper: run until pause trigger, drain queue, snapshot state(), then verify
    // BOTH in-place resume() and fresh-session setResume() match baseline with zero loss/dups.
    auto checkPausePoint = [&](const char * label, auto configureAndPause) {
      INFO("pause point: " << label);
      solverSession_c session(spec, opts);
      Collected before;
      configureAndPause(session, before);

      // Drain anything already in the queue when pause() returns
      std::vector<solverResult_t> tail;
      session.drain(tail);
      for (const auto & r : tail)
        recordResult(before, r);
      CHECK_FALSE(session.pop().has_value());

      if (session.sessionState() == solverState_e::Finished) {
        CHECK(before.assemblies == baseline.assemblies);
        if (c.disassemble)
          CHECK(before.solutions == baseline.solutions);
        return;
      }

      REQUIRE(session.sessionState() == solverState_e::Paused);
      resumeData_t saved = session.state();

      // Branch A: in-place resume on the same session
      REQUIRE(session.resume());
      Collected inPlace = before;
      while (auto r = session.pop())
        recordResult(inPlace, *r);
      session.join();
      REQUIRE(session.sessionState() == solverState_e::Finished);
      CHECK(inPlace.assemblies.size() == baseline.assemblies.size());
      CHECK(inPlace.assemblies == baseline.assemblies);
      if (c.disassemble) {
        CHECK(inPlace.solutions.size() == baseline.solutions.size());
        CHECK(inPlace.solutions == baseline.solutions);
      }

      // Branch B: fresh session from saved state()
      solverSession_c fresh(spec, opts);
      fresh.setResume(std::move(saved));
      REQUIRE(fresh.start());
      Collected fromSaved = before;
      while (auto r = fresh.pop())
        recordResult(fromSaved, *r);
      fresh.join();
      REQUIRE(fresh.sessionState() == solverState_e::Finished);
      CHECK(fromSaved.assemblies.size() == baseline.assemblies.size());
      CHECK(fromSaved.assemblies == baseline.assemblies);
      if (c.disassemble) {
        CHECK(fromSaved.solutions.size() == baseline.solutions.size());
        CHECK(fromSaved.solutions == baseline.solutions);
      }
    };

    // 1. Pause after popping one item (with queueCapacity=2, producers may be blocked on full queue)
    checkPausePoint("after first pop (full queue backpressure)", [](solverSession_c & s, Collected & before) {
      REQUIRE(s.start());
      if (auto first = s.pop())
        recordResult(before, *first);
      s.pause();
    });

    // 2. Pause right before pool seeding (via poolSeedHook)
    checkPausePoint("poolSeedHook", [](solverSession_c & s, Collected & before) {
      bool fired = false;
      s.setPoolSeedHook([&]() {
        if (!fired) {
          fired = true;
          s.requestPause();
        }
      });
      REQUIRE(s.start());
      while (auto r = s.pop())
        recordResult(before, *r);
      s.join();
    });

    // 3. Pause right after pool drains (via poolDrainedHook)
    checkPausePoint("poolDrainedHook", [](solverSession_c & s, Collected & before) {
      bool fired = false;
      s.setPoolDrainedHook([&]() {
        if (!fired) {
          fired = true;
          s.requestPause();
        }
      });
      REQUIRE(s.start());
      while (auto r = s.pop())
        recordResult(before, *r);
      s.join();
    });

    // 4. Pause right after assembly phase ends, before disassembly pool finishes
    if (c.disassemble) {
      checkPausePoint("assemblyPhaseEndedHook", [](solverSession_c & s, Collected & before) {
        bool fired = false;
        s.setAssemblyPhaseEndedHook([&]() {
          if (!fired) {
            fired = true;
            s.requestPause();
          }
        });
        REQUIRE(s.start());
        while (auto r = s.pop())
          recordResult(before, *r);
        s.join();
      });
    }
  }
}

TEST_CASE("solverSession_c: resume validation rejects incompatible or damaged state before starting workers",
          "[session][resume][save]") {
  auto p = loadPuzzle("examples/CubeInCage.xmpuzzle");
  auto spec = makeSolverSpec(*p->getProblem(0));

  solverOptions_t opts;
  opts.settings = bttest::withThreads(2);

  SECTION("wrong engine/version tag") {
    solverSession_c s(spec, opts);
    resumeData_t bad;
    bad.engine = "0.1";
    bad.state = "0 ";
    s.setResume(std::move(bad));
    CHECK_FALSE(s.start());
    CHECK(s.sessionState() == solverState_e::Error);
    auto info = s.runInfo();
    REQUIRE(info.has_value());
    CHECK(info->error == assembler_c::ERR_CAN_NOT_RESTORE_VERSION);
  }

  SECTION("corrupt syntax") {
    solverSession_c s(spec, opts);
    resumeData_t bad;
    bad.engine = "2.2";
    bad.state = "99 garbage";
    s.setResume(std::move(bad));
    CHECK_FALSE(s.start());
    CHECK(s.sessionState() == solverState_e::Error);
    auto info = s.runInfo();
    REQUIRE(info.has_value());
    CHECK(info->error == assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX);
  }

  SECTION("interrupted without seeds") {
    solverSession_c s(spec, opts);
    resumeData_t bad;
    bad.engine = "2.2";
    bad.state = "1 2 0 ";
    s.setResume(std::move(bad));
    CHECK_FALSE(s.start());
    CHECK(s.sessionState() == solverState_e::Error);
    auto info = s.runInfo();
    REQUIRE(info.has_value());
    CHECK(info->error == assembler_c::ERR_CAN_NOT_RESTORE_INTERRUPTED);
  }

  SECTION("mismatched pending assembly piece count") {
    solverSession_c s(spec, opts);
    resumeData_t bad;
    bad.engine = "2.2";
    bad.state = "0 ";
    // Empty assembly has 0 placements != spec->getNumberOfPieces()
    bad.pending.push_back(std::make_unique<assembly_c>(spec->getGridType()));
    s.setResume(std::move(bad));
    CHECK_FALSE(s.start());
    CHECK(s.sessionState() == solverState_e::Error);
    auto info = s.runInfo();
    REQUIRE(info.has_value());
    CHECK(info->error == assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX);
  }

  SECTION("precondition errors: state() outside Paused, double start(), setResume() after start()") {
    solverSession_c s(spec, opts);
    CHECK_THROWS_AS(s.state(), assert_exception);
    REQUIRE(s.start(/*stopAfterPreparation=*/true));
    s.join();
    CHECK_THROWS_AS(s.start(), assert_exception);
    resumeData_t dummy;
    dummy.engine = "2.2";
    dummy.state = "0 ";
    CHECK_THROWS_AS(s.setResume(std::move(dummy)), assert_exception);
    s.abort();
    CHECK(s.sessionState() == solverState_e::Stopped);
    CHECK_THROWS_AS(s.state(), assert_exception);
  }
}

TEST_CASE("solverSession_c: abort() is total, idempotent, and unblocks full queues",
          "[session][parallel][lifecycle]") {
  auto p = loadPuzzle("examples/SolidSixPieceBurrs.xmpuzzle");
  auto spec = makeSolverSpec(*p->getProblem(0));

  SECTION("abort while Idle") {
    solverSession_c s(spec, solverOptions_t{});
    s.abort();
    s.abort();
    CHECK(s.sessionState() == solverState_e::Stopped);
    CHECK_FALSE(s.pop().has_value());
  }

  SECTION("abort while Running with full queue (queueCapacity=1, never popping)") {
    solverOptions_t opts;
    opts.disassemble = false;
    opts.queueCapacity = 1;
    opts.settings = bttest::withThreads(4);

    solverSession_c s(spec, opts);
    REQUIRE(s.start());
    // Wait briefly so workers fill the 1-slot queue and block on backpressure
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    s.abort();
    s.abort(); // idempotent
    CHECK(s.sessionState() == solverState_e::Stopped);
    CHECK_FALSE(s.pop().has_value());
  }

  SECTION("destructor aborts a Running session cleanly") {
    solverOptions_t opts;
    opts.disassemble = true;
    opts.queueCapacity = 1;
    opts.settings = bttest::withThreads(4);
    {
      solverSession_c s(spec, opts);
      REQUIRE(s.start());
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  }
}

TEST_CASE("problemCollector_c + problem_c resumeData_t XML roundtrip without touching assembler_c",
          "[session][save][resume]") {
  auto p = loadPuzzle("examples/SolidSixPieceBurrs.xmpuzzle");
  problem_c * prob = p->getProblem(0);
  REQUIRE(prob != nullptr);
  prob->removeAllSolutions();

  // Reference full solve via solverSession_c
  solverOptions_t opts;
  opts.disassemble = true;
  opts.reduce = true;
  opts.settings = bttest::withThreads(2);
  Collected ref = runToCompletion(makeSolverSpec(*prob), opts);
  REQUIRE(ref.assemblies.size() > 5);
  REQUIRE(ref.solutions.size() > 1);

  SECTION("problemCollector_c enforces solutionLimit, sorting and dropDisassemblies") {
    prob->beginSolving();
    solverSession_c s(makeSolverSpec(*prob), opts);
    problemCollectorOptions_t copts;
    copts.disassemble = true;
    copts.dropDisassemblies = true;
    copts.sortMethod = problemCollectorOptions_t::SRT_COMPLETE_MOVES;
    copts.solutionLimit = 2;
    problemCollector_c collector(*prob, copts);
    collector.resetDropMultiplicator();

    REQUIRE(s.start());
    collector.consumeAll(s);
    s.join();
    prob->finishedSolving();

    CHECK(prob->getNumAssemblies() == ref.assemblies.size());
    CHECK(prob->getNumSolutions() == ref.solutions.size());
    CHECK(prob->getNumberOfSavedSolutions() == std::min<size_t>(2, ref.solutions.size()));
    for (unsigned int i = 0; i < prob->getNumberOfSavedSolutions(); ++i) {
      CHECK(prob->getSavedSolution(i)->getDisassembly() == nullptr);
      CHECK(prob->getSavedSolution(i)->getDisassemblyInfo() != nullptr);
    }
  }

  SECTION("problem_c stores resumeData_t across XML save/load without owning an assembler_c") {
    prob->beginSolving();
    solverOptions_t pauseOpts = opts;
    pauseOpts.queueCapacity = 1;
    solverSession_c s1(makeSolverSpec(*prob), pauseOpts);
    problemCollectorOptions_t copts;
    copts.disassemble = true;
    copts.solutionLimit = 0; // keep all
    problemCollector_c c1(*prob, copts);
    c1.resetDropMultiplicator();

    REQUIRE(s1.start());
    if (auto first = s1.pop())
      c1.consume(std::move(*first));
    s1.pause();
    c1.consumeAll(s1);

    REQUIRE(s1.sessionState() == solverState_e::Paused);
    prob->setResumeData(s1.state());
    // seq numbering goes on after the results already handed out
    const uint64_t savedSeq = prob->cloneResumeData().nextSeq;
    CHECK(savedSeq == prob->getNumAssemblies());
    CHECK(savedSeq > 0);
    REQUIRE(prob->getAssembler() == nullptr);
    REQUIRE(prob->hasResumeData());
    REQUIRE(prob->canStartSolving());

    // Save puzzle XML and reload into a brand-new puzzle_c
    std::ostringstream oss;
    {
      xmlWriter_c writer(oss);
      p->save(writer);
    }
    std::istringstream iss(oss.str());
    xmlParser_c parser(iss);
    auto loaded = std::make_unique<puzzle_c>(parser);
    problem_c * prob2 = loaded->getProblem(0);
    REQUIRE(prob2 != nullptr);
    CHECK(prob2->getSolveState() == SS_SOLVING);
    CHECK(prob2->getAssembler() == nullptr);
    REQUIRE(prob2->hasResumeData());
    REQUIRE(prob2->canStartSolving());
    CHECK(prob2->cloneResumeData().nextSeq == savedSeq);

    // Finish from loaded resumeData_t on a brand-new solverSession_c
    solverSession_c s2(makeSolverSpec(*prob2), opts);
    s2.setResume(prob2->takeResumeData());
    problemCollector_c c2(*prob2, copts);
    c2.resetDropMultiplicator();
    REQUIRE(s2.start());
    std::vector<uint64_t> seqs;
    while (auto r = s2.pop()) {
      seqs.push_back(r->seq);
      c2.consume(std::move(*r));
    }
    // numbered on from the saved number, unique and gap-free
    std::sort(seqs.begin(), seqs.end());
    for (size_t i = 0; i < seqs.size(); i++)
      CHECK(seqs[i] == savedSeq + i);
    s2.join();
    REQUIRE(s2.sessionState() == solverState_e::Finished);
    prob2->finishedSolving();

    CHECK(prob2->getNumAssemblies() == ref.assemblies.size());
    CHECK(prob2->getNumSolutions() == ref.solutions.size());
    CHECK(prob2->getNumberOfSavedSolutions() == ref.solutions.size());
  }
}

namespace {

void drainAll(solverSession_c & s, Collected & out) {
  std::vector<solverResult_t> tail;
  s.drain(tail);
  for (const auto & r : tail)
    recordResult(out, r);
}

/* seqs hold exactly first .. first + n - 1, each once, in any order */
void checkSeqs(const std::vector<uint64_t> & seqs, uint64_t first) {
  std::vector<uint64_t> sorted = seqs;
  std::sort(sorted.begin(), sorted.end());
  for (size_t i = 0; i < sorted.size(); i++)
    REQUIRE(sorted[i] == first + i);
}

Collected merged(const Collected & a, const Collected & b) {
  Collected out = a;
  out.assemblies.insert(b.assemblies.begin(), b.assemblies.end());
  out.solutions.insert(b.solutions.begin(), b.solutions.end());
  out.seqs.insert(out.seqs.end(), b.seqs.begin(), b.seqs.end());
  return out;
}

struct ChainStats {
  unsigned int pauses = 0;
  unsigned int midSearch = 0;   // pauses with the assembler not yet complete
  unsigned int withPending = 0; // pauses that left pending assemblies
  unsigned int freshChecks = 0;
};

/* B4's check. Chains pause -> resume -> pause on one session until the
 * search ends, pausing: during preparation (right after start()); after a
 * varying number of pops (backpressure keeps the search mid-way); at the
 * pool seed hook of whichever phase it is; at the pool drained or
 * assembly-phase-ended hook when a phase ends before the pops; and right
 * after resume(), while pending assemblies are re-submitted. Every few pauses a new session continues
 * from state() to the end. */
Collected pauseChain(std::shared_ptr<const solverSpec_t> spec, const solverOptions_t & opts,
                     const Collected & baseline, unsigned int freshEvery,
                     unsigned int stride, ChainStats & stats) {
  solverSession_c s(spec, opts);
  std::atomic<int> trigger{0};
  auto hook = [&](int kind) {
    int expected = kind;
    if (trigger.compare_exchange_strong(expected, 0))
      s.requestPause();
  };
  s.setPoolSeedHook([&]() { hook(1); });
  s.setPoolDrainedHook([&]() { hook(2); });
  s.setAssemblyPhaseEndedHook([&]() { hook(3); });

  Collected got;
  auto record = [&](const solverResult_t & r) { recordResult(got, r); };
  auto drainRecord = [&]() {
    std::vector<solverResult_t> tail;
    s.drain(tail);
    for (const auto & r : tail)
      record(r);
  };

  REQUIRE(s.start());
  s.pause(); // lands in preparation, or right after it
  drainRecord();

  const unsigned int popCounts[] = { 1, 5, 2, 9, 3 };
  unsigned int round = 0;
  while (s.sessionState() == solverState_e::Paused) {
    REQUIRE(round < 5000);
    stats.pauses++;

    resumeData_t snap = s.state();
    // every result handed out so far is in got: numbering continues here
    CHECK(snap.nextSeq == got.seqs.size());
    if (!snap.pending.empty())
      stats.withPending++;
    if (snap.state.rfind("1 ", 0) == 0)
      stats.midSearch++;

    if (freshEvery && round % freshEvery == 0) {
      stats.freshChecks++;
      solverSession_c fresh(spec, opts);
      fresh.setResume(std::move(snap));
      REQUIRE(fresh.start());
      Collected rest;
      while (auto r = fresh.pop())
        recordResult(rest, *r);
      fresh.join();
      REQUIRE(fresh.sessionState() == solverState_e::Finished);
      checkSeqs(rest.seqs, got.seqs.size());
      const Collected all = merged(got, rest);
      CHECK(all.assemblies == baseline.assemblies);
      CHECK(all.solutions == baseline.solutions);
    }

    const unsigned int how = round % 8;
    round++;
    if (how >= 5)
      trigger.store(static_cast<int>(how - 4)); // 1 seed, 2 drained, 3 phase ended
    else
      trigger.store(0);

    REQUIRE(s.resume());
    if (how == 4) {
      s.pause(); // pending re-submission / phase start
    } else if (how == 5) {
      // the seed hook pauses as this phase's search starts
      while (auto r = s.pop())
        record(*r);
      s.join();
    } else {
      // after n pops; with 6 and 7 the pool drained / phase ended hook
      // pauses instead if the phase ends first (the last phase)
      const unsigned int n = popCounts[(round / 8) % 5] * (how < 4 ? how + 1 : 2) * stride;
      for (unsigned int i = 0; i < n; i++) {
        auto r = s.pop();
        if (!r)
          break;
        record(*r);
      }
      s.pause();
    }
    drainRecord();
  }

  REQUIRE(s.sessionState() == solverState_e::Finished);
  CHECK_FALSE(s.pop().has_value());
  checkSeqs(got.seqs, 0);
  CHECK(got.assemblies == baseline.assemblies);
  CHECK(got.solutions == baseline.solutions);
  return got;
}

void runPauseChains(const char * path, unsigned int threads, size_t queueCapacity,
                    unsigned int targetPauses, unsigned int freshEvery,
                    bool bitParallel) {
  auto p = loadPuzzle(path);
  auto spec = makeSolverSpec(*p->getProblem(0));

  struct Mode { const char * name; bool disassemble; };
  const Mode modes[] = {
    { "assembly only", false },
    { "disassembly",   true  },
  };
  for (const auto & m : modes) {
    INFO(path << ", " << m.name << ", " << threads << " threads"
         << (bitParallel ? "" : ", Dancing Cells"));
    solverOptions_t opts;
    opts.disassemble = m.disassemble;
    opts.queueCapacity = queueCapacity;
    opts.settings = bttest::withThreads(threads);
    opts.settings.bitParallel = bitParallel;

    Collected baseline = runToCompletion(spec, opts);
    REQUIRE(baseline.assemblies.size() > 20);
    checkSeqs(baseline.seqs, 0);

    // about targetPauses pauses: a pop round takes 12 results per stride
    // on average, and 5 of 8 rounds pop
    const unsigned int stride = std::max<unsigned int>(
        1, static_cast<unsigned int>(baseline.seqs.size() * 8 / (targetPauses * 12 * 5)));
    ChainStats stats;
    pauseChain(spec, opts, baseline, freshEvery, stride, stats);
    INFO("pauses " << stats.pauses << ", mid-search " << stats.midSearch
         << ", with pending " << stats.withPending << ", fresh " << stats.freshChecks);
    // The pool buffers up to 64 assemblies in its queue, and one in flight
    // per worker, beyond the session's queue, so with disassembly a search much smaller
    // than that runs ahead of the host and only a few pauses land mid-search.
    if (!m.disassemble || baseline.seqs.size() > 400) {
      CHECK(stats.pauses > 10);
      CHECK(stats.midSearch > 5);
    } else {
      CHECK(stats.pauses >= 2);
    }
    // one thread disassembles inline: nothing is ever queued to salvage
    if (m.disassemble && threads > 1)
      CHECK(stats.withPending > 0);
  }
}

} // namespace

TEST_CASE("solverSession_c: chained pause/resume at many points matches an uninterrupted run",
          "[session][resume][parallel]") {
  runPauseChains("examples/CubeInCage.xmpuzzle", 4, 2, 30, 6, true);
  runPauseChains("examples/CubeInCage.xmpuzzle", 1, 2, 30, 6, true);
  runPauseChains("examples/CubeInCage.xmpuzzle", 4, 2, 30, 6, false);
}

TEST_CASE("solverSession_c: chained pause/resume on a larger search",
          "[session][resume][parallel][stress]") {
  // Dancing Cells (variable voxels with piece ranges)
  runPauseChains("examples/DemoPieceGenerator.xmpuzzle", 4, 3, 60, 8, true);
  runPauseChains("examples/SolidSixPieceBurrs.xmpuzzle", 4, 2, 60, 8, true);
  runPauseChains("examples/SolidSixPieceBurrs.xmpuzzle", 1, 1, 60, 8, true);
  runPauseChains("examples/SolidSixPieceBurrs.xmpuzzle", 4, 1, 60, 8, false);
}

TEST_CASE("solverSession_c: pending assemblies continue without disassembly",
          "[session][resume]") {
  // A run with disassembly stopped with queued work and continued without
  // disassembly (GUI: Disassemble unchecked before Continue; burrTxt2 -d,
  // then without -d): the stash comes out as plain assemblies, then Finished.
  auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  auto spec = makeSolverSpec(*p->getProblem(0));

  solverOptions_t opts;
  opts.disassemble = false;
  opts.settings = bttest::withThreads(2);
  Collected baseline = runToCompletion(spec, opts);
  REQUIRE(baseline.assemblies.size() == 12);

  std::vector<std::unique_ptr<assembly_c>> found;
  {
    solverSession_c s(spec, opts);
    REQUIRE(s.start());
    while (auto r = s.pop())
      found.push_back(std::move(r->assembly));
    s.join();
  }
  auto stash = [&](uint64_t nextSeq) {
    resumeData_t rd;
    rd.engine = "2.2";
    rd.state = "2 "; // the assembler is done: only the stash is left
    rd.nextSeq = nextSeq;
    for (const auto & a : found)
      rd.pending.push_back(std::make_unique<assembly_c>(a.get()));
    return rd;
  };

  SECTION("all delivered, numbered on from nextSeq, then Finished") {
    solverOptions_t o = opts;
    o.queueCapacity = 2;
    solverSession_c s(spec, o);
    s.setResume(stash(5));
    REQUIRE(s.start());
    Collected got;
    while (auto r = s.pop()) {
      CHECK(r->separation == nullptr);
      recordResult(got, *r);
    }
    s.join();
    REQUIRE(s.sessionState() == solverState_e::Finished);
    CHECK(got.assemblies == baseline.assemblies);
    checkSeqs(got.seqs, 5);
    CHECK(s.assembliesFound() == 12);
  }

  SECTION("countOnly counts them") {
    solverOptions_t o = opts;
    o.countOnly = true;
    solverSession_c s(spec, o);
    s.setResume(stash(0));
    REQUIRE(s.start());
    CHECK_FALSE(s.pop().has_value());
    s.join();
    REQUIRE(s.sessionState() == solverState_e::Finished);
    CHECK(s.assembliesFound() == 12);
  }

  SECTION("a pause in the middle keeps the rest pending, both resume paths finish") {
    solverOptions_t o = opts;
    o.queueCapacity = 1;
    solverSession_c s(spec, o);
    s.setResume(stash(0));
    REQUIRE(s.start());
    Collected before;
    if (auto r = s.pop())
      recordResult(before, *r);
    s.pause();
    drainAll(s, before);
    REQUIRE(s.sessionState() == solverState_e::Paused);
    resumeData_t snap = s.state();
    CHECK(snap.nextSeq == before.seqs.size());
    CHECK(snap.pending.size() + before.assemblies.size() == 12);

    solverSession_c fresh(spec, o);
    fresh.setResume(std::move(snap));
    REQUIRE(fresh.start());
    Collected rest;
    while (auto r = fresh.pop())
      recordResult(rest, *r);
    fresh.join();
    REQUIRE(fresh.sessionState() == solverState_e::Finished);
    CHECK(merged(before, rest).assemblies == baseline.assemblies);
    checkSeqs(merged(before, rest).seqs, 0);

    REQUIRE(s.resume());
    Collected inPlace = before;
    while (auto r = s.pop())
      recordResult(inPlace, *r);
    s.join();
    REQUIRE(s.sessionState() == solverState_e::Finished);
    CHECK(inPlace.assemblies == baseline.assemblies);
    checkSeqs(inPlace.seqs, 0);
  }
}

TEST_CASE("solverSession_c: abort() wins every race with the end of a phase",
          "[session][lifecycle][parallel]") {
  auto p = loadPuzzle("examples/SolidSixPieceBurrs.xmpuzzle");
  auto spec = makeSolverSpec(*p->getProblem(0));

  for (bool disasm : {false, true}) {
    INFO((disasm ? "disassembly" : "assembly only"));
    solverOptions_t opts;
    opts.disassemble = disasm;
    opts.queueCapacity = 2;
    opts.settings = bttest::withThreads(4);

    // requestPause() sends the driver towards Paused; an abort() landing
    // after its last check must still end Stopped
    for (unsigned int i = 0; i < 24; i++) {
      INFO("round " << i);
      solverSession_c s(spec, opts);
      REQUIRE(s.start());
      if (i % 3 == 1)
        (void)s.pop();
      std::this_thread::sleep_for(std::chrono::microseconds((i * 397) % 2500));
      s.requestPause();
      std::this_thread::sleep_for(std::chrono::microseconds((i * 131) % 400));
      s.abort();
      CHECK(s.sessionState() == solverState_e::Stopped);
      CHECK_FALSE(s.resume());
      CHECK_FALSE(s.pop().has_value());
      CHECK_THROWS_AS(s.state(), assert_exception);
    }

    // abort() while start() validates resume data on another thread
    solverSession_c src(spec, opts);
    REQUIRE(src.start());
    (void)src.pop();
    src.pause();
    REQUIRE(src.sessionState() == solverState_e::Paused);
    const resumeData_t saved = src.state();
    for (unsigned int i = 0; i < 12; i++) {
      INFO("start race " << i);
      solverSession_c s(spec, opts);
      s.setResume(saved.clone());
      std::thread starter([&]() {
        try {
          (void)s.start(i % 2 == 0);
        } catch (const assert_exception &) {
          // abort() won before start() began: no longer Idle (precondition)
        }
      });
      std::this_thread::sleep_for(std::chrono::microseconds((i * 211) % 1500));
      s.abort();
      starter.join();
      s.join();
      // start() lost the race (returned false) or its run was stopped; it
      // never revives the aborted session
      CHECK(s.sessionState() == solverState_e::Stopped);
      CHECK_FALSE(s.resume());
    }
  }

  SECTION("abort() during the final disassembly drain, pollers not stalled") {
    solverOptions_t opts;
    opts.disassemble = true;
    opts.queueCapacity = 0;
    opts.settings = bttest::withThreads(2);
    solverSession_c s(spec, opts);
    std::atomic<bool> drained{false};
    s.setAssemblyPhaseEndedHook([&]() { drained.store(true); });
    REQUIRE(s.start());
    while (!drained.load() && s.sessionState() == solverState_e::Running)
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    std::thread poller([&]() {
      for (int i = 0; i < 50; i++) {
        (void)s.getProgress();
        (void)s.workOutstanding();
      }
    });
    s.abort();
    poller.join();
    const solverState_e st = s.sessionState();
    CHECK((st == solverState_e::Stopped || st == solverState_e::Finished));
    CHECK_FALSE(s.pop().has_value());
  }
}

TEST_CASE("assembler_c::stopSticky() holds for a run that has not begun",
          "[session][assembler]") {
  // runPhase checks pauseRequested_ before assemble(), whose beginRun()
  // replaces the stop source: a plain stop() between the two is lost.
  auto p = loadPuzzle("examples/CubeInCage.xmpuzzle");
  for (bool bitParallel : {true, false}) {
    INFO((bitParallel ? "bit-parallel" : "Dancing Cells"));
    solverSettings_t settings = bttest::withThreads(2);
    settings.bitParallel = bitParallel;
    assembler_1_c assm(makeSolverSpec(*p->getProblem(0)), settings);
    REQUIRE(assm.createMatrix(false, false) == assembler_c::ERR_NONE);

    size_t found = 0;
    auto count = [&](std::unique_ptr<assembly_c>) { found++; return true; };

    assm.stop();       // a plain stop before the run is forgotten ...
    assm.stopSticky(); // ... a sticky one is not
    assm.assemble(count);
    CHECK(found == 0);
    CHECK(assm.getFinished() < 1.0f);

    assm.clearStickyStop();
    assm.assemble(count);
    CHECK(found == 96);
    CHECK(assm.getFinished() == 1.0f);
  }
}

TEST_CASE("solverSession_c: an exception from the solver ends the session in Error, not the process",
          "[session][lifecycle]") {
  auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  auto spec = makeSolverSpec(*p->getProblem(0));
  // The assembly-phase-ended hook runs on the session's driver thread inside
  // the phase, where the pool's finish() also rethrows a worker's exception.
  for (bool disasm : {false, true}) {
    INFO((disasm ? "disassembly" : "assembly only"));
    solverOptions_t opts;
    opts.disassemble = disasm;
    opts.settings = bttest::withThreads(2);
    solverSession_c s(spec, opts);
    s.setAssemblyPhaseEndedHook([]() { throw std::runtime_error("injected failure"); });
    REQUIRE(s.start());
    while (s.pop()) {
    }
    s.join();
    CHECK(s.sessionState() == solverState_e::Error);
    CHECK(s.currentAction() == solverSession_c::ACT_ERROR);
    REQUIRE(s.exception() != nullptr);
    CHECK(s.exceptionMessage() == "injected failure");
    auto info = s.runInfo();
    REQUIRE(info.has_value());
    CHECK(info->error == assembler_c::ERR_NONE);
    CHECK_FALSE(info->complete);
    CHECK_FALSE(s.resume());
  }
}

TEST_CASE("problemCollector_c: counting with disassembly still keeps single-piece assemblies",
          "[session][save]") {
  // The GUI's PAR_JUST_COUNT | PAR_DISASSM (and burrTxt2 -c -d): the session
  // enqueues every result, the collector counts them and, as solveThread_c
  // always did, stores a single-piece assembly as a solution.
  auto puzzle = std::make_unique<puzzle_c>(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
  const gridType_c & gt = *puzzle->getGridType();
  const unsigned int bar = puzzle->addShape(bttest::fromLayers(gt, {{"##"}}));
  const unsigned int result = puzzle->addShape(bttest::fromLayers(gt, {{"##"}}));
  problem_c * prob = puzzle->getProblem(puzzle->addProblem());
  prob->setResultId(result);
  prob->setShapeMinimum(bar, 1);
  prob->setShapeMaximum(bar, 1);

  solverOptions_t opts;
  opts.disassemble = true;
  opts.settings = bttest::withThreads(2);
  solverSession_c s(makeSolverSpec(*prob), opts);

  problemCollectorOptions_t copts;
  copts.disassemble = true;
  copts.countOnly = true;
  problemCollector_c collector(*prob, copts);
  collector.resetDropMultiplicator();
  prob->beginSolving();
  REQUIRE(s.start());
  collector.consumeAll(s);
  s.join();
  REQUIRE(s.sessionState() == solverState_e::Finished);
  prob->finishedSolving();

  CHECK(prob->getNumAssemblies() == 1);
  CHECK(prob->getNumSolutions() == 1);
  CHECK(prob->getNumberOfSavedSolutions() == 1);
}

TEST_CASE("solverSession_c: a problem without a result shape ends in Error, not a crash",
          "[session]") {
  // CoverUp_PiecesOnly.xmpuzzle has no result: createMatrix's bt_assert
  // vanished under NDEBUG and the release burrTxt crashed.
  auto p = loadPuzzle("examples/CubeInCage.xmpuzzle");
  auto noResult = std::make_shared<solverSpec_t>(*makeSolverSpec(*p->getProblem(0)));
  noResult->result = 0xFFFFFFFF;
  REQUIRE_FALSE(noResult->resultValid());

  solverOptions_t opts;
  opts.settings = bttest::withThreads(2);
  solverSession_c s(noResult, opts);
  s.start();
  s.join();
  CHECK(s.sessionState() == solverState_e::Error);
  auto info = s.runInfo();
  REQUIRE(info.has_value());
  CHECK(info->error == assembler_c::ERR_PUZZLE_UNHANDABLE);
}
