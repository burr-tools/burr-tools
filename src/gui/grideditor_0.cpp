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
#include "grideditor_0.h"

#include "../lib/voxel.h"
#include "../lib/puzzle.h"

#include <FL/fl_draw.H>

// round towards -inf instead of 0
static int floordiv(int a, int b) {
  if (a > 0)
    return a/b;
  else
    return (a-b+1)/b;
}

// this function calculates the size of the squares and the starting position
// for the grid inside the available space of the widget
void gridEditor_0_c::calcParameters(int *szx, int *szy, int *tx, int *ty) {

  // calculate the size of the squares
  int sx = (w() > 2) ? (w()-1) / puzzle->getShape(piecenumber)->getX() : 0;
  int sy = (h() > 2) ? (h()-1) / puzzle->getShape(piecenumber)->getY() : 0;

  *szx = (sx < sy) ? sx : sy;

  if (*szx > 20) *szx = 20;

  *szy = *szx;

  *tx = x() + (w() - puzzle->getShape(piecenumber)->getX()*(*szx) - 1) / 2;
  *ty = y() + (h() - puzzle->getShape(piecenumber)->getY()*(*szy) - 1) / 2;
}

void gridEditor_0_c::drawNormalTile(int x, int y, int, int tx, int ty, int sx, int sy) {
  fl_rectf(tx+x*sx, ty+y*sy, sx, sy);
}

void gridEditor_0_c::drawVariableTile(int x, int y, int, int tx, int ty, int sx, int sy) {
  fl_rectf(tx+x*sx+3, ty+y*sy+3, sx-5, sy-5);
}

void gridEditor_0_c::drawTileFrame(int x, int y, int, int tx, int ty, int sx, int sy) {
  fl_rect(tx+x*sx, ty+y*sy, sx+1, sy+1);
}

void gridEditor_0_c::drawTileColor(int x, int y, int, int tx, int ty, int sx, int sy) {
  fl_rectf(tx+x*sx, ty+y*sy, sx/2, sy/2);
}

void gridEditor_0_c::drawTileCursor(int x, int y, int, int x1, int y1, int x2, int y2, int tx, int ty, int sx, int sy) {
  voxel_c * space = puzzle->getShape(piecenumber);

  bool ins = inRegion(x, y, x1, x2, y1, y2, space->getX(), space->getY(), activeTools);

  if (ins ^ inRegion(x, y-1, x1, x2, y1, y2, space->getX(), space->getY(), activeTools)) {
    fl_line(tx+sx*x, ty+sy*y+1, tx+sx*x+sx, ty+sy*y+1);
    fl_line(tx+sx*x, ty+sy*y-1, tx+sx*x+sx, ty+sy*y-1);
  }

  if (ins ^ inRegion(x-1, y, x1, x2, y1, y2, space->getX(), space->getY(), activeTools)) {
    fl_line(tx+sx*x+1, ty+sy*y, tx+sx*x+1, ty+sy*y+sy);
    fl_line(tx+sx*x-1, ty+sy*y, tx+sx*x-1, ty+sy*y+sy);
  }
}

void gridEditor_0_c::calcGridPosition(int x, int y, int, int *gx, int *gy) {

  voxel_c * space = puzzle->getShape(piecenumber);

  int sx, sy, tx, ty;
  calcParameters(&sx, &sy, &tx, &ty);

  x -= tx;
  y -= ty;

  x = floordiv(x, sx);
  y = floordiv(y, sy);

  *gx = x;
  *gy = space->getY() - y - 1;
}

