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
#ifndef __PUZZLE_HISTORY_H__
#define __PUZZLE_HISTORY_H__

#include <deque>
#include <memory>
#include <set>
#include <stdint.h>
#include <vector>

class puzzle_c;
class voxel_c;

/**
 * Session-scoped undo/redo for all puzzle edits (Entities, Puzzle, and Solver
 * tabs). Snapshots live in RAM and are cleared when a new puzzle is loaded.
 *
 * Each snapshot captures: shape voxel data, colour palette, and per-problem
 * structure (piece counts, result, groups, colour constraints, name). Solutions
 * and disassemblies are intentionally excluded — those are large and are already
 * guarded by confirmation dialogs.
 *
 * Thread safety: record(), undo(), and redo() must only be called from the
 * main (GUI) thread. Callers are responsible for disabling undo/redo while a
 * solver thread is running.
 */
class puzzleHistory_c {

public:

  /** Coarse classification of what changed; controls tab affinity on restore. */
  enum actionKind_e {
    AK_NONE,
    AK_ENTITIES_GRID_PAINT,   ///< voxel paint drag on the Entities grid
    AK_ENTITIES_TRANSFORM,    ///< shape transform (rotate/mirror)
    AK_ENTITIES_STRUCTURAL,   ///< add/delete/copy/rename/weight/reorder shape
    AK_ENTITIES_CLICK_3D,     ///< voxel add/remove via 3D view on Entities tab
    AK_COLOR_PALETTE,         ///< add/remove/change a colour entry
    AK_PROBLEM_STRUCTURAL,    ///< problem add/delete/rename/piece-counts/result/constraints
    AK_SOLUTION,              ///< delete solutions or disassemblies
  };

  /** Which tab an undo/redo should switch to. */
  enum affectedTab_e {
    TAB_ENTITIES,
    TAB_PUZZLE,
    TAB_SOLVER,
  };

  struct undoResult_t {
    unsigned int selectedShape; ///< shape list index to restore; (unsigned)-1 if unknown
    affectedTab_e tab;          ///< tab to switch to after restore
  };

  static const unsigned int NO_SHAPE = (unsigned int)-1;
  static const unsigned int MAX_UNDO = 200; ///< absolute upper bound; runtime limit set via setMaxUndo()
  static const int GRID_PAINT_COALESCE_MS = 500;
  static const int TRANSFORM_COALESCE_MS  = 150;

  puzzleHistory_c(void);
  ~puzzleHistory_c(void);

  /** Drop all snapshots and record the current puzzle as baseline (step 0). */
  void reset(puzzle_c * puzzle);

  /** Stroke API — collapses an entire drag-paint gesture into one undo step. */
  void beginStroke(void);
  void markStrokeDirty(void);
  /** Commit the stroke as a snapshot if anything changed; returns true if taken. */
  bool endStroke(puzzle_c * puzzle, unsigned int selectedShape);

  /**
   * Record the current puzzle state. For AK_ENTITIES_GRID_PAINT and
   * AK_ENTITIES_TRANSFORM consecutive calls within the coalesce window on the
   * same shape replace the last snapshot rather than pushing a new one.
   */
  void record(puzzle_c * puzzle, actionKind_e kind,
              unsigned int selectedShape = NO_SHAPE);

  bool canUndo(void) const;
  bool canRedo(void) const;

  /** Restore previous snapshot into puzzle. Returns shape selection + tab. */
  undoResult_t undo(puzzle_c * puzzle);
  /** Re-apply the next snapshot. Returns shape selection + tab. */
  undoResult_t redo(puzzle_c * puzzle);

  /** Set the runtime undo stack limit (clamped to [1, MAX_UNDO]). */
  void setMaxUndo(unsigned int depth);

  /** Call after a successful save. */
  void markSaved(void);
  /** True if the current state differs from the last markSaved() position. */
  bool isModifiedFromSave(void) const;

  /** Pure mapping, no state — usable from tests. */
  static affectedTab_e tabForAction(actionKind_e kind);

private:

  struct groupSnap_t {
    unsigned short group;
    unsigned short count;
  };

  struct partSnap_t {
    unsigned int shapeId;
    unsigned int min;
    unsigned int max;
    std::vector<groupSnap_t> groups;
  };

  struct problemSnap_t {
    std::string name;
    bool resultValid;
    unsigned int resultId;
    std::set<uint32_t> colorConstraints;
    std::vector<partSnap_t> parts;
  };

  struct snapshot_t {
    std::vector<std::shared_ptr<const voxel_c>> shapes; ///< COW — unchanged shapes shared across snapshots
    std::vector<uint32_t> colors;                       ///< R | G<<8 | B<<16
    std::vector<problemSnap_t> problems;
    unsigned int selectedShape;
    actionKind_e kind;

    snapshot_t(void) : selectedShape(NO_SHAPE), kind(AK_NONE) {}
    ~snapshot_t(void) = default;
  };

  std::deque<std::unique_ptr<snapshot_t>> snapshots;
  unsigned int cursor;
  unsigned int savedCursor; ///< NO_SHAPE sentinel when save point was evicted

  unsigned int maxUndo_;

  bool inStroke;
  bool strokeDirty;

  actionKind_e lastKind;
  unsigned int lastShape;
  int64_t lastTimeMs;

  static std::unique_ptr<voxel_c> cloneShape(const voxel_c * src);
  static int64_t nowMs(void);

  std::unique_ptr<snapshot_t> capture(const puzzle_c * puzzle,
                                      unsigned int selectedShape,
                                      actionKind_e kind,
                                      const snapshot_t * prevSnap) const;
  void restore(puzzle_c * puzzle, const snapshot_t * snap) const;

  bool canCoalesce(actionKind_e kind, unsigned int selectedShape) const;
  void pushOrReplace(puzzle_c * puzzle, actionKind_e kind,
                     unsigned int selectedShape);
  void clearSnapshots(void);

  puzzleHistory_c(const puzzleHistory_c &) = delete;
  puzzleHistory_c & operator=(const puzzleHistory_c &) = delete;
};

#endif
