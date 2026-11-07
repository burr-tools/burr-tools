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

#include "../lib/problem.h"

#include <deque>
#include <memory>
#include <set>
#include <stdint.h>
#include <string>
#include <utility>
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
 * guarded by confirmation dialogs. The exception is what an edit itself
 * discarded (keepDiscarded): the problems' solve records, moved into that
 * step, not copied, so that undoing it puts them back. Only the redesigned
 * GUI hands any over; for the legacy GUI nothing changes.
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
    AK_PROBLEM_COUNT,         ///< a piece's count, stepped or typed: a burst on one piece is one step
  };

  /** Which tab an undo/redo should switch to. */
  enum affectedTab_e {
    TAB_ENTITIES,
    TAB_PUZZLE,
    TAB_SOLVER,
  };

  static const unsigned int NO_SHAPE = (unsigned int)-1;
  static const unsigned int NO_PROBLEM = (unsigned int)-1;

  /** The Puzzle workspace's selection, as a step found it or left it. */
  struct selection_t {
    unsigned int problem = NO_PROBLEM;  ///< NO_PROBLEM: the step says nothing about the selection
    unsigned int piece = NO_SHAPE;
  };

  struct undoResult_t {
    unsigned int selectedShape; ///< shape list index to restore; (unsigned)-1 if unknown
    affectedTab_e tab;          ///< tab to switch to after restore
    selection_t selection = {}; ///< where the step puts the Puzzle workspace's selection
  };

  static const unsigned int MAX_UNDO = 500; ///< absolute upper bound (the redesigned Settings offer 500); runtime limit set via setMaxUndo()
  static const int GRID_PAINT_COALESCE_MS = 500;
  static const int TRANSFORM_COALESCE_MS  = 150;
  /* a held stepper repeats every 80 ms; a number typed in two goes commits
   * after each pause, 350 ms plus the gap between the keys */
  static const int PROBLEM_COUNT_COALESCE_MS = 800;

  puzzleHistory_c(void);
  ~puzzleHistory_c(void);

  /** Drop all snapshots and record the current puzzle as baseline (step 0). */
  void reset(puzzle_c * puzzle);

  /** Stroke API — collapses an entire drag-paint gesture into one undo step. */
  void beginStroke(actionKind_e kind = AK_ENTITIES_GRID_PAINT);
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

  /**
   * The step the next record() makes begins with this selection: an undo of
   * it goes back there. For AK_PROBLEM_COUNT, coalesceKey names what is being
   * counted; only steps of one problem and key coalesce.
   */
  void beginStep(const selection_t & selection, unsigned int coalesceKey = 0);
  /**
   * Ends what beginStep() began. If a step was recorded since, this is where
   * it left the selection: a redo of it goes there.
   */
  void endStep(const selection_t & selection);

  bool canUndo(void) const;
  bool canRedo(void) const;

  /** Restore previous snapshot into puzzle. Returns shape selection + tab. */
  undoResult_t undo(puzzle_c * puzzle);
  /** Re-apply the next snapshot. Returns shape selection + tab. */
  undoResult_t redo(puzzle_c * puzzle);

  /** Solve records an edit discarded: the problem's index and what it held. */
  typedef std::vector<std::pair<unsigned int, problem_c::solveRecord_t>> discarded_t;

  /** Keep what the step just recorded discarded (a voxel edit drops the
   *  solutions of the problems using that shape): undoing the step swaps
   *  the records back into their problems, redoing it swaps them out again.
   *  A step that already holds a record for a problem (a coalesced paint)
   *  keeps its own, the state before the whole step. False, and d left as
   *  it was, when there is no step to keep them in (step 0). */
  bool keepDiscarded(discarded_t && d);

  /** What the records hold, counted as DocumentSession counts a problem's:
   *  its saved solutions, and one for a search state past unsolved. */
  static long solutionsIn(const discarded_t & d);
  /** The same over every step's kept records: an undo or redo moves them
   *  between the steps and the problems, and loses none. */
  long solutionsKept(void) const;

  /** Set the runtime undo stack limit (clamped to [1, MAX_UNDO]). A lower
   *  limit drops the oldest undo steps at once, then the redo steps furthest
   *  ahead, down to the depth plus the current state, as record() keeps. */
  void setMaxUndo(unsigned int depth);

  /** Call after a successful save. */
  void markSaved(void);
  /** True if the current state differs from the last markSaved() position. */
  bool isModifiedFromSave(void) const;

  /** Mark the puzzle as modified without pushing a snapshot.
   *  Use for changes that cannot be undone (solve completion, solution deletion,
   *  grid-type conversion). */
  void markModified(void);

  /** Pure mapping, no state — usable from tests. */
  static affectedTab_e tabForAction(actionKind_e kind);

private:

  struct groupSnap_t {
    unsigned short group = 0;
    unsigned short count = 0;
  };

  struct partSnap_t {
    unsigned int shapeId = 0;
    unsigned int min = 0;
    unsigned int max = 0;
    std::vector<groupSnap_t> groups;
  };

  struct problemSnap_t {
    std::string name;
    bool resultValid = false;
    unsigned int resultId = 0;
    std::set<uint32_t> colorConstraints;
    std::vector<partSnap_t> parts;
    unsigned int maxHoles = 0xFFFFFFFF;  ///< as problem_c stores it: this value is "no limit"
  };

  struct snapshot_t {
    std::vector<std::shared_ptr<const voxel_c>> shapes; ///< COW — unchanged shapes shared across snapshots
    std::vector<uint32_t> colors;                       ///< R | G<<8 | B<<16
    std::vector<problemSnap_t> problems;
    unsigned int selectedShape;
    actionKind_e kind;
    /* What this step discarded while in the state after it, what undoing it
     * put back while in the state before it: swapped at each undo / redo. */
    discarded_t discarded;
    selection_t before, after;                          ///< the selection around this step (beginStep, endStep)

    snapshot_t(void) : selectedShape(NO_SHAPE), kind(AK_NONE) {}
    ~snapshot_t(void) = default;
  };

  std::deque<std::unique_ptr<snapshot_t>> snapshots;
  unsigned int cursor;
  unsigned int savedCursor;
  bool savedCursorValid; ///< false when the save point was evicted from the front of the stack

  unsigned int maxUndo_;

  void evictOldest(void);   ///< drop the first snapshot, keeping the cursor and save point in step

  bool inStroke;
  bool strokeDirty;
  actionKind_e strokeKind; ///< kind recorded by beginStroke(), used by endStroke()

  actionKind_e lastKind;
  unsigned int lastShape;
  int64_t lastTimeMs;

  bool stepOpen;            ///< between beginStep() and endStep()
  bool stepRecorded;        ///< ... and a step was recorded in between
  selection_t stepBefore;
  unsigned int stepKey;
  selection_t lastBefore;   ///< the last step's, what the next may coalesce with
  unsigned int lastKey;

  static std::unique_ptr<voxel_c> cloneShape(const voxel_c * src);
  static int64_t nowMs(void);

  std::unique_ptr<snapshot_t> capture(const puzzle_c * puzzle,
                                      unsigned int selectedShape,
                                      actionKind_e kind,
                                      const snapshot_t * prevSnap) const;
  /* Returns the shapes whose voxels it changed. A problem whose structure
   * the snapshot leaves as it is keeps its solutions. */
  std::vector<unsigned int> restore(puzzle_c * puzzle, const snapshot_t * snap) const;
  static problemSnap_t snapProblem(const problem_c * pr);
  void matchProblems(puzzle_c * puzzle, const snapshot_t * snap) const;
  void matchShapes(puzzle_c * puzzle, const snapshot_t * snap) const;
  /* the same parts, result, groups, colour constraints and holes (the name aside) */
  static bool sameStructure(const problem_c & pr, const problemSnap_t & ps);
  /* after a restore: the step's kept solve records swapped in or out, and
   * the solutions of the other problems using a changed shape dropped */
  static void settleSolutions(puzzle_c * puzzle, const std::vector<unsigned int> & changedShapes,
                              snapshot_t & step);

  bool canCoalesce(actionKind_e kind, unsigned int selectedShape) const;
  void pushOrReplace(puzzle_c * puzzle, actionKind_e kind,
                     unsigned int selectedShape);
  void clearSnapshots(void);

  puzzleHistory_c(const puzzleHistory_c &) = delete;
  puzzleHistory_c & operator=(const puzzleHistory_c &) = delete;
};

#endif
