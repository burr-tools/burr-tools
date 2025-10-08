
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
#include "undomanager.h"

#include "../lib/puzzle.h"
#include "../lib/problem.h"
#include "../tools/xml.h"

#include <sstream>

/**
 * Return true if any problem in the puzzle is currently being solved.
 */
static bool puzzleIsBeingSolved(puzzle_c *puzzle) {
  for (unsigned int i=0; i<puzzle->getNumberOfProblems(); i++) {
    if(puzzle->getProblem(i)->getSolveState() == SS_SOLVING) {
      return true;
    }
  }
  return false;
}

UndoManager_c::UndoManager_c(void) {
    currentState = -1;
    savedState = -1;
}
UndoManager_c::~UndoManager_c(void) { }

void UndoManager_c::loadNew(puzzle_c * puzzle) {
  currentState = -1;
  savedState = -1;
  states.resize(0);

  recordState(puzzle, "Loaded Puzzle");
  markSaved();
}

void UndoManager_c::recordState(puzzle_c * puzzle, std::string description) {

  // For thread safety, skip recording if a problem is being solved.
  // See discussion of thread safety in undomanager.h.
  if (puzzleIsBeingSolved(puzzle)) {
    return;
  }

  std::ostringstream stateString;
  xmlWriter_c xml(stateString);
  puzzle->save(xml);

  if (savedState > currentState) {
    savedState = -1;
  }

  states.resize(currentState+1);
  states.push_back({
    .xml = stateString.str(),
    .description = description,
  });
  currentState = states.size()-1;
}

puzzle_c * UndoManager_c::undo(std::string *description_out) {
  if (!canUndo()) { return NULL; }
  *description_out = states[currentState].description;
  currentState--;

  std::istringstream stateString(states[currentState].xml);
  xmlParser_c xml(stateString);
  return new puzzle_c(xml);
}

puzzle_c * UndoManager_c::redo(std::string *description_out) {
  if (!canRedo()) { return NULL; }
  currentState++;
  *description_out = states[currentState].description;

  std::istringstream stateString(states[currentState].xml);
  xmlParser_c xml(stateString);
  return new puzzle_c(xml);
}

bool UndoManager_c::canUndo(void) {
  return currentState > 0;
}

bool UndoManager_c::canRedo(void) {
  return currentState >= 0 && currentState < (int) states.size()-1;
}

void UndoManager_c::markSaved(void) {
  savedState = currentState;
}

bool UndoManager_c::isChanged(void) {
  return currentState != -1 && currentState != savedState;
}