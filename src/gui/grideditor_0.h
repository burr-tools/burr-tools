/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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
#ifndef __GRID_EDITOR_0_H__
#define __GRID_EDITOR_0_H__

#include "grideditor.h"

class puzzle_c;

/**
 * implements grid editor functions to edit the cube grid
 */
class gridEditor_0_c : public gridEditor_c {

private:

  // calculate the size of the grid cells and the
  // top right corner position
  void calcParameters(int *sx, int *sy, int *tx, int *ty);

  bool calcGridPosition(int x, int y, int z, int *gx, int *gy);

  void drawNormalTile(int x, int y, int z, int tx, int ty, int sx, int sy);
  void drawVariableTile(int x, int y, int z, int tx, int ty, int sx, int sy);
  void drawTileFrame(int x, int y, int z, int tx, int ty, int sx, int sy);
  void drawTileColor(int x, int y, int z, int tx, int ty, int sx, int sy);
  void drawTileCursor(int x, int y, int z, int tx, int ty, int sx, int sy);

public:

  gridEditor_0_c(int x, int y, int w, int h, puzzle_c * p) : gridEditor_c(x, y, w, h, p) {}
};

#endif
