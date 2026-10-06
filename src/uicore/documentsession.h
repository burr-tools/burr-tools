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

#include "../lib/gridtype.h"
#include "../gui/puzzlehistory.h"

#include <filesystem>
#include <memory>
#include <string>

class puzzle_c;

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

      puzzle_c & puzzle(void) { return *puz; }
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

      /* Import a PuzzleSolver3D file; it becomes the document, under that
       * file's name, exactly as legacy did.
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

      bool isModified(void) const;

      /* Unsaved, but not an undo step -- the comment, solver results. */
      void markModified(void);

      void setComment(const std::string & comment);

      /* Undo/redo. The result says which workspace and shape the change
       * belonged to, so the GUI can show it.
       */
      bool canUndo(void) const;
      bool canRedo(void) const;
      puzzleHistory_c::undoResult_t undo(void);
      puzzleHistory_c::undoResult_t redo(void);

      /* Record the current state as one undo step. */
      void record(puzzleHistory_c::actionKind_e kind, unsigned int selectedShape = puzzleHistory_c::NO_SHAPE);

      void setUndoDepth(unsigned int depth);

      puzzleHistory_c & history(void) { return *hist; }

    private:

      void adopt(std::unique_ptr<puzzle_c> p);

      std::unique_ptr<puzzle_c> puz;
      std::unique_ptr<puzzleHistory_c> hist;
      std::filesystem::path fname;
  };
}

#endif
