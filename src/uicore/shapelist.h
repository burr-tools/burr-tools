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
#ifndef BTUI_SHAPELIST_H
#define BTUI_SHAPELIST_H

#include "../lib/gridtype.h"

#include <string>

class puzzle_c;

/* The Shapes list's operations (C01), without the list: what New, Duplicate,
 * Delete, Rename, the weight buttons and a reorder do to the puzzle. Legacy
 * mainWindow_c does the same in cb_NewShape, cb_CopyShape, cb_DeleteShape,
 * cb_NameShape, cb_WeightChange and cb_ShapeExchange; the differences are
 * the spec's and are named where they are made.
 *
 * Each function changes the puzzle it is given and nothing else; the caller
 * makes it one undo step (DocumentSession::edit). Shapes are given by their
 * position, as the GUI's rows are, and a position the GUI can produce but
 * that names no shape -- -1, or one past the end while a view tears down --
 * changes nothing. None of them throws for such input.
 *
 * A shape's id is its position (S1 ... SN): it is not stored, so every
 * operation that moves shapes renumbers them. The problems refer to shapes
 * by position too, and the library keeps those references with their shapes
 * when shapes are exchanged or removed (OQ-16).
 */
namespace btui {

  /* "S1" for position 0 */
  std::string shapeId(int position);

  /* The edge length of a new shape when there is none to copy the size from
   * (legacy guiGridType_c::defaultSize). */
  unsigned int defaultShapeSize(gridType_c::gridType type);

  /* An empty shape appended to the list: as big as the selected shape, or
   * the grid's default cube when nothing is selected (legacy cb_NewShape,
   * OQ-14). Returns its position. */
  int newShape(puzzle_c & p, int selected);

  /* A copy of shape i -- voxels, size, label and weight -- appended to the
   * list (the spec: never inserted after its source, which would renumber
   * the shapes after it). Returns its position, -1 if i is no shape. */
  int duplicateShape(puzzle_c & p, int i);

  /* Delete needs a second shape to remain (AC-C01-05; legacy allowed none). */
  bool canDeleteShape(const puzzle_c & p, int i);

  /* Whether deleting shape i loses what undo cannot bring back: the saved
   * solutions or the search state of a problem that uses it. The undo
   * history keeps the problems' structure, not their solutions. */
  bool deleteLosesSolutions(const puzzle_c & p, int i);

  /* Delete shape i; the problems drop it as the library does. Returns the
   * shape to select after it: the one now at i, else the new last one
   * (legacy, and the spec's "nearest remaining"); -1 when nothing was
   * deleted. */
  int deleteShape(puzzle_c & p, int i);

  /* Move shape `from` to position `to` (clamped to the list), shifting the
   * shapes between; every reference follows its shape. Done as adjacent
   * exchanges, legacy's ←/→ repeated. Returns whether anything moved. */
  bool moveShape(puzzle_c & p, int from, int to);

  /* Set shape i's label, without control characters (UTF-8 is kept as
   * is); empty means none. Returns whether it changed. */
  bool renameShape(puzzle_c & p, int i, const std::string & label);

  /* Shape i's weight plus delta, stepping down no further than 0 (legacy
   * had no floor). A weight below 0 from a legacy file goes up by delta and
   * does not go further down. Returns whether it changed. */
  bool changeWeight(puzzle_c & p, int i, int delta);
}

#endif
