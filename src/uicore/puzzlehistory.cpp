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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iterator>
#include <set>

namespace {
  /* every history's kept records together, and the budget for them; the
   * histories live on the GUI thread, the atomics only make that cheap to
   * get wrong */
  std::atomic<size_t> keptTotal{0};
  std::atomic<size_t> keptBudget{puzzleHistory_c::DEFAULT_SOLUTION_BUDGET};
}

puzzleHistory_c::snapshot_t::~snapshot_t(void) {
  keptTotal -= keptBytes;
}

void puzzleHistory_c::recount(snapshot_t & step) {
  const size_t bytes = bytesIn(step.discarded);
  keptTotal -= step.keptBytes;
  keptTotal += bytes;
  step.keptBytes = bytes;
}

void puzzleHistory_c::setSolutionBudget(size_t bytes) {
  keptBudget = bytes;
}

size_t puzzleHistory_c::solutionBudget(void) {
  return keptBudget;
}

size_t puzzleHistory_c::solutionBytesKept(void) {
  return keptTotal;
}

size_t puzzleHistory_c::bytesIn(const discarded_t & d) {
  size_t bytes = d.capacity() * sizeof(discarded_t::value_type);
  for (const auto & kept : d)
    bytes += kept.second.memoryBytes();
  return bytes;
}

bool puzzleHistory_c::roomForSolutions(size_t bytes) {
  const size_t total = keptTotal, budget = keptBudget;
  return total <= budget && bytes <= budget - total;
}

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
  lastTimeMs(0),
  stepOpen(false),
  stepRecorded(false),
  stepKey(0),
  lastKey(0)
{
}

void puzzleHistory_c::setMaxUndo(unsigned int depth) {
  if (depth < 1) depth = 1;
  if (depth > MAX_UNDO) depth = MAX_UNDO;
  maxUndo_ = depth;
  // a smaller limit applies to the steps already kept, not only to new
  // ones: the undo steps behind the current state first, then the redo
  // steps furthest ahead, down to the size record() keeps
  while (cursor > maxUndo_)
    evictOldest();
  while (snapshots.size() > size_t(maxUndo_) + 1)
    snapshots.pop_back();
  if (savedCursorValid && savedCursor >= snapshots.size())
    savedCursorValid = false;      // save point dropped with the redo steps
}

void puzzleHistory_c::evictOldest(void) {
  snapshots.pop_front(); // O(1) with deque
  if (cursor > 0) cursor--;
  if (savedCursorValid) {
    if (savedCursor > 0) savedCursor--;
    else savedCursorValid = false; // save point evicted from front
  }
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
  stepOpen = false;
  stepRecorded = false;
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
  // AK_COLOR_PALETTE too: removing a colour renumbers the voxels of every
  // shape (puzzle_c::removeColor), and a shared copy would keep the old ids.
  // AK_PROBLEM_STRUCTURAL, AK_PROBLEM_COUNT → no shape changes → share all.
  // Paint/transform/click → only selectedShape changed → share the rest.
  const bool cloneAll  = (kind == AK_NONE || kind == AK_ENTITIES_STRUCTURAL ||
                          kind == AK_COLOR_PALETTE);
  const bool cloneNone = !cloneAll && (kind == AK_PROBLEM_STRUCTURAL || kind == AK_PROBLEM_COUNT);

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

  for (unsigned int p = 0; p < puzzle->getNumberOfProblems(); p++)
    snap->problems.push_back(snapProblem(puzzle->getProblem(p)));

  return snap;
}

puzzleHistory_c::problemSnap_t puzzleHistory_c::snapProblem(const problem_c * pr) {
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
  if (pr->maxHolesDefined())
    ps.maxHoles = pr->getMaxHoles();
  return ps;
}

/* Whether the problem is what the snapshot holds, name aside. */
bool puzzleHistory_c::sameStructure(const problem_c & pr, const problemSnap_t & ps, bool rules) {
  if (pr.resultValid() != ps.resultValid || (ps.resultValid && pr.getResultId() != ps.resultId) ||
      (rules && pr.getColorConstraints() != ps.colorConstraints) || pr.getNumberOfParts() != ps.parts.size() ||
      (pr.maxHolesDefined() ? pr.getMaxHoles() : 0xFFFFFFFF) != ps.maxHoles)
    return false;
  for (unsigned int i = 0; i < ps.parts.size(); i++) {
    const partSnap_t & part = ps.parts[i];
    if (pr.getShapeIdOfPart(i) != part.shapeId || pr.getPartMinimum(i) != part.min ||
        pr.getPartMaximum(i) != part.max || pr.getNumberOfPartGroups(i) != part.groups.size())
      return false;
    for (unsigned int g = 0; g < part.groups.size(); g++)
      if (pr.getPartGroupId(i, g) != part.groups[g].group || pr.getPartGroupCount(i, g) != part.groups[g].count)
        return false;
  }
  return true;
}

/* Bring the problems into the snapshot's order, each snapshot entry with the
 * problem that matches it, so that a problem's solutions -- which the
 * snapshot does not hold -- stay with it through a reorder or a delete. The
 * problems the snapshot does not have end up last. */
void puzzleHistory_c::matchProblems(puzzle_c * puzzle, const snapshot_t * snap) const {
  const unsigned int count = puzzle->getNumberOfProblems();
  const unsigned int wanted = (unsigned int)snap->problems.size();
  const unsigned int unset = (unsigned int)-1;
  std::vector<unsigned int> from(count, unset);
  std::vector<bool> taken(count, false);

  /* the same name and structure first, in place before elsewhere: two
   * problems alike but for their solutions keep their places */
  const auto assign = [&](bool withName, bool inPlace) {
    for (unsigned int p = 0; p < wanted; p++) {
      if (from[p] != unset)
        continue;
      for (unsigned int j = inPlace ? p : 0; j < (inPlace ? p + 1 : count); j++) {
        const problem_c & pr = *puzzle->getProblem(j);
        if (!taken[j] && sameStructure(pr, snap->problems[p]) &&
            (!withName || pr.getName() == snap->problems[p].name)) {
          from[p] = j;
          taken[j] = true;
          break;
        }
      }
    }
  };
  assign(true, true);
  assign(true, false);
  assign(false, true);
  assign(false, false);

  unsigned int spare = 0;
  for (unsigned int p = 0; p < count; p++) {
    if (from[p] != unset)
      continue;
    while (taken[spare])
      spare++;
    from[p] = spare;
    taken[spare] = true;
  }

  for (unsigned int p = 0; p < count; p++) {
    if (from[p] == p)
      continue;
    const unsigned int q = from[p];
    puzzle->exchangeProblems(p, q);
    // what stood at p now stands at q
    for (unsigned int r = p + 1; r < count; r++)
      if (from[r] == p) {
        from[r] = q;
        break;
      }
    from[p] = p;
  }
}

namespace {
  bool sameVoxels(const voxel_c & a, const voxel_c & b) {
    if (a.getX() != b.getX() || a.getY() != b.getY() || a.getZ() != b.getZ())
      return false;
    for (unsigned int v = 0; v < a.getXYZ(); v++)
      if (a.getState(v) != b.getState(v) || a.getColor(v) != b.getColor(v))
        return false;
    return true;
  }

  // the colours a shape's voxels have, colour 0 (none) left out
  template <class Shapes>
  void addColoursOf(const Shapes & shapes, unsigned int shape, std::set<unsigned int> & colours) {
    if (shape >= shapes.size())
      return;
    const voxel_c & v = *shapes[shape];
    for (unsigned int i = 0; i < v.getXYZ(); i++)
      if (v.getState(i) != voxel_c::VX_EMPTY && v.getColor(i) != 0)
        colours.insert(v.getColor(i));
  }

  /* A colour rule lets a piece voxel of one colour sit in a result cell of
   * another (problem_c::allowPlacement stores it as (piece-1) << 16 |
   * (result-1)). It can change what a search finds only when a piece of the
   * problem has the one colour and its result the other: whether the two
   * sets differ in such a rule. */
  bool rulesThatMatterDiffer(const std::set<uint32_t> & a, const std::set<uint32_t> & b,
                             const std::set<unsigned int> & pieceColours,
                             const std::set<unsigned int> & resultColours) {
    std::vector<uint32_t> differ;
    std::set_symmetric_difference(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(differ));
    return std::any_of(differ.begin(), differ.end(), [&](uint32_t rule) {
      return pieceColours.count((rule >> 16) + 1) > 0 && resultColours.count((rule & 0xFFFF) + 1) > 0;
    });
  }
}

/* Bring the shapes into the snapshot's order, by the library's own exchange:
 * the problems follow, and a reorder undone leaves them their solutions. A
 * shape that stands right stays; one that matches elsewhere moves only from a
 * place that wants something else. */
void puzzleHistory_c::matchShapes(puzzle_c * puzzle, const snapshot_t * snap) const {
  const unsigned int count = puzzle->getNumberOfShapes();
  if (count != snap->shapes.size())
    return;
  for (unsigned int i = 0; i < count; i++) {
    if (sameVoxels(*puzzle->getShape(i), *snap->shapes[i]))
      continue;
    for (unsigned int j = i + 1; j < count; j++)
      if (sameVoxels(*puzzle->getShape(j), *snap->shapes[i]) && !sameVoxels(*puzzle->getShape(j), *snap->shapes[j])) {
        puzzle->exchangeShapes(i, j);
        break;
      }
  }
}

puzzleHistory_c::restored_t puzzleHistory_c::restore(puzzle_c * puzzle, const snapshot_t * snap,
                                                    std::vector<const problem_c *> * removed) const {
  matchShapes(puzzle, snap);

  // --- which shapes this restore changes: their problems' solutions no longer fit (settleSolutions) ---
  const unsigned int shapesNow = puzzle->getNumberOfShapes();
  const unsigned int shapesThen = (unsigned int)snap->shapes.size();
  restored_t out;
  for (unsigned int i = 0; i < std::max(shapesNow, shapesThen); i++)
    if (i >= shapesNow || i >= shapesThen || !sameVoxels(*puzzle->getShape(i), *snap->shapes[i]))
      out.changedShapes.push_back(i);

  // --- problems --- the right number, each where the snapshot has it ---
  while (puzzle->getNumberOfProblems() < snap->problems.size())
    puzzle->addProblem();
  matchProblems(puzzle, snap);
  while (puzzle->getNumberOfProblems() > snap->problems.size()) {
    if (removed)
      removed->push_back(puzzle->getProblem(puzzle->getNumberOfProblems() - 1));
    puzzle->removeProblem(puzzle->getNumberOfProblems() - 1);
  }

  /* The problems this restore changes give up their parts while the shapes
   * those name are still there: the library checks each id against them, and
   * a restore with fewer shapes would leave ids past the end. The snapshot
   * holds no solutions: a problem whose structure it does not change is left
   * alone, and keeps them -- rebuilding goes through setters (clearResult,
   * ...) that drop them, so every undo used to drop every problem's. One it
   * rebuilds holds its solve record aside meanwhile and gets it back after:
   * the caller settles the solutions of the problems it rebuilt and of those
   * using a shape it changed (settleSolutions), and may swap back those the
   * step kept. */
  std::vector<bool> rebuild(snap->problems.size(), false);
  std::vector<problem_c::solveRecord_t> aside(snap->problems.size());
  for (unsigned int p = 0; p < snap->problems.size(); p++) {
    problem_c * pr = puzzle->getProblem(p);
    const problemSnap_t & ps = snap->problems[p];

    pr->setName(ps.name);

    if (sameStructure(*pr, ps, false)) {
      if (pr->getColorConstraints() == ps.colorConstraints)
        continue;
      /* Rules no voxel of the problem falls under change none of its
       * solutions -- an Add colour's own rule, the new colour in pieces of
       * it: put them back and keep the solutions. The colours are those of
       * the snapshot's shapes; a shape the restore changes drops the
       * solutions anyway (settleSolutions). */
      std::set<unsigned int> pieceColours, resultColours;
      for (const partSnap_t & part : ps.parts)
        addColoursOf(snap->shapes, part.shapeId, pieceColours);
      if (ps.resultValid)
        addColoursOf(snap->shapes, ps.resultId, resultColours);
      if (!rulesThatMatterDiffer(pr->getColorConstraints(), ps.colorConstraints, pieceColours, resultColours)) {
        pr->setColorConstraints(ps.colorConstraints);
        continue;
      }
    }
    rebuild[p] = true;
    out.rebuiltProblems.push_back(p);

    pr->swapSolveRecord(aside[p]);

    std::vector<unsigned int> partIds;
    partIds.reserve(pr->getNumberOfParts());
    for (unsigned int i = 0; i < pr->getNumberOfParts(); i++)
      partIds.push_back(pr->getShapeIdOfPart(i));
    for (unsigned int sid : partIds)
      pr->setShapeMaximum(sid, 0);
    if (pr->resultValid())
      pr->clearResult();
  }

  // --- shapes ---
  std::vector<std::unique_ptr<voxel_c>> clones;
  clones.reserve(snap->shapes.size());
  for (unsigned int i = 0; i < snap->shapes.size(); i++)
    clones.push_back(cloneShape(snap->shapes[i].get()));
  puzzle->adoptShapes(std::move(clones));

  // --- colour palette ---
  puzzle->adoptColors(snap->colors);

  for (unsigned int p = 0; p < snap->problems.size(); p++) {
    if (!rebuild[p])
      continue;
    problem_c * pr = puzzle->getProblem(p);
    const problemSnap_t & ps = snap->problems[p];

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
    if (ps.maxHoles != 0xFFFFFFFF)
      pr->setMaxHoles(ps.maxHoles);
    else
      pr->setMaxHolesInvalid();
  }
  for (unsigned int p : out.rebuiltProblems)
    puzzle->getProblem(p)->swapSolveRecord(aside[p]);
  return out;
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

  if (kind == AK_PROBLEM_COUNT)
    return stepOpen && stepBefore.problem == lastBefore.problem && stepKey == lastKey &&
           dt >= 0 && dt <= PROBLEM_COUNT_COALESCE_MS;

  return false;
}

void puzzleHistory_c::pushOrReplace(puzzle_c * puzzle, actionKind_e kind,
                                    unsigned int selectedShape) {
  if (canCoalesce(kind, selectedShape)) {
    // Replace current snapshot; share unchanged shapes from the one before it.
    const snapshot_t * prevSnap = (cursor > 0) ? snapshots[cursor - 1].get() : nullptr;
    discarded_t discarded = std::move(snapshots[cursor]->discarded);   // the step's, still
    recount(*snapshots[cursor]);
    const selection_t before = snapshots[cursor]->before;    // the burst's first
    snapshots[cursor] = capture(puzzle, selectedShape, kind, prevSnap);
    snapshots[cursor]->discarded = std::move(discarded);
    recount(*snapshots[cursor]);
    snapshots[cursor]->before = before;
    snapshots[cursor]->after = stepBefore;
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
    if (stepOpen)
      snapshots[cursor]->before = snapshots[cursor]->after = stepBefore;

    while (snapshots.size() > maxUndo_ + 1)
      evictOldest();
  }

  lastKind = kind;
  lastShape = selectedShape;
  lastTimeMs = nowMs();
  lastBefore = stepBefore;
  lastKey = stepKey;
  stepRecorded = stepOpen;
}

void puzzleHistory_c::record(puzzle_c * puzzle, actionKind_e kind,
                              unsigned int selectedShape) {
  inStroke = false;
  strokeDirty = false;
  pushOrReplace(puzzle, kind, selectedShape);
}

void puzzleHistory_c::beginStep(const selection_t & selection, unsigned int coalesceKey) {
  stepOpen = true;
  stepRecorded = false;
  stepBefore = selection;
  stepKey = coalesceKey;
}

void puzzleHistory_c::endStep(const selection_t & selection) {
  if (stepOpen && stepRecorded)
    snapshots[cursor]->after = selection;
  stepOpen = false;
  stepRecorded = false;
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
  settleSolutions(puzzle, restore(puzzle, snapshots[cursor].get()), *snapshots[cursor + 1]);
  /* Tab and shape come from the action we just undid (cursor+1), not from
   * the baseline we restored to — the baseline has kind=AK_NONE which maps
   * to TAB_ENTITIES regardless of which tab the user was working on. */
  return { snapshots[cursor + 1]->selectedShape,
           tabForAction(snapshots[cursor + 1]->kind),
           snapshots[cursor + 1]->before };
}

/* The restore and the settling decide which problems lose what they hold
 * (their matching of shapes and problems is not repeated here, so the
 * answer cannot drift from what an undo does): run the restore on a copy,
 * which leaves the solutions behind, and see whether a problem it removes,
 * or one droppedBy() names, held any. The copy's problems stand in for the
 * puzzle's by position; one removed is collected before it goes, and no
 * problem is made after that, so a pointer is never reused. */
bool puzzleHistory_c::costsSolutions(const puzzle_c * puzzle, const snapshot_t * snap,
                                     const snapshot_t & step) const {
  std::vector<bool> holds(puzzle->getNumberOfProblems());
  for (unsigned int i = 0; i < holds.size(); i++) {
    const problem_c * pr = puzzle->getProblem(i);
    // as btui::problemHasSolutions: an unknown state (an edit made the counts
    // incomplete) with nothing saved holds nothing
    const solveState_e st = pr->getSolveState();
    holds[i] = pr->getNumberOfSavedSolutions() > 0 || st == SS_SOLVING || st == SS_SOLVED;
  }
  if (std::none_of(holds.begin(), holds.end(), [](bool h) { return h; }))
    return false;

  puzzle_c trial(puzzle);
  std::vector<const problem_c *> was(holds.size());
  for (unsigned int i = 0; i < was.size(); i++)
    was[i] = trial.getProblem(i);
  std::vector<const problem_c *> lost;
  const restored_t restored = restore(&trial, snap, &lost);
  for (unsigned int k : droppedBy(trial, restored, step))
    lost.push_back(trial.getProblem(k));
  for (unsigned int i = 0; i < was.size(); i++)
    if (holds[i] && std::find(lost.begin(), lost.end(), was[i]) != lost.end())
      return true;
  return false;
}

bool puzzleHistory_c::undoCostsSolutions(const puzzle_c * puzzle) const {
  // undo(): the snapshot before the step restored, the step settled
  return canUndo() && costsSolutions(puzzle, snapshots[cursor - 1].get(), *snapshots[cursor]);
}

bool puzzleHistory_c::redoCostsSolutions(const puzzle_c * puzzle) const {
  return canRedo() && costsSolutions(puzzle, snapshots[cursor + 1].get(), *snapshots[cursor + 1]);
}

puzzleHistory_c::undoResult_t puzzleHistory_c::redo(puzzle_c * puzzle) {
  if (!canRedo())
    return { NO_SHAPE, TAB_ENTITIES };
  cursor++;
  lastKind = AK_NONE;
  settleSolutions(puzzle, restore(puzzle, snapshots[cursor].get()), *snapshots[cursor]);
  return { snapshots[cursor]->selectedShape,
           tabForAction(snapshots[cursor]->kind),
           snapshots[cursor]->after };
}

puzzleHistory_c::keep_e puzzleHistory_c::keepDiscarded(discarded_t && d) {
  if (d.empty())
    return KEPT;
  if (cursor == 0 || cursor >= snapshots.size())
    return NO_STEP;
  discarded_t & kept = snapshots[cursor]->discarded;
  auto held = [&kept](unsigned int problem) {
    return std::any_of(kept.begin(), kept.end(), [problem](const auto & k) { return k.first == problem; });
  };
  size_t bytes = 0;
  for (const auto & [problem, record] : d)
    if (!held(problem))
      bytes += sizeof(discarded_t::value_type) + record.memoryBytes();
  if (!roomForSolutions(bytes))
    return NO_ROOM;
  for (auto & [problem, record] : d)
    if (!held(problem))
      kept.emplace_back(problem, std::move(record));
  d.clear();
  recount(*snapshots[cursor]);
  return KEPT;
}

long puzzleHistory_c::solutionsIn(const discarded_t & d) {
  long held = 0;
  for (const auto & kept : d)
    held += long(kept.second.solutions.size()) + (kept.second.solveState == SS_UNSOLVED ? 0 : 1);
  return held;
}

long puzzleHistory_c::solutionsKept(void) const {
  long held = 0;
  for (const auto & snap : snapshots)
    held += solutionsIn(snap->discarded);
  return held;
}

std::vector<unsigned int> puzzleHistory_c::droppedBy(const puzzle_c & puzzle, const restored_t & restored,
                                                     const snapshot_t & step) {
  const unsigned int count = puzzle.getNumberOfProblems();
  // the step's records go back (undo) or out again (redo): those lose nothing
  std::vector<bool> settled(count, false);
  for (const auto & kept : step.discarded)
    if (kept.first < count)
      settled[kept.first] = true;
  std::vector<unsigned int> dropped;
  for (unsigned int k = 0; k < count; k++) {
    if (settled[k])
      continue;
    const problem_c * pr = puzzle.getProblem(k);
    const bool rebuilt = std::find(restored.rebuiltProblems.begin(), restored.rebuiltProblems.end(), k)
                         != restored.rebuiltProblems.end();
    if (rebuilt || std::any_of(restored.changedShapes.begin(), restored.changedShapes.end(),
                               [pr](unsigned int s) { return pr->usesShape(s); }))
      dropped.push_back(k);
  }
  return dropped;
}

void puzzleHistory_c::settleSolutions(puzzle_c * puzzle, const restored_t & restored, snapshot_t & step) {
  /* the solutions the others held no longer fit them; found first, while
   * the step's records still name the problems they settle */
  const std::vector<unsigned int> dropped = droppedBy(*puzzle, restored, step);
  /* the solutions the step kept go back (undo) or out again (redo); the
   * step's problems are the restored state's, a voxel edit or a change to
   * what a problem asks for keeps them */
  for (auto & [problem, record] : step.discarded)
    if (problem < puzzle->getNumberOfProblems())
      puzzle->getProblem(problem)->swapSolveRecord(record);
  recount(step);   // it holds what the problems held
  for (unsigned int k : dropped)
    puzzle->getProblem(k)->removeAllSolutions();
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
  case AK_PROBLEM_COUNT:
    return TAB_PUZZLE;
  default:
    return TAB_ENTITIES;
  }
}
