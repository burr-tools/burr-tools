/* Tests for btui's solver rules: what the Solver workspace's cards ask of the
 * solver and show of its results (C19, C20).
 */
#include <catch2/catch_test_macros.hpp>

#include "../src/uicore/solver.h"

#include "../src/lib/assembly.h"
#include "../src/lib/problem.h"
#include "../src/lib/puzzle.h"
#include "../src/lib/solution.h"
#include "../src/lib/solvethread.h"

#include <algorithm>
#include <chrono>
#include <memory>
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
