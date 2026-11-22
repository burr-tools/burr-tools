/* Tests for btui's drawing colours: what the Drawing colour footer's Add,
 * Edit and Remove do to the puzzle (C09).
 */
#include <catch2/catch_test_macros.hpp>

#include "../src/uicore/documentsession.h"
#include "../src/uicore/drawcolours.h"

#include "../src/lib/problem.h"
#include "../src/lib/puzzle.h"
#include "../src/lib/voxel.h"

#include <algorithm>
#include <memory>
#include <vector>

using namespace btui;

namespace {

  std::unique_ptr<puzzle_c> bricks(void) {
    return std::make_unique<puzzle_c>(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
  }

  /* a 3 x 1 x 1 shape, filled, its voxels of colours a, b, c */
  unsigned shapeOfColours(puzzle_c & p, unsigned a, unsigned b, unsigned c) {
    unsigned s = p.addShape(3, 1, 1);
    voxel_c * v = p.getShape(s);
    v->setAll(voxel_c::VX_FILLED);
    v->setColor(0, a);
    v->setColor(1, b);
    v->setColor(2, c);
    return s;
  }
}

TEST_CASE("colours are named by their id: Default (C0), then C1, C2, ...", "[ui][drawcolours]") {
  CHECK(colourName(0) == "Default");
  CHECK(colourName(1) == "C1");
  CHECK(colourName(10) == "C10");
  CHECK(colourName(-1) == "Default");
}

TEST_CASE("adding appends a colour, allowed in every problem, until the library's limit", "[ui][drawcolours]") {
  auto p = bricks();
  unsigned prob = p->addProblem();
  CHECK(colourCount(*p) == 1);
  CHECK(addColour(*p, { 10, 20, 30 }) == 1);
  CHECK(colourCount(*p) == 2);
  CHECK(colourRgb(*p, 1) == Rgb8{ 10, 20, 30 });
  CHECK(p->getProblem(prob)->placementAllowed(1, 1));

  while (canAddColour(*p))
    REQUIRE(addColour(*p, { 1, 2, 3 }) > 0);
  CHECK(int(p->colorNumber()) == maxColours);
  CHECK(addColour(*p, { 1, 2, 3 }) == -1);       // no assert: refused
  CHECK(int(p->colorNumber()) == maxColours);
}

TEST_CASE("the quick add takes the first preset the puzzle lacks, then cycles", "[ui][drawcolours]") {
  auto p = bricks();
  const Rgb8 red{ 0xef, 0x44, 0x44 }, cyan{ 0x06, 0xb6, 0xd4 };
  CHECK(presetColour(*p) == red);                 // the mock's first
  addColour(*p, presetColour(*p));
  CHECK(presetColour(*p) == cyan);
  // one the user made by hand is skipped too
  auto q = bricks();
  addColour(*q, red);
  CHECK(presetColour(*q) == cyan);

  // the other nine, each new; then, all ten in, the set again by count
  std::vector<Rgb8> seen{ red };
  for (int i = 0; i < 9; i++) {
    const Rgb8 c = presetColour(*p);
    CHECK(std::find(seen.begin(), seen.end(), c) == seen.end());
    seen.push_back(c);
    addColour(*p, c);
  }
  CHECK(p->colorNumber() == 10);
  CHECK(presetColour(*p) == red);                 // 10 % 10
}

TEST_CASE("editing changes a colour's RGB; Default and no colour cannot be edited", "[ui][drawcolours]") {
  auto p = bricks();
  addColour(*p, { 1, 1, 1 });
  CHECK(editColour(*p, 1, { 9, 8, 7 }));
  CHECK(colourRgb(*p, 1) == Rgb8{ 9, 8, 7 });
  CHECK_FALSE(editColour(*p, 1, { 9, 8, 7 }));    // the same: no change
  CHECK_FALSE(editColour(*p, 0, { 9, 8, 7 }));
  CHECK_FALSE(editColour(*p, 2, { 9, 8, 7 }));
  CHECK(colourRgb(*p, 0) == Rgb8{});
}

TEST_CASE("removing a colour makes its voxels Default and moves the later ids down", "[ui][drawcolours]") {
  auto p = bricks();
  addColour(*p, { 1, 0, 0 });
  addColour(*p, { 0, 1, 0 });
  addColour(*p, { 0, 0, 1 });
  unsigned s = shapeOfColours(*p, 1, 2, 3);
  CHECK(removeColour(*p, 2));
  const voxel_c * v = p->getShape(s);
  CHECK(v->getColor(0) == 1);
  CHECK(v->getColor(1) == 0);                     // was the removed colour
  CHECK(v->getColor(2) == 2);                     // was 3
  CHECK(colourRgb(*p, 2) == Rgb8{ 0, 0, 1 });
  CHECK(colourCount(*p) == 3);

  CHECK_FALSE(removeColour(*p, 0));
  CHECK_FALSE(removeColour(*p, 3));
  CHECK_FALSE(removeColour(*p, -1));
}

TEST_CASE("a remove loses data undo cannot restore only for colours a solved problem's shapes use", "[ui][drawcolours]") {
  DocumentSession d;
  REQUIRE(d.load("examples/SolidSixPieceBurrs.xmpuzzle").ok);
  // a colour on a shape every solved problem uses, and one on no shape
  int used = -1, unused = -1;
  d.edit(puzzleHistory_c::AK_COLOR_PALETTE, 0, [&](puzzle_c & q) {
    used = addColour(q, { 255, 0, 0 });
    unused = addColour(q, { 0, 255, 0 });
    for (unsigned s = 0; s < q.getNumberOfShapes(); s++)
      for (unsigned j = 0; j < q.getShape(s)->getXYZ(); j++)
        if (q.getShape(s)->getState(j) != voxel_c::VX_EMPTY) {
          q.getShape(s)->setColor(j, unsigned(used));
          break;
        }
    return true;
  });
  const puzzle_c & p = d.puzzle();
  CHECK(removeColourLosesSolutions(p, used));
  CHECK_FALSE(removeColourLosesSolutions(p, unused));
  CHECK_FALSE(removeColourLosesSolutions(p, 0));
  CHECK_FALSE(removeColourLosesSolutions(p, 99));

  unsigned solved = 0;
  while (p.getProblem(solved)->getNumberOfSavedSolutions() == 0)
    solved++;
  d.edit(puzzleHistory_c::AK_COLOR_PALETTE, 0, [&](puzzle_c & q) { return removeColour(q, used); });
  CHECK(p.getProblem(solved)->getNumberOfSavedSolutions() == 0);

  // an unused colour goes without touching any solution
  auto q = bricks();
  addColour(*q, { 1, 2, 3 });
  shapeOfColours(*q, 0, 0, 0);
  CHECK_FALSE(removeColourLosesSolutions(*q, 1));
}

TEST_CASE("colour edits are undo steps that restore the list and the voxels", "[ui][drawcolours]") {
  DocumentSession d;
  d.edit(puzzleHistory_c::AK_COLOR_PALETTE, 0, [](puzzle_c & q) {
    addColour(q, { 10, 10, 10 });
    shapeOfColours(q, 1, 1, 0);
    return true;
  });
  const puzzle_c & p = d.puzzle();
  d.edit(puzzleHistory_c::AK_COLOR_PALETTE, 0, [](puzzle_c & q) { return removeColour(q, 1); });
  CHECK(p.colorNumber() == 0);
  CHECK(p.getShape(0)->getColor(0) == 0);
  d.undo();
  REQUIRE(p.colorNumber() == 1);
  CHECK(colourRgb(p, 1) == Rgb8{ 10, 10, 10 });
  CHECK(p.getShape(0)->getColor(0) == 1);
}
