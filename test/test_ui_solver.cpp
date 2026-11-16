/* Tests for btui's solver rules: what the Solver workspace's cards ask of the
 * solver and show of its results (C19, C20).
 */
#include <catch2/catch_test_macros.hpp>

#include "../src/uicore/solver.h"

#include "../src/lib/assembly.h"
#include "../src/lib/disassembler_0.h"
#include "../src/lib/disassembly.h"
#include "../src/lib/disasmtomoves.h"
#include "../src/lib/problem.h"
#include "../src/lib/puzzle.h"
#include "../src/lib/solution.h"
#include "../src/lib/solvethread.h"
#include "../src/lib/voxel.h"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <memory>
#include <random>
#include <thread>

using namespace btui;

namespace {

  /* the problem solved to the end with these options, as the GUI would */
  void solve(problem_c & prob, const SolverOptions & o) {
    const SolverParameters par = solverParameters(o);
    solveThread_c t(prob, par.flags, 1);
    t.setSortMethod(par.sortMethod);
    t.setSolutionLimits(par.limit, par.drop);
    t.setLiveSort(par.liveSort);
    REQUIRE(t.start());
    while (t.currentAction() != solveThread_c::ACT_FINISHED && t.currentAction() != solveThread_c::ACT_ERROR)
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    REQUIRE(t.currentAction() == solveThread_c::ACT_FINISHED);
  }

  SolutionFacts fact(int index, unsigned assembly, int pieces, int moves = -1, std::vector<unsigned> level = {}) {
    SolutionFacts f;
    f.index = index;
    f.assembly = assembly;
    f.pieces = pieces;
    f.moves = moves;
    f.level = std::move(level);
    f.da = moves >= 0 ? DaState::Stored : DaState::NotChecked;
    return f;
  }
}

// --- the search options -----------------------------------------------------

TEST_CASE("Don't keep the disassemblies implies the disassembly check", "[ui][solver]") {
  // AC-C19-02
  SolverOptions o;
  CHECK_FALSE(checksDisassembly(o));
  o.dropDisassemblies = true;
  CHECK(checksDisassembly(o));
  CHECK(normalised(o).disassemble);
  const SolverParameters p = solverParameters(o);
  CHECK((p.flags & solveThread_c::PAR_DISASSM) != 0);
  CHECK((p.flags & solveThread_c::PAR_DROP_DISASSEMBLIES) != 0);
}

TEST_CASE("Level and Moves need the disassembly and fall back to the search order", "[ui][solver]") {
  // AC-C19-03
  SolverOptions o;
  CHECK(o.sort == SearchSort::Moves);                  // the default
  CHECK_FALSE(searchSortAvailable(o, SearchSort::Level));
  CHECK_FALSE(searchSortAvailable(o, SearchSort::Moves));
  CHECK(searchSortAvailable(o, SearchSort::Pieces));
  CHECK(effectiveSort(o) == SearchSort::Id);
  CHECK(solverParameters(o).sortMethod == solveThread_c::SRT_UNSORT);
  o.disassemble = true;
  CHECK(effectiveSort(o) == SearchSort::Moves);
  CHECK(solverParameters(o).sortMethod == solveThread_c::SRT_COMPLETE_MOVES);
  o.sort = SearchSort::Level;
  CHECK(solverParameters(o).sortMethod == solveThread_c::SRT_LEVEL);
  o.sort = SearchSort::Pieces;
  CHECK(solverParameters(o).sortMethod == solveThread_c::SRT_UNSORT);
  CHECK(solverParameters(o).liveSort == 3);
}

TEST_CASE("the options map onto the solver's flags and limits", "[ui][solver]") {
  SolverOptions o;
  CHECK(solverParameters(o).flags == solveThread_c::PAR_REDUCE);
  o.justCount = o.keepMirror = o.keepRotated = o.thoroughRotation = true;
  o.keepEvery = 3;
  o.limit = 0;                                         // below the solver's bounds
  const SolverParameters p = solverParameters(o);
  CHECK(p.flags == (solveThread_c::PAR_REDUCE | solveThread_c::PAR_JUST_COUNT | solveThread_c::PAR_KEEP_MIRROR
                    | solveThread_c::PAR_KEEP_ROTATIONS | solveThread_c::PAR_COMPLETE_ROTATIONS));
  CHECK(p.drop == 3);
  CHECK(p.limit == 1);
}

TEST_CASE("the Search options count the rows changed from their defaults", "[ui][solver]") {
  // AC-C19-03: "· n changed"
  SolverOptions o;
  CHECK(changedOptions(o) == 0);
  o.disassemble = true;                                // not a Search options row
  CHECK(changedOptions(o) == 0);
  o.keepMirror = true;
  o.limit = 50;
  o.sort = SearchSort::Level;
  CHECK(changedOptions(o) == 3);
}

// --- the run ------------------------------------------------------------------

TEST_CASE("the Run buttons follow the state table", "[ui][solver]") {
  // AC-C19-04, OQ-29
  RunFacts f;
  f.problem = f.solvable = true;
  f.solveState = SS_UNSOLVED;
  CHECK(runState(f) == RunState::Idle);
  CHECK(runButtons(f) == RunButtons{ true, true, false, false, false, false });

  f.threadHere = true;
  f.action = solveThread_c::ACT_PREPARATION;
  CHECK(runState(f) == RunState::Preparing);
  CHECK(runButtons(f) == RunButtons{ false, false, false, true, false, false });

  f.action = solveThread_c::ACT_ASSEMBLING;
  CHECK(runState(f) == RunState::Running);
  CHECK(runButtons(f).stop);
  CHECK(activity(f, false) == Activity::Assembling);
  CHECK(activity(f, true) == Activity::Counting);

  f.action = solveThread_c::ACT_WAIT_TO_STOP;
  CHECK(runState(f) == RunState::Stopping);
  CHECK(runButtons(f) == RunButtons{});

  // stopped: Continue resumes, the explorer may look
  f.threadHere = false;
  f.action = -1;
  f.solveState = SS_SOLVING;
  f.canContinue = f.hasAssembler = true;
  CHECK(runState(f) == RunState::Paused);
  CHECK(runButtons(f) == RunButtons{ true, true, true, false, true, true });

  // prepared: the same buttons, the state named Ready
  f.prepared = true;
  CHECK(runState(f) == RunState::Ready);
  CHECK(activity(f, false) == Activity::Ready);
  CHECK(runButtons(f).cont);

  f.prepared = false;
  f.solveState = SS_SOLVED;
  f.hasAssembler = false;
  CHECK(runState(f) == RunState::Finished);
  CHECK(runButtons(f) == RunButtons{ true, true, false, false, false, false });
}

TEST_CASE("nothing runs while another problem is solved or an analysis runs", "[ui][solver]") {
  RunFacts f;
  f.problem = f.solvable = true;
  f.threadElsewhere = true;
  CHECK(runButtons(f) == RunButtons{});
  f.threadElsewhere = false;
  f.busy = true;
  CHECK(runButtons(f) == RunButtons{});
  f.busy = false;
  f.solvable = false;
  CHECK_FALSE(runButtons(f).start);
}

TEST_CASE("time is said in the largest unit that fits, as legacy", "[ui][solver]") {
  using U = TimeText::Unit;
  CHECK(timeText(0) == TimeText{ U::Seconds, 0 });
  CHECK(timeText(59.9) == TimeText{ U::Seconds, 59 });
  CHECK(timeText(90) == TimeText{ U::Minutes, 1.5 });
  CHECK(timeText(3 * 3600) == TimeText{ U::Hours, 3 });
  CHECK(timeText(36 * 3600) == TimeText{ U::Days, 1.5 });
  CHECK(timeText(1e30).unit == U::Ages);
  CHECK(timeText(-5) == TimeText{ U::Seconds, 0 });
}

TEST_CASE("time left is estimated once the progress passes about 6 percent", "[ui][solver]") {
  CHECK_FALSE(timeLeft(10, 0).has_value());
  CHECK_FALSE(timeLeft(10, 0.05).has_value());
  REQUIRE(timeLeft(10, 0.1).has_value());
  CHECK(*timeLeft(10, 0.1) == 90);
  CHECK(*timeLeft(10, 1.0) == 0);
}

// --- the solutions --------------------------------------------------------------

TEST_CASE("a click on the active column reverses, another starts in its own direction", "[ui][solver]") {
  ListSort s;
  CHECK(s == ListSort{ ListKey::Number, false });
  s = clickSort(s, ListKey::Number);
  CHECK(s == ListSort{ ListKey::Number, true });
  s = clickSort(s, ListKey::Moves);
  CHECK(s == ListSort{ ListKey::Moves, true });       // best first
  s = clickSort(s, ListKey::Moves);
  CHECK(s == ListSort{ ListKey::Moves, false });
  CHECK(clickSort(s, ListKey::Pieces) == ListSort{ ListKey::Pieces, true });
  CHECK(clickSort(s, ListKey::Level) == ListSort{ ListKey::Level, true });
}

TEST_CASE("the list shows what can be taken apart when the disassembly is checked", "[ui][solver]") {
  // AC-C20-03
  std::vector<SolutionFacts> all = { fact(0, 4, 3, 10, { 9, 1 }), fact(1, 2, 3), fact(2, 7, 4, 6, { 4, 2 }) };
  all[1].da = DaState::Cannot;
  CHECK(shownSolutions(all, false, {}) == std::vector<int>{ 1, 0, 2 });     // by number
  CHECK(shownSolutions(all, true, {}) == std::vector<int>{ 0, 2 });
  CHECK(shownSolutions(all, true, { ListKey::Moves, true }) == std::vector<int>{ 0, 2 });
  CHECK(shownSolutions(all, true, { ListKey::Moves, false }) == std::vector<int>{ 2, 0 });
  CHECK(shownSolutions(all, false, { ListKey::Pieces, true }) == std::vector<int>{ 2, 1, 0 });   // ties by number
  CHECK(shownSolutions({}, true, {}).empty());
}

TEST_CASE("sorting by level compares the moves piece by piece, unknown last when best first", "[ui][solver]") {
  std::vector<SolutionFacts> all = { fact(0, 1, 3, 5, { 3, 2 }), fact(1, 2, 3, 5, { 3, 1, 1 }), fact(2, 3, 3),
                                     fact(3, 4, 3, 4, { 4 }) };
  CHECK(shownSolutions(all, false, { ListKey::Level, true }) == std::vector<int>{ 3, 0, 1, 2 });
}

TEST_CASE("Level and Moves are offered once some solution has disassembly information", "[ui][solver]") {
  std::vector<SolutionFacts> all = { fact(0, 1, 3), fact(1, 2, 3) };
  CHECK_FALSE(listKeyAvailable(all, ListKey::Level));
  CHECK(listKeyAvailable(all, ListKey::Pieces));
  // an unavailable key sorts by number
  CHECK(shownSolutions(all, false, { ListKey::Moves, true }) == std::vector<int>{ 0, 1 });
  all[1].moves = 3;
  CHECK(listKeyAvailable(all, ListKey::Moves));
}

TEST_CASE("Before and After follow the list as shown, All and w/o DA reach hidden rows", "[ui][solver]") {
  // OQ-25 and the product owner's rule for Before / After
  std::vector<SolutionFacts> all = { fact(0, 1, 3, 9, { 9 }), fact(1, 2, 3), fact(2, 3, 3, 2, { 2 }),
                                     fact(3, 4, 3, 5, { 5 }) };
  const std::vector<int> shown = shownSolutions(all, true, { ListKey::Moves, true });   // 0, 3, 2
  REQUIRE(shown == std::vector<int>{ 0, 3, 2 });
  CHECK(toRemove(all, shown, 1, RemoveKind::At) == std::vector<int>{ 3 });
  CHECK(toRemove(all, shown, 1, RemoveKind::Before) == std::vector<int>{ 0 });
  CHECK(toRemove(all, shown, 1, RemoveKind::After) == std::vector<int>{ 2 });
  CHECK(toRemove(all, shown, 1, RemoveKind::WithoutDisassembly) == std::vector<int>{ 1 });
  CHECK(toRemove(all, shown, 1, RemoveKind::All) == std::vector<int>{ 0, 1, 2, 3 });
  // nothing selected: the row-relative ones remove nothing
  CHECK(toRemove(all, shown, -1, RemoveKind::At).empty());
  CHECK(toRemove(all, shown, 7, RemoveKind::After).empty());
}

TEST_CASE("after a removal the selection keeps its solution or takes the next", "[ui][solver]") {
  CHECK(rowAfterRemoval(3, 3, 5) == 0);    // Before
  CHECK(rowAfterRemoval(3, 0, 5) == 3);    // At: the next one took its place
  CHECK(rowAfterRemoval(4, 0, 4) == 3);    // At the last: the new last
  CHECK(rowAfterRemoval(2, 0, 0) == -1);
}

// --- the list kept between reads ------------------------------------------------

namespace {

  /* a saved list as a solve leaves it, for SolutionList::read */
  struct FakeList {
    std::vector<SolutionFacts> saved;
    int factsRead = 0;
    SolutionList::Basis basis;

    SolutionList::Source source(void) {
      SolutionList::Source s;
      s.count = unsigned(saved.size());
      s.assembly = [this](unsigned i) { return saved[i].assembly; };
      s.facts = [this](unsigned i) { factsRead++; return saved[i]; };
      return s;
    }
    SolutionList::Change read(SolutionList & l, bool only, ListSort sort) {
      return l.read(source(), basis, only, sort);
    }
  };

  /* the rows as assembly numbers */
  std::vector<unsigned> rowsOf(const std::vector<SolutionFacts> & all, const std::vector<int> & shown) {
    std::vector<unsigned> out;
    for (int r : shown)
      out.push_back(all[size_t(r)].assembly);
    return out;
  }

  /* what a view does with a Change: the rows it had, made the rows now */
  void apply(std::vector<unsigned> & rows, const SolutionList::Change & ch, const std::vector<unsigned> & now) {
    if (ch.reset) {
      rows = now;
      return;
    }
    for (auto it = ch.removed.rbegin(); it != ch.removed.rend(); ++it)
      rows.erase(rows.begin() + *it);
    for (int r : ch.inserted)
      rows.insert(rows.begin() + r, now[size_t(r)]);
  }
}

TEST_CASE("the kept list's rows are shownSolutions' through adds, removals and sorts", "[ui][solver]") {
  // a solve adding solutions at any place of the saved list, some removed at
  // the limit, the sort and the filter changed now and then: the rows always
  // shownSolutions' over the list, and the changes reported make them
  std::mt19937 rng(20261115);
  FakeList src;
  SolutionList list;
  std::vector<unsigned> view;
  unsigned next = 0;
  ListSort sort;
  bool only = false;
  const ListSort sorts[] = { {}, { ListKey::Number, true }, { ListKey::Moves, true }, { ListKey::Moves, false },
                             { ListKey::Level, true }, { ListKey::Pieces, true }, { ListKey::Pieces, false } };
  for (int step = 0; step < 400; step++) {
    const int adds = int(rng() % 12);
    for (int k = 0; k < adds; k++) {
      // assembly numbers mostly rising, as a solve finds them; moves known once past the first steps
      next += 1 + rng() % 3;
      const bool known = step > 20 || rng() % 2;
      SolutionFacts f = fact(0, next, int(3 + rng() % 3), known ? int(rng() % 9) : -1,
                             known ? std::vector<unsigned>{ unsigned(rng() % 4), unsigned(rng() % 3) } : std::vector<unsigned>{});
      if (known && rng() % 5 == 0)
        f.da = DaState::LevelOnly;
      const size_t at = src.saved.empty() ? 0 : rng() % (src.saved.size() + 1);
      src.saved.insert(src.saved.begin() + std::ptrdiff_t(at), f);
    }
    if (!src.saved.empty() && rng() % 3 == 0) {
      const int drops = 1 + int(rng() % 3);
      for (int k = 0; k < drops && !src.saved.empty(); k++)
        src.saved.erase(src.saved.begin() + std::ptrdiff_t(rng() % src.saved.size()));
    }
    if (rng() % 40 == 0)
      sort = sorts[rng() % std::size(sorts)];
    if (rng() % 60 == 0)
      only = !only;

    const auto ch = src.read(list, only, sort);
    const std::vector<unsigned> want = rowsOf(src.saved, shownSolutions(src.saved, only, sort));
    const std::vector<unsigned> now = rowsOf(list.all(), list.shown());
    REQUIRE(now == want);
    apply(view, ch, now);
    REQUIRE(view == want);
    // every solution's place in the saved list as it is now
    for (const SolutionFacts & f : list.all())
      REQUIRE(src.saved[size_t(f.index)].assembly == f.assembly);
    REQUIRE(list.all().size() == src.saved.size());
  }
}

TEST_CASE("an analysis's solutions read again move only their rows", "[ui][solver]") {
  // a list read once, then batches of its solutions given (or robbed of) a
  // disassembly, as Manage > Analyse stores them: the rows always
  // shownSolutions' over the list, the changes reported make them, and only
  // the changed solutions' facts are read again
  std::mt19937 rng(20261116);
  FakeList src;
  for (unsigned a = 0; a < 300; a++)
    src.saved.push_back(fact(0, a * 2 + 1, int(3 + rng() % 3)));       // none checked yet
  for (size_t i = 0; i < src.saved.size(); i++)
    src.saved[i].index = int(i);
  const ListSort sorts[] = { {}, { ListKey::Number, true }, { ListKey::Moves, true }, { ListKey::Moves, false },
                             { ListKey::Level, true }, { ListKey::Pieces, false } };
  for (bool only : { false, true })
    for (const ListSort & sort : sorts) {
      SolutionList list;
      src.read(list, only, sort);
      std::vector<unsigned> view = rowsOf(list.all(), list.shown());
      for (int step = 0; step < 40; step++) {
        std::vector<unsigned> analysed;
        const int n = 1 + int(rng() % 25);
        for (int k = 0; k < n; k++) {
          SolutionFacts & f = src.saved[rng() % src.saved.size()];
          if (rng() % 4 == 0) {
            f.da = DaState::Cannot;
            f.moves = -1;
            f.level.clear();
          } else {
            f.da = DaState::Stored;
            f.moves = int(rng() % 9);
            f.level = { unsigned(f.moves), unsigned(rng() % 3) };
          }
          analysed.push_back(f.assembly);
        }
        src.factsRead = 0;
        src.basis.changes++;
        const auto ch = list.readChanged(src.source(), src.basis, analysed, only, sort);
        const std::vector<unsigned> want = rowsOf(src.saved, shownSolutions(src.saved, only, sort));
        const std::vector<unsigned> now = rowsOf(list.all(), list.shown());
        REQUIRE(now == want);
        apply(view, ch, now);
        REQUIRE(view == want);
        if (!ch.reset)
          REQUIRE(src.factsRead <= n);
      }
    }
}

TEST_CASE("readChanged reads everything when it cannot be sure", "[ui][solver]") {
  FakeList src;
  for (unsigned a = 0; a < 10; a++)
    src.saved.push_back(fact(0, a, 3));
  for (size_t i = 0; i < src.saved.size(); i++)
    src.saved[i].index = int(i);
  SolutionList list;
  // not read before
  auto ch = list.readChanged(src.source(), src.basis, { 3 }, false, {});
  CHECK(ch.reset);
  CHECK(list.shown().size() == 10);
  // a number it does not know
  src.factsRead = 0;
  ch = list.readChanged(src.source(), src.basis, { 99 }, false, {});
  CHECK(ch.reset);
  CHECK(src.factsRead == 10);
  // the list grew meanwhile
  src.saved.push_back(fact(0, 10, 3));
  src.saved.back().index = 10;
  ch = list.readChanged(src.source(), src.basis, { 3 }, false, {});
  CHECK(ch.reset);
  CHECK(list.shown().size() == 11);
}

TEST_CASE("the kept list reads the facts of new solutions only", "[ui][solver]") {
  FakeList src;
  for (unsigned a = 0; a < 100; a++)
    src.saved.push_back(fact(0, a, 3, int(a % 7), { a % 7 }));
  SolutionList list;
  const ListSort byMoves{ ListKey::Moves, true };
  CHECK(src.read(list, true, byMoves).reset);
  CHECK(src.factsRead == 100);

  // three new ones in the middle of the saved list, one gone
  src.saved.insert(src.saved.begin() + 10, fact(0, 200, 3, 6, { 6 }));
  src.saved.insert(src.saved.begin() + 50, fact(0, 201, 3, 0, { 0 }));
  src.saved.push_back(fact(0, 202, 3, 3, { 3 }));
  src.saved.erase(src.saved.begin() + 70);
  const auto ch = src.read(list, true, byMoves);
  CHECK_FALSE(ch.reset);
  CHECK(src.factsRead == 103);
  CHECK(ch.removed.size() == 1);
  CHECK(ch.inserted.size() == 3);

  // nothing new: nothing read, nothing changed
  CHECK(src.read(list, true, byMoves).none());
  CHECK(src.factsRead == 103);

  // a solution's own facts may have changed (an analysis): everything again
  src.basis.changes++;
  CHECK(src.read(list, true, byMoves).reset);
  CHECK(src.factsRead == 103 + 102);

  // another sort: the rows anew, the facts kept
  CHECK(src.read(list, true, { ListKey::Number, false }).reset);
  CHECK(src.factsRead == 205);

  // cleared: the next read reads everything
  CHECK(list.clear().reset);
  CHECK(list.all().empty());
  CHECK(src.read(list, true, byMoves).reset);
  CHECK(src.factsRead == 307);
}

TEST_CASE("the kept list reads everything while two solutions share a number", "[ui][solver]") {
  FakeList src;
  src.saved = { fact(0, 5, 3, 2, { 2 }), fact(0, 5, 3, 1, { 1 }), fact(0, 7, 3, 4, { 4 }) };
  SolutionList list;
  CHECK(src.read(list, false, {}).reset);
  src.saved.push_back(fact(0, 9, 3, 1, { 1 }));
  CHECK(src.read(list, false, {}).reset);
  CHECK(rowsOf(list.all(), list.shown()) == rowsOf(src.saved, shownSolutions(src.saved, false, {})));
  // a new one with a number already there: read anew, the rows still right
  FakeList b;
  b.saved = { fact(0, 1, 3), fact(0, 2, 3) };
  SolutionList l2;
  b.read(l2, false, {});
  b.saved.push_back(fact(0, 2, 4));
  CHECK(b.read(l2, false, {}).reset);
  CHECK(l2.all().size() == 3);
}

TEST_CASE("the kept list follows a problem's saved solutions", "[ui][solver]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p);
  problem_c & prob = *p->getProblem(0);
  prob.removeAllSolutions();
  solve(prob, SolverOptions{});
  auto g = prob.lockSolutions();
  SolutionList list;
  CHECK(list.read(prob, {}, false, {}).reset);
  REQUIRE(list.shown().size() == 12);
  const unsigned third = prob.getSavedSolution(3)->getAssemblyNumber();
  prob.removeSolution(3);
  auto ch = list.read(prob, {}, false, {});
  CHECK_FALSE(ch.reset);
  CHECK(ch.removed == std::vector<int>{ 3 });
  CHECK(list.shown().size() == 11);
  // an analysis's verdict changes a fact: read anew
  ch = list.read(prob, { prob.getSavedSolution(0)->getAssemblyNumber() }, false, {});
  CHECK(ch.reset);
  CHECK(list.all()[size_t(list.shown()[0])].da == DaState::Cannot);
  // a disassembly deleted counts as a change inside a solution
  ch = list.read(prob, { prob.getSavedSolution(0)->getAssemblyNumber() }, false, {});
  CHECK(ch.none());
  prob.getSavedSolution(1)->removeDisassembly();
  CHECK(list.read(prob, { prob.getSavedSolution(0)->getAssemblyNumber() }, false, {}).reset);
  for (const SolutionFacts & f : list.all())
    CHECK(f.assembly != third);
}

TEST_CASE("a solved problem's solutions read with their disassembly state", "[ui][solver]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p);
  problem_c & prob = *p->getProblem(0);
  prob.removeAllSolutions();

  SECTION("assemblies only: every assembly kept, none checked") {
    SolverOptions o;
    solve(prob, o);
    auto g = prob.lockSolutions();
    const auto all = solutionFacts(prob);
    CHECK(all.size() == 12);
    for (const SolutionFacts & f : all) {
      CHECK(f.da == DaState::NotChecked);
      CHECK(f.moves == -1);
      CHECK(f.pieces == 7);
    }
    CHECK(shownSolutions(all, true, {}).empty());
    // an analysis that failed marks its assembly
    const auto marked = solutionFacts(prob, { all[2].assembly });
    CHECK(marked[2].da == DaState::Cannot);
  }

  SECTION("with the disassembly: the one solution, stored, its level read") {
    SolverOptions o;
    o.disassemble = true;
    solve(prob, o);
    auto g = prob.lockSolutions();
    const auto all = solutionFacts(prob);
    REQUIRE(all.size() == 1);
    CHECK(all[0].da == DaState::Stored);
    CHECK(all[0].levelText == "98.2.4.2");
    CHECK(all[0].level == std::vector<unsigned>{ 98, 2, 4, 2 });
    CHECK(all[0].moves > 0);
  }

  SECTION("without keeping the disassemblies: level only") {
    SolverOptions o;
    o.dropDisassemblies = true;
    solve(prob, o);
    auto g = prob.lockSolutions();
    const auto all = solutionFacts(prob);
    REQUIRE(all.size() == 1);
    CHECK(all[0].da == DaState::LevelOnly);
    CHECK(all[0].levelText == "98.2.4.2");
  }
}

// --- the pieces ---------------------------------------------------------------------

TEST_CASE("pieces are named by shape, copies of one shape S5.1, S5.2", "[ui][solver]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p);
  problem_c & prob = *p->getProblem(0);
  prob.removeAllSolutions();
  solve(prob, SolverOptions{});
  auto g = prob.lockSolutions();
  REQUIRE(prob.getNumberOfSavedSolutions() > 0);
  const auto chips = pieceChips(prob, prob.getSavedSolution(0)->getAssembly());
  REQUIRE(chips.size() == 7);
  // one id per piece, in the assembly's piece order
  for (size_t i = 0; i < chips.size(); i++) {
    CHECK(chips[i].piece == int(i));
    const unsigned copies = [&] {
      for (unsigned k = 0; k < prob.getNumberOfParts(); k++)
        if (int(prob.getShapeIdOfPart(k)) == chips[i].shape)
          return prob.getPartMaximum(k);
      return 0u;
    }();
    const std::string base = "S" + std::to_string(chips[i].shape + 1);
    CHECK(chips[i].id == (copies > 1 ? base + "." + std::to_string(chips[i].copy + 1) : base));
  }
  CHECK(pieceChips(prob, nullptr).empty());
  // every piece the problem may hold: the placed ones and more
  CHECK(pieceChips(prob).size() == prob.getNumberOfPieces());
}

TEST_CASE("a solution's STL files are one per placed shape, named safely", "[ui][solver]") {
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p);
  problem_c & prob = *p->getProblem(0);
  prob.removeAllSolutions();
  solve(prob, SolverOptions{});
  auto g = prob.lockSolutions();
  const auto shapes = placedShapes(prob, prob.getSavedSolution(0)->getAssembly());
  REQUIRE_FALSE(shapes.empty());
  CHECK(std::is_sorted(shapes.begin(), shapes.end()));
  CHECK(std::adjacent_find(shapes.begin(), shapes.end()) == shapes.end());
  CHECK(placedShapes(prob, nullptr).empty());
  // legacy safeName(): only letters, digits, '-' and '_'
  CHECK(solutionStlName("", 2) == "S3.stl");
  CHECK(solutionStlName("Cage 2/b", 0) == "Cage_2_b.stl");
  CHECK(solutionStlName("a-b_c", 0) == "a-b_c.stl");
  CHECK(solutionStlName("\xc3\xa9", 0) == "__.stl");     // a UTF-8 letter is two bytes
}

TEST_CASE("a chip cycles solid, wireframe, hidden", "[ui][solver]") {
  CHECK(nextPieceState(PieceState::Solid) == PieceState::Wireframe);
  CHECK(nextPieceState(PieceState::Wireframe) == PieceState::Hidden);
  CHECK(nextPieceState(PieceState::Hidden) == PieceState::Solid);
}

// --- the player ------------------------------------------------------------------------

TEST_CASE("playing runs at 1.2 moves a second times the speed and stops at the end", "[ui][solver]") {
  CHECK(playedTo(0, 19, 1, 1) == 1.2);
  CHECK(playedTo(0, 19, 1, 8) == 9.6);
  CHECK(playedTo(18, 19, 5, 2) == 19);
  CHECK(playedTo(3, 0, 1, 1) == 0);
}

TEST_CASE("a step goes to the next whole move", "[ui][solver]") {
  CHECK(stepMove(14.2, 19, -1) == 14);
  CHECK(stepMove(14.2, 19, 1) == 15);
  CHECK(stepMove(14, 19, -1) == 13);
  CHECK(stepMove(14, 19, 1) == 15);
  CHECK(stepMove(0, 19, -1) == 0);
  CHECK(stepMove(19, 19, 1) == 19);
}

// --- the keys -----------------------------------------------------------------------------

TEST_CASE("the list, the solution slider and the seekbar share one key map", "[ui][solver]") {
  // AC-C20-07, overview/04 §8
  for (KeyContext c : { KeyContext::List, KeyContext::Slider, KeyContext::Seekbar }) {
    CHECK(solverKey(c, SolverKey::Space, false) == SolverAction::TogglePlay);
    CHECK(solverKey(c, SolverKey::Enter, false) == SolverAction::TogglePlay);
    CHECK(solverKey(c, SolverKey::Left, false) == SolverAction::MoveBack);
    CHECK(solverKey(c, SolverKey::Right, true) == SolverAction::ToDisassembled);
    CHECK(solverKey(c, SolverKey::Left, true) == SolverAction::ToAssembled);
    CHECK(solverKey(c, SolverKey::Up, false) == SolverAction::Previous);
    CHECK(solverKey(c, SolverKey::PageDown, false) == SolverAction::PageForward);
    CHECK(solverKey(c, SolverKey::Escape, false) == SolverAction::Pause);
  }
  // Home / End: solutions, but the seekbar's moves
  CHECK(solverKey(KeyContext::List, SolverKey::Home, false) == SolverAction::First);
  CHECK(solverKey(KeyContext::Slider, SolverKey::End, false) == SolverAction::Last);
  CHECK(solverKey(KeyContext::Seekbar, SolverKey::Home, false) == SolverAction::ToAssembled);
  CHECK(solverKey(KeyContext::Seekbar, SolverKey::End, false) == SolverAction::ToDisassembled);
  // Delete removes from the list only
  CHECK(solverKey(KeyContext::List, SolverKey::Delete, false) == SolverAction::Remove);
  CHECK(solverKey(KeyContext::List, SolverKey::Backspace, false) == SolverAction::Remove);
  CHECK(solverKey(KeyContext::Slider, SolverKey::Delete, false) == SolverAction::None);
}

TEST_CASE("solution moves stay in the list", "[ui][solver]") {
  CHECK(rowAfter(SolverAction::Next, 3, 5) == 4);
  CHECK(rowAfter(SolverAction::Next, 4, 5) == 4);
  CHECK(rowAfter(SolverAction::Previous, 0, 5) == 0);
  CHECK(rowAfter(SolverAction::PageForward, 1, 20) == 9);
  CHECK(rowAfter(SolverAction::PageBack, 3, 20) == 0);
  CHECK(rowAfter(SolverAction::Last, 0, 20) == 19);
  CHECK(rowAfter(SolverAction::First, 7, 20) == 0);
  CHECK(rowAfter(SolverAction::TogglePlay, 7, 20) == 7);
  CHECK(rowAfter(SolverAction::Next, -1, 0) == -1);
}

TEST_CASE("the thread's every action names the run and its activity", "[ui][solver]") {
  RunFacts f;
  f.problem = f.solvable = f.threadHere = true;
  f.solveState = SS_SOLVING;

  f.action = solveThread_c::ACT_DISASSEMBLING;
  CHECK(runState(f) == RunState::Running);
  CHECK(activity(f, false) == Activity::Disassembling);
  CHECK(activity(f, true) == Activity::Counting);

  f.action = solveThread_c::ACT_REDUCE;
  CHECK(runState(f) == RunState::Preparing);
  CHECK(activity(f, false) == Activity::Optimising);

  f.action = solveThread_c::ACT_WAIT_TO_STOP;
  CHECK(activity(f, false) == Activity::Stopping);

  // the thread has stopped and not yet been retired
  f.action = solveThread_c::ACT_PAUSING;
  CHECK(runState(f) == RunState::Paused);
  CHECK(activity(f, false) == Activity::Paused);
  f.prepared = true;
  CHECK(runState(f) == RunState::Ready);
  CHECK(activity(f, false) == Activity::Ready);
  f.prepared = false;

  f.action = solveThread_c::ACT_FINISHED;
  CHECK(runState(f) == RunState::Finished);
  CHECK(activity(f, false) == Activity::Finished);

  // an error or an internal error ends the run
  for (int a : { int(solveThread_c::ACT_ERROR), int(solveThread_c::ACT_ASSERT) }) {
    f.action = a;
    CHECK(runState(f) == RunState::Idle);
    CHECK(activity(f, false) == Activity::Error);
  }

  // without a thread, a changed problem reads as partial; no problem as nothing
  f.threadHere = false;
  f.action = -1;
  f.solveState = SS_UNKNOWN;
  CHECK(runState(f) == RunState::Idle);
  CHECK(activity(f, false) == Activity::Partial);
  f.problem = false;
  CHECK(activity(f, false) == Activity::None);
  CHECK(runButtons(f) == RunButtons{});
}

TEST_CASE("PageUp goes back a page of solutions in every context", "[ui][solver]") {
  for (KeyContext c : { KeyContext::List, KeyContext::Slider, KeyContext::Seekbar })
    CHECK(solverKey(c, SolverKey::PageUp, false) == SolverAction::PageBack);
  CHECK(rowAfter(SolverAction::PageBack, 20, 30) == 12);
}

TEST_CASE("Keep every and Limit are held to the solver's bounds", "[ui][solver]") {
  SolverOptions o;
  o.keepEvery = 0;
  o.limit = maxSolverCount + 5;
  const SolverOptions n = normalised(o);
  CHECK(n.keepEvery == 1);
  CHECK(n.limit == maxSolverCount);
  // a time in each unit legacy uses
  using U = TimeText::Unit;
  CHECK(timeText(40 * 86400.0).unit == U::Months);
  CHECK(timeText(3 * 365.2422 * 86400).unit == U::Years);
  CHECK(timeText(5000 * 365.2422 * 86400).unit == U::Millennia);
  CHECK_FALSE(timeLeft(-1, 0.5).has_value());
  CHECK_FALSE(timeLeft(10, 1.5).has_value());
}

TEST_CASE("a heavier shape stays put while the lighter pieces move (AC-C21-06, T-C21-4)", "[ui][solver]") {
  /* The player shows the stored disassembly as it is: the disassembler
   * keeps the heavier side of each separation still (movementAnalysator_c).
   * A piece that moves with all weights equal stays where it is once its
   * shape weighs more than the others, at every step the player shows. */
  auto p = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(p);
  problem_c * pr = p->getProblem(0);
  REQUIRE(pr->getNumberOfSavedSolutions() > 0);
  const assembly_c * assembled = pr->getSavedSolution(0)->getAssembly();
  REQUIRE(assembled);

  // pieces are numbered part by part, each part's copies together
  auto shapeOf = [&](unsigned piece) {
    for (unsigned part = 0; part < pr->getNumberOfParts(); part++) {
      if (piece < pr->getPartMaximum(part))
        return pr->getShapeIdOfPart(part);
      piece -= pr->getPartMaximum(part);
    }
    return 0u;
  };
  // whether the piece leaves its assembled place at some step of the disassembly
  auto moves = [&](unsigned piece) {
    disassembler_0_c dis(*pr);
    std::unique_ptr<separation_c> tree = dis.disassemble(assembled);
    REQUIRE(tree);
    disasmToMoves_c player(tree.get(), 20, pr->getNumberOfPieces());
    player.setStep(0, false);
    const float x = player.getX(piece), y = player.getY(piece), z = player.getZ(piece);
    for (unsigned step = 1; step <= tree->sumMoves(); step++) {
      player.setStep(float(step), false);
      if (player.getX(piece) != x || player.getY(piece) != y || player.getZ(piece) != z)
        return true;
    }
    return false;
  };

  unsigned mover = pr->getNumberOfPieces();
  for (unsigned i = 0; i < pr->getNumberOfPieces() && mover == pr->getNumberOfPieces(); i++)
    if (moves(i))
      mover = i;
  REQUIRE(mover < pr->getNumberOfPieces());
  for (unsigned s = 0; s < p->getNumberOfShapes(); s++)
    p->getShape(s)->setWeight(1);
  p->getShape(shapeOf(mover))->setWeight(3);
  CHECK_FALSE(moves(mover));
}
