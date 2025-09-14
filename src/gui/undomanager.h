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
#ifndef __UNDO_MANAGER_H__
#define __UNDO_MANAGER_H__

#include "../lib/puzzle.h"

#include <string>

/**
 * Manages saving and loading states for undo and redo.
 *
 * There are a few thread safety issues to consider here:
 *
 * 1) We don't want to record states while the solve thread is running, since
 * solutions could be added in the solve thread, and we don't want to undo
 * those additions.
 *
 * 2) We can't undo or redo while the solve thread is running. Doing so could
 * crash the solver thread due to changes in the puzzle (for example deleting a
 * piece the solve thread was using). This is normally prevented by disabling
 * edits in the GUI.
 *
 * 3) Calling recordState/undo/redo could also read or write the puzzle in the
 * middle of an update from the solve thread, possibly at an inconsistent
 * state.
 *
 * Here's how the user of this class (mainWindow_c) must handle thread safety:
 *   * Do not call undo or redo while the solve thread is running.
 *   * Be aware that recordState is a no-op if a problem is being solved in the
 *     given puzzle. This is not usually a problem since the user can't make
 *     most edits while the puzzle is being solved anyways.
 *
 * This leaves one quirk of how mainWindow_c uses this class: since it calls
 * recordState at the end of solving, an undo after solving finishes will undo
 * all changes made during the solve.
 */
class UndoManager_c {

public:

  UndoManager_c(void);
  ~UndoManager_c(void);

  /**
   * Starts a new set of undo history. Called when a new puzzle is loaded or created.
   */
  void loadNew(puzzle_c * puzzle);

  /**
   * Saves the current state of the puzzle for undo. The given description
   * should describe the change which was made from the last recorded state.
   */
  void recordState(puzzle_c * puzzle, std::string description);

  /**
   * Returns the puzzle from the previous state in the undo history, and sets that
   * state as current. Returns NULL if there is nothing to undo.
   * `description_out` will be set to a description of the change which was
   * undone.
   */
  puzzle_c * undo(std::string *description_out);

  /**
   * Returns the puzzle from the next state in the undo history, and sets that state
   * as current. Returns NULL if there is nothing to redo.
   * `description_out` will be set to a description of the change which was
   * undone.
   */
  puzzle_c * redo(std::string *description_out);

  /**
   * Returns true if there is something to undo.
   */
  bool canUndo(void);

  /**
   * Returns true if there is something to redo.
   */
  bool canRedo(void);

  /**
   * Marks the current state as saved to disk, so isChanged() will return false.
   */
  void markSaved(void);

  /**
   * Returns true if the currently saved state is not saved to disk.
   */
  bool isChanged(void);

private:

  typedef struct {
    std::string xml;
    std::string description;
  } UndoState;

  std::vector<UndoState> states;
  int currentState;
  int savedState;

};

#endif
