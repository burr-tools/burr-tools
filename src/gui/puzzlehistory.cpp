/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */
#include "puzzlehistory.h"

#include "../lib/gridtype.h"
#include "../lib/problem.h"
#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <chrono>

puzzleHistory_c::puzzleHistory_c(void) :
  cursor(0),
  savedCursor(0),
  savedCursorValid(true),
  maxUndo_(MAX_UNDO),
  inStroke(false),
  strokeDirty(false),
  strokeKind(AK_ENTITIES_GRID_PAINT),
  lastKind(AK_NONE),
  lastShape(NO_SHAPE),
  lastTimeMs(0)
{
}

void puzzleHistory_c::setMaxUndo(unsigned int depth) {
  if (depth < 1) depth = 1;
  if (depth > MAX_UNDO) depth = MAX_UNDO;
  maxUndo_ = depth;
}

puzzleHistory_c::~puzzleHistory_c(void) {
  clearSnapshots();
}

void puzzleHistory_c::clearSnapshots(void) {
  snapshots.clear();
  cursor = 0;
  savedCursor = 0;
  savedCursorValid = true;
  inStroke = false;
  strokeDirty = false;
  strokeKind = AK_ENTITIES_GRID_PAINT;
  lastKind = AK_NONE;
  lastShape = NO_SHAPE;
  lastTimeMs = 0;
}

int64_t puzzleHistory_c::nowMs(void) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

static std::unique_ptr<voxel_c> cloneShapeUnique(const voxel_c * src) {
  std::unique_ptr<voxel_c> v(src->getGridType()->getVoxel(src));
  v->setName(src->getName());
  return v;
}

std::unique_ptr<voxel_c> puzzleHistory_c::cloneShape(const voxel_c * src) {
  return cloneShapeUnique(src);
}

// ---------------------------------------------------------------------------
// capture / restore
// ---------------------------------------------------------------------------

std::unique_ptr<puzzleHistory_c::snapshot_t>
puzzleHistory_c::capture(const puzzle_c * puzzle,
                         unsigned int selectedShape,
                         actionKind_e kind,
                         const snapshot_t * prevSnap) const {
  auto snap = std::make_unique<snapshot_t>();
  snap->selectedShape = selectedShape;
  snap->kind = kind;

  // COW shape sharing: only clone shapes that could have changed.
  // AK_NONE and AK_ENTITIES_STRUCTURAL may change any shape → clone all.
  // AK_COLOR_PALETTE, AK_PROBLEM_STRUCTURAL → no shape changes → share all.
  // Paint/transform/click → only selectedShape changed → share the rest.
  const bool cloneAll  = (kind == AK_NONE || kind == AK_ENTITIES_STRUCTURAL);
  const bool cloneNone = !cloneAll &&
                         (kind == AK_COLOR_PALETTE ||
                          kind == AK_PROBLEM_STRUCTURAL);

  for (unsigned int i = 0; i < puzzle->getNumberOfShapes(); i++) {
    const bool doClone = cloneAll || (!cloneNone && i == selectedShape);
    if (!doClone && prevSnap && i < prevSnap->shapes.size()) {
      snap->shapes.push_back(prevSnap->shapes[i]); // share immutable copy
    } else {
      snap->shapes.push_back(std::shared_ptr<const voxel_c>(cloneShapeUnique(puzzle->getShape(i))));
    }
  }

  for (unsigned int i = 0; i < puzzle->colorNumber(); i++) {
    unsigned char r, g, b;
    puzzle->getColor(i, &r, &g, &b);
    snap->colors.push_back((uint32_t)r | (uint32_t)g << 8 | (uint32_t)b << 16);
  }

  for (unsigned int p = 0; p < puzzle->getNumberOfProblems(); p++) {
    const problem_c * pr = puzzle->getProblem(p);
    problemSnap_t ps;
    ps.name = pr->getName();
    ps.resultValid = pr->resultValid();
    ps.resultId = ps.resultValid ? pr->getResultId() : 0;
    ps.colorConstraints = pr->getColorConstraints();

    for (unsigned int i = 0; i < pr->getNumberOfParts(); i++) {
      partSnap_t part;
      part.shapeId = pr->getShapeIdOfPart(i);
      part.min     = pr->getPartMinimum(i);
      part.max     = pr->getPartMaximum(i);
      unsigned short ng = pr->getNumberOfPartGroups(i);
      for (unsigned short g = 0; g < ng; g++) {
        groupSnap_t gs;
        gs.group = pr->getPartGroupId(i, g);
        gs.count = pr->getPartGroupCount(i, g);
        part.groups.push_back(gs);
      }
      ps.parts.push_back(part);
    }
    snap->problems.push_back(std::move(ps));
  }

  return snap;
}

void puzzleHistory_c::restore(puzzle_c * puzzle, const snapshot_t * snap) const {
  // --- shapes ---
  std::vector<std::unique_ptr<voxel_c>> clones;
  clones.reserve(snap->shapes.size());
  for (unsigned int i = 0; i < snap->shapes.size(); i++)
    clones.push_back(cloneShape(snap->shapes[i].get()));
  puzzle->adoptShapes(std::move(clones));

  // --- colour palette ---
  puzzle->adoptColors(snap->colors);

  // --- problems --- adjust count first so the restore loop always sees the right size ---
  while (puzzle->getNumberOfProblems() > snap->problems.size())
    puzzle->removeProblem(puzzle->getNumberOfProblems() - 1);
  while (puzzle->getNumberOfProblems() < snap->problems.size())
    puzzle->addProblem();

  for (unsigned int p = 0; p < snap->problems.size(); p++) {
    problem_c * pr = puzzle->getProblem(p);
    const problemSnap_t & ps = snap->problems[p];

    pr->setName(ps.name);

    // Collect part shape-IDs before clearing: the while-loop pattern of
    // calling getShapeIdOfPart(0) inside the loop is safe only if the
    // problem part list and the current shapes list are in sync, which is
    // not guaranteed immediately after adoptShapes (especially for
    // AK_ENTITIES_STRUCTURAL restores where the shape count changed).
    std::vector<unsigned int> partIds;
    partIds.reserve(pr->getNumberOfParts());
    for (unsigned int i = 0; i < pr->getNumberOfParts(); i++)
      partIds.push_back(pr->getShapeIdOfPart(i));
    for (unsigned int sid : partIds)
      pr->setShapeMaximum(sid, 0);
    if (pr->resultValid())
      pr->clearResult();

    // restore parts
    for (unsigned int i = 0; i < ps.parts.size(); i++) {
      const partSnap_t & part = ps.parts[i];
      if (part.shapeId >= puzzle->getNumberOfShapes() || part.max == 0)
        continue;
      pr->setShapeMaximum(part.shapeId, part.max);
      pr->setShapeMinimum(part.shapeId, part.min);
      unsigned int partId = pr->getPartIdForShape(part.shapeId);
      for (unsigned int g = 0; g < part.groups.size(); g++)
        pr->setPartGroup(partId, part.groups[g].group, part.groups[g].count);
    }

    if (ps.resultValid && ps.resultId < puzzle->getNumberOfShapes())
      pr->setResultId(ps.resultId);

    pr->setColorConstraints(ps.colorConstraints);
  }
}

// ---------------------------------------------------------------------------
// public API
// ---------------------------------------------------------------------------

void puzzleHistory_c::reset(puzzle_c * puzzle) {
  clearSnapshots();
  snapshots.push_back(capture(puzzle, NO_SHAPE, AK_NONE, nullptr));
  cursor = 0;
  savedCursor = 0;
}

void puzzleHistory_c::beginStroke(actionKind_e kind) {
  inStroke = true;
  strokeDirty = false;
  strokeKind = kind;
}

void puzzleHistory_c::markStrokeDirty(void) {
  strokeDirty = true;
}

bool puzzleHistory_c::endStroke(puzzle_c * puzzle, unsigned int selectedShape) {
  bool took = false;
  if (inStroke && strokeDirty) {
    record(puzzle, strokeKind, selectedShape);
    /* A completed stroke gesture is a discrete undo step; the next gesture
     * must not coalesce with it even if it starts immediately. */
    lastKind = AK_NONE;
    took = true;
  }
  inStroke = false;
  strokeDirty = false;
  return took;
}

bool puzzleHistory_c::canCoalesce(actionKind_e kind,
                                  unsigned int selectedShape) const {
  if (cursor + 1 != snapshots.size()) return false;
  if (lastKind != kind) return false;

  int64_t dt = nowMs() - lastTimeMs;

  if (kind == AK_ENTITIES_GRID_PAINT)
    return selectedShape == lastShape && dt >= 0 && dt <= GRID_PAINT_COALESCE_MS;

  if (kind == AK_ENTITIES_TRANSFORM)
    return selectedShape == lastShape && dt >= 0 && dt <= TRANSFORM_COALESCE_MS;

  return false;
}

void puzzleHistory_c::pushOrReplace(puzzle_c * puzzle, actionKind_e kind,
                                    unsigned int selectedShape) {
  if (canCoalesce(kind, selectedShape)) {
    // Replace current snapshot; share unchanged shapes from the one before it.
    const snapshot_t * prevSnap = (cursor > 0) ? snapshots[cursor - 1].get() : nullptr;
    snapshots[cursor] = capture(puzzle, selectedShape, kind, prevSnap);
  } else {
    /* A save point in the redo tail goes with it: the new step would land on
     * its index, and the document would read as saved though it differs. */
    if (savedCursorValid && savedCursor > cursor)
      savedCursorValid = false;
    while (snapshots.size() > cursor + 1)
      snapshots.pop_back();

    const snapshot_t * prevSnap = snapshots.empty() ? nullptr : snapshots[cursor].get();
    snapshots.push_back(capture(puzzle, selectedShape, kind, prevSnap));
    cursor = (unsigned int)(snapshots.size() - 1);

    while (snapshots.size() > maxUndo_ + 1) {
      snapshots.pop_front(); // O(1) with deque
      if (cursor > 0) cursor--;
      if (savedCursorValid) {
        if (savedCursor > 0) savedCursor--;
        else savedCursorValid = false; // save point evicted from front
      }
    }
  }

  lastKind = kind;
  lastShape = selectedShape;
  lastTimeMs = nowMs();
}

void puzzleHistory_c::record(puzzle_c * puzzle, actionKind_e kind,
                              unsigned int selectedShape) {
  inStroke = false;
  strokeDirty = false;
  pushOrReplace(puzzle, kind, selectedShape);
}

bool puzzleHistory_c::canUndo(void) const {
  return cursor > 0;
}

bool puzzleHistory_c::canRedo(void) const {
  return cursor + 1 < snapshots.size();
}

puzzleHistory_c::undoResult_t puzzleHistory_c::undo(puzzle_c * puzzle) {
  if (!canUndo())
    return { NO_SHAPE, TAB_ENTITIES };
  cursor--;
  lastKind = AK_NONE;
  restore(puzzle, snapshots[cursor].get());
  /* Tab and shape come from the action we just undid (cursor+1), not from
   * the baseline we restored to — the baseline has kind=AK_NONE which maps
   * to TAB_ENTITIES regardless of which tab the user was working on. */
  return { snapshots[cursor + 1]->selectedShape,
           tabForAction(snapshots[cursor + 1]->kind) };
}

puzzleHistory_c::undoResult_t puzzleHistory_c::redo(puzzle_c * puzzle) {
  if (!canRedo())
    return { NO_SHAPE, TAB_ENTITIES };
  cursor++;
  lastKind = AK_NONE;
  restore(puzzle, snapshots[cursor].get());
  return { snapshots[cursor]->selectedShape,
           tabForAction(snapshots[cursor]->kind) };
}

void puzzleHistory_c::markSaved(void) {
  savedCursor = cursor;
  savedCursorValid = true;
  // the next edit must not coalesce into the saved snapshot, which would
  // change it in place and leave the document reading as saved
  lastKind = AK_NONE;
}

bool puzzleHistory_c::isModifiedFromSave(void) const {
  return !savedCursorValid || cursor != savedCursor;
}

void puzzleHistory_c::markModified(void) {
  savedCursorValid = false;
}

puzzleHistory_c::affectedTab_e
puzzleHistory_c::tabForAction(actionKind_e kind) {
  switch (kind) {
  case AK_ENTITIES_GRID_PAINT:
  case AK_ENTITIES_TRANSFORM:
  case AK_ENTITIES_STRUCTURAL:
  case AK_ENTITIES_CLICK_3D:
  case AK_COLOR_PALETTE:
    return TAB_ENTITIES;
  case AK_PROBLEM_STRUCTURAL:
    return TAB_PUZZLE;
  default:
    return TAB_ENTITIES;
  }
}
