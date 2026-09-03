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
#ifndef __SHAPE_HISTORY_H__
#define __SHAPE_HISTORY_H__

#include <vector>
#include <stdint.h>

class puzzle_c;
class voxel_c;

/**
 * Session-scoped undo/redo for Entities-tab shape edits.
 * Snapshots live in RAM only and are cleared when a new puzzle is loaded.
 */
class shapeHistory_c {

public:

  enum actionKind_e {
    AK_NONE,
    AK_GRID_PAINT,
    AK_TRANSFORM,
    AK_STRUCTURAL,
    AK_CLICK_3D
  };

  shapeHistory_c(void);
  ~shapeHistory_c(void);

  /** Drop all snapshots and take snapshot 0 from the live puzzle. */
  void reset(puzzle_c * puzzle);

  void beginStroke(void);
  void markStrokeDirty(void);
  /** Snapshot the stroke if anything changed. Returns true if a snapshot was taken. */
  bool endStroke(puzzle_c * puzzle, unsigned int selectedShape);

  /**
   * Record the current puzzle as a new undo step (or coalesce with the last one).
   * selectedShape is stored so undo/redo can restore the list selection.
   */
  void record(puzzle_c * puzzle, actionKind_e kind, unsigned int selectedShape);

  bool canUndo(void) const;
  bool canRedo(void) const;

  /** Restore the previous snapshot. Returns the selected shape index, or (unsigned)-1. */
  unsigned int undo(puzzle_c * puzzle);
  unsigned int redo(puzzle_c * puzzle);

  /** Call after a successful save so undo back to this point is not "dirty". */
  void markSaved(void);
  bool isModifiedFromSave(void) const;

private:

  struct groupSnap_c {
    unsigned short group;
    unsigned short count;
  };

  struct partSnap_c {
    unsigned int shapeId;
    unsigned int min;
    unsigned int max;
    std::vector<groupSnap_c> groups;
  };

  struct problemSnap_c {
    bool resultValid;
    unsigned int resultId;
    std::vector<partSnap_c> parts;
  };

  struct snapshot_c {
    std::vector<voxel_c*> shapes;
    std::vector<problemSnap_c> problems;
    unsigned int selectedShape;
    snapshot_c(void) : selectedShape((unsigned int)-1) {}
    ~snapshot_c(void);
  };

  static const unsigned int MAX_UNDO = 200;
  static const int GRID_PAINT_COALESCE_MS = 500;
  static const int TRANSFORM_COALESCE_MS = 150;

  std::vector<snapshot_c*> snapshots;
  unsigned int cursor;
  unsigned int savedCursor;
  bool inStroke;
  bool strokeDirty;
  actionKind_e lastKind;
  unsigned int lastShape;
  int64_t lastTimeMs;

  static voxel_c * cloneShape(const voxel_c * src);
  static int64_t nowMs(void);

  snapshot_c * capture(const puzzle_c * puzzle, unsigned int selectedShape) const;
  void restore(puzzle_c * puzzle, const snapshot_c * snap) const;
  void clearSnapshots(void);
  bool canCoalesce(actionKind_e kind, unsigned int selectedShape) const;
  void pushOrReplace(puzzle_c * puzzle, actionKind_e kind, unsigned int selectedShape);

  // no copying
  shapeHistory_c(const shapeHistory_c&);
  void operator=(const shapeHistory_c&);
};

#endif
