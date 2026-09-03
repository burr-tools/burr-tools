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
#include "shapehistory.h"

#include "../lib/puzzle.h"
#include "../lib/problem.h"
#include "../lib/voxel.h"
#include "../lib/gridtype.h"

#include <chrono>

shapeHistory_c::snapshot_c::~snapshot_c(void) {
  for (unsigned int i = 0; i < shapes.size(); i++)
    delete shapes[i];
}

shapeHistory_c::shapeHistory_c(void) :
  cursor(0),
  savedCursor(0),
  inStroke(false),
  strokeDirty(false),
  lastKind(AK_NONE),
  lastShape((unsigned int)-1),
  lastTimeMs(0)
{
}

shapeHistory_c::~shapeHistory_c(void) {
  clearSnapshots();
}

void shapeHistory_c::clearSnapshots(void) {
  for (unsigned int i = 0; i < snapshots.size(); i++)
    delete snapshots[i];
  snapshots.clear();
  cursor = 0;
  savedCursor = 0;
  inStroke = false;
  strokeDirty = false;
  lastKind = AK_NONE;
  lastShape = (unsigned int)-1;
  lastTimeMs = 0;
}

voxel_c * shapeHistory_c::cloneShape(const voxel_c * src) {
  voxel_c * v = src->getGridType()->getVoxel(src);
  v->setName(src->getName());
  return v;
}

int64_t shapeHistory_c::nowMs(void) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

void shapeHistory_c::reset(puzzle_c * puzzle) {
  clearSnapshots();
  snapshots.push_back(capture(puzzle, 0));
  cursor = 0;
  savedCursor = 0;
}

void shapeHistory_c::beginStroke(void) {
  inStroke = true;
  strokeDirty = false;
}

void shapeHistory_c::markStrokeDirty(void) {
  strokeDirty = true;
}

bool shapeHistory_c::endStroke(puzzle_c * puzzle, unsigned int selectedShape) {
  bool took = false;
  if (inStroke && strokeDirty) {
    record(puzzle, AK_GRID_PAINT, selectedShape);
    took = true;
  }
  inStroke = false;
  strokeDirty = false;
  return took;
}

void shapeHistory_c::record(puzzle_c * puzzle, actionKind_e kind, unsigned int selectedShape) {
  inStroke = false;
  strokeDirty = false;
  pushOrReplace(puzzle, kind, selectedShape);
}

bool shapeHistory_c::canCoalesce(actionKind_e kind, unsigned int selectedShape) const {
  if (cursor + 1 != snapshots.size())
    return false;
  if (lastKind != kind)
    return false;

  int64_t dt = nowMs() - lastTimeMs;

  if (kind == AK_GRID_PAINT)
    return selectedShape == lastShape && dt >= 0 && dt <= GRID_PAINT_COALESCE_MS;

  if (kind == AK_TRANSFORM)
    return dt >= 0 && dt <= TRANSFORM_COALESCE_MS;

  return false;
}

void shapeHistory_c::pushOrReplace(puzzle_c * puzzle, actionKind_e kind, unsigned int selectedShape) {
  snapshot_c * snap = capture(puzzle, selectedShape);

  if (canCoalesce(kind, selectedShape)) {
    delete snapshots[cursor];
    snapshots[cursor] = snap;
  } else {
    while (snapshots.size() > cursor + 1) {
      delete snapshots.back();
      snapshots.pop_back();
    }
    snapshots.push_back(snap);
    cursor = (unsigned int)(snapshots.size() - 1);

    while (snapshots.size() > MAX_UNDO + 1) {
      delete snapshots.front();
      snapshots.erase(snapshots.begin());
      if (cursor > 0)
        cursor--;
      if (savedCursor > 0)
        savedCursor--;
      else
        savedCursor = (unsigned int)-1;
    }
  }

  lastKind = kind;
  lastShape = selectedShape;
  lastTimeMs = nowMs();
}

bool shapeHistory_c::canUndo(void) const {
  return cursor > 0;
}

bool shapeHistory_c::canRedo(void) const {
  return cursor + 1 < snapshots.size();
}

unsigned int shapeHistory_c::undo(puzzle_c * puzzle) {
  if (!canUndo())
    return (unsigned int)-1;
  cursor--;
  lastKind = AK_NONE;
  restore(puzzle, snapshots[cursor]);
  return snapshots[cursor]->selectedShape;
}

unsigned int shapeHistory_c::redo(puzzle_c * puzzle) {
  if (!canRedo())
    return (unsigned int)-1;
  cursor++;
  lastKind = AK_NONE;
  restore(puzzle, snapshots[cursor]);
  return snapshots[cursor]->selectedShape;
}

void shapeHistory_c::markSaved(void) {
  savedCursor = cursor;
}

bool shapeHistory_c::isModifiedFromSave(void) const {
  return cursor != savedCursor;
}

shapeHistory_c::snapshot_c * shapeHistory_c::capture(const puzzle_c * puzzle, unsigned int selectedShape) const {
  snapshot_c * snap = new snapshot_c();
  snap->selectedShape = selectedShape;

  for (unsigned int i = 0; i < puzzle->getNumberOfShapes(); i++)
    snap->shapes.push_back(cloneShape(puzzle->getShape(i)));

  for (unsigned int p = 0; p < puzzle->getNumberOfProblems(); p++) {
    const problem_c * pr = puzzle->getProblem(p);
    problemSnap_c ps;
    ps.resultValid = pr->resultValid();
    ps.resultId = ps.resultValid ? pr->getResultId() : 0;

    for (unsigned int i = 0; i < pr->getNumberOfParts(); i++) {
      partSnap_c part;
      part.shapeId = pr->getShapeIdOfPart(i);
      part.min = pr->getPartMinimum(i);
      part.max = pr->getPartMaximum(i);
      unsigned short ng = pr->getNumberOfPartGroups(i);
      for (unsigned short g = 0; g < ng; g++) {
        groupSnap_c gs;
        gs.group = pr->getPartGroupId(i, g);
        gs.count = pr->getPartGroupCount(i, g);
        part.groups.push_back(gs);
      }
      ps.parts.push_back(part);
    }
    snap->problems.push_back(ps);
  }

  return snap;
}

void shapeHistory_c::restore(puzzle_c * puzzle, const snapshot_c * snap) const {

  std::vector<voxel_c*> clones;
  clones.reserve(snap->shapes.size());
  for (unsigned int i = 0; i < snap->shapes.size(); i++)
    clones.push_back(cloneShape(snap->shapes[i]));
  puzzle->adoptShapes(clones);

  // Check if problem structure actually needs restoration
  bool problemsNeedRestore = false;
  if (puzzle->getNumberOfProblems() != snap->problems.size()) {
    problemsNeedRestore = true;
  } else {
    for (unsigned int p = 0; p < puzzle->getNumberOfProblems(); p++) {
      const problem_c * pr = puzzle->getProblem(p);
      const problemSnap_c & ps = snap->problems[p];
      if (pr->resultValid() != ps.resultValid ||
          (ps.resultValid && pr->getResultId() != ps.resultId) ||
          pr->getNumberOfParts() != ps.parts.size()) {
        problemsNeedRestore = true;
        break;
      }
      for (unsigned int i = 0; i < ps.parts.size(); i++) {
        if (pr->getShapeIdOfPart(i) != ps.parts[i].shapeId ||
            pr->getPartMinimum(i) != ps.parts[i].min ||
            pr->getPartMaximum(i) != ps.parts[i].max) {
          problemsNeedRestore = true;
          break;
        }
      }
      if (problemsNeedRestore) break;
    }
  }

  if (problemsNeedRestore) {
    for (unsigned int p = 0; p < puzzle->getNumberOfProblems(); p++) {
      problem_c * pr = puzzle->getProblem(p);
      while (pr->getNumberOfParts() > 0)
        pr->setShapeMaximum(pr->getShapeIdOfPart(0), 0);
      if (pr->resultValid())
        pr->clearResult();
    }

    unsigned int np = puzzle->getNumberOfProblems();
    unsigned int ns = (unsigned int)snap->problems.size();
    unsigned int n = (np < ns) ? np : ns;

    for (unsigned int p = 0; p < n; p++) {
      problem_c * pr = puzzle->getProblem(p);
      const problemSnap_c & ps = snap->problems[p];

      for (unsigned int i = 0; i < ps.parts.size(); i++) {
        const partSnap_c & part = ps.parts[i];
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
    }
  }
}
