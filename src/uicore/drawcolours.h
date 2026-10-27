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
#ifndef BTUI_DRAWCOLOURS_H
#define BTUI_DRAWCOLOURS_H

#include <cstdint>
#include <string>

class puzzle_c;

/* The puzzle's colours as the Drawing colour footer edits them (C09): add,
 * edit, remove, and what to call each. Legacy mainWindow_c does the same in
 * cb_AddColor, cb_ChangeColor and cb_RemoveColor (with changeColor); the
 * differences are named where they are made.
 *
 * Colours are given by id, as voxels store them: 0 is Default -- "the
 * piece's own colour", not in the puzzle's colour list -- and 1 ... n are
 * the puzzle's colours (puzzle_c::getColor(id - 1)). Each function changes
 * only the puzzle it is given; the caller makes it one undo step
 * (DocumentSession::edit, AK_COLOR_PALETTE). An id the GUI can produce but
 * that names no colour changes nothing, and nothing here throws for it.
 */
namespace btui {

  struct Rgb8 {
    std::uint8_t r = 0, g = 0, b = 0;
    bool operator==(const Rgb8 &) const = default;
  };

  /* The library's limit: a voxel holds a colour id in 6 bits, and 0 is
   * Default (puzzle_c::addColor asserts beyond it; legacy's Add did not
   * check, so it could reach the assert). */
  constexpr int maxColours = 63;

  /* "Default" for 0 (C0), "C1", "C2", ... for 1, 2, ...: a colour is called
   * by its id (product decision 2026-10-27; legacy's chips counted from C1
   * for Default, so its colours were C2, C3, ...; OQ-8) */
  std::string colourName(int id);

  /* how many ids there are: Default and the puzzle's colours */
  int colourCount(const puzzle_c & p);

  bool canAddColour(const puzzle_c & p);

  /* The colour + adds, at once, without a chooser (the mock's quick add,
   * product decision 2026-10-27; Edit changes it after): the first of a
   * fixed set of distinct colours -- the mock's five, then more -- that the
   * puzzle does not have yet; when it has them all, the set again in turn.
   * Legacy's Add opened a chooser starting from the selected colour. */
  Rgb8 presetColour(const puzzle_c & p);

  /* the RGB of a puzzle colour (id >= 1); black for Default or no colour */
  Rgb8 colourRgb(const puzzle_c & p, int id);

  /* Append a colour, and allow pieces of it in cells of it in every problem
   * (legacy cb_AddColor). Returns its id, -1 when the list is full. */
  int addColour(puzzle_c & p, Rgb8 c);

  /* Change a colour's RGB; Default cannot be changed. Returns whether it
   * changed. */
  bool editColour(puzzle_c & p, int id, Rgb8 c);

  /* Whether removing colour id loses what undo cannot bring back: legacy
   * clears every solution of a problem using a shape that has a voxel of
   * that colour, and the undo history keeps no solutions. True when such a
   * problem has saved solutions or a search state. */
  bool removeColourLosesSolutions(const puzzle_c & p, int id);

  /* Remove colour id as legacy does: the problems using a shape with a
   * voxel of it lose their solutions, its voxels become Default, the later
   * ids move down by one, and the problems' colour rules drop it. Returns
   * whether a colour was removed; Default cannot be. */
  bool removeColour(puzzle_c & p, int id);
}

#endif
