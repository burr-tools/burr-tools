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

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef BTUI_DOCUMENTSESSION_H
#define BTUI_DOCUMENTSESSION_H

#include "puzzlehistory.h"

#include "../lib/gridtype.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class puzzle_c;
class voxel_c;
class solutionReader_c;

/* The open puzzle file: the puzzle itself, its file name, and its undo
 * history.
 *
 * This is the legacy mainWindow_c's document handling (cb_New, tryToLoad,
 * cb_Load_Ps3d, cb_Save, cb_SaveAs, cb_Undo/Redo) without the dialogs. Every
 * question the user has to answer -- which file, overwrite?, discard
 * changes? -- stays with the caller; this class only performs the operation
 * and reports what happened, so it can be driven and checked from tests with
 * no GUI at all.
 */
namespace btui {

  class DocumentSession {

    public:

      /* Starts with a new, empty brick puzzle, as legacy main() does. */
      DocumentSession(void);
      ~DocumentSession(void);

      DocumentSession(const DocumentSession &) = delete;
      DocumentSession & operator=(const DocumentSession &) = delete;

      /* Reading only: a change goes through edit() or editWithoutUndo(), so
       * that none can miss its undo step.
       */
      const puzzle_c & puzzle(void) const { return *puz; }

      /* Empty for a document that has never been saved or loaded. */
      const std::filesystem::path & fileName(void) const { return fname; }

      /* Replace the document with an empty puzzle of the given grid type. */
      void newDocument(gridType_c::gridType type);

      struct LoadResult {
        bool ok = false;
        std::string error;              ///< why it failed, for the user; empty on success
        bool containsStartedSearch = false;  ///< a problem has an unfinished solve (legacy warns)
        bool showComment = false;       ///< the file asks for its comment to pop up on open
        bool solutionsPending = false;  ///< the solutions are still being read: loadingSolutions()
      };

      /* Load a BurrTools file. On failure the current document is untouched.
       * A file with more than stageAbove() bytes of solutions loads staged
       * (puzzleloader.h): the document is the puzzle without them at once,
       * and they are read on a thread of their own while loadingSolutions()
       * -- the puzzle is not to be changed meanwhile, its solution lists
       * read under their lock (problem_c::lockSolutions), as while a solver
       * runs. */
      LoadResult load(const std::filesystem::path & file);

      /* Bytes of solutions above which load() stages; by default it never
       * does. */
      void setStageAbove(size_t bytes) { stageAbove = bytes; }
      size_t stageAboveBytes(void) const { return stageAbove; }
      /* the threads that read the solutions (solutionReader_c), from the next load */
      void setLoadThreads(unsigned threads) { loadThreads = threads; }

      /* A staged load's solutions being read, until finishLoading() takes
       * the result; save() and saveAs() refuse meanwhile. */
      bool loadingSolutions(void) const { return reader != nullptr; }
      /* how far the reading is, 0 to 1 */
      double loadProgress(void) const;
      struct LoadEnd {
        bool done = false;              ///< the reading is over: the rest says how
        std::string error;              ///< why the file could not be read, for the user
      };
      /* Once the reading is over: ends it. With an error the document is a
       * new, empty one, as for a file that does not load at all. Before
       * that: done is false and nothing changes. */
      LoadEnd finishLoading(void);

      /* Import a PuzzleSolver3D file; it becomes the document, unsaved and
       * without a file name, so the first Save asks where. (Legacy kept the
       * .puz name, and its Save wrote xmpuzzle over the imported file.)
       */
      LoadResult importPuzzleSolver3D(const std::filesystem::path & file);

      /* Convert the puzzle to another grid type (legacy cb_Convert): the
       * converted puzzle replaces this one, the undo history starts afresh and
       * the document counts as unsaved. False when the conversion fails, and
       * while loadingSolutions(). */
      bool convert(gridType_c::gridType to);

      /* Write to fileName(). False when there is no name yet, the write
       * failed or loadingSolutions(); the document stays modified then.
       */
      bool save(void);

      /* Write to file (with ".xmpuzzle" appended when missing) and adopt it as
       * the document's name. On failure, and while loadingSolutions(),
       * neither the name nor the modified state change.
       */
      bool saveAs(const std::filesystem::path & file);

      /* The name saveAs() will really write to. */
      static std::filesystem::path withPuzzleExtension(const std::filesystem::path & file);

      /* Unsaved changes, a stroke's too while its button is still down. */
      bool isModified(void) const;

      /* Unsaved, but not an undo step -- the comment, solver results. */
      void markModified(void);

      void setComment(const std::string & comment);

      /* The Puzzle workspace's selection around the undo step of an edit()
       * (puzzleHistory_c::beginStep, endStep). */
      void beginStep(const puzzleHistory_c::selection_t & selection, unsigned int coalesceKey = 0);
      void endStep(const puzzleHistory_c::selection_t & selection);

      /* Undo/redo. The result says which workspace and shape the change
       * belonged to, so the GUI can show it. A stroke that changed something
       * can be undone before it ends: undo() ends it first, which makes it a
       * step.
       */
      bool canUndo(void) const;
      bool canRedo(void) const;
      /* whether the undo / redo would drop solutions or a search state a
       * problem holds (the history keeps neither), for the warning before it */
      bool undoCostsSolutions(void) const { return hist->undoCostsSolutions(puz.get()); }
      bool redoCostsSolutions(void) const { return hist->redoCostsSolutions(puz.get()); }
      puzzleHistory_c::undoResult_t undo(void);
      puzzleHistory_c::undoResult_t redo(void);

      /* Change the puzzle as one undo step of the given kind. f(puzzle_c &)
       * returns whether it changed anything; only then is the step recorded.
       * If f throws, the step is recorded all the same -- the change may be
       * half done, and undo must still reach the state before it -- and the
       * exception goes on. Returns what f returned.
       */
      template <class F> bool edit(puzzleHistory_c::actionKind_e kind, unsigned int selectedShape, F && f) {
        endStroke();
        const long held = solutionsHeld();
        bool changed;
        try {
          changed = f(*puz);
        } catch (...) {
          record(kind, selectedShape);
          noteSolutionsLost(held);
          throw;
        }
        if (changed) {
          record(kind, selectedShape);
          noteSolutionsLost(held);
        }
        return changed;
      }

      /* An edit of shapes' voxels -- painting, a 3D click, the grid size, an
       * inspector task -- as edit(), naming the shapes f may change. A
       * problem that uses one of them whose voxels really changed loses its
       * solutions and search state, as legacy's changeShape() did; they are
       * kept in the undo step, so undo brings them back and redo drops them
       * again (PR 181 review: a saved file kept solutions that no longer fit
       * its pieces). Kept only while the history's memory budget has room
       * (keepsSolutionsOf); otherwise they are lost for good.
       */
      template <class F> bool editVoxels(puzzleHistory_c::actionKind_e kind, unsigned int selectedShape,
                                         const std::vector<unsigned int> & shapes, F && f) {
        const ShapeCopies before = copyShapes(shapes);
        bool changed;
        /* the solutions go after the step is recorded (it holds none), so
         * edit() does not count what the step keeps as lost */
        try {
          changed = edit(kind, selectedShape, std::forward<F>(f));
        } catch (...) {
          keepDiscarded(discardChanged(before), true);   // the step was recorded all the same
          throw;
        }
        keepDiscarded(discardChanged(before), changed);
        return changed;
      }

      /* An edit of these shapes' voxels would keep the solutions it drops
       * in its undo step: they fit in the history's memory budget
       * (puzzleHistory_c::roomForSolutions), with what an open stroke holds.
       * True when there is nothing to drop. */
      bool keepsSolutionsOf(const std::vector<unsigned int> & shapes) const;

      /* A change to what problems ask for -- piece counts, the result,
       * groups, colour rules -- as edit(), naming the problems f changes;
       * f must not add, remove or move problems. Those problems lose their
       * solutions and search state, kept in the undo step as editVoxels()
       * keeps them; the same budget (keepsSolutionsOfProblems). */
      template <class F> bool editProblems(puzzleHistory_c::actionKind_e kind, unsigned int selectedShape,
                                           const std::vector<unsigned int> & problems, F && f) {
        puzzleHistory_c::discarded_t taken = takeSolutions(problems);
        bool changed;
        try {
          changed = edit(kind, selectedShape, std::forward<F>(f));
        } catch (...) {
          keepDiscarded(std::move(taken), true);   // the step was recorded all the same
          throw;
        }
        keepDiscarded(std::move(taken), changed);
        return changed;
      }

      /* editProblems() would keep these problems' solutions in its step */
      bool keepsSolutionsOfProblems(const std::vector<unsigned int> & problems) const;

      /* Change the puzzle without an undo step, but as unsaved (solver
       * results; the comment goes through setComment()). */
      template <class F> void editWithoutUndo(F && f) {
        f(*puz);
        markModified();
      }

      /* A stroke: a drag that paints (C08), one undo step however many
       * changes it makes. Between beginStroke() and endStroke(), strokeEdit(f)
       * changes the puzzle as edit() does, without a step of its own; the
       * stroke is recorded at its end if any of them changed anything. An
       * edit(), undo, redo or another document ends an open stroke first, so
       * what it painted so far is a step of its own and is never lost. A
       * save during a stroke splits it there: the part saved is one step, the
       * rest of the drag the next, so an undo back to the save reads clean.
       * Outside a stroke, strokeEdit() is an edit() of the stroke kind.
       * A stroke paints the shape it began on: what it changes discards the
       * solutions of the problems using that shape, kept in its step as
       * editVoxels() keeps them.
       */
      void beginStroke(puzzleHistory_c::actionKind_e kind, unsigned int selectedShape);
      template <class F> bool strokeEdit(F && f) {
        if (!stroking) {
          if (strokeShape == puzzleHistory_c::NO_SHAPE)
            return edit(strokeKind, strokeShape, std::forward<F>(f));
          return editVoxels(strokeKind, strokeShape, { strokeShape }, std::forward<F>(f));
        }
        const long held = solutionsHeld();
        bool changed;
        try {
          changed = f(*puz);
        } catch (...) {
          markStrokeChanged();
          noteSolutionsLost(held);
          throw;
        }
        if (changed) {
          markStrokeChanged();
          noteSolutionsLost(held);
        }
        return changed;
      }
      /* true when the stroke changed the puzzle (an undo step was recorded) */
      bool endStroke(void);
      bool inStroke(void) const { return stroking; }
      /* an open stroke has changed the puzzle (it becomes a step at its end) */
      bool strokeChanged(void) const { return stroking && strokeDirty; }

      void setUndoDepth(unsigned int depth);

    private:

      /* an open stroke so far as one undo step, the stroke going on (a save) */
      void splitStroke(void);

      /* the stroke changed its shape: the solutions it invalidates go */
      void markStrokeChanged(void);

      /* copies of shapes before an edit (editVoxels), by index; made and
       * dropped out of line, so callers need not see voxel_c */
      struct ShapeCopies {
        std::vector<std::pair<unsigned int, std::unique_ptr<voxel_c>>> shapes;
        ShapeCopies(void);
        ~ShapeCopies(void);
        ShapeCopies(ShapeCopies &&) noexcept;
      };
      ShapeCopies copyShapes(const std::vector<unsigned int> & shapes) const;
      /* the solve records of the problems using a shape that no longer
       * matches its copy, or one of shapes, swapped out of them */
      puzzleHistory_c::discarded_t discardChanged(const ShapeCopies & before);
      puzzleHistory_c::discarded_t discardUsing(const std::vector<unsigned int> & shapes);
      /* the problems' solve records, taken out (editProblems) */
      puzzleHistory_c::discarded_t takeSolutions(const std::vector<unsigned int> & problems);
      /* into the step just recorded; with no step, back into the problems;
       * with no room in the history's budget, lost for good */
      void keepDiscarded(puzzleHistory_c::discarded_t && lost, bool recorded);

      /* the current state as one undo step */
      void record(puzzleHistory_c::actionKind_e kind, unsigned int selectedShape);

      /* The history holds no solutions but the solve records a step keeps
       * (keepDiscarded), so undo cannot bring other lost ones back: the file
       * still has them, and an undo to the save must not read clean. The
       * records the history or an open stroke keeps count as held. */
      long solutionsHeld(void) const;
      void noteSolutionsLost(long heldBefore);

      /* the reading of a staged load's solutions stopped and dropped; the
       * document is about to go */
      void stopLoading(void);
      void adopt(std::unique_ptr<puzzle_c> p);

      std::unique_ptr<puzzle_c> puz;
      std::unique_ptr<puzzleHistory_c> hist;
      std::filesystem::path fname;
      // reads into *puz: declared after it, so it goes first
      std::unique_ptr<solutionReader_c> reader;
      size_t stageAbove = SIZE_MAX;
      unsigned loadThreads = 1;
      bool stroking = false;
      bool strokeDirty = false;      // the open stroke changed something (hist has it too)
      puzzleHistory_c::discarded_t strokeLost;   // what the open stroke discarded, for its step
      puzzleHistory_c::actionKind_e strokeKind = puzzleHistory_c::AK_ENTITIES_GRID_PAINT;
      unsigned int strokeShape = puzzleHistory_c::NO_SHAPE;
  };
}

#endif
