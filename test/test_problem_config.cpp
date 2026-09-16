#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "lib/assembler_0.h"
#include "lib/assembly.h"
#include "lib/gridtype.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/solution.h"
#include "lib/voxel.h"

#include <memory>
#include <vector>

using namespace bttest;

/* problem_c's configuration surface: the colour-placement constraints, the
   part-group table, the piece-count ranges and the hole limit. #63 covered
   the parts/result/solution-list side of problem_c; this covers the
   settings an assembler reads before it runs, which #63 deliberately left
   alone.

   All of it is plain bookkeeping -- no solver runs anywhere in this file. */

namespace {

std::unique_ptr<puzzle_c> makePuzzle(gridType_c::gridType t = gridType_c::GT_BRICKS) {
  return std::make_unique<puzzle_c>(std::make_unique<gridType_c>(t));
}

/** a puzzle with `numShapes` cube shapes sized 1x1x1, 2x2x2, ... and one
    empty problem. Sizes differ so a shape can be identified by getX(). */
struct Fixture {
  std::unique_ptr<puzzle_c> puzzle;
  problem_c * problem;
};

Fixture makeFixture(unsigned int numShapes) {
  Fixture f;
  f.puzzle = makePuzzle();
  for (unsigned int i = 1; i <= numShapes; i++)
    f.puzzle->addShape(i, i, i);
  f.problem = f.puzzle->getProblem(f.puzzle->addProblem());
  return f;
}

} // namespace

/* ------------------------------------------------------------------ */
/* colour placement constraints                                        */
/* ------------------------------------------------------------------ */

TEST_CASE("problem: with no colours defined every placement is allowed", "[problem][colour]") {
  Fixture f = makeFixture(1);

  REQUIRE(f.puzzle->colorNumber() == 0);

  /* placementAllowed returns early on colorNumber() == 0 (problem.cpp) --
     a puzzle that does not use colours must not have its assembler
     constrained by an empty constraint set. Probe a pair that would be
     DISALLOWED under the allow-list rule the next case establishes, so this
     is distinguishable from that behaviour rather than compatible with it. */
  REQUIRE(f.problem->placementAllowed(0, 0));
}

TEST_CASE("problem: once colours exist the constraint set is an allow-list -- a pair is refused "
          "until it is allowed", "[problem][colour]") {
  Fixture f = makeFixture(1);

  f.puzzle->addColor(255, 0, 0);   // colour id 1
  f.puzzle->addColor(0, 255, 0);   // colour id 2

  /* nothing has been allowed yet, so a piece of colour 1 may not go into a
     result cell of colour 2. This is the direction that matters: the set
     starts empty and membership GRANTS permission, so a reader who assumes
     it is a deny-list has it exactly backwards. */
  REQUIRE_FALSE(f.problem->placementAllowed(1, 2));

  f.problem->allowPlacement(1, 2);
  REQUIRE(f.problem->placementAllowed(1, 2));

  f.problem->disallowPlacement(1, 2);
  REQUIRE_FALSE(f.problem->placementAllowed(1, 2));
}

TEST_CASE("problem: a colour constraint is directional -- allowing one way does not allow the other",
          "[problem][colour]") {
  Fixture f = makeFixture(1);

  f.puzzle->addColor(255, 0, 0);
  f.puzzle->addColor(0, 255, 0);

  f.problem->allowPlacement(1, 2);

  REQUIRE(f.problem->placementAllowed(1, 2));

  /* the key is (pc-1) << 16 | (res-1), so the pair is ordered. Were the two
     arguments ever swapped at a call site -- or the key made symmetric --
     this is what would catch it. */
  REQUIRE_FALSE(f.problem->placementAllowed(2, 1));
}

TEST_CASE("problem: colour 0 is always placeable and cannot be constrained", "[problem][colour]") {
  Fixture f = makeFixture(1);

  f.puzzle->addColor(255, 0, 0);

  /* colour 0 means "no colour". allowPlacement and disallowPlacement return
     early when either argument is 0, and placementAllowed short-circuits to
     true, so the neutral colour can never be locked out. */
  REQUIRE(f.problem->placementAllowed(0, 1));
  REQUIRE(f.problem->placementAllowed(1, 0));

  f.problem->disallowPlacement(0, 1);
  f.problem->disallowPlacement(1, 0);

  REQUIRE(f.problem->placementAllowed(0, 1));
  REQUIRE(f.problem->placementAllowed(1, 0));
}

TEST_CASE("problem: allowPlacement involving colour 0 grants no real colour pair a permission",
          "[problem][colour]") {
  Fixture f = makeFixture(1);

  f.puzzle->addColor(255, 0, 0);
  f.puzzle->addColor(0, 255, 0);

  /* allowPlacement returns early when either argument is 0, before the
     insert. What that must not do is leak a permission onto a real colour
     pair, so the assertions below name real pairs and require them still
     refused.

     Be precise about what this can and cannot catch, because the obvious
     reading is wrong. Deleting the early return entirely does NOT fail this
     case, and no test using the public API could. The key is
     (pc-1) << 16 | (res-1) on unsigned ints, so a fall-through with pc == 0
     computes (0u-1) << 16 == 0xFFFF0000 -- and reaching that key from a
     query needs pc == 65537, which placementAllowed's own
     bt_assert(pc <= colorNumber()) forbids long before any realistic puzzle.
     The stray entry is unreachable rather than harmful, and asserting
     otherwise would be asserting something untrue.

     What this DOES catch is colour 0 being folded into a real colour
     instead of skipped -- rewriting the guard as `if (pc == 0) pc = 1;`,
     the plausible way someone "fixes" colour-0 handling, lands a live
     permission on pair (1,1) and turns the first assertion red. Verified by
     making exactly that edit. */
  f.problem->allowPlacement(0, 1);
  f.problem->allowPlacement(1, 0);

  REQUIRE_FALSE(f.problem->placementAllowed(1, 1));
  REQUIRE_FALSE(f.problem->placementAllowed(1, 2));
  REQUIRE_FALSE(f.problem->placementAllowed(2, 1));
}

TEST_CASE("problem: disallowPlacement of a pair that was never allowed is a no-op", "[problem][colour]") {
  Fixture f = makeFixture(1);

  f.puzzle->addColor(255, 0, 0);
  f.puzzle->addColor(0, 255, 0);

  f.problem->allowPlacement(1, 2);

  /* erase-if-present: the find() miss branch. Removing an absent pair must
     leave the pair that IS present alone -- the assertion that makes this
     more than a crash test. */
  f.problem->disallowPlacement(2, 1);

  REQUIRE(f.problem->placementAllowed(1, 2));
}

/* ------------------------------------------------------------------ */
/* part groups                                                         */
/* ------------------------------------------------------------------ */

TEST_CASE("problem: setPartGroup records a group and its count against a part", "[problem][group]") {
  Fixture f = makeFixture(2);

  f.problem->setShapeMaximum(0, 1);
  REQUIRE(f.problem->getNumberOfParts() == 1);

  REQUIRE(f.problem->getNumberOfPartGroups(0) == 0);

  f.problem->setPartGroup(0, 3, 2);

  REQUIRE(f.problem->getNumberOfPartGroups(0) == 1);
  REQUIRE(f.problem->getPartGroupId(0, 0) == 3);
  REQUIRE(f.problem->getPartGroupCount(0, 0) == 2);
}

TEST_CASE("problem: setPartGroup on a group already present updates its count instead of adding "
          "a second entry", "[problem][group]") {
  Fixture f = makeFixture(2);

  f.problem->setShapeMaximum(0, 1);
  f.problem->setPartGroup(0, 3, 2);
  f.problem->setPartGroup(0, 3, 5);

  /* the count changed and the entry did NOT duplicate. Asserting both is
     the point: checking only the count would pass against an implementation
     that appended a second (3, 5) entry and happened to read the newer one
     first. */
  REQUIRE(f.problem->getNumberOfPartGroups(0) == 1);
  REQUIRE(f.problem->getPartGroupId(0, 0) == 3);
  REQUIRE(f.problem->getPartGroupCount(0, 0) == 5);
}

TEST_CASE("problem: setPartGroup with a count of zero removes the group", "[problem][group]") {
  Fixture f = makeFixture(2);

  f.problem->setShapeMaximum(0, 1);
  f.problem->setPartGroup(0, 3, 2);
  f.problem->setPartGroup(0, 7, 1);
  REQUIRE(f.problem->getNumberOfPartGroups(0) == 2);

  f.problem->setPartGroup(0, 3, 0);

  /* group 3 is gone and group 7 survived the erase -- which is what rules
     out an erase at the wrong index */
  REQUIRE(f.problem->getNumberOfPartGroups(0) == 1);
  REQUIRE(f.problem->getPartGroupId(0, 0) == 7);
  REQUIRE(f.problem->getPartGroupCount(0, 0) == 1);
}

TEST_CASE("problem: group id 0 and count 0 both register nothing", "[problem][group]") {
  Fixture f = makeFixture(2);

  f.problem->setShapeMaximum(0, 1);

  /* group 0 means "belongs to no group" and is not storable; a count of 0
     means no instances and is equally pointless. The add is guarded on
     `groupId && count`, so both are dropped. */
  f.problem->setPartGroup(0, 0, 5);
  f.problem->setPartGroup(0, 4, 0);

  REQUIRE(f.problem->getNumberOfPartGroups(0) == 0);
}

/* ------------------------------------------------------------------ */
/* piece count ranges                                                  */
/* ------------------------------------------------------------------ */

TEST_CASE("problem: getPartMinimum/getPartMaximum index by part -- not by shape", "[problem][parts]") {
  Fixture f = makeFixture(4);

  /* use shapes 2 and 3, leaving 0 and 1 unused, so a part index and a shape
     id can never be confused for one another by coincidence */
  f.problem->setShapeMinimum(2, 1);
  f.problem->setShapeMaximum(2, 3);
  f.problem->setShapeMinimum(3, 2);
  f.problem->setShapeMaximum(3, 2);

  REQUIRE(f.problem->getNumberOfParts() == 2);

  /* parts are in the order they were first mentioned, so part 0 is shape 2 */
  REQUIRE(f.problem->getShapeIdOfPart(0) == 2);
  REQUIRE(f.problem->getPartMinimum(0) == 1);
  REQUIRE(f.problem->getPartMaximum(0) == 3);

  REQUIRE(f.problem->getShapeIdOfPart(1) == 3);
  REQUIRE(f.problem->getPartMinimum(1) == 2);
  REQUIRE(f.problem->getPartMaximum(1) == 2);

  /* and the shape-indexed readers agree, addressed the other way round */
  REQUIRE(f.problem->getShapeMinimum(2) == 1);
  REQUIRE(f.problem->getShapeMaximum(2) == 3);

  /* an unused shape reports 0 rather than asserting */
  REQUIRE(f.problem->getShapeMinimum(0) == 0);
  REQUIRE(f.problem->getShapeMaximum(0) == 0);
}

TEST_CASE("problem: raising the minimum above the maximum drags the maximum up with it",
          "[problem][parts]") {
  Fixture f = makeFixture(2);

  f.problem->setShapeMaximum(0, 2);
  REQUIRE(f.problem->getPartMaximum(0) == 2);

  /* setShapeMinimum re-enters setShapeMaximum when min would exceed max, so
     the range stays well-formed rather than inverting */
  f.problem->setShapeMinimum(0, 5);

  REQUIRE(f.problem->getPartMinimum(0) == 5);
  REQUIRE(f.problem->getPartMaximum(0) == 5);
}

TEST_CASE("problem: setShapeMaximum of zero drops the part entirely", "[problem][parts]") {
  Fixture f = makeFixture(3);

  f.problem->setShapeMaximum(0, 1);
  f.problem->setShapeMaximum(1, 1);
  REQUIRE(f.problem->getNumberOfParts() == 2);

  f.problem->setShapeMaximum(0, 0);

  /* the part is gone and the OTHER part survived -- and still names its own
     shape, which is what catches an erase that shifted the wrong element */
  REQUIRE(f.problem->getNumberOfParts() == 1);
  REQUIRE(f.problem->getShapeIdOfPart(0) == 1);
  REQUIRE_FALSE(f.problem->usesShape(0));
  REQUIRE(f.problem->usesShape(1));
}

/* ------------------------------------------------------------------ */
/* hole limit                                                          */
/* ------------------------------------------------------------------ */

TEST_CASE("problem: the hole limit is undefined until set and can be invalidated again",
          "[problem][holes]") {
  Fixture f = makeFixture(1);

  REQUIRE_FALSE(f.problem->maxHolesDefined());

  f.problem->setMaxHoles(7);
  REQUIRE(f.problem->maxHolesDefined());
  REQUIRE(f.problem->getMaxHoles() == 7);

  /* zero is a meaningful limit -- "no holes at all" -- and must not be
     confused with "no limit". 0xFFFFFFFF is the sentinel, not 0. */
  f.problem->setMaxHoles(0);
  REQUIRE(f.problem->maxHolesDefined());
  REQUIRE(f.problem->getMaxHoles() == 0);

  f.problem->setMaxHolesInvalid();
  REQUIRE_FALSE(f.problem->maxHolesDefined());
}

#ifndef NDEBUG
TEST_CASE("problem: reading an undefined hole limit asserts rather than returning the sentinel",
          "[problem][holes]") {
  Fixture f = makeFixture(1);

  /* getMaxHoles() asserts maxHolesDefined(). Guarded for NDEBUG because
     bt_assert compiles to ((void)0) in a release build, where this would
     return 0xFFFFFFFF instead of throwing. */
  REQUIRE_THROWS_AS(f.problem->getMaxHoles(), assert_exception);
}
#endif

/* ------------------------------------------------------------------ */
/* the assembler slot                                                  */
/* ------------------------------------------------------------------ */

TEST_CASE("problem: a fresh problem holds no assembler", "[problem][assembler]") {
  Fixture f = makeFixture(1);

  REQUIRE(f.problem->getAssembler() == nullptr);

  /* and the const overload agrees -- it is a separate function body */
  const problem_c & cp = *f.problem;
  REQUIRE(cp.getAssembler() == nullptr);
}

/* ------------------------------------------------------------------ */
/* parts and saved solutions move together                             */
/* ------------------------------------------------------------------ */

namespace {

/* A problem carrying real solutions, without running a solver.

   addSolution asserts solveState == SS_SOLVING, and the only way into that
   state is setAssembler() on a problem with no saved assembler blob
   (problem.cpp: the else branch). So the fixture attaches a genuine
   assembler_0_c -- it is never asked to assemble anything -- and then hands
   the problem assemblies built by hand.

   The result is a 1x3 bar and the pieces are unit cubes, so any placement
   triple is structurally plausible; nothing here depends on the assemblies
   being solutions the assembler would actually have found, only on
   problem_c storing and re-indexing them correctly. */
struct SolvingFixture {
  std::unique_ptr<puzzle_c> puzzle;
  problem_c * problem;
  unsigned int shapeA, shapeB, result;
};

SolvingFixture makeSolvingFixture(unsigned int maxA, unsigned int maxB) {
  SolvingFixture f;
  f.puzzle = makePuzzle();
  const gridType_c & gt = *f.puzzle->getGridType();

  f.shapeA = f.puzzle->addShape(fromLayers(gt, {{"#"}}));
  f.shapeB = f.puzzle->addShape(fromLayers(gt, {{"##"}}));
  f.result = f.puzzle->addShape(fromLayers(gt, {{"####"}}));

  f.problem = f.puzzle->getProblem(f.puzzle->addProblem());
  f.problem->setResultId(f.result);
  f.problem->setShapeMaximum(f.shapeA, maxA);
  f.problem->setShapeMaximum(f.shapeB, maxB);

  f.problem->setAssembler(std::make_unique<assembler_0_c>(*f.problem));

  return f;
}

/** an assembly whose piece i sits at x == xs[i]; a negative entry means the
    piece is not placed. Ownership passes to problem_c::addSolution. */
assembly_c * assemblyAtX(const gridType_c & gt, std::initializer_list<int> xs) {
  assembly_c * a = new assembly_c(&gt);
  for (int x : xs) {
    if (x < 0) a->addNonPlacement();
    else       a->addPlacement(0, x, 0, 0);
  }
  return a;
}

} // namespace

TEST_CASE("problem: exchangeParts swaps two adjacent parts and permutes every saved solution's "
          "placements to match", "[problem][parts][solutions]") {
  /* part 0 is one piece of shape A; part 1 is two pieces of shape B. Piece
     numbering is therefore 0 -> A, 1 and 2 -> B. */
  SolvingFixture f = makeSolvingFixture(1, 2);
  const gridType_c & gt = *f.puzzle->getGridType();

  REQUIRE(f.problem->getNumberOfParts() == 2);
  REQUIRE(f.problem->getNumberOfPieces() == 3);

  /* distinct x per piece, so the permutation is readable off the result
     rather than inferred */
  f.problem->addSolution(assemblyAtX(gt, {10, 11, 12}));

  f.problem->exchangeParts(0, 1);

  /* the parts swapped: part 0 is now shape B with maximum 2 */
  REQUIRE(f.problem->getShapeIdOfPart(0) == f.shapeB);
  REQUIRE(f.problem->getPartMaximum(0) == 2);
  REQUIRE(f.problem->getShapeIdOfPart(1) == f.shapeA);
  REQUIRE(f.problem->getPartMaximum(1) == 1);

  /* Piece numbering follows the parts list, so the pieces must be permuted
     to match or every saved solution silently starts describing different
     shapes. Piece slots are now 0 and 1 -> B, 2 -> A, so the placements
     that were at 1 and 2 must have moved to 0 and 1, and the one at 0 to 2.

     Asserting the exact permutation rather than "the multiset is
     unchanged": a no-op would preserve the multiset, and a no-op is
     precisely the bug worth catching here. */
  REQUIRE(f.problem->getNumberOfSavedSolutions() == 1);
  const assembly_c * a = f.problem->getSavedSolution(0)->getAssembly();

  REQUIRE(a->placementCount() == 3);
  REQUIRE(a->getX(0) == 11);
  REQUIRE(a->getX(1) == 12);
  REQUIRE(a->getX(2) == 10);
}

TEST_CASE("problem: exchangeParts with both indices the same changes nothing",
          "[problem][parts][solutions]") {
  SolvingFixture f = makeSolvingFixture(1, 2);
  const gridType_c & gt = *f.puzzle->getGridType();

  f.problem->addSolution(assemblyAtX(gt, {10, 11, 12}));

  f.problem->exchangeParts(1, 1);

  REQUIRE(f.problem->getShapeIdOfPart(0) == f.shapeA);
  REQUIRE(f.problem->getShapeIdOfPart(1) == f.shapeB);

  const assembly_c * a = f.problem->getSavedSolution(0)->getAssembly();
  REQUIRE(a->getX(0) == 10);
  REQUIRE(a->getX(1) == 11);
  REQUIRE(a->getX(2) == 12);
}

TEST_CASE("problem: raising a part's maximum inserts unplaced piece slots into saved solutions "
          "rather than discarding them", "[problem][parts][solutions]") {
  SolvingFixture f = makeSolvingFixture(1, 1);
  const gridType_c & gt = *f.puzzle->getGridType();

  REQUIRE(f.problem->getNumberOfPieces() == 2);
  f.problem->addSolution(assemblyAtX(gt, {10, 11}));

  /* part 0 (shape A) grows from one piece to three. The two new slots are
     inserted straight after the existing A piece, so the B piece that was
     at index 1 must move to index 3. */
  f.problem->setShapeMaximum(f.shapeA, 3);

  REQUIRE(f.problem->getNumberOfPieces() == 4);
  REQUIRE(f.problem->getNumberOfSavedSolutions() == 1);

  const assembly_c * a = f.problem->getSavedSolution(0)->getAssembly();
  REQUIRE(a->placementCount() == 4);

  REQUIRE(a->isPlaced(0));
  REQUIRE(a->getX(0) == 10);
  REQUIRE_FALSE(a->isPlaced(1));
  REQUIRE_FALSE(a->isPlaced(2));
  REQUIRE(a->isPlaced(3));
  REQUIRE(a->getX(3) == 11);
}

TEST_CASE("problem: lowering a part's maximum drops the solutions that used the pieces being "
          "removed and trims the ones that did not", "[problem][parts][solutions]") {
  SolvingFixture f = makeSolvingFixture(2, 1);
  const gridType_c & gt = *f.puzzle->getGridType();

  /* pieces 0 and 1 are shape A, piece 2 is shape B */
  REQUIRE(f.problem->getNumberOfPieces() == 3);

  /* a solution that uses BOTH A pieces -- it cannot survive A dropping to a
     maximum of one */
  f.problem->addSolution(assemblyAtX(gt, {10, 11, 12}));

  /* a solution that leaves the second A piece unplaced -- it can survive,
     with that slot trimmed away */
  f.problem->addSolution(assemblyAtX(gt, {20, -1, 22}));

  REQUIRE(f.problem->getNumberOfSavedSolutions() == 2);

  f.problem->setShapeMaximum(f.shapeA, 1);

  /* exactly one survivor, and it is the second one -- checking the count
     alone would pass against an implementation that deleted the wrong one */
  REQUIRE(f.problem->getNumberOfPieces() == 2);
  REQUIRE(f.problem->getNumberOfSavedSolutions() == 1);

  const assembly_c * a = f.problem->getSavedSolution(0)->getAssembly();
  REQUIRE(a->placementCount() == 2);
  REQUIRE(a->getX(0) == 20);
  REQUIRE(a->getX(1) == 22);
}

TEST_CASE("problem: raising a part's minimum drops the saved solutions that place too few of it",
          "[problem][parts][solutions]") {
  SolvingFixture f = makeSolvingFixture(2, 1);
  const gridType_c & gt = *f.puzzle->getGridType();

  /* places both A pieces: meets a minimum of 2 */
  f.problem->addSolution(assemblyAtX(gt, {10, 11, 12}));
  /* places only the first: does not */
  f.problem->addSolution(assemblyAtX(gt, {20, -1, 22}));

  f.problem->setShapeMinimum(f.shapeA, 2);

  REQUIRE(f.problem->getPartMinimum(0) == 2);
  REQUIRE(f.problem->getNumberOfSavedSolutions() == 1);

  /* and it is the one that placed both, identified by its own x values */
  const assembly_c * a = f.problem->getSavedSolution(0)->getAssembly();
  REQUIRE(a->getX(0) == 10);
  REQUIRE(a->getX(1) == 11);
}

TEST_CASE("problem: setShapeMaximum of zero deletes the solutions that used the part",
          "[problem][parts][solutions]") {
  SolvingFixture f = makeSolvingFixture(1, 1);
  const gridType_c & gt = *f.puzzle->getGridType();

  /* uses the A piece */
  f.problem->addSolution(assemblyAtX(gt, {10, 11}));
  /* leaves the A piece unplaced */
  f.problem->addSolution(assemblyAtX(gt, {-1, 21}));

  f.problem->setShapeMaximum(f.shapeA, 0);

  REQUIRE(f.problem->getNumberOfParts() == 1);
  REQUIRE(f.problem->getShapeIdOfPart(0) == f.shapeB);

  REQUIRE(f.problem->getNumberOfSavedSolutions() == 1);
  const assembly_c * a = f.problem->getSavedSolution(0)->getAssembly();
  REQUIRE(a->placementCount() == 1);
  REQUIRE(a->getX(0) == 21);
}
