/* Tests for btui's shape list: what the Shapes card's New, Duplicate,
 * Delete, Rename, weight buttons and reorder do to the puzzle (C01).
 */
#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "../src/uicore/documentsession.h"
#include "../src/uicore/shapelist.h"

#include "../src/lib/problem.h"
#include "../src/lib/puzzle.h"
#include "../src/lib/voxel.h"

#include <memory>
#include <string>
#include <vector>

using namespace btui;

namespace {

  /* a brick puzzle of n shapes; shape k is (k+1) x 1 x 1, labelled "k" */
  std::unique_ptr<puzzle_c> shapes(unsigned n) {
    auto p = std::make_unique<puzzle_c>(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
    for (unsigned k = 0; k < n; k++) {
      unsigned i = p->addShape(k + 1, 1, 1);
      p->getShape(i)->setName(std::to_string(k));
    }
    return p;
  }

  /* the labels in list order: who is where */
  std::vector<std::string> labels(const puzzle_c & p) {
    std::vector<std::string> out;
    for (unsigned k = 0; k < p.getNumberOfShapes(); k++)
      out.push_back(p.getShape(k)->getName());
    return out;
  }

  /* a problem whose result is shape `result` and whose one piece is shape `piece` */
  unsigned problemUsing(puzzle_c & p, unsigned result, unsigned piece) {
    unsigned k = p.addProblem();
    p.getProblem(k)->setResultId(result);
    p.getProblem(k)->setShapeMaximum(piece, 1);
    p.getProblem(k)->setShapeMinimum(piece, 1);
    return k;
  }
}

TEST_CASE("shape ids are S1 ... SN by position", "[ui][shapelist]") {
  CHECK(shapeId(0) == "S1");
  CHECK(shapeId(9) == "S10");
}

TEST_CASE("a new shape copies the selected shape's size, else the grid's default, and is appended", "[ui][shapelist]") {
  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    CAPTURE(int(t));
    puzzle_c p(std::make_unique<gridType_c>(t));
    const unsigned d = defaultShapeSize(t);

    int a = newShape(p, -1);
    REQUIRE(a == 0);
    CHECK(p.getShape(0)->getX() == d);
    CHECK(p.getShape(0)->getY() == d);
    CHECK(p.getShape(0)->getZ() == d);
    CHECK(p.getShape(0)->countState(voxel_c::VX_EMPTY) == p.getShape(0)->getXYZ());

    // a row past the end (a view tearing down) is "nothing selected"
    CHECK(newShape(p, 5) == 1);
    CHECK(p.getShape(1)->getX() == d);
  }

  auto p = shapes(3);
  CHECK(newShape(*p, 1) == 3);
  CHECK(p->getShape(3)->getX() == 2);
  CHECK(p->getShape(3)->getName().empty());
  CHECK(p->getShape(3)->getWeight() == 1);
  CHECK(labels(*p) == std::vector<std::string>{ "0", "1", "2", "" });
}

TEST_CASE("legacy's default sizes: rhombic 5, spheres 3, the others 6", "[ui][shapelist]") {
  CHECK(defaultShapeSize(gridType_c::GT_BRICKS) == 6);
  CHECK(defaultShapeSize(gridType_c::GT_TRIANGULAR_PRISM) == 6);
  CHECK(defaultShapeSize(gridType_c::GT_SPHERES) == 3);
  CHECK(defaultShapeSize(gridType_c::GT_RHOMBIC) == 5);
  CHECK(defaultShapeSize(gridType_c::GT_TETRA_OCTA) == 6);
}

TEST_CASE("a duplicate copies voxels, size, label and weight, appended as the last shape", "[ui][shapelist]") {
  auto p = shapes(3);
  voxel_c * src = p->getShape(1);
  src->setState(1, 0, 0, voxel_c::VX_FILLED);
  src->setWeight(4);

  CHECK(duplicateShape(*p, 1) == 3);
  const voxel_c * copy = p->getShape(3);
  CHECK(copy != src);
  CHECK(copy->getX() == 2);
  CHECK(copy->getState(1, 0, 0) == voxel_c::VX_FILLED);
  CHECK(copy->getName() == "1");
  CHECK(copy->getWeight() == 4);
  // the existing shapes keep their positions, so their ids
  CHECK(labels(*p) == std::vector<std::string>{ "0", "1", "2", "1" });

  CHECK(duplicateShape(*p, -1) == -1);
  CHECK(duplicateShape(*p, 4) == -1);
  CHECK(p->getNumberOfShapes() == 4);
}

TEST_CASE("delete needs a second shape, renumbers the later ones and selects the nearest", "[ui][shapelist]") {
  auto one = shapes(1);
  CHECK_FALSE(canDeleteShape(*one, 0));
  CHECK(deleteShape(*one, 0) == -1);
  CHECK(one->getNumberOfShapes() == 1);

  auto p = shapes(4);
  CHECK_FALSE(canDeleteShape(*p, -1));
  CHECK_FALSE(canDeleteShape(*p, 4));
  CHECK(deleteShape(*p, -1) == -1);

  // the middle: the next shape moves up into the place, and is selected
  CHECK(deleteShape(*p, 1) == 1);
  CHECK(labels(*p) == std::vector<std::string>{ "0", "2", "3" });
  // the last: the new last one is selected
  CHECK(deleteShape(*p, 2) == 1);
  CHECK(labels(*p) == std::vector<std::string>{ "0", "2" });
}

TEST_CASE("the problems' references follow their shapes through a delete", "[ui][shapelist]") {
  auto p = shapes(4);
  unsigned k = problemUsing(*p, 3, 2);     // result "3", piece "2"
  REQUIRE(deleteShape(*p, 0) == 0);
  const problem_c * pr = p->getProblem(k);
  REQUIRE(pr->resultValid());
  CHECK(p->getShape(pr->getResultId())->getName() == "3");
  REQUIRE(pr->getNumberOfParts() == 1);
  CHECK(p->getShape(pr->getShapeIdOfPart(0))->getName() == "2");
}

TEST_CASE("a delete loses data undo cannot restore only for shapes a solved problem uses", "[ui][shapelist]") {
  auto fresh = shapes(3);
  problemUsing(*fresh, 0, 1);
  // used, but nothing solved yet: undo brings everything back
  CHECK_FALSE(deleteLosesSolutions(*fresh, 0));
  CHECK_FALSE(deleteLosesSolutions(*fresh, 1));
  CHECK_FALSE(deleteLosesSolutions(*fresh, -1));
  CHECK_FALSE(deleteLosesSolutions(*fresh, 3));

  DocumentSession d;
  REQUIRE(d.load("examples/SolidSixPieceBurrs.xmpuzzle").ok);
  const puzzle_c & p = d.puzzle();
  unsigned prob = 0;
  while (prob < p.getNumberOfProblems() && p.getProblem(prob)->getNumberOfSavedSolutions() == 0)
    prob++;
  REQUIRE(prob < p.getNumberOfProblems());
  bool sawUsed = false;
  for (unsigned s = 0; s < p.getNumberOfShapes(); s++) {
    bool usedBySolved = false;
    for (unsigned k = 0; k < p.getNumberOfProblems(); k++) {
      const problem_c * pr = p.getProblem(k);
      if (pr->usesShape(s) && (pr->getNumberOfSavedSolutions() > 0 || pr->getSolveState() == SS_SOLVED ||
                               pr->getSolveState() == SS_SOLVING))
        usedBySolved = true;
    }
    CAPTURE(s);
    CHECK(deleteLosesSolutions(p, int(s)) == usedBySolved);
    sawUsed = sawUsed || usedBySolved;
  }
  CHECK(sawUsed);
}

TEST_CASE("a move shifts the shapes between, and references follow", "[ui][shapelist]") {
  auto p = shapes(5);
  unsigned k = problemUsing(*p, 0, 3);     // result "0", piece "3"

  CHECK(moveShape(*p, 0, 3));              // later
  CHECK(labels(*p) == std::vector<std::string>{ "1", "2", "3", "0", "4" });
  CHECK(moveShape(*p, 4, 1));              // earlier
  CHECK(labels(*p) == std::vector<std::string>{ "1", "4", "2", "3", "0" });

  const problem_c * pr = p->getProblem(k);
  CHECK(p->getShape(pr->getResultId())->getName() == "0");
  CHECK(p->getShape(pr->getShapeIdOfPart(0))->getName() == "3");

  // past either end: clamped
  CHECK(moveShape(*p, 1, 99));
  CHECK(labels(*p).back() == "4");
  CHECK(moveShape(*p, 4, -3));
  CHECK(labels(*p).front() == "4");

  // nowhere to go, or no such shape
  CHECK_FALSE(moveShape(*p, 2, 2));
  CHECK_FALSE(moveShape(*p, 0, -1));
  CHECK_FALSE(moveShape(*p, -1, 2));
  CHECK_FALSE(moveShape(*p, 5, 0));
}

TEST_CASE("rename sets the label, and reports whether it changed", "[ui][shapelist]") {
  auto p = shapes(2);
  CHECK(renameShape(*p, 0, "Arm"));
  CHECK(p->getShape(0)->getName() == "Arm");
  CHECK_FALSE(renameShape(*p, 0, "Arm"));
  CHECK(renameShape(*p, 0, ""));              // empty: no label
  CHECK(p->getShape(0)->getName().empty());
  CHECK_FALSE(renameShape(*p, 2, "x"));
  CHECK_FALSE(renameShape(*p, -1, "x"));
}

TEST_CASE("the weight steps by the given amount and stops at 0", "[ui][shapelist]") {
  auto p = shapes(1);
  REQUIRE(p->getShape(0)->getWeight() == 1);
  CHECK(changeWeight(*p, 0, +1));
  CHECK(p->getShape(0)->getWeight() == 2);
  CHECK(changeWeight(*p, 0, -2));
  CHECK(p->getShape(0)->getWeight() == 0);
  CHECK_FALSE(changeWeight(*p, 0, -1));
  CHECK(p->getShape(0)->getWeight() == 0);
  CHECK_FALSE(changeWeight(*p, 1, +1));
  CHECK_FALSE(changeWeight(*p, 0, 0));
}

TEST_CASE("each list operation is one undo step, and undo restores order and references", "[ui][shapelist]") {
  DocumentSession d;
  // the starting state is an undo step of its own, so the steps below undo to it
  d.edit(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0, [](puzzle_c & q) {
    for (unsigned k = 0; k < 3; k++)
      q.getShape(q.addShape(k + 1, 1, 1))->setName(std::to_string(k));
    problemUsing(q, 2, 0);
    return true;
  });
  const puzzle_c & p = d.puzzle();
  auto step = [&](auto f) { return d.edit(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0, f); };

  REQUIRE(step([](puzzle_c & q) { return moveShape(q, 2, 0); }));
  REQUIRE(step([](puzzle_c & q) { return deleteShape(q, 1) >= 0; }));
  REQUIRE(step([](puzzle_c & q) { return duplicateShape(q, 0) >= 0; }));
  CHECK(labels(p) == std::vector<std::string>{ "2", "1", "2" });
  // a no-op records nothing
  CHECK_FALSE(step([](puzzle_c & q) { return changeWeight(q, 0, 0); }));

  d.undo();
  CHECK(labels(p) == std::vector<std::string>{ "2", "1" });
  d.undo();
  CHECK(labels(p) == std::vector<std::string>{ "2", "0", "1" });
  d.undo();
  CHECK(labels(p) == std::vector<std::string>{ "0", "1", "2" });
  const problem_c * pr = p.getProblem(0);
  CHECK(p.getShape(pr->getResultId())->getName() == "2");
  CHECK(p.getShape(pr->getShapeIdOfPart(0))->getName() == "0");
  d.undo();
  CHECK(p.getNumberOfShapes() == 0);
  CHECK_FALSE(d.canUndo());
}
