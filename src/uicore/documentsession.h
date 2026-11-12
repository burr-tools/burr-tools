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

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class puzzle_c;
class voxel_c;

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
      };

      /* Load a BurrTools file. On failure the current document is untouched. */
      LoadResult load(const std::filesystem::path & file);

      /* Import a PuzzleSolver3D file; it becomes the document, unsaved and
       * without a file name, so the first Save asks where. (Legacy kept the
       * .puz name, and its Save wrote xmpuzzle over the imported file.)
       */
      LoadResult importPuzzleSolver3D(const std::filesystem::path & file);

      /* Convert the puzzle to another grid type (legacy cb_Convert): the
       * converted puzzle replaces this one, the undo history starts afresh and
       * the document counts as unsaved. False when the conversion fails. */
      bool convert(gridType_c::gridType to);

      /* Write to fileName(). False when there is no name yet or the write
       * failed; the document stays modified in both cases.
       */
      bool save(void);

      /* Write to file (with ".xmpuzzle" appended when missing) and adopt it as
       * the document's name. On failure neither the name nor the modified
       * state change.
       */
      bool saveAs(const std::filesystem::path & file);

      /* The name saveAs() will really write to. */
      static std::filesystem::path withPuzzleExtension(const std::filesystem::path & file);

      /* Unsaved changes, a stroke's too while its button is still down. */
      bool isModified(void) const;

      /* Unsaved, but not an undo step -- the comment, solver results. */
      void markModified(void);

      void setComment(const std::string & comment);

      /* Undo/redo. The result says which workspace and shape the change
       * belonged to, so the GUI can show it. A stroke that changed something
       * can be undone before it ends: undo() ends it first, which makes it a
       * step.
       */
      bool canUndo(void) const;
      bool canRedo(void) const;
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
        bool changed;
        try {
          changed = f(*puz);
        } catch (...) {
          record(kind, selectedShape);
          throw;
        }
        if (changed)
          record(kind, selectedShape);
        return changed;
      }

      /* An edit of shapes' voxels -- painting, a 3D click, the grid size, an
       * inspector task -- as edit(), naming the shapes f may change. A
       * problem that uses one of them whose voxels really changed loses its
       * solutions and search state, as legacy's changeShape() did; they are
       * kept in the undo step, so undo brings them back and redo drops them
       * again (PR 181 review: a saved file kept solutions that no longer fit
       * its pieces).
       */
      template <class F> bool editVoxels(puzzleHistory_c::actionKind_e kind, unsigned int selectedShape,
                                         const std::vector<unsigned int> & shapes, F && f) {
        const ShapeCopies before = copyShapes(shapes);
        puzzleHistory_c::discarded_t lost;
        bool changed;
        try {
          changed = edit(kind, selectedShape, [&](puzzle_c & p) {
            bool c;
            try {
              c = f(p);
            } catch (...) {
              lost = discardChanged(before);
              throw;
            }
            lost = discardChanged(before);
            return c;
          });
        } catch (...) {
          keepDiscarded(std::move(lost), true);   // the step was recorded all the same
          throw;
        }
        keepDiscarded(std::move(lost), changed);
        return changed;
      }

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
        bool changed;
        try {
          changed = f(*puz);
        } catch (...) {
          markStrokeChanged();
          throw;
        }
        if (changed)
          markStrokeChanged();
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
      /* into the step just recorded; with no step, back into the problems */
      void keepDiscarded(puzzleHistory_c::discarded_t && lost, bool recorded);

      /* the current state as one undo step */
      void record(puzzleHistory_c::actionKind_e kind, unsigned int selectedShape);

      void adopt(std::unique_ptr<puzzle_c> p);

      std::unique_ptr<puzzle_c> puz;
      std::unique_ptr<puzzleHistory_c> hist;
      std::filesystem::path fname;
      bool stroking = false;
      bool strokeDirty = false;      // the open stroke changed something (hist has it too)
      puzzleHistory_c::discarded_t strokeLost;   // what the open stroke discarded, for its step
      puzzleHistory_c::actionKind_e strokeKind = puzzleHistory_c::AK_ENTITIES_GRID_PAINT;
      unsigned int strokeShape = puzzleHistory_c::NO_SHAPE;
  };
}

#endif
