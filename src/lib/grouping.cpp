/* Burr Solver
 * Copyright (C) 2003-2006  Andreas Röver
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
#include "grouping.h"

#include <assert.h>

void grouping_c::addPieces(unsigned int pc, unsigned int group, unsigned int count) {

  if (group == 0)
    return;

  unsigned int i = findPiece(pc, group);

  if (i < pieces.size())
    pieces[i].count += count;

  else {

    struct piece s;

    s.piece = pc;
    s.group = group;
    s.count = count;

    pieces.push_back(s);

    if (group+1 > numGroups) numGroups = group+1;
  }
}

void grouping_c::reSet(void) {
  sets.clear();
  failed = false;
}

void grouping_c::newSet(void) {

  if (!failed) {

    struct set s;

    s.currentGroup = 1;

    sets.push_back(s);
  }
}

bool grouping_c::addPieceToSet(unsigned int pc) {

  if (failed) return false;

  assert(sets.size() > 0);

  unsigned int set = sets.size()-1;

  // add the piece to the current set
  sets[set].pieces.push_back(pc);

  // try to add the set to the pieces
  unsigned int i = findPiece(pc, sets[set].currentGroup);

  // check, if the required group piece combination is available
  if ((i < pieces.size()) && (pieces[i].count > 0)) {

    pieces[i].count--;
    return true;

  } else {
    // not available -> remove set from group
    for (unsigned int p = 0; p < sets[set].pieces.size()-1; p++)
      pieces[findPiece(sets[set].pieces[p], sets[set].currentGroup)].count++;
  }

  // OK when we get here something doesn't fit, so
  // take the last set and increase the group
  // if we reached the last group restart with the
  // first and increment the 2nd last group, until
  // the first group reached the end
  // as soon as we find something suitable in here, stop

  // now increment the groups until we either find a fitting
  // assignment or tried everything
  do {

    // increment current Group
    sets[set].currentGroup++;

    // all groups have been tried for this set
    if (sets[set].currentGroup >= numGroups) {
      sets[set].currentGroup = 0;

      if (set == 0) {
        failed = true;
        return false;
      }

      set--;

      // remove pieces of the current set from the current group
      for (unsigned int p = 0; p < sets[set].pieces.size(); p++)
        pieces[findPiece(sets[set].pieces[p], sets[set].currentGroup)].count++;

    } else {

      // try to place the pieces of the set into the current group

      for (unsigned int p = 0; p < sets[set].pieces.size(); p++) {
        unsigned int i = findPiece(sets[set].pieces[p], sets[set].currentGroup);

        // if the required group doesn't exists or not enough pieces are in there
        // try again with next group for set
        if ((i < pieces.size()) && (pieces[i].count > 0))
          pieces[i].count--;

        else {

          // remove already placed pieces from the group
          for (unsigned int p2 = 0; p2 < p; p++)
            pieces[findPiece(sets[set].pieces[p2], sets[set].currentGroup)].count++;

          set--;
          break;

        }
      }
      set++;
    }

  } while (set < sets.size());

  return true;
}
