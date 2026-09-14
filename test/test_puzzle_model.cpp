#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "lib/assembly.h"
#include "lib/disassembly.h"
#include "lib/gridtype.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/solution.h"
#include "lib/symmetries.h"
#include "lib/voxel.h"
#include "tools/gzstream.h"
#include "tools/xml.h"

#include <memory>
#include <string>

using namespace bttest;

namespace {

/** an empty puzzle of the given grid; the caller owns it */
std::unique_ptr<puzzle_c> makePuzzle(gridType_c::gridType t = gridType_c::GT_BRICKS) {
  return std::make_unique<puzzle_c>(std::make_unique<gridType_c>(t));
}

/**
 * A puzzle with `numShapes` cube shapes, sized 1x1x1, 2x2x2, ..., and one
 * empty problem attached to it. The shapes are deliberately sized
 * differently (rather than named) so a test can tell them apart by reading
 * getX() off whatever voxel_c a problem_c accessor hands back -- a much
 * harder thing to satisfy by accident than an index happening to be in
 * range.
 */
struct ProblemFixture {
  std::unique_ptr<puzzle_c> puzzle;
  problem_c * problem;
};

ProblemFixture makeProblemFixture(unsigned int numShapes) {
  ProblemFixture f;
  f.puzzle = makePuzzle();
  for (unsigned int i = 1; i <= numShapes; i++)
    f.puzzle->addShape(i, i, i);
  unsigned int probIdx = f.puzzle->addProblem();
  f.problem = f.puzzle->getProblem(probIdx);
  return f;
}

/** parses the given bundled example file into a fresh, independently-owned
 * puzzle_c; the caller owns it. */
std::unique_ptr<puzzle_c> loadFreshPuzzle(const char * path) {
  std::unique_ptr<std::istream> str(openGzFile(path));
  REQUIRE(str != nullptr);
  xmlParser_c pars(*str);
  return std::make_unique<puzzle_c>(pars);
}

/* examples/PelikanBurr.xmpuzzle's only problem is fully solved (state "2",
   i.e. SS_SOLVED) with 12 known assemblies and 1 known solution, and it
   carries one saved solution with a full <separation> disassembly tree --
   all baked straight into the file, so nothing here ever calls a solver.
   Every case below that uses this problem only reads from it, or copies the
   assembly_c/separation_c its saved solution owns (via their own copy
   constructors) to build an independent solution_c of its own -- none of
   them mutate this problem or its puzzle. That makes one shared load safe,
   so -- as in test_disassembly.cpp's cubeInCageSeparation()/cubeInCageProblem()
   -- the file is parsed exactly once, via a function-local static, and the
   puzzle_c is kept alive for the rest of the test binary so the problem_c
   reference handed out here stays valid. */
problem_c & pelikanProblem() {
  static const std::unique_ptr<puzzle_c> puzzle = loadFreshPuzzle("examples/PelikanBurr.xmpuzzle");
  problem_c * p = puzzle->getProblem(0);
  REQUIRE(p != nullptr);
  return *p;
}

/**
 * A GT_BRICKS puzzle whose result shape is a straight domino -- two
 * touching unit cells along x -- built from two individually-symmetric
 * unit-cube pieces (one part, maximum 2). Small enough to work out
 * assembly_c::transform()'s rotation-matrix arithmetic
 * (src/lib/tabs_0/rotmatrix.inc) and smallerRotationExists()'s
 * self-symmetry search entirely by hand: a filled 1x1x1 voxel space is
 * unchanged by every transformation, so its own hotspot and bounding box
 * are (0,0,0) under all of them, which means transform()'s per-piece
 * re-normalisation step (assembly.cpp, the "tr != placements[p].transformation"
 * branch) never shifts a placement -- only the domino's own rotation and
 * hotspot shift move anything.
 */
struct DominoFixture {
  std::unique_ptr<puzzle_c> puzzle;
  problem_c * problem;
};

DominoFixture makeDominoFixture() {
  DominoFixture f;
  f.puzzle = makePuzzle(gridType_c::GT_BRICKS);
  const gridType_c & gt = *f.puzzle->getGridType();

  // shape ids stay local: no case needs them, only the wiring below does
  unsigned int pieceShape  = f.puzzle->addShape(fromLayers(gt, {{"#"}}));   // a single filled unit cube
  unsigned int resultShape = f.puzzle->addShape(fromLayers(gt, {{"##"}}));  // two touching cells along x

  unsigned int probIdx = f.puzzle->addProblem();
  f.problem = f.puzzle->getProblem(probIdx);
  f.problem->setResultId(resultShape);
  f.problem->setShapeMaximum(pieceShape, 2);

  return f;
}

/**
 * Reimplements, using only assembly_c's public getters, the pivot-then-
 * sequential placement ordering documented on placement_c::operator< in
 * assembly.h (compare transformation, then x, then y, then z; the pivot
 * placement decides first, and only on a tie do the remaining placements
 * get compared in index order) -- the same two-phase rule
 * assembly_c::compare() applies internally, but compare() is private and
 * is the very thing a test of smallerRotationExists needs to stay
 * independent of. Only meaningful when every placement in both assemblies
 * is placed, which holds for every fixture this is used with below.
 */
bool isLexSmaller(const assembly_c & a, const assembly_c & b, unsigned int pivot) {
  auto lessAt = [](const assembly_c & x, const assembly_c & y, unsigned int i) {
    if (x.getTransformation(i) != y.getTransformation(i)) return x.getTransformation(i) < y.getTransformation(i);
    if (x.getX(i) != y.getX(i)) return x.getX(i) < y.getX(i);
    if (x.getY(i) != y.getY(i)) return x.getY(i) < y.getY(i);
    if (x.getZ(i) != y.getZ(i)) return x.getZ(i) < y.getZ(i);
    return false;
  };
  auto equalAt = [](const assembly_c & x, const assembly_c & y, unsigned int i) {
    return x.getTransformation(i) == y.getTransformation(i) &&
           x.getX(i) == y.getX(i) && x.getY(i) == y.getY(i) && x.getZ(i) == y.getZ(i);
  };

  unsigned int n = a.placementCount();

  if (pivot < n) {
    if (lessAt(a, b, pivot)) return true;
    if (!equalAt(a, b, pivot)) return false;
  }

  for (unsigned int i = 0; i < n; i++) {
    if (i == pivot) continue;
    if (lessAt(a, b, i)) return true;
    if (!equalAt(a, b, i)) return false;
  }

  return false;
}

} // namespace

TEST_CASE("puzzle: construction from a gridType_c round-trips getGridType()->getType()", "[model][puzzle]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));

    // the unique_ptr<gridType_c> constructor
    puzzle_c viaUnique(std::make_unique<gridType_c>(t));
    REQUIRE(viaUnique.getGridType()->getType() == t);

    // the raw-pointer constructor -- ownership is taken over, per its header comment
    puzzle_c viaRaw(new gridType_c(t));
    REQUIRE(viaRaw.getGridType()->getType() == t);
  }
}

TEST_CASE("puzzle: addShape returns successive indices tracked by getNumberOfShapes", "[model][puzzle]") {
  std::unique_ptr<puzzle_c> p = makePuzzle();

  REQUIRE(p->getNumberOfShapes() == 0);

  REQUIRE(p->addShape(1, 1, 1) == 0);
  REQUIRE(p->getNumberOfShapes() == 1);

  REQUIRE(p->addShape(2, 2, 2) == 1);
  REQUIRE(p->getNumberOfShapes() == 2);

  REQUIRE(p->addShape(3, 3, 3) == 2);
  REQUIRE(p->getNumberOfShapes() == 3);
}

TEST_CASE("puzzle: removeShape renumbers shapes -- a problem's result still names the same shape",
          "[model][puzzle][problem]") {
  std::unique_ptr<puzzle_c> p = makePuzzle();

  unsigned int first  = p->addShape(1, 1, 1);
  unsigned int second = p->addShape(1, 1, 1);
  unsigned int third  = p->addShape(1, 1, 1);
  REQUIRE(first == 0);
  REQUIRE(second == 1);
  REQUIRE(third == 2);

  // every shape built above is an identical empty 1x1x1 space, so the only
  // way to recognise "the third shape" after its index has shifted is a
  // marker that travels with the object itself, not with its position
  p->getShape(third)->setName("third");

  unsigned int probIdx = p->addProblem();
  problem_c * problem = p->getProblem(probIdx);
  problem->setResultId(third);

  REQUIRE(problem->getResultId() == 2);

  p->removeShape(first);   // shapes 1 and 2 (second, third) shift down to 0 and 1

  REQUIRE(p->getNumberOfShapes() == 2);

  // the property that matters: the problem's result id was renumbered to
  // follow the shift...
  REQUIRE(problem->getResultId() == 1);
  // ...and it is still the SAME shape -- the one named "third" -- not just
  // whatever now happens to occupy index 1. If removeShape only shrank the
  // shape list without walking the problems to decrement their references,
  // getResultId() would still read 1 by coincidence (index 1 exists either
  // way), but the shape sitting there would be "second", not "third". Only
  // checking the name catches that.
  REQUIRE(p->getShape(problem->getResultId())->getName() == "third");
}

TEST_CASE("puzzle: removeShape of the shape a problem's result names invalidates the result",
          "[model][puzzle][problem]") {
  std::unique_ptr<puzzle_c> p = makePuzzle();

  /* Two shapes, not one. resultValid() is `result < getNumberOfShapes()`,
     so with a single shape the count falls to 0 on removal and the result
     reads invalid whether or not removeShape actually cleared it -- the
     assertion below would hold even against a removeShape that never
     touched the problem. Keeping a second shape alive means the count stays
     1, so an uncleared result of 0 would still satisfy `0 < 1` and report
     valid. Only then does REQUIRE_FALSE below depend on the clearing. */
  unsigned int target    = p->addShape(1, 1, 1);
  unsigned int bystander = p->addShape(2, 2, 2);
  p->getShape(bystander)->setName("bystander");

  unsigned int probIdx = p->addProblem();
  problem_c * problem = p->getProblem(probIdx);
  problem->setResultId(target);

  REQUIRE(problem->resultValid());
  REQUIRE(p->getNumberOfShapes() == 2);

  p->removeShape(target);

  // the shape list did not empty out -- the bystander survived the removal
  // and slid down into index 0, so an index of 0 is still in range
  REQUIRE(p->getNumberOfShapes() == 1);
  REQUIRE(p->getShape(0)->getName() == "bystander");

  REQUIRE_FALSE(problem->resultValid());
}

TEST_CASE("puzzle: exchangeShapes swaps two shapes and leaves the others alone", "[model][puzzle]") {
  std::unique_ptr<puzzle_c> p = makePuzzle();

  p->addShape(1, 1, 1);
  p->addShape(2, 2, 2);
  p->addShape(3, 3, 3);

  p->getShape(0)->setName("a");
  p->getShape(1)->setName("b");
  p->getShape(2)->setName("c");

  p->exchangeShapes(0, 2);

  REQUIRE(p->getShape(0)->getName() == "c");
  REQUIRE(p->getShape(1)->getName() == "b");   // untouched by an exchange of 0 and 2
  REQUIRE(p->getShape(2)->getName() == "a");
}

TEST_CASE("puzzle: addColor assigns successive 0-based ids tracked by colorNumber", "[model][puzzle][color]") {
  std::unique_ptr<puzzle_c> p = makePuzzle();

  REQUIRE(p->colorNumber() == 0);

  REQUIRE(p->addColor(255, 0, 0) == 0);
  REQUIRE(p->colorNumber() == 1);

  REQUIRE(p->addColor(0, 255, 0) == 1);
  REQUIRE(p->colorNumber() == 2);

  REQUIRE(p->addColor(0, 0, 255) == 2);
  REQUIRE(p->colorNumber() == 3);
}

TEST_CASE("puzzle: getColor and changeColor use the 0-based id addColor returned", "[model][puzzle][color]") {
  std::unique_ptr<puzzle_c> p = makePuzzle();

  unsigned int red   = p->addColor(255, 0, 0);
  unsigned int green = p->addColor(0, 255, 0);

  unsigned char r, g, b;

  p->getColor(red, &r, &g, &b);
  REQUIRE(r == 255);
  REQUIRE(g == 0);
  REQUIRE(b == 0);

  p->getColor(green, &r, &g, &b);
  REQUIRE(r == 0);
  REQUIRE(g == 255);
  REQUIRE(b == 0);

  p->changeColor(red, 10, 20, 30);

  p->getColor(red, &r, &g, &b);
  REQUIRE(r == 10);
  REQUIRE(g == 20);
  REQUIRE(b == 30);

  // changeColor(red, ...) must not have touched the other colour
  p->getColor(green, &r, &g, &b);
  REQUIRE(r == 0);
  REQUIRE(g == 255);
  REQUIRE(b == 0);
}

TEST_CASE("puzzle: removeColor takes a 1-based colour id and renumbers the ones above it",
          "[model][puzzle][color]") {
  /* addColor/getColor/changeColor agree with each other on a 0-based colour
     index: addColor returns `colors.size()-1` after pushing, and both
     getColor and changeColor assert `idx < colors.size()` before indexing
     `colors[idx]` directly (src/lib/puzzle.cpp). removeColor does not use
     that same convention: it asserts `col <= colors.size()` (note <=, not
     <) and erases `colors.begin() + (col - 1)`, i.e. col is a 1-based
     colour id, matching the numbering problem_c's colour-constraint
     functions use where colour id 0 is reserved to mean "no colour" (see
     problem_c::allowPlacement, which stores `(pc-1) << 16 | (res-1)`).
     Concretely: the colour addColor handed back as index 0 must be removed
     by calling removeColor(1), not removeColor(0) -- removeColor(0) is not
     exercised here since `col - 1` on col==0 underflows the unsigned
     parameter and erases at a wild offset instead of asserting, which looks
     like a real defect worth flagging rather than a case safe to run in a
     test binary. This mismatch between addColor's return convention and
     removeColor's parameter convention is reported separately as a
     candidate API inconsistency. */
  std::unique_ptr<puzzle_c> p = makePuzzle();

  unsigned int first  = p->addColor(255, 0, 0);
  unsigned int second = p->addColor(0, 255, 0);
  unsigned int third  = p->addColor(0, 0, 255);
  REQUIRE(first == 0);
  REQUIRE(second == 1);
  REQUIRE(third == 2);

  p->removeColor(first + 1);   // 1-based: removes the colour addColor returned as index 0

  REQUIRE(p->colorNumber() == 2);

  unsigned char r, g, b;

  p->getColor(0, &r, &g, &b);   // the second colour renumbered down to 0
  REQUIRE(r == 0);
  REQUIRE(g == 255);
  REQUIRE(b == 0);

  p->getColor(1, &r, &g, &b);   // the third colour renumbered down to 1
  REQUIRE(r == 0);
  REQUIRE(g == 0);
  REQUIRE(b == 255);
}

TEST_CASE("puzzle: comment and comment-popup flag round-trip through their setters", "[model][puzzle]") {
  std::unique_ptr<puzzle_c> p = makePuzzle();

  REQUIRE(p->getComment().empty());
  REQUIRE_FALSE(p->getCommentPopup());   // commentPopup starts false (see puzzle_c's constructor)

  p->setComment("a comment describing the puzzle");
  REQUIRE(p->getComment() == "a comment describing the puzzle");

  p->setCommentPopup(true);
  REQUIRE(p->getCommentPopup());

  p->setCommentPopup(false);
  REQUIRE_FALSE(p->getCommentPopup());
}

TEST_CASE("problem: setShapeMinimum/setShapeMaximum and their readers add and describe a part",
          "[model][problem]") {
  ProblemFixture f = makeProblemFixture(2);   // shapes 0 (1x1x1), 1 (2x2x2)
  problem_c * pr = f.problem;

  // an untouched shape has min/max 0 and is not used
  REQUIRE(pr->getShapeMinimum(0) == 0);
  REQUIRE(pr->getShapeMaximum(0) == 0);
  REQUIRE_FALSE(pr->usesShape(0));
  REQUIRE(pr->getNumberOfParts() == 0);

  pr->setShapeMaximum(0, 3);
  REQUIRE(pr->getShapeMaximum(0) == 3);
  REQUIRE(pr->usesShape(0));
  REQUIRE(pr->getNumberOfParts() == 1);

  pr->setShapeMinimum(0, 2);
  REQUIRE(pr->getShapeMinimum(0) == 2);
  REQUIRE(pr->getShapeMaximum(0) == 3);   // setShapeMinimum must not have touched the maximum

  // shape 1 was never touched -- usesShape and the readers must not leak
  // across shapes
  REQUIRE_FALSE(pr->usesShape(1));
  REQUIRE(pr->getShapeMinimum(1) == 0);
  REQUIRE(pr->getShapeMaximum(1) == 0);
  REQUIRE(pr->getNumberOfParts() == 1);

  // usesShape is also true for a shape that is only the result, even though
  // it was never given a piece count -- it is "used" without being a part
  pr->setResultId(1);
  REQUIRE(pr->usesShape(1));
  REQUIRE(pr->getShapeMaximum(1) == 0);   // still not a part
  REQUIRE(pr->getNumberOfParts() == 1);   // and still doesn't count as one

  // setShapeMaximum(shape, 0) drops the part again
  pr->setShapeMaximum(0, 0);
  REQUIRE(pr->getShapeMaximum(0) == 0);
  REQUIRE_FALSE(pr->usesShape(0));
  REQUIRE(pr->getNumberOfParts() == 0);
}

TEST_CASE("problem: getShapeIdOfPart/getPartIdForShape/getPartShape map parts to shapes -- not by position",
          "[model][problem]") {
  ProblemFixture f = makeProblemFixture(3);   // shapes 0 (1x1x1), 1 (2x2x2), 2 (3x3x3)
  problem_c * pr = f.problem;

  // shape 1 is deliberately left unused, so the part list (indices 0, 1)
  // does not line up with the shape ids it names (0, 2) -- a test that
  // only ever used consecutive shape ids could pass by coincidence even if
  // getPartIdForShape ignored its argument and just returned its index
  pr->setShapeMaximum(0, 1);
  pr->setShapeMaximum(2, 1);

  REQUIRE(pr->getNumberOfParts() == 2);

  unsigned int partForShape0 = pr->getPartIdForShape(0);
  unsigned int partForShape2 = pr->getPartIdForShape(2);
  REQUIRE(partForShape0 != partForShape2);

  REQUIRE(pr->getShapeIdOfPart(partForShape0) == 0);
  REQUIRE(pr->getShapeIdOfPart(partForShape2) == 2);

  // getPartShape must return the voxel space of the shape the part names,
  // not merely some voxel space -- checked via the size that makeProblemFixture
  // gave each shape
  REQUIRE(pr->getPartShape(partForShape0)->getX() == 1);
  REQUIRE(pr->getPartShape(partForShape2)->getX() == 3);

  // and the const overload agrees
  const problem_c * cpr = pr;
  REQUIRE(cpr->getPartShape(partForShape0)->getX() == 1);
}

TEST_CASE("problem: getNumberOfPieces sums part maxima -- getPartIdToPieceId/getPartIndexToPieceId "
          "walk the piece range -- pieces are not parts",
          "[model][problem]") {
  ProblemFixture f = makeProblemFixture(4);   // shapes 0..3, sized 1..4
  problem_c * pr = f.problem;

  // shape 1 is skipped, so part indices (0, 1, 2) and shape ids (0, 2, 3)
  // diverge -- and shape 0 and shape 2 have a piece count greater than 1,
  // so pieceId and (partId, indexWithinPart) genuinely diverge too
  pr->setShapeMaximum(0, 2);   // part 0: 2 pieces of the 1x1x1 shape
  pr->setShapeMaximum(2, 3);   // part 1: 3 pieces of the 3x3x3 shape
  pr->setShapeMaximum(3, 1);   // part 2: 1 piece of the 4x4x4 shape

  REQUIRE(pr->getNumberOfParts() == 3);          // 3 distinct parts...
  REQUIRE(pr->getNumberOfPieces() == 6);         // ...but 6 individual pieces (2+3+1)
  // if getNumberOfPieces() had been aliased to getNumberOfParts(), this
  // fixture is exactly the case that would catch it: 3 != 6

  // expected (partId, indexWithinPart, shapeId) for each of the 6 pieces,
  // in piece-id order
  const unsigned int expectedPart[6]    = {0, 0, 1, 1, 1, 2};
  const unsigned int expectedIndex[6]   = {0, 1, 0, 1, 2, 0};
  const unsigned int expectedShapeId[6] = {0, 0, 2, 2, 2, 3};
  const unsigned int expectedSize[6]    = {1, 1, 3, 3, 3, 4};

  for (unsigned int pieceId = 0; pieceId < 6; pieceId++) {
    INFO("pieceId " << pieceId);

    unsigned int partId = pr->getPartIdToPieceId(pieceId);
    unsigned int index  = pr->getPartIndexToPieceId(pieceId);

    REQUIRE(partId == expectedPart[pieceId]);
    REQUIRE(index == expectedIndex[pieceId]);
    REQUIRE(pr->getShapeIdOfPart(partId) == expectedShapeId[pieceId]);
    REQUIRE(pr->getPartShape(partId)->getX() == expectedSize[pieceId]);
  }
}

TEST_CASE("problem: setResultId/resultValid/getResultId/clearResult round-trip -- getResultId asserts "
          "when the result is invalid",
          "[model][problem]") {
  ProblemFixture f = makeProblemFixture(3);   // shapes 0 (1x1x1), 1 (2x2x2), 2 (3x3x3)
  problem_c * pr = f.problem;

  // a freshly-built problem has no valid result
  REQUIRE_FALSE(pr->resultValid());

  pr->setResultId(1);
  REQUIRE(pr->resultValid());
  REQUIRE(pr->getResultId() == 1);
  REQUIRE(f.puzzle->getShape(pr->getResultId())->getX() == 2);

  pr->setResultId(2);
  REQUIRE(pr->getResultId() == 2);   // setResultId can move to a different shape

  pr->clearResult();
  REQUIRE_FALSE(pr->resultValid());

#ifndef NDEBUG
  // getResultId's header comment says "only call ... when you know the
  // shape is valid"; bt_assert enforces that in debug builds by throwing
  // assert_exception rather than returning a stale or out-of-range id. This
  // provokes that assertion deliberately, so it must run only in debug
  // builds -- under NDEBUG bt_assert compiles away and there is nothing to
  // throw.
  REQUIRE_THROWS_AS(pr->getResultId(), assert_exception);
#endif
}

TEST_CASE("problem: removeShape drops the matching part -- the remaining parts and result still "
          "name the right shapes",
          "[model][problem]") {
  ProblemFixture f = makeProblemFixture(4);   // shapes 0..3, sized 1..4
  problem_c * pr = f.problem;

  // shape 1 (size 2) is used only as the result, never as a part -- so this
  // fixture exercises the result-renumbering branch of removeShape at the
  // same time as the part-list branch, with distinguishable sizes rather
  // than shapes that would look the same if the wrong one were kept
  pr->setShapeMaximum(0, 1);   // part naming shape 0 (size 1)
  pr->setShapeMaximum(2, 1);   // part naming shape 2 (size 3)
  pr->setShapeMaximum(3, 2);   // part naming shape 3 (size 4), max 2
  pr->setResultId(1);          // result names shape 1 (size 2)

  REQUIRE(pr->getNumberOfParts() == 3);

  // removeShape is called through the puzzle, the only entry point that
  // keeps the puzzle's shape list and every problem's bookkeeping
  // consistent with each other (problem_c::removeShape's own header
  // comment: "when a shape is removed from the puzzle this function is
  // called"). This also renumbers shape ids 1..3 down to 0..2.
  f.puzzle->removeShape(0);

  REQUIRE(f.puzzle->getNumberOfShapes() == 3);
  REQUIRE(pr->getNumberOfParts() == 2);   // the part naming the removed shape is gone

  // the result followed shape 1 down to its new id 0, and is still the
  // size-2 shape -- not merely "some valid index"
  REQUIRE(pr->resultValid());
  REQUIRE(pr->getResultId() == 0);
  REQUIRE(f.puzzle->getShape(pr->getResultId())->getX() == 2);

  // the two surviving parts still name their original shapes by identity
  // (size), even though both the shape ids and the part order have shifted
  unsigned int partForSize3 = pr->getPartIdForShape(1);   // old shape 2, now id 1
  unsigned int partForSize4 = pr->getPartIdForShape(2);   // old shape 3, now id 2

  REQUIRE(pr->getPartShape(partForSize3)->getX() == 3);
  REQUIRE(pr->getPartShape(partForSize4)->getX() == 4);
  REQUIRE(pr->getShapeMaximum(2) == 2);   // the max==2 part kept its own count through the shift
}

TEST_CASE("problem: exchangeShapes swaps which shape each part and the result name",
          "[model][problem]") {
  ProblemFixture f = makeProblemFixture(4);   // shapes 0..3, sized 1..4
  problem_c * pr = f.problem;

  pr->setShapeMaximum(0, 1);   // part naming shape 0 (size 1)
  pr->setShapeMaximum(1, 1);   // part naming shape 1 (size 2) -- not swapped, must be untouched
  pr->setShapeMaximum(3, 2);   // part naming shape 3 (size 4), max 2
  pr->setResultId(2);          // result names shape 2 (size 3) -- not swapped either

  // exchangeShapes is called through the puzzle, which swaps the shapes
  // themselves (so id 0 now holds what used to be size 4, and id 3 now
  // holds what used to be size 1) and then tells every problem to follow
  // with problem_c::exchangeShapes -- exercising both halves together is
  // the only way a mismatch between them would show up
  f.puzzle->exchangeShapes(0, 3);

  REQUIRE(f.puzzle->getShape(0)->getX() == 4);
  REQUIRE(f.puzzle->getShape(3)->getX() == 1);

  // the part that used to name shape 0 (size 1) now names shape 3, and
  // still points at the size-1 voxel space -- not just "id 3"
  unsigned int partNowAt3 = pr->getPartIdForShape(3);
  REQUIRE(pr->getPartShape(partNowAt3)->getX() == 1);
  REQUIRE(pr->getShapeMaximum(3) == 1);   // that part's own max (1) travelled with it

  // the part that used to name shape 3 (size 4, max 2) now names shape 0
  unsigned int partNowAt0 = pr->getPartIdForShape(0);
  REQUIRE(pr->getPartShape(partNowAt0)->getX() == 4);
  REQUIRE(pr->getShapeMaximum(0) == 2);   // that part's own max (2) travelled with it too

  // the part naming shape 1 was not involved in the exchange and is untouched
  REQUIRE(pr->getPartShape(pr->getPartIdForShape(1))->getX() == 2);
  REQUIRE(pr->getShapeMaximum(1) == 1);

  // the result named shape 2, neither end of the exchange, so it is untouched
  REQUIRE(pr->getResultId() == 2);
  REQUIRE(f.puzzle->getShape(pr->getResultId())->getX() == 3);
}

TEST_CASE("problem: getNumberOfSavedSolutions/getSavedSolution read the solution straight out of a loaded puzzle",
          "[model][problem][solution]") {
  problem_c & pr = pelikanProblem();

  REQUIRE(pr.getNumberOfSavedSolutions() == 1);

  const solution_c * sol = pr.getSavedSolution(0);
  REQUIRE(sol != nullptr);
  REQUIRE(sol->getAssembly() != nullptr);
  // the file's <solution> tag carries asmNum="9" and no solNum attribute
  // (solNum is only written when non-zero -- see solution_c::save)
  REQUIRE(sol->getAssemblyNumber() == 9);
  REQUIRE(sol->getSolutionNumber() == 0);
  // this solution's XML carries a full <separation>, not merely a
  // <separationInfo> summary
  REQUIRE(sol->getDisassembly() != nullptr);
}

TEST_CASE("problem: numAssemblies/numSolutions/usedTime report through the Known predicates without "
          "asserting -- their getters assert unless the problem has been solved",
          "[model][problem]") {
  // the solved side: examples/PelikanBurr.xmpuzzle's only problem is fully
  // solved, with the figures baked into the file (assemblies="12"
  // solutions="1" time="0")
  problem_c & solved = pelikanProblem();
  REQUIRE(solved.getSolveState() == SS_SOLVED);
  REQUIRE(solved.numAssembliesKnown());
  REQUIRE(solved.getNumAssemblies() == 12);
  REQUIRE(solved.numSolutionsKnown());
  REQUIRE(solved.getNumSolutions() == 1);
  REQUIRE(solved.usedTimeKnown());
  REQUIRE(solved.getUsedTime() == 0);

  // the unsolved side: a freshly built problem has never been solved, so
  // the *Known() predicates must say so without throwing
  ProblemFixture f = makeProblemFixture(1);
  problem_c * fresh = f.problem;
  REQUIRE(fresh->getSolveState() == SS_UNSOLVED);
  REQUIRE_FALSE(fresh->numAssembliesKnown());
  REQUIRE_FALSE(fresh->numSolutionsKnown());
  REQUIRE_FALSE(fresh->usedTimeKnown());

#ifndef NDEBUG
  // the getters themselves are documented ("Throws an exception, when not
  // known") and enforce that with bt_assert, which only throws in debug
  // builds -- under NDEBUG it compiles to ((void)0), so this is guarded to
  // run only where the throw actually happens
  REQUIRE_THROWS_AS(fresh->getNumAssemblies(), assert_exception);
  REQUIRE_THROWS_AS(fresh->getNumSolutions(), assert_exception);
  REQUIRE_THROWS_AS(fresh->getUsedTime(), assert_exception);
#endif
}

TEST_CASE("problem: removeSolution drops exactly the addressed saved solution -- later ones shift down",
          "[model][problem][solution]") {
  // examples/SolidSixPieceBurrs.xmpuzzle's only problem carries 314 saved
  // solutions (each with a <separationInfo> summary) -- plenty to remove one
  // from the middle and see its neighbours shift
  std::unique_ptr<puzzle_c> puzzle = loadFreshPuzzle("examples/SolidSixPieceBurrs.xmpuzzle");
  problem_c * pr = puzzle->getProblem(0);
  REQUIRE(pr != nullptr);

  const unsigned int before = pr->getNumberOfSavedSolutions();
  REQUIRE(before > 5);

  // identify the solution at index 5 by its own assembly number, not by
  // position -- after removeSolution(4), index 4 must hold what WAS at
  // index 5, not merely "some" solution that happens to still be there
  unsigned int survivorAsmNum = pr->getSavedSolution(5)->getAssemblyNumber();

  pr->removeSolution(4);

  REQUIRE(pr->getNumberOfSavedSolutions() == before - 1);
  REQUIRE(pr->getSavedSolution(4)->getAssemblyNumber() == survivorAsmNum);
}

TEST_CASE("problem: removeAllSolutions clears the saved list and resets solving to SS_UNSOLVED",
          "[model][problem][solution]") {
  std::unique_ptr<puzzle_c> puzzle = loadFreshPuzzle("examples/SolidSixPieceBurrs.xmpuzzle");
  problem_c * pr = puzzle->getProblem(0);
  REQUIRE(pr != nullptr);

  REQUIRE(pr->getNumberOfSavedSolutions() > 0);
  REQUIRE(pr->getSolveState() == SS_SOLVED);
  REQUIRE(pr->numAssembliesKnown());

  pr->removeAllSolutions();

  // the saved list is gone...
  REQUIRE(pr->getNumberOfSavedSolutions() == 0);
  // ...and so is everything solving learned -- not merely the list
  REQUIRE(pr->getSolveState() == SS_UNSOLVED);
  REQUIRE_FALSE(pr->numAssembliesKnown());
  REQUIRE_FALSE(pr->numSolutionsKnown());
  REQUIRE_FALSE(pr->usedTimeKnown());
}

namespace {

/* Scan `count` items for the first adjacent pair that `outOfOrder` rejects,
   returning that index, or `count` when the whole run is ordered.

   "The list is sorted" is one fact. Asserting it with one check per adjacent
   pair states that single fact once per pair -- several hundred times over on
   a fixture this size, since SolidSixPieceBurrs ships 314 saved solutions --
   which inflates the suite's assertion total without establishing anything
   further. Returning the first offending index keeps the one thing the
   per-pair loop was good for: a failure names the exact place the order
   breaks. */
template <typename Pred>
unsigned int firstOutOfOrder(unsigned int count, Pred outOfOrder) {
  for (unsigned int i = 0; i + 1 < count; i++)
    if (outOfOrder(i)) return i;
  return count;
}

/* The same idea for a check that looks at ONE item rather than a pair:
   the index of the first item in [0, count) that `fails`, or `count` when
   none do. */
template <typename Pred>
unsigned int firstFailing(unsigned int count, Pred fails) {
  for (unsigned int i = 0; i < count; i++)
    if (fails(i)) return i;
  return count;
}

} // namespace

TEST_CASE("problem: sortSolutions(0) orders the saved solutions by ascending assembly number",
          "[model][problem][solution]") {
  std::unique_ptr<puzzle_c> puzzle = loadFreshPuzzle("examples/SolidSixPieceBurrs.xmpuzzle");
  problem_c * pr = puzzle->getProblem(0);
  REQUIRE(pr != nullptr);

  const unsigned int n = pr->getNumberOfSavedSolutions();
  REQUIRE(n > 1);

  // confirm the file's own save order is NOT already ascending by assembly
  // number, so a sortSolutions that silently did nothing could not pass the
  // check below by accident
  auto descends = [pr](unsigned int i) {
    return pr->getSavedSolution(i)->getAssemblyNumber() > pr->getSavedSolution(i + 1)->getAssemblyNumber();
  };

  REQUIRE(firstOutOfOrder(n, descends) < n);

  pr->sortSolutions(0);

  REQUIRE(firstOutOfOrder(n, descends) == n);
}

TEST_CASE("problem: sortSolutions(1) orders the saved solutions by ascending disassembly level",
          "[model][problem][solution]") {
  std::unique_ptr<puzzle_c> puzzle = loadFreshPuzzle("examples/SolidSixPieceBurrs.xmpuzzle");
  problem_c * pr = puzzle->getProblem(0);
  REQUIRE(pr != nullptr);

  const unsigned int n = pr->getNumberOfSavedSolutions();
  REQUIRE(n > 1);

  // every saved solution here carries a <separationInfo> summary, so
  // getDisassemblyInfo() answers for all of them and comp_1_level's
  // "both have info" guard never suppresses a comparison
  REQUIRE(firstFailing(n, [pr](unsigned int i) {
    return pr->getSavedSolution(i)->getDisassemblyInfo() == nullptr;
  }) == n);

  // confirm the key this sort uses -- disassembly_c::compare() -- is not
  // already monotonic across the file's save order
  auto descends = [pr](unsigned int i) {
    return pr->getSavedSolution(i)->getDisassemblyInfo()->compare(
             pr->getSavedSolution(i + 1)->getDisassemblyInfo()) > 0;
  };

  REQUIRE(firstOutOfOrder(n, descends) < n);

  pr->sortSolutions(1);

  REQUIRE(firstOutOfOrder(n, descends) == n);
}

TEST_CASE("problem: sortSolutions(2) orders the saved solutions by ascending total move count",
          "[model][problem][solution]") {
  std::unique_ptr<puzzle_c> puzzle = loadFreshPuzzle("examples/SolidSixPieceBurrs.xmpuzzle");
  problem_c * pr = puzzle->getProblem(0);
  REQUIRE(pr != nullptr);

  const unsigned int n = pr->getNumberOfSavedSolutions();
  REQUIRE(n > 1);

  // the file's own save order already happens to be ascending in sumMoves()
  // (it groups every 5-move solution before every 6-move one), which would
  // make the "sorted after" check below pass even if sortSolutions(2) were
  // a no-op. Scramble that first with a sort by a DIFFERENT, independently
  // tested key (assembly number) -- asmNum and sumMoves are not perfectly
  // correlated here, so this produces a real interleaving of 5s and 6s to
  // sort back out
  pr->sortSolutions(0);

  auto descends = [pr](unsigned int i) {
    return pr->getSavedSolution(i)->getDisassemblyInfo()->sumMoves() >
           pr->getSavedSolution(i + 1)->getDisassemblyInfo()->sumMoves();
  };

  REQUIRE(firstOutOfOrder(n, descends) < n);

  pr->sortSolutions(2);

  REQUIRE(firstOutOfOrder(n, descends) == n);
}

TEST_CASE("problem: sortSolutions(3) orders the saved solutions by assembly_c::comparePieces()",
          "[model][problem][solution]") {
  std::unique_ptr<puzzle_c> puzzle = loadFreshPuzzle("examples/SolidSixPieceBurrs.xmpuzzle");
  problem_c * pr = puzzle->getProblem(0);
  REQUIRE(pr != nullptr);

  const unsigned int n = pr->getNumberOfSavedSolutions();
  REQUIRE(n > 1);

  // comp_3_pieces(s1, s2) treats s1 as belonging before s2 when
  // comparePieces(s1, s2) > 0 (src/lib/problem.cpp); a correctly sorted
  // list must therefore never have a solution whose comparePieces() against
  // its predecessor is > 0 -- check the file's save order violates that
  // first, so the "no violation" check after sorting is not vacuous
  auto laterBelongsBefore = [pr](unsigned int i) {
    return pr->getSavedSolution(i + 1)->getAssembly()->comparePieces(pr->getSavedSolution(i)->getAssembly()) > 0;
  };

  REQUIRE(firstOutOfOrder(n, laterBelongsBefore) < n);

  pr->sortSolutions(3);

  REQUIRE(firstOutOfOrder(n, laterBelongsBefore) == n);
}

TEST_CASE("solution: getAssembly/getDisassembly return exactly the objects handed to the constructor",
          "[model][solution]") {
  // borrow real assembly_c and separation_c objects out of PelikanBurr's
  // already-loaded saved solution via their own copy constructors, so this
  // solution_c owns independent objects rather than sharing pelikanProblem()'s
  const solution_c * src = pelikanProblem().getSavedSolution(0);
  REQUIRE(src->getDisassembly() != nullptr);

  assembly_c * assm = new assembly_c(src->getAssembly());
  separation_c * tree = new separation_c(src->getDisassembly());

  solution_c sol(assm, 7, tree, 3);

  REQUIRE(sol.getAssembly() == assm);
  REQUIRE(sol.getDisassembly() == tree);
  REQUIRE(sol.getAssemblyNumber() == 7);
  REQUIRE(sol.getSolutionNumber() == 3);
}

TEST_CASE("solution: setDisassembly replaces the tree in both overloads -- leaving the assembly untouched",
          "[model][solution]") {
  const solution_c * src = pelikanProblem().getSavedSolution(0);
  REQUIRE(src->getDisassembly() != nullptr);

  assembly_c * assm = new assembly_c(src->getAssembly());
  separation_c * treeA = new separation_c(src->getDisassembly());
  separation_c * treeB = new separation_c(src->getDisassembly());

  solution_c sol(assm, 0, treeA, 0);
  REQUIRE(sol.getDisassembly() == treeA);

  // the raw-pointer overload: treeA is replaced by treeB and freed by
  // solution_c itself (tree.reset(sep) in solution_c::setDisassembly) --
  // this test never touches treeA again after this call, so it can only
  // check that ownership genuinely moved to treeB, not that treeA's memory
  // was released
  sol.setDisassembly(treeB);
  REQUIRE(sol.getDisassembly() == treeB);
  REQUIRE(sol.getAssembly() == assm);

  // the unique_ptr overload does the same, and empties the caller's pointer
  std::unique_ptr<separation_c> treeC = std::make_unique<separation_c>(src->getDisassembly());
  separation_c * treeCRaw = treeC.get();
  sol.setDisassembly(std::move(treeC));
  REQUIRE(sol.getDisassembly() == treeCRaw);
  REQUIRE(treeC == nullptr);
  REQUIRE(sol.getAssembly() == assm);
}

TEST_CASE("solution: removeDisassembly clears the tree but keeps the assembly -- getDisassemblyInfo "
          "still answers from the summary it leaves behind",
          "[model][solution]") {
  const solution_c * src = pelikanProblem().getSavedSolution(0);
  REQUIRE(src->getDisassembly() != nullptr);

  assembly_c * assm = new assembly_c(src->getAssembly());
  separation_c * tree = new separation_c(src->getDisassembly());

  solution_c sol(assm, 0, tree, 0);
  REQUIRE(sol.getDisassembly() != nullptr);
  REQUIRE(sol.getDisassemblyInfo() != nullptr);

  sol.removeDisassembly();

  // the full tree is gone...
  REQUIRE(sol.getDisassembly() == nullptr);
  // ...but it left a separationInfo_c summary behind, so callers that only
  // need level/move-count information still get an answer
  REQUIRE(sol.getDisassemblyInfo() != nullptr);
  // and the assembly -- the thing removeDisassembly has no business
  // touching -- is exactly what it was before
  REQUIRE(sol.getAssembly() == assm);
}

TEST_CASE("assembly: addPlacement/addNonPlacement add slots that placementCount/isPlaced/getX/getY/getZ/"
          "getTransformation report back exactly",
          "[model][assembly]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  assembly_c a(&gt);

  REQUIRE(a.placementCount() == 0);

  a.addPlacement(3, 10, -2, 7);
  a.addNonPlacement();
  a.addPlacement(5, 0, 0, 0);

  REQUIRE(a.placementCount() == 3);

  REQUIRE(a.isPlaced(0));
  REQUIRE(a.getTransformation(0) == 3);
  REQUIRE(a.getX(0) == 10);
  REQUIRE(a.getY(0) == -2);
  REQUIRE(a.getZ(0) == 7);

  REQUIRE_FALSE(a.isPlaced(1));

  REQUIRE(a.isPlaced(2));
  REQUIRE(a.getTransformation(2) == 5);
  REQUIRE(a.getX(2) == 0);
  REQUIRE(a.getY(2) == 0);
  REQUIRE(a.getZ(2) == 0);

#ifndef NDEBUG
  // getX/getY/getZ/getTransformation each assert that the slot is placed
  // (assembly.h) -- that only throws in debug builds, so this is guarded
  REQUIRE_THROWS_AS(a.getX(1), assert_exception);
  REQUIRE_THROWS_AS(a.getTransformation(1), assert_exception);
#endif
}

TEST_CASE("assembly: comparePieces treats an assembly with a piece placed where the other has none as sorting "
          "before it",
          "[model][assembly]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  assembly_c full(&gt);
  full.addPlacement(0, 0, 0, 0);
  full.addPlacement(0, 1, 0, 0);

  assembly_c partial(&gt);
  partial.addPlacement(0, 0, 0, 0);
  partial.addNonPlacement();

  // `full` has piece 1 placed where `partial` does not; per the code
  // (assembly.cpp's comparePieces), the assembly with the piece placed
  // compares as +1 ("this" sorts first), the other as -1 -- placed pieces
  // sort ahead of the same index left unplaced, not the reverse
  REQUIRE(full.comparePieces(&partial) == 1);
  REQUIRE(partial.comparePieces(&full) == -1);
}

TEST_CASE("assembly: transform matches the grid's own rotation matrix and hotspot geometry worked out by hand",
          "[model][assembly]") {
  DominoFixture f = makeDominoFixture();
  const gridType_c & gt = *f.puzzle->getGridType();

  assembly_c a(&gt);
  a.addPlacement(0, 1, 0, 0);  // piece 0 at cell x=1
  a.addPlacement(0, 0, 0, 0);  // piece 1 at cell x=0

  /* transformation 10 is tabs_0/rotmatrix.inc row 11 (0-based index 10):
   * { -1, 0, 0,  0, -1, 0,  0, 0, 1 } -- a 180-degree rotation about z
   * (x' = -x, y' = -y, z' = z). Applied to the domino (sx=2, sy=sz=1):
   * voxel_0_c::transform's shift is shx = -(rot[0]*(sx-1)) = 1, shy = shz = 0,
   * so cell x=0 -> x=1 and cell x=1 -> x=0 -- the domino maps onto itself
   * with its two cells swapped, confirming it by hand as a self-symmetry.
   *
   * assembly_c::transform() first reads the domino's own getHotspot(10):
   * its hotspot starts at (0,0,0) (voxel_c's default), transformPoint(10)
   * leaves the origin at the origin, so getHotspot(10) reports the shift
   * alone, (1,0,0); the hotspot-jump correction subtracts transformPoint's
   * unshifted image of that same origin, i.e. 0, so rx,ry,rz stay (1,0,0);
   * and the bounding-box correction adds getBoundingBox(0)-getBoundingBox(10),
   * which are the same box (0,1,0,0,0,0) for this symmetric shape, i.e. 0 --
   * so the final offset applied to every placement is exactly (1,0,0).
   *
   * Each placement's own position is rotated by the same matrix and then
   * offset by (1,0,0): piece 0 at x=1 -> -1*1+1 = 0; piece 1 at x=0 ->
   * -1*0+1 = 1. Both land on a unit cube, which is unchanged by every
   * transformation (a single filled voxel), so the piece-level
   * re-normalisation step leaves the transformation at 0 and the position
   * untouched. sort() then places the two pieces of the one part in
   * ascending x, which is already the order the raw numbers came out in. */
  REQUIRE(a.transform(10, *f.problem, nullptr));

  REQUIRE(a.getTransformation(0) == 0);
  REQUIRE(a.getX(0) == 0);
  REQUIRE(a.getY(0) == 0);
  REQUIRE(a.getZ(0) == 0);

  REQUIRE(a.getTransformation(1) == 0);
  REQUIRE(a.getX(1) == 1);
  REQUIRE(a.getY(1) == 0);
  REQUIRE(a.getZ(1) == 0);
}

namespace {

/* Work out, independently of assembly_c::smallerRotationExists, whether any
   self-symmetry of the problem's result maps `a` to a lexicographically
   smaller assembly. This walks the same candidate set the production code
   walks -- which transformations exist is a property of the shape, not of
   the function under test -- but ranks candidates with isLexSmaller above,
   a from-scratch comparator, rather than with assembly_c's own private
   compare(). So the expected answer never comes from the code being
   checked. */
bool aSmallerRotationExists(const assembly_c & a, const problem_c & problem,
                            const symmetries_c & sym) {
  for (unsigned int t = 0; t < sym.getNumTransformations(); t++) {
    if (!sym.symmetrieContainsTransformation(getResultShape(problem)->selfSymmetries(), t))
      continue;

    assembly_c tmp(&a);
    if (!tmp.transform(t, problem, nullptr))
      continue;

    if (isLexSmaller(tmp, a, 0))
      return true;
  }
  return false;
}

} // namespace

TEST_CASE("assembly: smallerRotationExists is false when the pivot-first order already sorts the assembly first",
          "[model][assembly]") {
  DominoFixture f = makeDominoFixture();
  const gridType_c & gt = *f.puzzle->getGridType();
  const symmetries_c * sym = gt.getSymmetries();

  assembly_c a(&gt);
  a.addPlacement(0, 0, 0, 0);  // piece 0 at cell x=0
  a.addPlacement(0, 1, 0, 0);  // piece 1 at cell x=1 -- ascending, the ordinary form

  // enumerate, independently of smallerRotationExists, every transformation
  // the grid defines that is an actual self-symmetry of the domino result
  // shape (found via voxel_c::selfSymmetries(), never asking the function
  // under test), transform a COPY through assembly_c::transform() -- proven
  // against hand-worked geometry in the transform() case above -- and judge
  // "smaller" with isLexSmaller() rather than assembly_c::compare()
  // (private, and the very thing this case is trying to check independently)
  const bool expected = aSmallerRotationExists(a, *f.problem, *sym);

  // the domino's only non-trivial self-symmetry swaps its two cells
  // (transformation 10 of tabs_0/rotmatrix.inc, worked out by hand in the
  // transform() case); applied here it maps this already-ascending
  // assembly back to itself, so no candidate comes out smaller
  REQUIRE_FALSE(expected);

  REQUIRE_FALSE(a.smallerRotationExists(*f.problem, 0, nullptr, false));
}

TEST_CASE("assembly: smallerRotationExists is true when the domino's cell swap maps the assembly to a smaller one",
          "[model][assembly]") {
  DominoFixture f = makeDominoFixture();
  const gridType_c & gt = *f.puzzle->getGridType();
  const symmetries_c * sym = gt.getSymmetries();

  assembly_c a(&gt);
  a.addPlacement(0, 1, 0, 0);  // piece 0 at cell x=1
  a.addPlacement(0, 0, 0, 0);  // piece 1 at cell x=0 -- descending, not the ordinary form

  const bool expected = aSmallerRotationExists(a, *f.problem, *sym);

  // transformation 10 (hand-derived in the transform() case) swaps the two
  // cells and lands piece 0 at x=0, strictly smaller than its original x=1
  // at the pivot placement -- so a real smaller rotation does exist here
  REQUIRE(expected);

  REQUIRE(a.smallerRotationExists(*f.problem, 0, nullptr, false));
}

TEST_CASE("assembly: sort orders the placements of each part by ascending placement -- leaving other parts alone",
          "[model][assembly]") {
  ProblemFixture f = makeProblemFixture(2);
  problem_c * pr = f.problem;
  pr->setShapeMaximum(0, 3);  // part 0: 3 pieces of the 1x1x1 shape
  pr->setShapeMaximum(1, 1);  // part 1: 1 piece of the 2x2x2 shape

  const gridType_c & gt = *f.puzzle->getGridType();
  assembly_c a(&gt);
  a.addPlacement(0, 5, 0, 0);  // part 0, slot 0
  a.addPlacement(0, 1, 0, 0);  // part 0, slot 1
  a.addPlacement(0, 3, 0, 0);  // part 0, slot 2
  a.addPlacement(0, 9, 0, 0);  // part 1, slot 0 -- the only piece of its part

  // confirm part 0's three slots are not already ascending, so a no-op
  // sort could not pass the check below by accident
  REQUIRE_FALSE((a.getX(0) < a.getX(1) && a.getX(1) < a.getX(2)));

  a.sort(*pr);

  REQUIRE(a.getX(0) == 1);
  REQUIRE(a.getX(1) == 3);
  REQUIRE(a.getX(2) == 5);
  REQUIRE(a.getX(3) == 9);   // part 1's single slot is untouched
}

TEST_CASE("assembly: exchangeShape swaps exactly the two addressed placements -- including placed/unplaced status",
          "[model][assembly]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  assembly_c a(&gt);
  a.addPlacement(1, 2, 3, 4);
  a.addNonPlacement();
  a.addPlacement(5, 6, 7, 8);

  a.exchangeShape(0, 1);

  REQUIRE_FALSE(a.isPlaced(0));

  REQUIRE(a.isPlaced(1));
  REQUIRE(a.getTransformation(1) == 1);
  REQUIRE(a.getX(1) == 2);
  REQUIRE(a.getY(1) == 3);
  REQUIRE(a.getZ(1) == 4);

  // slot 2 was not addressed, and must be untouched
  REQUIRE(a.isPlaced(2));
  REQUIRE(a.getTransformation(2) == 5);
  REQUIRE(a.getX(2) == 6);
  REQUIRE(a.getY(2) == 7);
  REQUIRE(a.getZ(2) == 8);
}

TEST_CASE("assembly: removePieces erases exactly the addressed range -- later placements shift down",
          "[model][assembly]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  assembly_c a(&gt);
  a.addPlacement(0, 100, 0, 0);  // slot 0: placed, kept
  a.addNonPlacement();           // slot 1: removed
  a.addNonPlacement();           // slot 2: removed
  a.addNonPlacement();           // slot 3: removed
  a.addPlacement(0, 200, 0, 0);  // slot 4: placed, shifts down to slot 1

  REQUIRE(a.placementCount() == 5);

  a.removePieces(1, 3);

  REQUIRE(a.placementCount() == 2);
  REQUIRE(a.getX(0) == 100);
  REQUIRE(a.getX(1) == 200);
}

TEST_CASE("assembly: addNonPlacedPieces inserts unplaced slots at the addressed position -- later placements "
          "shift up",
          "[model][assembly]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  assembly_c a(&gt);
  a.addPlacement(0, 100, 0, 0);
  a.addPlacement(0, 200, 0, 0);

  a.addNonPlacedPieces(1, 2);

  REQUIRE(a.placementCount() == 4);
  REQUIRE(a.getX(0) == 100);
  REQUIRE_FALSE(a.isPlaced(1));
  REQUIRE_FALSE(a.isPlaced(2));
  REQUIRE(a.getX(3) == 200);   // the piece that was slot 1 shifted to slot 3
}

TEST_CASE("assembly: createSpace assembles a voxel space matching the problem's result shape cell for cell",
          "[model][assembly]") {
  problem_c & pr = pelikanProblem();
  const assembly_c * assm = pr.getSavedSolution(0)->getAssembly();

  std::unique_ptr<voxel_c> assembled = assm->createSpace(pr);
  const voxel_c * result = getResultShape(pr);

  REQUIRE(assembled->getX() == result->getX());
  REQUIRE(assembled->getY() == result->getY());
  REQUIRE(assembled->getZ() == result->getZ());

  // a VX_FILLED result cell must be filled by the assembly and a VX_EMPTY
  // one must stay empty, but a VX_VARIABLE cell (a permitted hole) is
  // allowed to come out either way -- this particular solution leaves at
  // least one variable cell unfilled, so the per-cell check below has to
  // make that allowance rather than requiring an exact state match
  for (unsigned int z = 0; z < result->getZ(); z++)
    for (unsigned int y = 0; y < result->getY(); y++)
      for (unsigned int x = 0; x < result->getX(); x++) {
        INFO("cell " << x << "," << y << "," << z);
        switch (result->getState(x, y, z)) {
          case voxel_c::VX_FILLED:
            REQUIRE(assembled->getState(x, y, z) == voxel_c::VX_FILLED);
            break;
          case voxel_c::VX_EMPTY:
            REQUIRE(assembled->getState(x, y, z) == voxel_c::VX_EMPTY);
            break;
          case voxel_c::VX_VARIABLE:
            REQUIRE((assembled->getState(x, y, z) == voxel_c::VX_FILLED ||
                     assembled->getState(x, y, z) == voxel_c::VX_EMPTY));
            break;
        }
      }
}
