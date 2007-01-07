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
#include "grideditor_2.h"

#include "../lib/voxel.h"
#include "../lib/puzzle.h"

#include <FL/fl_draw.h>

// round towards -inf instead of 0
static int floordiv(int a, int b) {
  if (a > 0)
    return a/b;
  else
    return (a-b+1)/b;
}

// this function calculates the size of the circles and the starting position
// for the grid inside the available space of the widget
void gridEditor_2_c::calcParameters(int *szx, int *szy, int *tx, int *ty) {

  // find out how many circles there must fit (this is double the
  // size of the circles, so that we can have halve circles
  int cx = puzzle->getShape(piecenumber)->getX();
  int cy = puzzle->getShape(piecenumber)->getY();

  // calculate the size of the squares
  int sx = (w() > 2) ? (10000*(w()-3)) / (cx*7071) : 0;
  int sy = (h() > 2) ? (10000*(h()-3)) / (cy*7071) : 0;

  *szx = (sx < sy) ? sx : sy;

  if (*szx > 20) *szx = 20;

  *szy = *szx;

  *tx = x() + (w() - 7071*puzzle->getShape(piecenumber)->getX()*(*szx)/10000 - 3) / 2;
  *ty = y() + (h() - 7071*puzzle->getShape(piecenumber)->getY()*(*szy)/10000 - 3) / 2;
}

void gridEditor_2_c::drawNormalTile(int x, int y, int z, int tx, int ty, int sx, int sy) {

  voxel_c * space = puzzle->getShape(piecenumber);
  // only draw tiles with x+y+z & 1 == 0

  if (((x+(space->getY()-y)+z-1) & 1) != 0)
    return;

  int sxc = 7071*sx/10000;
  int syc = 7071*sy/10000;

  fl_pie(tx+x*sxc, ty+y*syc, sx, sy, 0, 360);
}

void gridEditor_2_c::drawVariableTile(int x, int y, int z, int tx, int ty, int sx, int sy) {

  voxel_c * space = puzzle->getShape(piecenumber);

  if (((x+(space->getY()-y)+z-1) & 1) != 0)
    return;

  int sxc = 7071*sx/10000;
  int syc = 7071*sy/10000;

  fl_pie(tx+x*sxc+2, ty+y*syc+2, sx-4, sy-4, 0, 360);
}

void gridEditor_2_c::drawTileFrame(int x, int y, int z, int tx, int ty, int sx, int sy) {

  voxel_c * space = puzzle->getShape(piecenumber);

  if (((x+(space->getY()-y)+z-1) & 1) != 0)
    return;

  int sxc = 7071*sx/10000;
  int syc = 7071*sy/10000;

  fl_arc(tx+x*sxc, ty+y*syc, sx, sy, 0, 360);
}

void gridEditor_2_c::drawTileColor(int x, int y, int z, int tx, int ty, int sx, int sy) {

  voxel_c * space = puzzle->getShape(piecenumber);

  if (((x+(space->getY()-y)+z) & 1) != 0)
    return;

  int sxc = 7071*sx/10000;
  int syc = 7071*sy/10000;

  fl_pie(tx+x*sxc, ty+y*syc, sx/2, sy/2, 0, 360);
}

void gridEditor_2_c::drawTileCursor(int x, int y, int z, int x1, int y1, int x2, int y2, int tx, int ty, int sx, int sy) {

  voxel_c * space = puzzle->getShape(piecenumber);

  int sxc = 7071*sx/10000;
  int syc = 7071*sy/10000;

  bool ins = inRegion(x, y, x1, x2, y1, y2, space->getX(), space->getY(), activeTools);

  if (ins && (((x+(space->getY()-y-1)+z) & 1) == 0)) {
    fl_arc(tx+x*sxc-1, ty+y*syc-1, sx+2, sy+2, 0, 360);

    fl_arc(tx+x*sxc-1, ty+y*syc-1, sx+1, sy+1, 0, 360);
    fl_arc(tx+x*sxc,   ty+y*syc-1, sx+1, sy+1, 0, 360);
    fl_arc(tx+x*sxc-1, ty+y*syc,   sx+1, sy+1, 0, 360);
    fl_arc(tx+x*sxc,   ty+y*syc,   sx+1, sy+1, 0, 360);
  }
}

void gridEditor_2_c::calcGridPosition(int x, int y, int z, int *gx, int *gy) {

  voxel_c * space = puzzle->getShape(piecenumber);

  int sx, sy, tx, ty;
  calcParameters(&sx, &sy, &tx, &ty);

  x -= tx;
  y -= ty;

  x -= 2929*sx/20000;
  y -= 2929*sy/20000;

  x = floordiv(x, 7071*sx/10000);
  y = floordiv(y, 7071*sy/10000);

  *gx = x;
  *gy = space->getY() - y - 1;
}

