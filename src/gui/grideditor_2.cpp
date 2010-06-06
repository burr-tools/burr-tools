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
#include "grideditor_2.h"

#include "../lib/voxel.h"
#include "../lib/puzzle.h"

#include "../tools/intdiv.h"

#include <FL/fl_draw.H>

// this function calculates the size of the circles and the starting position
// for the grid inside the available space of the widget
void gridEditor_2_c::calcParameters(int *szx, int *szy, int *tx, int *ty) {

  // find out how many circles there must fit (this is double the
  // size of the circles, so that we can have halve circles
  int cx = puzzle->getShape(piecenumber)->getX();
  int cy = puzzle->getShape(piecenumber)->getY();

  // calculate the size of the squares
  int sx = (w() > 2) ? ((10*w()-30)/(10*cx+6)) : 0;
  int sy = (h() > 2) ? ((10*h()-30)/(10*cy+6)) : 0;

  *szx = (sx < sy) ? sx : sy;

  if (*szx > 20) *szx = 20;

  *szy = *szx;
  sx = sy = *szx;

  *tx = x()       + (w() - cx*sx) / 2;
  *ty = y()+h()-1 - (h() - cy*sy) / 2;
}

void gridEditor_2_c::drawNormalTile(int x, int y, int z, int tx, int ty, int sx, int sy) {

  bt_assert(puzzle->getShape(piecenumber)->validCoordinate(x, y, z));

  int sxc = 10000*sx/7072;
  int syc = 10000*sy/7072;

  tx -= 3*sx/10;
  ty -= 3*sy/10;

  fl_pie(tx+x*sx, ty-y*sy-sy, sxc, syc, 0, 360);
}

void gridEditor_2_c::drawVariableTile(int x, int y, int z, int tx, int ty, int sx, int sy) {

  bt_assert(puzzle->getShape(piecenumber)->validCoordinate(x, y, z));

  int sxc = 10000*sx/7072;
  int syc = 10000*sy/7072;

  tx -= 3*sx/10;
  ty -= 3*sy/10;

  fl_pie(tx+x*sx+2, ty-y*sy+2-sy, sxc-4, syc-4, 0, 360);
}

void gridEditor_2_c::drawTileFrame(int x, int y, int z, int tx, int ty, int sx, int sy) {

  bt_assert(puzzle->getShape(piecenumber)->validCoordinate(x, y, z));

  int sxc = 10000*sx/7072;
  int syc = 10000*sy/7072;

  tx -= 3*sx/10;
  ty -= 3*sy/10;

  fl_arc(tx+x*sx, ty-y*sy-sy, sxc, syc, 0, 360);
}

void gridEditor_2_c::drawTileColor(int x, int y, int z, int tx, int ty, int sx, int sy) {

  bt_assert(puzzle->getShape(piecenumber)->validCoordinate(x, y, z));

  int sxc = 10000*sx/7072;
  int syc = 10000*sy/7072;

  tx -= 3*sx/20;
  ty -= 3*sy/20;

  fl_pie(tx+x*sx, ty-y*sy-sy, sxc/2, syc/2, 0, 360);
}

void gridEditor_2_c::drawTileCursor(int x, int y, int /*z*/, int tx, int ty, int sx, int sy) {

  int sxc = 10000*sx/7072;
  int syc = 10000*sy/7072;

  tx -= 3*sx/10;
  ty -= 3*sy/10;

  if (inRegion(x, y)) {

    fl_arc(tx+x*sx-1, ty-y*sy-sy-1, sxc+2, syc+2, 0, 360);

    fl_arc(tx+x*sx-1, ty-y*sy-sy-1, sxc+1, syc+1, 0, 360);
    fl_arc(tx+x*sx,   ty-y*sy-sy-1, sxc+1, syc+1, 0, 360);
    fl_arc(tx+x*sx-1, ty-y*sy-sy,   sxc+1, syc+1, 0, 360);
    fl_arc(tx+x*sx,   ty-y*sy-sy,   sxc+1, syc+1, 0, 360);
  }
}

bool gridEditor_2_c::calcGridPosition(int x, int y, int /*z*/, int *gx, int *gy) {

  voxel_c * space = puzzle->getShape(piecenumber);

  int sx, sy, tx, ty;
  calcParameters(&sx, &sy, &tx, &ty);

  if (sx == 0 || sy == 0) return false;

  x -= tx;
  y -= (ty-sy*space->getY());

  x = intdiv_inf(x, sx);
  y = intdiv_inf(y, sy);

  *gx = x;
  *gy = space->getY() - y - 1;

  return true;
}

