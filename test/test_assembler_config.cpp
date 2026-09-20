#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "lib/assembler.h"
#include "lib/assembler_0.h"
#include "lib/assembler_1.h"
#include "lib/assembly.h"
#include "lib/gridtype.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/voxel.h"

#include <memory>
#include <string>
#include <vector>

using namespace bttest;

/* The assembler's configuration and rejection surface.
 *
 * assembler_0.cpp (395 uncovered lines, 48%) and assembler_1.cpp (403,
 * 57%) are the heart of the application, and the next-phase plan puts them
 * last because covering them needs actual solves and therefore runtime.
 *
 * Most of this file needs none. createMatrix does its validation before any
 * search begins -- it counts cells, checks every piece has somewhere to go,
 * and refuses puzzles the assembler cannot express -- and every one of
 * those paths is reachable from a puzzle small enough to build inline and
 * fast enough for the everyday suite.
 *
 * The cases that do solve are tagged [stress] and land in the slow suite,
 * per the plan's convention: `just test` stays the fast loop, `just
 * test-all` is what CI runs.
 */

namespace {

struct Fixture {
  std::unique_ptr<puzzle_c> puzzle;
  problem_c * problem;
};

/**
 * A problem on the cube grid whose result is a 1 x `resultCells` bar and
 * whose pieces are bars of the given lengths, one of each.
 *
 * Deliberately one-dimensional: the arithmetic createMatrix does -- total
 * piece cells against result cells -- is then obvious by inspection, and
 * so is whether a piece can be placed at all.
 */
Fixture bars(unsigned int resultCells, std::initializer_list<unsigned int> pieceLengths) {
  Fixture f;
  f.puzzle = std::make_unique<puzzle_c>(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
  const gridType_c & gt = *f.puzzle->getGridType();

  std::vector<unsigned int> pieceShapes;
  for (unsigned int len : pieceLengths) {
    std::unique_ptr<voxel_c> v = makeVoxel(gt, len, 1, 1);
    for (unsigned int x = 0; x < len; x++)
      v->setState(x, 0, 0, voxel_c::VX_FILLED);
    pieceShapes.push_back(f.puzzle->addShape(v.release()));
  }

  std::unique_ptr<voxel_c> res = makeVoxel(gt, resultCells, 1, 1);
  for (unsigned int x = 0; x < resultCells; x++)
    res->setState(x, 0, 0, voxel_c::VX_FILLED);
  unsigned int resultShape = f.puzzle->addShape(res.release());

  f.problem = f.puzzle->getProblem(f.puzzle->addProblem());
  f.problem->setResultId(resultShape);

  /* Both bounds, deliberately. setShapeMaximum alone creates the part with
     a MINIMUM of 0 (problem.cpp: part_c(shape, 0, count, 0)), which makes
     it the range [0..1] rather than "exactly one" -- and assembler_0_c
     requires max == min == 1, so every case below would have come back
     ERR_PUZZLE_UNHANDABLE instead of the error it was probing for.
     setShapeMinimum creates the part with min == max == count. */
  for (unsigned int s : pieceShapes) {
    f.problem->setShapeMinimum(s, 1);
    f.problem->setShapeMaximum(s, 1);
  }

  return f;
}

/**
 * Which implementation an assembler_c actually is.
 *
 * The obvious-looking alternative -- calling the static
 * assembler_0_c::canHandle() next to a findAssembler() call -- asks about
 * the PROBLEM, not about the object that came back, so it holds whatever
 * the factory did with the answer. A dynamic_cast is the only thing here
 * that inspects the returned object, and neither assembler exposes a name
 * or version to identify itself by.
 */
const char * assemblerKind(const assembler_c * a) {
  if (dynamic_cast<const assembler_0_c *>(a)) return "assembler_0";
  if (dynamic_cast<const assembler_1_c *>(a)) return "assembler_1";
  return "neither";
}

/** counts what an assemble() run produces, and can stop early */
class CountingCallback : public assembler_cb {
public:
  int assemblies = 0;
  int stopAfter = -1;         //< -1 means never stop early

  bool assembly(std::unique_ptr<assembly_c>) override {
    assemblies++;
    if (stopAfter >= 0 && assemblies >= stopAfter) return false;
    return true;
  }
};

} // namespace

/* ------------------------------------------------------------------ */
/* the cell-count checks createMatrix makes before searching           */
/* ------------------------------------------------------------------ */

TEST_CASE("assembler: pieces holding more cells than the result is rejected as too many units",
          "[assembler][config]") {
  /* a four-cell result and pieces totalling five */
  Fixture f = bars(4, {3, 2});

  assembler_0_c assm(*f.problem);

  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_TOO_MANY_UNITS);

  /* getErrorsParam reports the size of the mismatch, which is what the GUI
     shows the user. Asserting the value, not merely that it is non-zero:
     the surplus is exactly one cell. */
  REQUIRE(assm.getErrorsParam() == 1);
}

TEST_CASE("assembler: pieces holding fewer cells than the result is rejected as too few units",
          "[assembler][config]") {
  /* a six-cell result and pieces totalling four */
  Fixture f = bars(6, {3, 1});

  assembler_0_c assm(*f.problem);

  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_TOO_FEW_UNITS);
  REQUIRE(assm.getErrorsParam() == 2);
}

TEST_CASE("assembler: variable cells in the result absorb a shortfall rather than being rejected",
          "[assembler][config]") {
  /* Variable cells may or may not be filled, so they are the slack the
     too-few check measures against -- `if (h > res_vari)`. A result with
     two variable cells accepts pieces two cells short, where the previous
     case rejected the same shortfall in a fully-filled result.

     Pairing the two is the point: without the rejection case above this
     would be consistent with a check that never fires at all. */
  Fixture f = bars(6, {3, 1});

  voxel_c * res = f.puzzle->getShape(f.problem->getResultId());
  res->setState(4, 0, 0, voxel_c::VX_VARIABLE);
  res->setState(5, 0, 0, voxel_c::VX_VARIABLE);

  assembler_0_c assm(*f.problem);

  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
}

TEST_CASE("assembler: a piece that fits nowhere in the result is rejected before any search",
          "[assembler][config]") {
  /* The cell counts balance exactly -- 2 + 2 = 4 -- so this gets past both
     checks above and fails on placement instead. The result is an L: four
     cells, but no straight run of two in the second row.

     Built here rather than through bars(), which only makes straight
     lines. */
  auto puzzle = std::make_unique<puzzle_c>(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
  const gridType_c & gt = *puzzle->getGridType();

  /* An unused shape first, so that the bar's shape id (1) and its
     problem-local part index (0) differ. With the bar added first both
     are zero and the assertion below cannot tell them apart -- which is
     what it was previously getting wrong. */
  puzzle->addShape(fromLayers(gt, {{"#"}}));

  /* a 1x3 bar and a single cell: four cells in total */
  unsigned int longBar = puzzle->addShape(fromLayers(gt, {{"###"}}));
  unsigned int single = puzzle->addShape(fromLayers(gt, {{"#"}}));

  /* a result of four cells in which no straight run of three exists */
  unsigned int result = puzzle->addShape(fromLayers(gt, {{"##",
                                                          "##"}}));

  problem_c * problem = puzzle->getProblem(puzzle->addProblem());
  problem->setResultId(result);
  problem->setShapeMinimum(longBar, 1);
  problem->setShapeMaximum(longBar, 1);
  problem->setShapeMinimum(single, 1);
  problem->setShapeMaximum(single, 1);

  assembler_0_c assm(*problem);

  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_CAN_NOT_PLACE);

  /* The parameter names WHICH piece could not be placed, so the user is
     told what to change -- and it names it by SHAPE id, not by the
     problem-local part index. prepare() returns -getShapeIdOfPart(pc)
     (assembler_0.cpp:686) and createMatrix negates it back
     (assembler_0.cpp:748); mainwindow.cpp:3141 then feeds it straight to
     selectShape. Asserting against getPartIdForShape here would be
     asserting the wrong identifier, and the fixture above is arranged so
     that the difference is visible. */
  REQUIRE(assm.getErrorsParam() == (int)longBar);
  REQUIRE((int)longBar != (int)problem->getPartIdForShape(longBar));
}

TEST_CASE("assembler: a well-formed problem passes createMatrix", "[assembler][config]") {
  /* The control for every rejection above: the same machinery, on a
     problem that balances and whose pieces fit. Without it the three
     rejections would be consistent with a createMatrix that refused
     everything. */
  Fixture f = bars(5, {3, 2});

  assembler_0_c assm(*f.problem);

  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
}

/* ------------------------------------------------------------------ */
/* which assembler handles which problem                               */
/* ------------------------------------------------------------------ */

TEST_CASE("assembler: assembler_0 refuses piece ranges and assembler_1 accepts them",
          "[assembler][config]") {
  Fixture f = bars(5, {2, 3});

  /* exactly one of each piece: assembler_0's own precondition */
  REQUIRE(assembler_0_c::canHandle(*f.problem));
  REQUIRE(assembler_1_c::canHandle(*f.problem));

  /* now allow between one and two of the first part -- a range */
  f.problem->setShapeMinimum(0, 1);
  f.problem->setShapeMaximum(0, 2);

  REQUIRE_FALSE(assembler_0_c::canHandle(*f.problem));
  REQUIRE(assembler_1_c::canHandle(*f.problem));

  /* a fixed count above one is equally out of assembler_0's reach: its
     rule is max == min == 1, so both halves of the condition need a case */
  Fixture g = bars(5, {2, 3});
  g.problem->setShapeMinimum(0, 2);
  g.problem->setShapeMaximum(0, 2);

  REQUIRE_FALSE(assembler_0_c::canHandle(*g.problem));
  REQUIRE(assembler_1_c::canHandle(*g.problem));
}

TEST_CASE("assembler: findAssembler picks assembler_0 when it can and assembler_1 otherwise",
          "[assembler][config]") {
  Fixture f = bars(5, {2, 3});
  const gridType_c & gt = *f.puzzle->getGridType();

  /* Both assemblers accept a fixed-count problem, so "assembler_0 could
     handle this" says nothing about which one the factory chose. The
     assertions below name the object that came back; asserting
     eligibility instead would pass even against a findAssembler that had
     lost its assembler_0 branch altogether. */
  {
    std::unique_ptr<assembler_c> a = gt.findAssembler(*f.problem);
    REQUIRE(a != nullptr);

    REQUIRE(assembler_0_c::canHandle(*f.problem));   // both are eligible...
    REQUIRE(assembler_1_c::canHandle(*f.problem));
    REQUIRE(std::string(assemblerKind(a.get())) == "assembler_0");   // ...and 0 is preferred

    REQUIRE(a->createMatrix(false, false, false) == assembler_c::ERR_NONE);
  }

  f.problem->setShapeMinimum(0, 1);
  f.problem->setShapeMaximum(0, 2);

  {
    std::unique_ptr<assembler_c> a = gt.findAssembler(*f.problem);
    REQUIRE(a != nullptr);
    REQUIRE_FALSE(assembler_0_c::canHandle(*f.problem));

    REQUIRE(std::string(assemblerKind(a.get())) == "assembler_1");

    /* assembler_1 must accept the very problem assembler_0 refused --
       otherwise findAssembler's fallback is decorative */
    REQUIRE(a->createMatrix(false, false, false) == assembler_c::ERR_NONE);
  }
}

TEST_CASE("assembler: createMatrix on a range problem is rejected by assembler_0 as unhandable",
          "[assembler][config]") {
  Fixture f = bars(5, {2, 3});
  f.problem->setShapeMinimum(0, 1);
  f.problem->setShapeMaximum(0, 2);

  assembler_0_c assm(*f.problem);

  /* canHandle is consulted inside createMatrix too, so constructing
     assembler_0 directly on an unsuitable problem fails cleanly rather
     than misbehaving later */
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_PUZZLE_UNHANDABLE);
}

TEST_CASE("assembler: every error state has a message and they are all distinct",
          "[assembler][config]") {
  const assembler_c::errState states[] = {
    assembler_c::ERR_NONE,
    assembler_c::ERR_TOO_MANY_UNITS,
    assembler_c::ERR_TOO_FEW_UNITS,
    assembler_c::ERR_CAN_NOT_PLACE,
    assembler_c::ERR_CAN_NOT_RESTORE_VERSION,
    assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX,
    assembler_c::ERR_PUZZLE_UNHANDABLE,
  };

  std::vector<std::string> seen;

  for (assembler_c::errState s : states) {
    const char * msg = assembler_c::getErrorMessage(s);

    INFO("error state " << (int)s);
    REQUIRE(msg != nullptr);
    REQUIRE(std::string(msg).size() > 0);

    /* Distinct messages, so a user shown one can tell which failure it
       was. A switch with a fallen-through case would show up here and
       nowhere else. */
    for (const std::string & prev : seen)
      REQUIRE(prev != std::string(msg));

    seen.push_back(msg);
  }

  REQUIRE(seen.size() == 7);
}

/* ------------------------------------------------------------------ */
/* solving                                                             */
/* ------------------------------------------------------------------ */

TEST_CASE("assembler: a one-dimensional puzzle yields the assemblies its geometry allows",
          "[assembler][config]") {
  /* A six-cell bar filled by a 2-bar and a 4-bar. Geometrically there are
     two arrangements -- short piece first or long piece first -- but the
     assembler reports ONE, and that is correct rather than a shortfall.

     The result is a straight bar, so it is symmetric under the half-turn
     that maps x to 5-x. That transformation carries one arrangement onto
     the other, so they are the same assembly seen from two sides, and the
     rotation reduction discards the duplicate. It is exactly the machinery
     assembly_c::smallerRotationExists implements, covered directly in
     test_puzzle_model.cpp.

     Worked out from the geometry, not recorded from a run -- the first
     draft of this case asserted two and was wrong about why. */
  Fixture f = bars(6, {2, 4});

  assembler_0_c assm(*f.problem);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  CountingCallback cb;
  assm.assemble(&cb);

  REQUIRE(cb.assemblies == 1);

  /* the search really ran rather than short-circuiting */
  REQUIRE(assm.getIterations() > 0);
}

TEST_CASE("assembler: returning false from the callback stops the search", "[assembler][config]") {
  /* Three distinct bar lengths, so the rotation reduction cannot collapse
     the arrangements to one and there is genuinely more than one assembly
     to find. Established by the unrestricted run first -- without that,
     "stopped after one" would be indistinguishable from "there was only
     one". */
  Fixture f = bars(6, {1, 2, 3});

  int total = 0;
  {
    assembler_0_c assm(*f.problem);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

    CountingCallback cb;
    assm.assemble(&cb);
    total = cb.assemblies;
  }

  REQUIRE(total > 1);

  assembler_0_c assm(*f.problem);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  CountingCallback cb;
  cb.stopAfter = 1;
  assm.assemble(&cb);

  REQUIRE(cb.assemblies == 1);
}

TEST_CASE("assembler: stopped() reports idleness -- and stop() before a run is cleared by it",
          "[assembler][config]") {
  Fixture f = bars(6, {1, 2, 3});

  assembler_0_c assm(*f.problem);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  /* Two things here read the wrong way round from their names, and both
     are pinned rather than assumed.

     stopped() is `return !running;` -- it means "not currently running",
     not "was asked to stop". So it is true before a search has begun, and
     true again once one has finished. It is an idleness flag. */
  REQUIRE(assm.stopped());

  /* And stop() before assemble() does nothing: assemble() stores false
     into the abort flag on entry (assembler_0.cpp), clearing any request
     made beforehand. The search below runs to completion despite the
     stop().

     This matters for a caller that sets up an assembler, requests a stop
     on some condition, and then starts it -- the request is silently
     dropped. The supported way to halt a search is returning false from
     the callback, which the previous case covers. */
  assm.stop();

  CountingCallback cb;
  assm.assemble(&cb);

  REQUIRE(cb.assemblies > 1);
  REQUIRE(assm.stopped());
}

TEST_CASE("assembler: stop() called from inside the callback halts the search",
          "[assembler][config]") {
  Fixture f = bars(6, {1, 2, 3});

  int total = 0;
  {
    assembler_0_c assm(*f.problem);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    CountingCallback cb;
    assm.assemble(&cb);
    total = cb.assemblies;
  }
  REQUIRE(total > 1);

  /* stop() during a run is the case it is built for -- the GUI calls it
     from another thread while the solver works. Calling it from the
     callback exercises the same path deterministically, without a thread. */
  class StoppingCallback : public assembler_cb {
  public:
    assembler_c * assm = nullptr;
    int assemblies = 0;

    bool assembly(std::unique_ptr<assembly_c>) override {
      assemblies++;
      assm->stop();
      return true;      // not an early exit via the return value
    }
  };

  assembler_0_c assm(*f.problem);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  StoppingCallback cb;
  cb.assm = &assm;
  assm.assemble(&cb);

  /* Returning true means the callback did NOT ask to stop; only the
     stop() did. So a search that ignored stop() would have found `total`
     assemblies. */
  REQUIRE(cb.assemblies < total);
}

TEST_CASE("assembler: getFinished reports completion between zero and one",
          "[assembler][config]") {
  Fixture f = bars(6, {1, 2, 3});

  assembler_0_c assm(*f.problem);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  REQUIRE(assm.getFinished() >= 0.0f);
  REQUIRE(assm.getFinished() <= 1.0f);

  CountingCallback cb;
  assm.assemble(&cb);

  /* a completed search reports done */
  REQUIRE(assm.getFinished() == 1.0f);
}

TEST_CASE("assembler: a range lets assembler_1 use a different number of pieces",
          "[assembler][config]") {
  /* A six-cell bar and a single 2-bar allowed to appear between one and
     three times. Only three copies fill it, so exactly one count works --
     but the assembler has to consider the others to find out, which is the
     range machinery assembler_0 cannot express at all.

     The expected assembly count is 1: three identical 2-bars in a row, and
     identical pieces make the arrangements indistinguishable. */
  auto puzzle = std::make_unique<puzzle_c>(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
  const gridType_c & gt = *puzzle->getGridType();

  unsigned int piece = puzzle->addShape(fromLayers(gt, {{"##"}}));
  unsigned int result = puzzle->addShape(fromLayers(gt, {{"######"}}));

  problem_c * problem = puzzle->getProblem(puzzle->addProblem());
  problem->setResultId(result);
  problem->setShapeMinimum(piece, 1);
  problem->setShapeMaximum(piece, 3);

  REQUIRE_FALSE(assembler_0_c::canHandle(*problem));

  assembler_1_c assm(*problem);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  CountingCallback cb;
  assm.assemble(&cb);

  REQUIRE(cb.assemblies == 1);
}

TEST_CASE("assembler: assembler_0 ignores the hole limit entirely",
          "[assembler][config]") {
  /* Worth pinning as its own claim, because it is easy to read a passing
     "the limit changed nothing" case as evidence that the limit was
     honoured and happened to exclude nothing.

     It was not honoured. maxHoles appears nowhere in assembler_0.cpp: the
     setting is read only by assembler_1 (assembler_1.cpp:689-690), and
     only on the ranged branch at that. assembler_0 requires every part to
     have min == max == 1, so its result is always filled exactly and the
     question never arises.

     The case below is where the limit is actually exercised. */
  Fixture f = bars(6, {1, 2, 3});

  int withoutLimit = 0;
  {
    assembler_0_c assm(*f.problem);
    REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);
    CountingCallback cb;
    assm.assemble(&cb);
    withoutLimit = cb.assemblies;
  }
  REQUIRE(withoutLimit > 0);

  REQUIRE_FALSE(f.problem->maxHolesDefined());
  f.problem->setMaxHoles(0);
  REQUIRE(f.problem->getMaxHoles() == 0);

  assembler_0_c assm(*f.problem);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  CountingCallback cb;
  assm.assemble(&cb);

  REQUIRE(cb.assemblies == withoutLimit);
}

namespace {

/**
 * A problem whose configured hole limit assembler_1 will actually read.
 *
 * Two conditions have to hold together, and the setting is silently
 * ignored unless both do (assembler_1.cpp:687-692):
 *
 *   - the total minimum and maximum piece volumes must DIFFER, or the
 *     hole count is pinned to res_filled - min and the configured limit
 *     is never consulted; hence the piece range rather than a fixed count
 *   - the result must have variable cells, or there is nowhere for a hole
 *     to be
 *
 * A 1x1 piece with the range [3..5] against a five-cell result of three
 * filled and two variable cells gives four assemblies: three pieces
 * covering the filled cells (two holes), four pieces covering the filled
 * cells and either variable cell (one hole, two ways), and five pieces
 * covering everything (no holes).
 */
Fixture rangedWithVariableResult() {
  Fixture f;
  f.puzzle = std::make_unique<puzzle_c>(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
  const gridType_c & gt = *f.puzzle->getGridType();

  unsigned int single = f.puzzle->addShape(fromLayers(gt, {{"#"}}));
  unsigned int result = f.puzzle->addShape(fromLayers(gt, {{"###++"}}));

  f.problem = f.puzzle->getProblem(f.puzzle->addProblem());
  f.problem->setResultId(result);
  f.problem->setShapeMinimum(single, 3);
  f.problem->setShapeMaximum(single, 5);

  return f;
}

/** how many assemblies assembler_1 finds, optionally under a hole limit */
int assembliesUnderHoleLimit(int maxHoles) {
  Fixture f = rangedWithVariableResult();
  if (maxHoles >= 0) f.problem->setMaxHoles(maxHoles);

  assembler_1_c assm(*f.problem);
  REQUIRE(assm.createMatrix(false, false, false) == assembler_c::ERR_NONE);

  CountingCallback cb;
  assm.assemble(&cb);
  return cb.assemblies;
}

} // namespace

TEST_CASE("assembler: the configured hole limit excludes the assemblies that exceed it",
          "[assembler][config]") {
  /* The limit is a search constraint, so the thing to assert is that it
     removes assemblies -- and which ones. Asserting only that a limit
     satisfied by every assembly changes nothing would pass just as well
     against an assembler that never read the setting at all.

     The four assemblies have 2, 1, 1 and 0 holes respectively, so each
     limit below has a different, predictable effect. */
  const int unlimited = assembliesUnderHoleLimit(-1);
  REQUIRE(unlimited == 4);

  /* no holes tolerated: only the assembly that fills every cell */
  REQUIRE(assembliesUnderHoleLimit(0) == 1);

  /* one hole tolerated: loses the two-hole assembly, keeps the other three */
  REQUIRE(assembliesUnderHoleLimit(1) == 3);

  /* a limit no assembly exceeds excludes nothing -- the case the old test
     made, which only means something next to the three above */
  REQUIRE(assembliesUnderHoleLimit(2) == unlimited);
}

/* ------------------------------------------------------------------ */
/* the bundled puzzles as oracles                                      */
/* ------------------------------------------------------------------ */

namespace {

/* Re-solve a bundled problem and compare against the assembly count the
   file records.
 *
 * This is the design doc's recommendation for the assembler package:
 * several example puzzles ship fully solved, with getNumAssemblies()
 * baked into the XML, so the expected value is known-correct and does not
 * have to be invented or recorded from a run of the code under test. It is
 * repo data, so it cannot shift underneath the test either.
 *
 * Returns the pair (recorded, found) so a caller can assert on both. */
struct SolveCheck {
  unsigned int recorded;
  int found;
};

SolveCheck resolve(const char * path, unsigned int problemIdx, bool keepMirror) {
  std::unique_ptr<puzzle_c> p = puzzle_c::load(path);
  REQUIRE(p != nullptr);
  REQUIRE(problemIdx < p->getNumberOfProblems());

  problem_c * problem = p->getProblem(problemIdx);
  REQUIRE(problem != nullptr);

  /* the recorded figure is only readable once the file says it was solved;
     getNumAssemblies() asserts on that */
  REQUIRE(problem->numAssembliesKnown());
  const unsigned int recorded = problem->getNumAssemblies();

  const gridType_c * gt = problem->getPuzzle().getGridType();
  REQUIRE(gt != nullptr);

  std::unique_ptr<assembler_c> assm = gt->findAssembler(*problem);
  REQUIRE(assm != nullptr);
  REQUIRE(assm->createMatrix(keepMirror, false, false) == assembler_c::ERR_NONE);

  CountingCallback cb;
  assm->assemble(&cb);

  return { recorded, cb.assemblies };
}

} // namespace

TEST_CASE("assembler: re-solving the bundled puzzles reproduces the assembly counts their files "
          "record", "[assembler][stress]") {
  /* Twelve problems across nine puzzles, spanning both assemblers and
     three grids, with recorded counts from 1 to 1063. Each is a fresh
     solve compared against a figure that came from the repository rather
     than from this code -- the design doc's recommendation for this
     package, and the reason the expected values here did not have to be
     invented or recorded from a run.

     The keepMirror column is not decoration. A problem's recorded count
     depends on which reductions were enabled when whoever saved the file
     ran the solver, and they did not all use the same setting. Solid Six
     Piece Burrs records 1063, which is what the assembler produces with
     mirror solutions KEPT; with mirror reduction on it finds 588, and
     both are correct answers to different questions. The next case pins
     that relationship rather than leaving this column looking arbitrary.

     Tagged [stress] so it lands in the slow suite: `just test` stays the
     fast loop and `just test-all` -- what CI runs -- includes this. */
  struct Case { const char * path; unsigned int problem; bool keepMirror; const char * name; };

  const Case cases[] = {
    { "examples/12PieceSeparation.xmpuzzle",         0, false, "12 Piece Separation" },
    { "examples/AlPackino.xmpuzzle",                 0, false, "Al Packino" },
    { "examples/BrokenSticks.xmpuzzle",              0, false, "Broken Sticks" },
    { "examples/DiagonalCube.xmpuzzle",              0, false, "Diagonal Cube" },
    { "examples/PermutatedThirdStellation.xmpuzzle", 0, false, "Permutated Third Stellation" },
    { "examples/FourPieceTetrahedron.xmpuzzle",      0, false, "Four Piece Tetrahedron" },
    { "examples/BallRoom.xmpuzzle",                  0, false, "Ball Room problem 0" },
    { "examples/BallRoom.xmpuzzle",                  1, false, "Ball Room problem 1" },
    { "examples/BallRoom.xmpuzzle",                  2, false, "Ball Room problem 2" },
    { "examples/HexSticks.xmpuzzle",                 0, false, "Hex Sticks" },
    { "examples/PiecesOfEight.xmpuzzle",             2, false, "Pieces of Eight problem 2" },
    { "examples/SolidSixPieceBurrs.xmpuzzle",        0, true,  "Solid Six Piece Burrs" },
  };

  for (const Case & c : cases) {
    INFO(c.name << (c.keepMirror ? " (mirror solutions kept)" : ""));

    SolveCheck r = resolve(c.path, c.problem, c.keepMirror);

    /* the premise: the file records a real search, not an empty one. A
       problem recording zero assemblies would make the comparison below
       hold for any assembler that found nothing. */
    REQUIRE(r.recorded > 0);

    REQUIRE((unsigned int)r.found == r.recorded);
  }
}

TEST_CASE("assembler: relaxing a reduction reports at least as many assemblies",
          "[assembler][stress]") {
  /* Each reduction can only ever merge assemblies, never invent one, so
     turning one off must report at least as many. That ordering is the
     property, and it holds whatever the absolute figures are.

     It takes two puzzles, because no single cheap one exercises both
     flags. Solid Six Piece Burrs is where mirror reduction bites -- 588
     with it on, 1063 with mirror solutions kept -- and that second figure
     is what its file records, which is what makes the keepMirror column in
     the case above a derived value rather than a magic constant. Hex
     Sticks is where rotation reduction bites: 33 against 744.

     This is also the only place createMatrix's keepMirror and
     keepRotations arguments are exercised at all; every other case in the
     suite passes false for both.

     Solid Six Piece Burrs with keepRotations was deliberately dropped from
     this case. It finds 12,597 assemblies, and under gcov instrumentation
     the dancing-links inner loop in assembler_1.cpp then accumulates over
     four billion hits -- past the threshold at which gcovr rejects the
     counter as a parse error (gcc bug 68080) and `just coverage` fails
     outright. Hex Sticks demonstrates the same property for a thousandth
     of the work. */
  auto count = [](const char * path, bool keepMirror, bool keepRotations) {
    std::unique_ptr<puzzle_c> p = puzzle_c::load(path);
    REQUIRE(p != nullptr);

    problem_c * problem = p->getProblem(0);
    REQUIRE(problem != nullptr);

    std::unique_ptr<assembler_c> assm = p->getGridType()->findAssembler(*problem);
    REQUIRE(assm != nullptr);
    REQUIRE(assm->createMatrix(keepMirror, keepRotations, false) == assembler_c::ERR_NONE);

    CountingCallback cb;
    assm->assemble(&cb);
    return cb.assemblies;
  };

  const char * burrs = "examples/SolidSixPieceBurrs.xmpuzzle";
  const char * sticks = "examples/HexSticks.xmpuzzle";

  const int burrsReduced = count(burrs, false, false);
  const int burrsMirrorKept = count(burrs, true, false);

  REQUIRE(burrsReduced > 0);

  /* relaxing cannot lose an assembly... */
  REQUIRE(burrsMirrorKept >= burrsReduced);
  /* ...and here it genuinely gains some, so the inequality is not holding
     by equality -- which it would against an assembler ignoring the flag */
  REQUIRE(burrsMirrorKept > burrsReduced);

  /* the figure the file records is the mirror-kept one */
  std::unique_ptr<puzzle_c> p = puzzle_c::load(burrs);
  REQUIRE(p != nullptr);
  REQUIRE(p->getProblem(0)->getNumAssemblies() == (unsigned int)burrsMirrorKept);

  const int sticksReduced = count(sticks, false, false);
  const int sticksRotationsKept = count(sticks, false, true);

  REQUIRE(sticksReduced > 0);
  REQUIRE(sticksRotationsKept >= sticksReduced);
  REQUIRE(sticksRotationsKept > sticksReduced);
}

TEST_CASE("assembler: the bundled counts span both assemblers and more than one grid",
          "[assembler][stress]") {
  /* The case above would be much weaker if every puzzle in it went through
     the same assembler on the same grid. This asserts the coverage the
     list was chosen for, so a future edit that quietly narrows it fails
     here rather than silently reducing what the suite exercises. */
  struct Case { const char * path; unsigned int problem; };

  const Case cases[] = {
    { "examples/12PieceSeparation.xmpuzzle",      0 },
    { "examples/AlPackino.xmpuzzle",              0 },
    { "examples/BrokenSticks.xmpuzzle",           0 },
    { "examples/DiagonalCube.xmpuzzle",           0 },
    { "examples/PermutatedThirdStellation.xmpuzzle", 0 },
    { "examples/FourPieceTetrahedron.xmpuzzle",   0 },
    { "examples/BallRoom.xmpuzzle",               0 },
    { "examples/HexSticks.xmpuzzle",              0 },
    { "examples/PiecesOfEight.xmpuzzle",          2 },
    { "examples/SolidSixPieceBurrs.xmpuzzle",     0 },
  };

  bool sawAssembler0 = false, sawAssembler1 = false;
  std::vector<gridType_c::gridType> grids;

  for (const Case & c : cases) {
    std::unique_ptr<puzzle_c> p = puzzle_c::load(c.path);
    REQUIRE(p != nullptr);

    problem_c * problem = p->getProblem(c.problem);
    REQUIRE(problem != nullptr);

    /* Ask the factory and inspect what it returned. Reading eligibility
       off canHandle() instead would be inferring which implementation ran
       from which one was allowed to, and the case would keep claiming
       both were exercised however findAssembler actually chose. */
    std::unique_ptr<assembler_c> a = p->getGridType()->findAssembler(*problem);
    REQUIRE(a != nullptr);

    const std::string kind = assemblerKind(a.get());
    if (kind == "assembler_0")      sawAssembler0 = true;
    else if (kind == "assembler_1") sawAssembler1 = true;
    else                            FAIL("unrecognised assembler for " << c.path);

    gridType_c::gridType g = p->getGridType()->getType();
    bool seen = false;
    for (gridType_c::gridType h : grids) if (h == g) seen = true;
    if (!seen) grids.push_back(g);
  }

  REQUIRE(sawAssembler0);
  REQUIRE(sawAssembler1);
  REQUIRE(grids.size() >= 2);
}


