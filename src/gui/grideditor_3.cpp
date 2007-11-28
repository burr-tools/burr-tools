/* Burr Solver
 * Copyright (C) 2003-2007  Andreas Röver
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
#include "grideditor_3.h"

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
void gridEditor_3_c::calcParameters(int *szx, int *szy, int *tx, int *ty) {

  // find out how many different squared there are
  int vx = (puzzle->getShape(piecenumber)->getX() + 4) / 5;
  int vy = (puzzle->getShape(piecenumber)->getY() + 4) / 5;

  // calculate the size of the squares
  int sx = (w() > 2) ? (w()-1) / vx : 0;
  int sy = (h() > 2) ? (h()-1) / vy : 0;

  *szx = (sx < sy) ? sx : sy;

  if (*szx > 40) *szx = 40;

  *szx &= ~1;

  *szy = *szx;

  *tx = x()       + (w() - vx*(*szx) - 1) / 2;
  *ty = y()+h()-1 - (h() - vy*(*szy) - 1) / 2;
}

static void getTriangle(int x, int y, int z, int tx, int ty, int sx, int sy,
    int *x1, int *y1, int *x2, int *y2, int *x3, int *y3, int offs) {

  int xc = x / 5;
  int yc = y / 5;

  x %= 5;
  y %= 5;
  z %= 5;

  if (z == 2) {

    if (x == 0 && y == 1) {

      *x1 = tx+xc*sx+sx/2-2*offs; *y1 = ty-yc*sy-sy/2  +offs;
      *x2 = tx+xc*sx       +offs; *y2 = ty-yc*sy-sy/2  +offs;
      *x3 = tx+xc*sx       +offs; *y3 = ty-yc*sy     -2*offs;

    } else if (x == 0 && y == 3) {

      *x1 = tx+xc*sx+sx/2-2*offs; *y1 = ty-yc*sy-sy/2  -offs;
      *x2 = tx+xc*sx       +offs; *y2 = ty-yc*sy-sy/2  -offs;
      *x3 = tx+xc*sx       +offs; *y3 = ty-yc*sy-sy  +2*offs;

    } else if (x == 1 && y == 0) {

      *x1 = tx+xc*sx+sx/2  -offs; *y1 = ty-yc*sy-sy/2+2*offs;
      *x2 = tx+xc*sx+sx/2  -offs; *y2 = ty-yc*sy       -offs;
      *x3 = tx+xc*sx     +2*offs; *y3 = ty-yc*sy       -offs;

    } else if (x == 1 && y == 4) {

      *x1 = tx+xc*sx+sx/2  -offs; *y1 = ty-yc*sy-sy/2-2*offs;
      *x2 = tx+xc*sx+sx/2  -offs; *y2 = ty-yc*sy-sy    +offs;
      *x3 = tx+xc*sx     +2*offs; *y3 = ty-yc*sy-sy    +offs;

    } else if (x == 3 && y == 0) {

      *x1 = tx+xc*sx+sx/2  +offs; *y1 = ty-yc*sy-sy/2+2*offs;
      *x2 = tx+xc*sx+sx/2  +offs; *y2 = ty-yc*sy       -offs;
      *x3 = tx+xc*sx+sx  -2*offs; *y3 = ty-yc*sy       -offs;

    } else if (x == 3 && y == 4) {

      *x1 = tx+xc*sx+sx/2  +offs; *y1 = ty-yc*sy-sy/2-2*offs;
      *x2 = tx+xc*sx+sx/2  +offs; *y2 = ty-yc*sy-sy    +offs;
      *x3 = tx+xc*sx+sx  -2*offs; *y3 = ty-yc*sy-sy    +offs;

    } else if (x == 4 && y == 1) {

      *x1 = tx+xc*sx+sx/2+2*offs; *y1 = ty-yc*sy-sy/2  +offs;
      *x2 = tx+xc*sx+sx    -offs; *y2 = ty-yc*sy-sy/2  +offs;
      *x3 = tx+xc*sx+sx    -offs; *y3 = ty-yc*sy     -2*offs;

    } else if (x == 4 && y == 3) {

      *x1 = tx+xc*sx+sx/2+2*offs; *y1 = ty-yc*sy-sy/2  -offs;
      *x2 = tx+xc*sx+sx    -offs; *y2 = ty-yc*sy-sy/2  -offs;
      *x3 = tx+xc*sx+sx    -offs; *y3 = ty-yc*sy-sy  +2*offs;
    }

  } else {

    if (x < 2) {

      *x1 = tx+xc*sx     +offs; *y1 = ty-yc*sy     -2*offs;
      *x2 = tx+xc*sx+sx/2-offs; *y2 = ty-yc*sy-sy/2+0;
      *x3 = tx+xc*sx     +offs; *y3 = ty-yc*sy-sy  +2*offs;

    } else if (x > 2) {

      *x1 = tx+xc*sx+sx  -offs; *y1 = ty-yc*sy     -2*offs;
      *x2 = tx+xc*sx+sx/2+offs; *y2 = ty-yc*sy-sy/2+0;
      *x3 = tx+xc*sx+sx  -offs; *y3 = ty-yc*sy-sy  +2*offs;

    } else if (y < 2) {

      *x1 = tx+xc*sx     +2*offs; *y1 = ty-yc*sy     -offs;
      *x2 = tx+xc*sx+sx/2+0;      *y2 = ty-yc*sy-sy/2+offs;
      *x3 = tx+xc*sx+sx  -2*offs; *y3 = ty-yc*sy     -offs;

    } else {

      *x1 = tx+xc*sx     +2*offs; *y1 = ty-yc*sy-sy  +offs;
      *x2 = tx+xc*sx+sx/2+0;      *y2 = ty-yc*sy-sy/2-offs;
      *x3 = tx+xc*sx+sx  -2*offs; *y3 = ty-yc*sy-sy  +offs;

    }
  }
}

void gridEditor_3_c::drawNormalTile(int x, int y, int z, int tx, int ty, int sx, int sy) {
  int x1, y1, x2, y2, x3, y3;

  getTriangle(x, y, z, tx, ty, sx, sy, &x1, &y1, &x2, &y2, &x3, &y3, 1);

  fl_polygon(x1, y1, x2, y2, x3, y3);
}

void gridEditor_3_c::drawVariableTile(int x, int y, int z, int tx, int ty, int sx, int sy) {
  int x1, y1, x2, y2, x3, y3;

  getTriangle(x, y, z, tx, ty, sx, sy, &x1, &y1, &x2, &y2, &x3, &y3, 3);

  fl_polygon(x1, y1, x2, y2, x3, y3);
}

void gridEditor_3_c::drawTileFrame(int x, int y, int z, int tx, int ty, int sx, int sy) {
  int x1, y1, x2, y2, x3, y3;

  getTriangle(x, y, z, tx, ty, sx, sy, &x1, &y1, &x2, &y2, &x3, &y3, 0);

  fl_loop(x1, y1, x2, y2, x3, y3);
}

void gridEditor_3_c::drawTileColor(int x, int y, int z, int tx, int ty, int sx, int sy) {
  int x1, y1, x2, y2, x3, y3;

  getTriangle(x, y, z, tx, ty, sx, sy, &x1, &y1, &x2, &y2, &x3, &y3, 1);

  x1 = (x1+x2)/2;
  x3 = (x3+x2)/2;

  y1 = (y1+y2)/2;
  y3 = (y3+y2)/2;

  fl_polygon(x1, y1, x2, y2, x3, y3);
}

void gridEditor_3_c::drawTileCursor(int x, int y, int z, int tx, int ty, int sx, int sy) {

  bool ins = inRegion(x, y);

  if (ins) {
    int xd1, yd1, xd2, yd2, xd3, yd3;

    getTriangle(x, y, z, tx, ty, sx, sy, &xd1, &yd1, &xd2, &yd2, &xd3, &yd3, 0);

    fl_loop(xd1-1, yd1, xd2-1, yd2, xd3-1, yd3);
    fl_loop(xd1+1, yd1, xd2+1, yd2, xd3+1, yd3);
    fl_loop(xd1, yd1-1, xd2, yd2-1, xd3, yd3-1);
    fl_loop(xd1, yd1+1, xd2, yd2+1, xd3, yd3+1);
  }
}

bool gridEditor_3_c::calcGridPosition(int x, int y, int z, int *gx, int *gy) {

  voxel_c * space = puzzle->getShape(piecenumber);

  int sx, sy, tx, ty;
  calcParameters(&sx, &sy, &tx, &ty);

  if (sx == 0 || sy == 0) return false;

  int by = (space->getY()+4)/5;

  x -= tx;
  y -= (ty-sy*by);

  int xp = x;
  int yp = y;

  x = floordiv(x, sx);
  y = floordiv(y, sy);

  xp -= x*sx;
  yp -= y*sy;

  y = by-y-1;

  x *= 5;
  y *= 5;

  // x and y now contains the square we are in, depending on that
  // we need to find out which triangle

  int mask = 0;
  if (xp<yp)    mask |= 1;
  if (xp+yp<sx) mask |= 2;

  if (z % 5 == 2) {

    switch (mask) {
      case 0:
        x += 4;
        // fallthrough
      case 3:
        if (yp > sx/2) y += 1;
        else           y += 3;
        break;
      case 2:
        y += 4;
        // fallthrough
      case 1:
        if (xp < sx/2) x += 1;
        else           x += 3;
        break;
    }

  } else {

    switch (mask) {
      case 0:
        y += 2;
        if (z%5 == 0 || z%5 == 4) x += 3;
        else                      x += 4;
        break;
      case 3:
        y += 2;
        if (z%5 == 0 || z%5 == 4) x += 1;
        break;
      case 1:
        x += 2;
        if (z%5 == 0 || z%5 == 4) y += 1;
        break;
      case 2:
        x += 2;
        if (z%5 == 0 || z%5 == 4) y += 3;
        else                      y += 4;
        break;
    }
  }

  if (y >= (int)space->getY() || x >= (int)space->getX()) return false;

  *gx = x;
  *gy = y;

  return true;
}

