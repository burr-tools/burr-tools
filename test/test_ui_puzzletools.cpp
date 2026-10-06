/* Tests for btui's puzzle tools: Convert, Import assemblies and the Status
 * window's shape table -- the work behind three legacy menu dialogs.
 */
#include <catch2/catch_test_macros.hpp>

#include "../src/uicore/documentsession.h"
#include "../src/uicore/puzzletools.h"

#include "../src/lib/problem.h"
#include "../src/lib/puzzle.h"
#include "../src/lib/voxel.h"

#include <algorithm>

using namespace btui;

namespace {

  /* a solid w x h x d box as a new shape of p; returns its index */
  unsigned int addBox(puzzle_c & p, unsigned w, unsigned h, unsigned d) {
    unsigned int i = p.addShape(w, h, d);
    p.getShape(i)->setAll(voxel_c::VX_FILLED);
    return i;
  }
}

TEST_CASE("bricks can be converted, and conversion replaces the document", "[ui][puzzletools]") {
  auto targets = convertTargets(gridType_c::GT_BRICKS);
  REQUIRE_FALSE(targets.empty());
  CHECK(std::find(targets.begin(), targets.end(), gridType_c::GT_BRICKS) == targets.end());

  DocumentSession d;
  addBox(d.puzzle(), 2, 2, 2);
  REQUIRE(d.convert(targets.front()));
  CHECK(d.puzzle().getGridType()->getType() == targets.front());
  CHECK(d.puzzle().getNumberOfShapes() == 1);
  CHECK(d.isModified());
  CHECK_FALSE(d.canUndo());
}

TEST_CASE("assemblies become shapes, filtered and placed as asked", "[ui][puzzletools]") {
  DocumentSession d;
  REQUIRE(d.load("examples/SolidSixPieceBurrs.xmpuzzle").ok);
  puzzle_c & p = d.puzzle();
  REQUIRE(p.getNumberOfProblems() > 0);
  unsigned int src = 0;
  for (unsigned i = 0; i < p.getNumberOfProblems(); i++)
    if (p.getProblem(i)->getNumberOfSavedSolutions() > 1) { src = i; break; }
  REQUIRE(p.getProblem(src)->getNumberOfSavedSolutions() > 1);

  const unsigned shapesBefore = p.getNumberOfShapes();
  const unsigned problemsBefore = p.getNumberOfProblems();

  ImportAssembliesOptions o;
  o.sourceProblem = src;
  o.destination = ImportAssembliesOptions::Destination::NewProblem;
  o.rangeMin = 0;
  o.rangeMax = 2;
  const unsigned added = importAssemblies(p, o);
  CHECK(added > 0);
  CHECK(p.getNumberOfShapes() == shapesBefore + added);
  REQUIRE(p.getNumberOfProblems() == problemsBefore + 1);
  // the new problem uses the new shapes with the given range
  const problem_c * np = p.getProblem(problemsBefore);
  CHECK(np->getNumberOfParts() == added);
  CHECK(np->getPartMaximum(0) == 2);

  // identical assemblies are dropped by default: all added shapes differ
  for (unsigned a = shapesBefore; a < p.getNumberOfShapes(); a++)
    for (unsigned b = a + 1; b < p.getNumberOfShapes(); b++)
      CHECK_FALSE(p.getShape(a)->identicalWithRots(p.getShape(b), false, false));
}

TEST_CASE("the shape-size filter limits what is imported", "[ui][puzzletools]") {
  DocumentSession d;
  REQUIRE(d.load("examples/SolidSixPieceBurrs.xmpuzzle").ok);
  ImportAssembliesOptions o;
  o.shapeMin = 1000000;    // no assembly is that big
  CHECK(importAssemblies(d.puzzle(), o) == 0);
}

TEST_CASE("the status table reports counts, identity, connectivity and holes", "[ui][puzzletools]") {
  puzzle_c p(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
  addBox(p, 3, 3, 3);                     // S1: solid cube
  addBox(p, 3, 3, 3);                     // S2: the same
  unsigned shell = addBox(p, 3, 3, 3);    // S3: a hollow cube
  p.getShape(shell)->setState(1, 1, 1, voxel_c::VX_EMPTY);
  unsigned two = p.addShape(3, 1, 1);     // S4: two separate voxels
  p.getShape(two)->setState(0, 0, 0, voxel_c::VX_FILLED);
  p.getShape(two)->setState(2, 0, 0, voxel_c::VX_VARIABLE);

  ShapeStatusCalculator calc(p);
  ShapeStatus s1 = calc.next(), s2 = calc.next(), s3 = calc.next(), s4 = calc.next();
  CHECK(calc.done());

  CHECK(s1.fixed == 27);
  CHECK(s1.identicalShape == -1);
  CHECK(s2.identicalShape == 0);
  CHECK(s2.identicalMirror == 0);
  CHECK(s2.identicalComplete == 0);
  CHECK(s1.connectedFace);
  CHECK_FALSE(s1.holes3d);
  CHECK(s1.notchable);
  CHECK(s1.symmetryKnown);

  CHECK(s3.fixed == 26);
  CHECK(s3.holes3d);

  CHECK(s4.fixed == 1);
  CHECK(s4.variable == 1);
  CHECK_FALSE(s4.connectedFace);
  CHECK_FALSE(s4.connectedCorner);
}

TEST_CASE("removing shapes clears the solutions of problems that use them", "[ui][puzzletools]") {
  DocumentSession d;
  REQUIRE(d.load("examples/SolidSixPieceBurrs.xmpuzzle").ok);
  puzzle_c & p = d.puzzle();
  unsigned prob = 0;
  for (unsigned i = 0; i < p.getNumberOfProblems(); i++)
    if (p.getProblem(i)->getNumberOfSavedSolutions() > 0) { prob = i; break; }
  REQUIRE(p.getProblem(prob)->getNumberOfSavedSolutions() > 0);

  // a shape this problem uses
  unsigned used = 0;
  while (!p.getProblem(prob)->usesShape(used)) used++;

  const unsigned before = p.getNumberOfShapes();
  removeShapes(p, { used, used });       // duplicates are fine
  CHECK(p.getNumberOfShapes() == before - 1);
  CHECK(p.getProblem(prob)->getNumberOfSavedSolutions() == 0);
}
