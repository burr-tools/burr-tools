#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "lib/threadconfig.h"
#include "lib/assembler.h"
#include "lib/assembly.h"
#include "lib/disassembler_0.h"
#include "lib/disassemblerpool.h"
#include "lib/disassembly.h"
#include "lib/gridtype.h"
#include "lib/problem.h"
#include "lib/puzzle.h"

#include "tools/gzstream.h"
#include "tools/xml.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <set>
#include <utility>
#include <thread>

/* threadConfig is the single resolver every front end shares - burrTxt,
 * burrTxt2, the GUI and the python module - so its precedence rules and its
 * budget invariant are pinned down here rather than in each front end.
 *
 * The cases at the bottom go further and check that a resolved count actually
 * reaches the worker threads, because the interesting failure mode is not a
 * wrong number but a number that is computed correctly and then ignored.
 */

namespace {

  using bttest::scopedEnv_c;

  std::unique_ptr<puzzle_c> loadPuzzle(const char * path) {
    auto str = openGzFile(path);
    if (!str) return nullptr;
    xmlParser_c pars(*str);
    return std::make_unique<puzzle_c>(pars);
  }

  /* records which threads the assembler hands assemblies back on. The count of
   * distinct ids is a lower bound on the number of workers that did useful
   * work, and is the only portable observation of "how many threads ran" -
   * counting OS threads needs mach / proc / Win32 in three different ways.
   */
  class threadWatchingCb_c : public assembler_cb {
    std::mutex mutex;
    std::set<std::thread::id> ids;
    unsigned long count = 0;
  public:
    bool assembly(std::unique_ptr<assembly_c> /*a*/) override {
      std::lock_guard<std::mutex> guard(mutex);
      ids.insert(std::this_thread::get_id());
      count++;
      return true;
    }
    size_t distinctThreads(void) { std::lock_guard<std::mutex> g(mutex); return ids.size(); }
    unsigned long assemblies(void) { std::lock_guard<std::mutex> g(mutex); return count; }
  };
}

TEST_CASE("threadConfig resolves both stages from one contract", "[threads][unit]") {

  /* start from a clean slate; the suite may be run with these set */
  scopedEnv_c clearAll("BURRTOOLS_THREADS", 0);
  scopedEnv_c clearAsm("BURRTOOLS_ASSEMBLER_THREADS", 0);
  scopedEnv_c clearDis("BURRTOOLS_DISASSEMBLER_THREADS", 0);

  const unsigned int mx = threadConfig::maxThreads();
  REQUIRE(mx >= 1);
  REQUIRE(mx <= threadConfig::MAX_THREADS);

  SECTION("defaults are 60% of the machine for assembly and inline disassembly") {
    REQUIRE(threadConfig::defaultAssemblerThreads() == std::max(1u, mx * 6 / 10));
    REQUIRE(threadConfig::defaultDisassemblerThreads() == 1u);

    /* 0 means "not specified" and lands on the default */
    REQUIRE(threadConfig::resolveAssembler(0) == threadConfig::defaultAssemblerThreads());
    REQUIRE(threadConfig::resolveDisassembler(0) == 1u);
  }

  SECTION("an explicit count wins and is clamped into range") {
    REQUIRE(threadConfig::resolveAssembler(3) == 3u);
    REQUIRE(threadConfig::resolveDisassembler(3) == 3u);
    REQUIRE(threadConfig::resolveAssembler(100000) == threadConfig::MAX_THREADS);
    REQUIRE(threadConfig::resolveDisassembler(100000) == threadConfig::MAX_THREADS);
  }

  SECTION("the per stage variable beats the shared one") {
    scopedEnv_c shared("BURRTOOLS_THREADS", "5");
    REQUIRE(threadConfig::resolveAssembler(0) == 5u);
    REQUIRE(threadConfig::resolveDisassembler(0) == 5u);

    scopedEnv_c stage("BURRTOOLS_ASSEMBLER_THREADS", "7");
    REQUIRE(threadConfig::resolveAssembler(0) == 7u);
    REQUIRE(threadConfig::resolveDisassembler(0) == 5u);   // untouched

    /* and an explicit request still beats both */
    REQUIRE(threadConfig::resolveAssembler(2) == 2u);
  }

  SECTION("a malformed variable is ignored rather than wrapping") {
    scopedEnv_c bad("BURRTOOLS_ASSEMBLER_THREADS", "-4");
    REQUIRE(threadConfig::resolveAssembler(0) == threadConfig::defaultAssemblerThreads());

    scopedEnv_c junk("BURRTOOLS_DISASSEMBLER_THREADS", "12abc");
    REQUIRE(threadConfig::resolveDisassembler(0) == 1u);
  }

  SECTION("the cost model matches the threads the pool really starts") {
    /* inline starts nothing */
    REQUIRE(threadConfig::disassemblerThreadCost(1) == 0u);

    /* a pool of N is N workers plus the merger that reorders their results */
    REQUIRE(threadConfig::disassemblerThreadCost(4) == 5u);

    /* the whole machine assembling plus inline disassembly is legal */
    REQUIRE_FALSE(threadConfig::exceedsBudget(mx, 1));

    /* ... but half and half is not, because of the merger. This is the case
     * that slipped through when the merger was left out of the budget.
     */
    if (mx >= 4 && (mx % 2) == 0)
      REQUIRE(threadConfig::exceedsBudget(mx / 2, mx / 2));
  }

  SECTION("fitToBudget keeps the concurrent pair inside the machine") {
    /* asking for the machine twice over: the pool gives way to inline, and
     * the assembler keeps its full share because inline costs nothing
     */
    unsigned int a = mx, d = mx;
    threadConfig::fitToBudget(&a, &d);
    REQUIRE(a == mx);
    REQUIRE(d == 1u);
    REQUIRE_FALSE(threadConfig::exceedsBudget(a, d));

    if (mx >= 5) {
      /* a modest assembler leaves real room for a pool; the assembler keeps
       * what it asked for and the pool takes the rest, merger included
       */
      unsigned int a2 = 2, d2 = mx;
      threadConfig::fitToBudget(&a2, &d2);
      REQUIRE(a2 == 2u);
      REQUIRE(d2 == mx - 3);                        // workers; + 1 merger
      REQUIRE(a2 + threadConfig::disassemblerThreadCost(d2) == mx);
      REQUIRE_FALSE(threadConfig::exceedsBudget(a2, d2));
    }

    if (mx >= 3) {
      /* an assembler that leaves too little for a worthwhile pool sends the
       * disassembler inline rather than keeping a token worker
       */
      unsigned int a3 = mx - 1, d3 = mx;
      threadConfig::fitToBudget(&a3, &d3);
      REQUIRE(a3 == mx - 1);                        // assembler preference
      REQUIRE(d3 == 1u);                            // inline
      REQUIRE_FALSE(threadConfig::exceedsBudget(a3, d3));
    }
  }

  SECTION("the two clamp directions agree with the budget") {
    /* these are what the settings dialogue drags against */
    REQUIRE(threadConfig::maxAssemblerFor(1) == mx);          // inline frees everything
    REQUIRE(threadConfig::maxDisassemblerFor(mx) == 1u);      // no room left: inline

    /* maxDisassemblerFor(1) is the disassembler slider's upper bound. The pool
     * can never have the whole machine: it also needs its merger thread and at
     * least one assembler thread to feed it.
     */
    const unsigned int sliderMax = threadConfig::maxDisassemblerFor(1);
    if (mx >= 3) {
      REQUIRE(sliderMax == mx - 2);
      REQUIRE(sliderMax < mx);
      /* a pool at the bound still leaves exactly one assembler thread */
      REQUIRE(threadConfig::maxAssemblerFor(sliderMax) == 1u);
      REQUIRE_FALSE(threadConfig::exceedsBudget(1, sliderMax));
    } else {
      /* too small for a pool to be worth anything: the slider degenerates to
       * 1..1 and the dialogue deactivates it
       */
      REQUIRE(sliderMax == 1u);
    }

    /* every assembler count must yield a pool that fits, and vice versa */
    for (unsigned int a = 1; a <= mx; a++) {
      const unsigned int d = threadConfig::maxDisassemblerFor(a);
      REQUIRE(d >= 1);
      REQUIRE_FALSE(threadConfig::exceedsBudget(a, d));
    }

    for (unsigned int d = 1; d <= mx; d++) {
      const unsigned int a = threadConfig::maxAssemblerFor(d);
      REQUIRE(a >= 1);
      /* a pool so big that even one assembler thread does not fit is the
       * caller's problem to cap - see the drag simulation below
       */
      if (threadConfig::disassemblerThreadCost(d) < mx)
        REQUIRE_FALSE(threadConfig::exceedsBudget(a, d));
    }
  }

  SECTION("dragging either settings slider always lands on a legal pair") {
    /* mirrors configuration.cpp's enforceThreadBudget: whichever slider moved
     * keeps its value, the other gives way, and neither is ever raised
     */
    auto dragAssembler = [](unsigned int a, unsigned int d) {
      const unsigned int dMax = threadConfig::maxDisassemblerFor(a);
      return std::make_pair(a, std::min(d, dMax));
    };

    auto dragDisassembler = [](unsigned int a, unsigned int d) {
      /* the slider cannot go above this, so the simulation does not either */
      const unsigned int dNew = std::min(d, threadConfig::maxDisassemblerFor(1));
      const unsigned int aMax = threadConfig::maxAssemblerFor(dNew);
      return std::make_pair(std::min(a, aMax), dNew);
    };

    for (unsigned int a = 1; a <= mx; a++) {
      for (unsigned int d = 1; d <= mx; d++) {

        auto byAsm = dragAssembler(a, d);
        CHECK(byAsm.first == a);                      // dragged slider keeps its value
        CHECK(byAsm.second <= d);                     // the other only ever drops
        CHECK_FALSE(threadConfig::exceedsBudget(byAsm.first, byAsm.second));

        auto byDis = dragDisassembler(a, d);
        CHECK(byDis.second <= d);
        CHECK(byDis.first <= a);
        CHECK_FALSE(threadConfig::exceedsBudget(byDis.first, byDis.second));
      }
    }
  }

  SECTION("parseThreadArg accepts what the flags document and nothing else") {
    unsigned int v = 99;
    REQUIRE(threadConfig::parseThreadArg("0", &v));
    REQUIRE(v == 0u);                       // 0 stays legal: it means auto
    REQUIRE(threadConfig::parseThreadArg("8", &v));
    REQUIRE(v == 8u);

    REQUIRE_FALSE(threadConfig::parseThreadArg("-1", &v));
    REQUIRE_FALSE(threadConfig::parseThreadArg("4x", &v));
    REQUIRE_FALSE(threadConfig::parseThreadArg("", &v));
    REQUIRE_FALSE(threadConfig::parseThreadArg(0, &v));
    REQUIRE(v == 8u);                       // rejected input leaves it alone
  }
}

TEST_CASE("disassembler thread count selects inline or pooled dispatch", "[threads][disasm][pool]") {

  /* 1 is not "one worker", it is a different mode: no worker and no merger
   * thread exists and submit() disassembles on the calling thread. Observing
   * which thread the result callback arrives on tells the two apart without
   * having to count OS threads.
   */
  scopedEnv_c clearAll("BURRTOOLS_THREADS", 0);
  scopedEnv_c clearDis("BURRTOOLS_DISASSEMBLER_THREADS", 0);

  auto p = loadPuzzle("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p != nullptr);
  problem_c * problem = p->getProblem(0);
  REQUIRE(problem != nullptr);

  const gridType_c * gt = problem->getPuzzle().getGridType();
  REQUIRE(gt != nullptr);

  auto dispatchThreads = [&](unsigned int requested, bool forceNoPool) {
    scopedEnv_c noPool("BURRTOOLS_NO_DISASM_POOL", forceNoPool ? "1" : 0);

    std::mutex mutex;
    std::set<std::thread::id> ids;
    unsigned int received = 0;

    {
      disassemblerPool_c pool(
        *problem,
        requested,
        [&](uint64_t, std::unique_ptr<assembly_c>, std::unique_ptr<separation_c>) {
          std::lock_guard<std::mutex> guard(mutex);
          ids.insert(std::this_thread::get_id());
          received++;
        }
      );

      for (unsigned int i = 0; i < 8; i++)
        pool.submit(std::make_unique<assembly_c>(gt));

      pool.finish();
    }

    REQUIRE(received == 8u);
    REQUIRE(ids.size() == 1u);       // inline: the submitter; pooled: the merger
    return *ids.begin();
  };

  const std::thread::id self = std::this_thread::get_id();

  SECTION("1 means inline: the callback runs on the submitting thread") {
    REQUIRE(dispatchThreads(1, false) == self);
  }

  SECTION("more than 1 builds a pool: the callback runs on the merger") {
    REQUIRE(dispatchThreads(4, false) != self);
  }

  SECTION("BURRTOOLS_NO_DISASM_POOL forces inline whatever was asked for") {
    REQUIRE(dispatchThreads(4, true) == self);
  }

  SECTION("0 resolves to the default, which is inline") {
    REQUIRE(dispatchThreads(0, false) == self);
  }
}

/* The biggest puzzle shipped in examples/. 588 assemblies over ~4.3M search
 * tree nodes, which is enough work for the parallel search to split into
 * several tasks and hand assemblies back from several threads - a small
 * puzzle finishes inside one task and would prove nothing. It costs a few
 * seconds, hence [stress].
 */
TEST_CASE("the resolved assembler count reaches the workers", "[threads][solver][stress]") {

  scopedEnv_c clearAll("BURRTOOLS_THREADS", 0);
  scopedEnv_c clearAsm("BURRTOOLS_ASSEMBLER_THREADS", 0);

  const char * PUZZLE = "examples/SolidSixPieceBurrs.xmpuzzle";

  auto solve = [&](unsigned int threads, threadWatchingCb_c & cb) {
    auto p = loadPuzzle(PUZZLE);
    REQUIRE(p != nullptr);
    problem_c * problem = p->getProblem(0);
    REQUIRE(problem != nullptr);
    problem->removeAllSolutions();

    const gridType_c * gt = problem->getPuzzle().getGridType();
    REQUIRE(gt != nullptr);

    std::unique_ptr<assembler_c> assm = gt->findAssembler(*problem);
    REQUIRE(assm != nullptr);
    REQUIRE(assm->createMatrix(false, false, false) == assembler_c::ERR_NONE);

    assm->setNumThreads(threads);
    REQUIRE(assm->getNumThreads() == threads);

    assm->assemble(&cb);
  };

  threadWatchingCb_c serial;
  solve(1, serial);

  threadWatchingCb_c parallel;
  solve(4, parallel);

  /* 1 thread must stay on one thread: that is the serial search path */
  CHECK(serial.distinctThreads() == 1u);

  /* 4 threads must actually spread, and must never exceed what was asked for */
  CHECK(parallel.distinctThreads() > 1u);
  CHECK(parallel.distinctThreads() <= 4u);

  /* and the whole point: the thread count must not change the answer */
  REQUIRE(serial.assemblies() > 0);
  CHECK(parallel.assemblies() == serial.assemblies());
}
