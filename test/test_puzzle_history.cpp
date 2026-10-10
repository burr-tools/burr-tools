/* Tests for puzzleHistory_c — session-scoped undo/redo engine.
 *
 * All tests work directly on puzzleHistory_c + puzzle_c from the lib.
 * No FLTK dependency; follows the same pattern as test_solveprogresscache.cpp.
 */
#include <catch2/catch_test_macros.hpp>

#include "../src/gui/puzzlehistory.h"

#include "../src/lib/gridtype.h"
#include "../src/lib/problem.h"
#include "../src/lib/puzzle.h"
#include "../src/lib/voxel.h"

#include <memory>

namespace {

std::unique_ptr<puzzle_c> makePuzzle() {
  return std::make_unique<puzzle_c>(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
}

/* Puzzle with numShapes empty brick shapes (1x1x1) and one problem. */
std::unique_ptr<puzzle_c> makePuzzleWithShapes(unsigned int numShapes) {
  auto p = makePuzzle();
  for (unsigned int i = 0; i < numShapes; i++)
    p->addShape(1, 1, 1);
  p->addProblem();
  return p;
}

} // namespace

// ---------------------------------------------------------------------------
// Core state machine
// ---------------------------------------------------------------------------

TEST_CASE("history is empty after reset: canUndo=false, canRedo=false", "[gui][history]") {
  auto puzzle = makePuzzle();
  puzzleHistory_c h;
  h.reset(puzzle.get());

  CHECK_FALSE(h.canUndo());
  CHECK_FALSE(h.canRedo());
}

TEST_CASE("record makes canUndo true", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);

  CHECK(h.canUndo());
}

TEST_CASE("undo restores shape voxel state", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  /* step 1: fill the voxel and record */
  puzzle->getShape(0)->set(0, 0, 0, voxel_c::VX_FILLED);
  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);

  /* step 2: record a further change so undo goes back to step 1, not baseline */
  puzzle->getShape(0)->set(0, 0, 0, voxel_c::VX_EMPTY);
  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);

  h.undo(puzzle.get());

  CHECK(puzzle->getShape(0)->get(0, 0, 0) == voxel_c::VX_FILLED);
}

TEST_CASE("redo after undo reapplies the state", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  puzzle->getShape(0)->set(0, 0, 0, voxel_c::VX_FILLED);
  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0);

  h.undo(puzzle.get());
  h.redo(puzzle.get());

  CHECK(puzzle->getShape(0)->get(0, 0, 0) == voxel_c::VX_FILLED);
}

TEST_CASE("canRedo becomes true after undo", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());
  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);

  h.undo(puzzle.get());

  CHECK(h.canRedo());
}

TEST_CASE("undo at oldest returns NO_SHAPE sentinel, leaves puzzle unchanged", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  /* No record — already at oldest. */
  auto res = h.undo(puzzle.get());

  CHECK(res.selectedShape == puzzleHistory_c::NO_SHAPE);
  CHECK_FALSE(h.canUndo());
}

TEST_CASE("redo at newest returns NO_SHAPE sentinel", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());
  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);

  /* Already at head. */
  auto res = h.redo(puzzle.get());

  CHECK(res.selectedShape == puzzleHistory_c::NO_SHAPE);
  CHECK_FALSE(h.canRedo());
}

TEST_CASE("record after partial undo truncates redo branch", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);
  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);
  h.undo(puzzle.get()); // cursor moves back one

  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0); // should truncate

  CHECK_FALSE(h.canRedo());
}

TEST_CASE("selectedShape is stored per snapshot and returned on undo", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(3);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 2);

  auto res = h.undo(puzzle.get());

  /* undo returns the shape from the action being undone (snapshot[1]),
   * so the UI keeps that shape selected even after restoring the baseline. */
  CHECK(res.selectedShape == 2u);
}

TEST_CASE("selectedShape from record step is returned on redo", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(3);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 2);
  h.undo(puzzle.get());

  auto res = h.redo(puzzle.get());

  CHECK(res.selectedShape == 2u);
}

// ---------------------------------------------------------------------------
// Tab affinity
// ---------------------------------------------------------------------------

TEST_CASE("tabForAction maps entities kinds to TAB_ENTITIES", "[gui][history]") {
  using H = puzzleHistory_c;
  CHECK(H::tabForAction(H::AK_ENTITIES_GRID_PAINT) == H::TAB_ENTITIES);
  CHECK(H::tabForAction(H::AK_ENTITIES_TRANSFORM)  == H::TAB_ENTITIES);
  CHECK(H::tabForAction(H::AK_ENTITIES_STRUCTURAL) == H::TAB_ENTITIES);
  CHECK(H::tabForAction(H::AK_ENTITIES_CLICK_3D)   == H::TAB_ENTITIES);
  CHECK(H::tabForAction(H::AK_COLOR_PALETTE)        == H::TAB_ENTITIES);
}

TEST_CASE("tabForAction maps problem kind to TAB_PUZZLE", "[gui][history]") {
  CHECK(puzzleHistory_c::tabForAction(puzzleHistory_c::AK_PROBLEM_STRUCTURAL)
        == puzzleHistory_c::TAB_PUZZLE);
}

TEST_CASE("markModified makes isModifiedFromSave return true", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());
  h.markSaved();
  CHECK_FALSE(h.isModifiedFromSave());
  h.markModified();
  CHECK(h.isModifiedFromSave());
}

TEST_CASE("undo result carries correct tab from recorded action", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  h.record(puzzle.get(), puzzleHistory_c::AK_PROBLEM_STRUCTURAL, 0);
  h.undo(puzzle.get()); // moves to reset snapshot (AK_NONE -> TAB_ENTITIES)
  auto res = h.redo(puzzle.get());

  CHECK(res.tab == puzzleHistory_c::TAB_PUZZLE);
}

// ---------------------------------------------------------------------------
// Stroke batching
// ---------------------------------------------------------------------------

TEST_CASE("stroke with no dirty mark takes no snapshot", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  h.beginStroke();
  /* no markStrokeDirty */
  bool took = h.endStroke(puzzle.get(), 0);

  CHECK_FALSE(took);
  CHECK_FALSE(h.canUndo());
}

TEST_CASE("stroke with dirty mark takes exactly one snapshot", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  h.beginStroke();
  h.markStrokeDirty();
  h.markStrokeDirty(); // multiple marks, still one snapshot
  bool took = h.endStroke(puzzle.get(), 0);

  CHECK(took);
  CHECK(h.canUndo());
  CHECK_FALSE(h.canRedo());
}

TEST_CASE("two strokes produce two undo steps", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  h.beginStroke(); h.markStrokeDirty(); h.endStroke(puzzle.get(), 0);
  h.beginStroke(); h.markStrokeDirty(); h.endStroke(puzzle.get(), 0);

  h.undo(puzzle.get());
  CHECK(h.canUndo()); // still one more
}

// ---------------------------------------------------------------------------
// Save / dirty tracking
// ---------------------------------------------------------------------------

TEST_CASE("isModifiedFromSave is false right after reset", "[gui][history]") {
  auto puzzle = makePuzzle();
  puzzleHistory_c h;
  h.reset(puzzle.get());

  CHECK_FALSE(h.isModifiedFromSave());
}

TEST_CASE("isModifiedFromSave becomes true after record", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);

  CHECK(h.isModifiedFromSave());
}

TEST_CASE("markSaved clears the modified flag", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());
  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);

  h.markSaved();

  CHECK_FALSE(h.isModifiedFromSave());
}

TEST_CASE("undo back to saved position clears modified flag", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);
  h.markSaved();
  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);

  CHECK(h.isModifiedFromSave());
  h.undo(puzzle.get());
  CHECK_FALSE(h.isModifiedFromSave());
}

TEST_CASE("a new edit after undoing past the save point stays modified", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);
  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);
  h.markSaved();
  h.undo(puzzle.get());
  // drops the redo tail holding the save point, and lands on its index
  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);

  CHECK(h.isModifiedFromSave());
  h.undo(puzzle.get());
  CHECK(h.isModifiedFromSave());
}

TEST_CASE("a paint right after saving does not coalesce into the saved step", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0);
  h.markSaved();
  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0);

  CHECK(h.isModifiedFromSave());
  h.undo(puzzle.get());
  CHECK_FALSE(h.isModifiedFromSave());
}

TEST_CASE("MAX_UNDO cap: oldest snapshot evicted, history stays bounded", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  /* Push MAX_UNDO+1 steps — requires eviction of the oldest. */
  for (unsigned int i = 0; i <= puzzleHistory_c::MAX_UNDO; i++)
    h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);

  /* We should still be able to undo MAX_UNDO times, not crash. */
  for (unsigned int i = 0; i < puzzleHistory_c::MAX_UNDO; i++) {
    REQUIRE(h.canUndo());
    h.undo(puzzle.get());
  }

  CHECK_FALSE(h.canUndo());
}

TEST_CASE("savedCursor sentinel: isModifiedFromSave true when save point evicted", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  /* Mark saved at the baseline. */
  h.markSaved();

  /* Evict the saved point by pushing MAX_UNDO+1 new steps. */
  for (unsigned int i = 0; i <= puzzleHistory_c::MAX_UNDO; i++)
    h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);

  /* Save point was evicted — must report modified. */
  CHECK(h.isModifiedFromSave());
}

TEST_CASE("a lower undo limit drops the oldest steps at once", "[gui][history]") {
  // Settings ▸ Undo history depth (C12 AC-08): choosing a smaller value trims
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());
  h.setMaxUndo(100);
  for (unsigned int i = 0; i < 60; i++)
    h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);

  h.setMaxUndo(25);
  unsigned int undos = 0;
  while (h.canUndo()) {
    h.undo(puzzle.get());
    undos++;
  }
  CHECK(undos == 25);
  // the redo side is untouched by the trim
  for (unsigned int i = 0; i < 25; i++) {
    REQUIRE(h.canRedo());
    h.redo(puzzle.get());
  }
  CHECK_FALSE(h.canRedo());
}

TEST_CASE("trimming keeps the save point, or drops it with the steps", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());
  h.setMaxUndo(100);
  for (unsigned int i = 0; i < 10; i++)
    h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);
  h.markSaved();
  for (unsigned int i = 0; i < 10; i++)
    h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);

  h.setMaxUndo(15);              // the save point (10 steps back) survives
  for (unsigned int i = 0; i < 10; i++)
    h.undo(puzzle.get());
  CHECK_FALSE(h.isModifiedFromSave());
  for (unsigned int i = 0; i < 10; i++)
    h.redo(puzzle.get());

  h.setMaxUndo(5);               // now it is gone with the steps before it
  CHECK(h.isModifiedFromSave());
}

TEST_CASE("the largest offered undo depth, 500, is not capped lower", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());
  h.setMaxUndo(500);
  for (unsigned int i = 0; i < 300; i++)
    h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);
  unsigned int undos = 0;
  while (h.canUndo()) {
    h.undo(puzzle.get());
    undos++;
  }
  CHECK(undos == 300);
}

// ---------------------------------------------------------------------------
// Extended snapshot: colour palette
// ---------------------------------------------------------------------------

TEST_CASE("colour palette is captured and restored on undo", "[gui][history]") {
  auto puzzle = makePuzzle();
  puzzle->addColor(255, 0, 0); // colour 0 = red
  puzzleHistory_c h;
  h.reset(puzzle.get());

  /* Change colour to blue, then record. */
  puzzle->changeColor(0, 0, 0, 255);
  h.record(puzzle.get(), puzzleHistory_c::AK_COLOR_PALETTE, puzzleHistory_c::NO_SHAPE);

  h.undo(puzzle.get());

  unsigned char r = 0, g = 0, b = 0;
  puzzle->getColor(0, &r, &g, &b);
  CHECK(r == 255);
  CHECK(g == 0);
  CHECK(b == 0);
}

// ---------------------------------------------------------------------------
// Extended snapshot: problem name
// ---------------------------------------------------------------------------

TEST_CASE("problem name is captured and restored on undo", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzle->getProblem(0)->setName("Alpha");
  puzzleHistory_c h;
  h.reset(puzzle.get());

  puzzle->getProblem(0)->setName("Beta");
  h.record(puzzle.get(), puzzleHistory_c::AK_PROBLEM_STRUCTURAL, puzzleHistory_c::NO_SHAPE);

  h.undo(puzzle.get());

  CHECK(puzzle->getProblem(0)->getName() == "Alpha");
}

// ---------------------------------------------------------------------------
// Extended snapshot: colour constraints
// ---------------------------------------------------------------------------

TEST_CASE("colour constraints are captured and restored on undo", "[gui][history]") {
  auto puzzle = makePuzzle();
  puzzle->addColor(255, 0, 0); // colour id 1
  puzzle->addColor(0, 255, 0); // colour id 2
  puzzle->addProblem();
  problem_c * pr = puzzle->getProblem(0);

  /* No constraint at baseline. */
  puzzleHistory_c h;
  h.reset(puzzle.get());

  /* Allow colour 1 piece in colour 2 result, then record. */
  pr->allowPlacement(1, 2);
  h.record(puzzle.get(), puzzleHistory_c::AK_PROBLEM_STRUCTURAL, puzzleHistory_c::NO_SHAPE);

  h.undo(puzzle.get());

  /* Constraint should have been removed. */
  CHECK_FALSE(puzzle->getProblem(0)->placementAllowed(1, 2));
}

// ---------------------------------------------------------------------------
// Extended snapshot: piece min/max ranges
// ---------------------------------------------------------------------------

TEST_CASE("piece min/max ranges are captured and restored on undo", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(2);
  problem_c * pr = puzzle->getProblem(0);
  pr->setShapeMaximum(0, 3);
  pr->setShapeMinimum(0, 1);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  pr->setShapeMaximum(0, 5);
  pr->setShapeMinimum(0, 2);
  h.record(puzzle.get(), puzzleHistory_c::AK_PROBLEM_STRUCTURAL, puzzleHistory_c::NO_SHAPE);

  h.undo(puzzle.get());

  unsigned int partId = puzzle->getProblem(0)->getPartIdForShape(0);
  CHECK(puzzle->getProblem(0)->getPartMaximum(partId) == 3u);
  CHECK(puzzle->getProblem(0)->getPartMinimum(partId) == 1u);
}

// ---------------------------------------------------------------------------
// Extended snapshot: piece group assignments
// ---------------------------------------------------------------------------

TEST_CASE("piece group assignments are captured and restored on undo", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(2);
  problem_c * pr = puzzle->getProblem(0);
  pr->setShapeMaximum(0, 2);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  unsigned int partId = pr->getPartIdForShape(0);
  pr->setPartGroup(partId, 1, 2);
  h.record(puzzle.get(), puzzleHistory_c::AK_PROBLEM_STRUCTURAL, puzzleHistory_c::NO_SHAPE);

  h.undo(puzzle.get());

  CHECK(puzzle->getProblem(0)->getNumberOfPartGroups(partId) == 0);
}

// ---------------------------------------------------------------------------
// Extended snapshot: result shape
// ---------------------------------------------------------------------------

TEST_CASE("result shape is captured and restored on undo", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(2);
  problem_c * pr = puzzle->getProblem(0);
  pr->setResultId(0);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  pr->setResultId(1);
  h.record(puzzle.get(), puzzleHistory_c::AK_PROBLEM_STRUCTURAL, puzzleHistory_c::NO_SHAPE);

  h.undo(puzzle.get());

  REQUIRE(puzzle->getProblem(0)->resultValid());
  CHECK(puzzle->getProblem(0)->getResultId() == 0u);
}

// ---------------------------------------------------------------------------
// Extended snapshot: problem add/remove (structural count changes)
// ---------------------------------------------------------------------------

TEST_CASE("adding a problem and undoing removes it", "[gui][history]") {
  auto puzzle = makePuzzle();
  puzzleHistory_c h;
  h.reset(puzzle.get());

  puzzle->addProblem();
  h.record(puzzle.get(), puzzleHistory_c::AK_PROBLEM_STRUCTURAL, puzzleHistory_c::NO_SHAPE);

  h.undo(puzzle.get());

  CHECK(puzzle->getNumberOfProblems() == 0u);
}

TEST_CASE("removing a problem and undoing restores it", "[gui][history]") {
  auto puzzle = makePuzzle();
  puzzle->addProblem();
  puzzle->getProblem(0)->setName("ToKeep");
  puzzleHistory_c h;
  h.reset(puzzle.get());

  puzzle->removeProblem(0);
  h.record(puzzle.get(), puzzleHistory_c::AK_PROBLEM_STRUCTURAL, puzzleHistory_c::NO_SHAPE);

  h.undo(puzzle.get());

  REQUIRE(puzzle->getNumberOfProblems() == 1u);
  CHECK(puzzle->getProblem(0)->getName() == "ToKeep");
}

// ---------------------------------------------------------------------------
// Extended snapshot: shape add/remove (structural count changes)
// ---------------------------------------------------------------------------

TEST_CASE("adding a shape and undoing removes it", "[gui][history]") {
  auto puzzle = makePuzzle();
  puzzleHistory_c h;
  h.reset(puzzle.get());

  puzzle->addShape(1, 1, 1);
  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);

  h.undo(puzzle.get());

  CHECK(puzzle->getNumberOfShapes() == 0u);
}

TEST_CASE("removing a shape and undoing restores it", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzle->getShape(0)->setName("MyShape");
  puzzleHistory_c h;
  h.reset(puzzle.get());

  puzzle->removeShape(0);
  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, puzzleHistory_c::NO_SHAPE);

  h.undo(puzzle.get());

  REQUIRE(puzzle->getNumberOfShapes() == 1u);
  CHECK(puzzle->getShape(0)->getName() == "MyShape");
}

// ---------------------------------------------------------------------------
// Extended snapshot: per-voxel color
// ---------------------------------------------------------------------------

TEST_CASE("per-voxel colour is captured and restored on undo", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzle->getShape(0)->setState(0, 0, 0, voxel_c::VX_FILLED);
  puzzle->getShape(0)->setColor(0, 0, 0, 3); // colour id 3
  puzzleHistory_c h;
  h.reset(puzzle.get());

  puzzle->getShape(0)->setColor(0, 0, 0, 7);
  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0);

  h.undo(puzzle.get());

  CHECK(puzzle->getShape(0)->getColor(0, 0, 0) == 3u);
}

// ---------------------------------------------------------------------------
// Extended snapshot: shape weight and name
// ---------------------------------------------------------------------------

TEST_CASE("shape weight is captured and restored on undo", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzle->getShape(0)->setWeight(5);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  puzzle->getShape(0)->setWeight(99);
  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);

  h.undo(puzzle.get());

  CHECK(puzzle->getShape(0)->getWeight() == 5);
}

TEST_CASE("shape name is captured and restored on undo", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzle->getShape(0)->setName("Original");
  puzzleHistory_c h;
  h.reset(puzzle.get());

  puzzle->getShape(0)->setName("Changed");
  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);

  h.undo(puzzle.get());

  CHECK(puzzle->getShape(0)->getName() == "Original");
}

// ---------------------------------------------------------------------------
// Coalescing
// ---------------------------------------------------------------------------

TEST_CASE("rapid grid paints on same shape coalesce into one undo step", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  /* Two rapid records of the same kind on the same shape — should coalesce. */
  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0);
  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0);

  h.undo(puzzle.get());
  CHECK_FALSE(h.canUndo()); // coalesced: one undo step returns to baseline
}

TEST_CASE("grid paints on different shapes do not coalesce", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(2);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0);
  h.record(puzzle.get(), puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 1);

  h.undo(puzzle.get());
  CHECK(h.canUndo()); // two distinct steps
}

TEST_CASE("endStroke resets coalesce window so next stroke is a new step", "[gui][history]") {
  auto puzzle = makePuzzleWithShapes(1);
  puzzleHistory_c h;
  h.reset(puzzle.get());

  h.beginStroke(); h.markStrokeDirty(); h.endStroke(puzzle.get(), 0);
  /* Second stroke — must NOT coalesce with first even though same shape. */
  h.beginStroke(); h.markStrokeDirty(); h.endStroke(puzzle.get(), 0);

  h.undo(puzzle.get());
  CHECK(h.canUndo()); // two distinct steps
}
