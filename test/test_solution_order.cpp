/* The order the solve thread keeps its saved solutions in.
 *
 * solveThread_c places each new solution where its sort puts it. It used to
 * scan every saved solution for the place (SRT_COMPLETE_MOVES, SRT_LEVEL) or
 * sort the whole list after each one (the live sort), at a cost growing with
 * the list; now it searches the list it knows to be in order. The order must
 * be the very one the scan made, so these cases replay the old placement
 * exactly -- scan, limit, thinning, stable sort -- over the solutions in the
 * order they arrived, and compare. One thread and no disassembler pool, so
 * that a second solve sees the same solutions arrive in the same order.
 */
#include <catch2/catch_test_macros.hpp>

#include "../src/lib/assembly.h"
#include "../src/lib/disassembly.h"
#include "../src/lib/problem.h"
#include "../src/lib/puzzle.h"
#include "../src/lib/solution.h"
#include "../src/lib/solvethread.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

  const char * kPuzzle = "examples/SolidSixPieceBurrs.xmpuzzle";   // 179 solutions, 5 and 6 moves

  /* "keep every solution", as the GUIs ask for it: a limit far above any
   * count. (A limit of 0 never ends solveThread_c::start() on a problem that
   * counted solutions before -- its drop loop halves a towards 1 for ever.) */
  const unsigned int kAll = 100000000u;

  struct Run {
    std::unique_ptr<puzzle_c> puzzle;
    problem_c * problem = nullptr;
  };

  /* a fresh copy of the puzzle solved to the end, as the GUIs start a solve */
  Run solved(int sort, unsigned int limit, unsigned int drop, int liveSort) {
    Run r;
    r.puzzle = puzzle_c::load(kPuzzle);
    REQUIRE(r.puzzle != nullptr);
    r.problem = r.puzzle->getProblem(0);
    r.problem->removeAllSolutions();
    // no placement reduction: the order does not depend on it, and it is slow unoptimised
    solveThread_c t(*r.problem, solveThread_c::PAR_DISASSM, 1);
    t.setSortMethod(sort);
    t.setSolutionLimits(limit, drop);
    t.setLiveSort(liveSort);
    REQUIRE(t.start());
    while (t.currentAction() != solveThread_c::ACT_FINISHED && t.currentAction() != solveThread_c::ACT_ERROR &&
           t.currentAction() != solveThread_c::ACT_ASSERT)
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    REQUIRE(t.currentAction() == solveThread_c::ACT_FINISHED);
    return r;
  }

  /* the solution numbers in list order: which solution stands where */
  std::vector<unsigned int> order(const problem_c & p) {
    std::vector<unsigned int> out;
    for (unsigned int i = 0; i < p.getNumberOfSavedSolutions(); i++)
      out.push_back(p.getSavedSolution(i)->getSolutionNumber());
    return out;
  }

  /* Every disassemblable solution, in the order the solve found them: an
   * unsorted solve without a limit keeps each, appended. */
  struct Arrivals {
    Run run;
    std::vector<const solution_c *> found;
  };

  const Arrivals & arrivals(void) {
    static const Arrivals a = [] {
      Arrivals out;
      out.run = solved(solveThread_c::SRT_UNSORT, kAll, 1, -1);
      for (unsigned int i = 0; i < out.run.problem->getNumberOfSavedSolutions(); i++)
        out.found.push_back(out.run.problem->getSavedSolution(i));
      return out;
    }();
    return a;
  }

  // the old onDisassemblyResult(), step by step, over the arrivals
  std::vector<unsigned int> replayScan(int sort, unsigned int limit, unsigned int drop) {
    std::vector<const solution_c *> list;
    unsigned int numSolutions = 0;
    unsigned int dropMultiplicator = 1;
    for (const solution_c * s : arrivals().found) {
      const disassembly_c * d = s->getDisassembly();
      unsigned int i = 0;
      for (; i < list.size(); i++) {
        const disassembly_c * s2 = sort == solveThread_c::SRT_COMPLETE_MOVES ? list[i]->getDisassembly()
                                                                            : list[i]->getDisassemblyInfo();
        if (s2 && (sort == solveThread_c::SRT_COMPLETE_MOVES ? s2->sumMoves() > d->sumMoves() : s2->compare(d) > 0))
          break;
      }
      list.insert(list.begin() + i, s);
      if (limit && list.size() > limit)
        list.erase(list.begin());
      numSolutions++;
      if (limit && list.size() > limit) {
        unsigned int idx = numSolutions - 1;
        idx = (idx % (limit * drop * dropMultiplicator)) / (drop * dropMultiplicator);
        if (idx == limit - 1)
          dropMultiplicator *= 2;
        list.erase(list.begin() + idx + 1);
      }
    }
    std::vector<unsigned int> out;
    for (const solution_c * s : list)
      out.push_back(s->getSolutionNumber());
    return out;
  }

  // problem_c::sortSolutions(by)'s orders, for the replay of the live sort
  bool before(int by, const solution_c * a, const solution_c * b) {
    switch (by) {
      case 0: return a->getAssemblyNumber() < b->getAssemblyNumber();
      case 1: return a->getDisassemblyInfo() && b->getDisassemblyInfo() &&
                     a->getDisassemblyInfo()->compare(b->getDisassemblyInfo()) < 0;
      case 2: return a->getDisassemblyInfo() && b->getDisassemblyInfo() &&
                     a->getDisassemblyInfo()->sumMoves() < b->getDisassemblyInfo()->sumMoves();
      default: return a->getAssembly()->comparePieces(b->getAssembly()) > 0;
    }
  }

  // the old unsorted step with the live sort: append, thin, sort it all
  std::vector<unsigned int> replayLiveSort(int by, unsigned int limit) {
    std::vector<const solution_c *> list;
    unsigned int numSolutions = 0;
    unsigned int dropMultiplicator = 1;
    for (const solution_c * s : arrivals().found) {
      // drop 1, but a thinned list keeps only every dropMultiplicator-th
      if (numSolutions % dropMultiplicator == 0)
        list.push_back(s);
      numSolutions++;
      if (limit && list.size() > limit) {
        unsigned int idx = numSolutions - 1;
        idx = (idx % (limit * dropMultiplicator)) / dropMultiplicator;
        if (idx == limit - 1)
          dropMultiplicator *= 2;
        list.erase(list.begin() + idx + 1);
      }
      if (list.size() >= 2)
        std::stable_sort(list.begin(), list.end(), [by](const solution_c * a, const solution_c * b) { return before(by, a, b); });
    }
    std::vector<unsigned int> out;
    for (const solution_c * s : list)
      out.push_back(s->getSolutionNumber());
    return out;
  }

  struct NoPool {
    // one thread, the disassembly inline: the same arrivals every solve
    NoPool() { setenv_("BURRTOOLS_NO_DISASM_POOL", "1"); }
    ~NoPool() { setenv_("BURRTOOLS_NO_DISASM_POOL", nullptr); }
    static void setenv_(const char * name, const char * value) {
#ifdef _WIN32
      _putenv_s(name, value ? value : "");
#else
      if (value) setenv(name, value, 1); else unsetenv(name);
#endif
    }
  };
}

TEST_CASE("solution order: the arrivals are distinct, numbered, and varied in moves", "[solver][order]") {
  NoPool np;
  const auto & a = arrivals().found;
  REQUIRE(a.size() > 100);
  std::vector<unsigned int> nums;
  unsigned int fewest = ~0u, most = 0;
  for (const solution_c * s : a) {
    REQUIRE(s->getDisassembly() != nullptr);
    nums.push_back(s->getSolutionNumber());
    fewest = std::min(fewest, s->getDisassembly()->sumMoves());
    most = std::max(most, s->getDisassembly()->sumMoves());
  }
  std::vector<unsigned int> sorted = nums;
  std::sort(sorted.begin(), sorted.end());
  REQUIRE(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());
  REQUIRE(nums == sorted);                 // unsorted: in the order found
  REQUIRE(fewest < most);                  // something to sort
}

TEST_CASE("solution order: sorted by moves as the scan placed them", "[solver][order]") {
  NoPool np;
  const Run r = solved(solveThread_c::SRT_COMPLETE_MOVES, kAll, 1, -1);
  REQUIRE(order(*r.problem) == replayScan(solveThread_c::SRT_COMPLETE_MOVES, kAll, 1));
}

TEST_CASE("solution order: sorted by level as the scan placed them", "[solver][order]") {
  NoPool np;
  const Run r = solved(solveThread_c::SRT_LEVEL, kAll, 1, -1);
  REQUIRE(order(*r.problem) == replayScan(solveThread_c::SRT_LEVEL, kAll, 1));
}

TEST_CASE("solution order: a limit and a drop thin the sorted list as before", "[solver][order]") {
  NoPool np;
  for (const int sort : { int(solveThread_c::SRT_COMPLETE_MOVES), int(solveThread_c::SRT_LEVEL) })
    for (const unsigned int limit : { 10u, 37u }) {
      const Run r = solved(sort, limit, 1, -1);
      INFO("sort " << sort << ", limit " << limit);
      REQUIRE(order(*r.problem) == replayScan(sort, limit, 1));
    }
}

TEST_CASE("solution order: the live sort keeps the list as sorting it all after each solution did",
          "[solver][order]") {
  NoPool np;
  for (const int by : { 0, 1, 2, 3 })
    for (const unsigned int limit : { kAll, 25u }) {
      const Run r = solved(solveThread_c::SRT_UNSORT, limit, 1, by);
      INFO("live sort " << by << ", limit " << limit);
      REQUIRE(order(*r.problem) == replayLiveSort(by, limit));
    }
}

TEST_CASE("solution order: the live sort over a moves-sorted solve", "[solver][order]") {
  // legacy's sort chosen while it solves: placed by moves, then sorted by another
  NoPool np;
  for (const int by : { 0, 1, 2, 3 }) {
    const Run r = solved(solveThread_c::SRT_COMPLETE_MOVES, kAll, 1, by);
    INFO("live sort " << by);
    // replay: the scan placed each one, then the old code sorted the whole list
    std::vector<const solution_c *> list;
    for (const solution_c * s : arrivals().found) {
      unsigned int i = 0;
      for (; i < list.size(); i++)
        if (list[i]->getDisassembly() && list[i]->getDisassembly()->sumMoves() > s->getDisassembly()->sumMoves())
          break;
      list.insert(list.begin() + i, s);
      if (list.size() >= 2)
        std::stable_sort(list.begin(), list.end(), [by](const solution_c * a, const solution_c * b) { return before(by, a, b); });
    }
    std::vector<unsigned int> want;
    for (const solution_c * s : list)
      want.push_back(s->getSolutionNumber());
    REQUIRE(order(*r.problem) == want);
  }
}

TEST_CASE("solution order: solutionsSortedBy and placeSorted", "[solver][order][problem]") {
  auto p = puzzle_c::load(kPuzzle);
  REQUIRE(p != nullptr);
  problem_c * pr = p->getProblem(0);
  REQUIRE(pr->getNumberOfSavedSolutions() > 1);
  for (const int by : { 0, 1, 2, 3 }) {
    INFO("by " << by);
    pr->sortSolutions(by);
    REQUIRE(pr->solutionsSortedBy(by));
    const std::vector<unsigned int> sorted = [&] {
      std::vector<unsigned int> out;
      for (unsigned int i = 0; i < pr->getNumberOfSavedSolutions(); i++)
        out.push_back(pr->getSavedSolution(i)->getAssemblyNumber());
      return out;
    }();
    // in its place already: it stays, and the list with it
    const uint64_t rev = pr->solutionsRevision();
    REQUIRE(pr->placeSorted(by, pr->getNumberOfSavedSolutions() / 2));
    REQUIRE(pr->solutionsRevision() != rev);
    for (unsigned int i = 0; i < sorted.size(); i++)
      REQUIRE(pr->getSavedSolution(i)->getAssemblyNumber() == sorted[i]);
  }
  REQUIRE_FALSE(pr->solutionsSortedBy(7));   // no such order
}

TEST_CASE("solution order: a changed disassembly counts as a change", "[solver][order][solution]") {
  auto p = puzzle_c::load(kPuzzle);
  REQUIRE(p != nullptr);
  problem_c * pr = p->getProblem(0);
  const uint64_t before = solution_c::changeCount();
  pr->getSavedSolution(0)->removeDisassembly();
  REQUIRE(solution_c::changeCount() > before);
}
