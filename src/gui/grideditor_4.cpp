/* Burr Solver
 * Copyright (C) 2003-2009  Andreas Röver
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
#include "grideditor_4.h"

#include "../lib/voxel.h"
#include "../lib/puzzle.h"

#include "../tools/intdiv.h"

#include <FL/fl_draw.H>

// this function calculates the size of the squares and the starting position
// for the grid inside the available space of the widget
void gridEditor_4_c::calcParameters(int *szx, int *szy, int *tx, int *ty) {

  // find out how many different squared there are
  int vx = (puzzle->getShape(piecenumber)->getX() + 2) / 3;
  int vy = (puzzle->getShape(piecenumber)->getY() + 2) / 3;

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

static int getPolygon(int x, int y, int z, int tx, int ty, int sx, int sy,
    int *x1, int *y1, int *x2, int *y2, int *x3, int *y3, int *x4, int *y4, int offs) {

  int xc = intdiv_inf(x, 3);
  int yc = intdiv_inf(y, 3);
  int zc = intdiv_inf(z, 3);

  x -= 3*xc;
  y -= 3*yc;
  z -= 3*zc;

  if (z == 1) {

    *x1 = tx+xc*sx   +offs;   *y1 = ty-yc*sy   -offs;
    *x2 = tx+xc*sx+sx-offs;   *y2 = ty-yc*sy   -offs;
    *x3 = tx+xc*sx+sx-offs;   *y3 = ty-yc*sy-sy+offs;
    *x4 = tx+xc*sx   +offs;   *y4 = ty-yc*sy-sy+offs;

    return 4;

  } else if (x == 0 && y == 0) {

    *x1 = tx+xc*sx     +offs;   *y1 = ty-yc*sy     -offs;
    *x2 = tx+xc*sx+sx-2*offs;   *y2 = ty-yc*sy     -offs;
    *x3 = tx+xc*sx     +offs;   *y3 = ty-yc*sy-sy+2*offs;

  } else if (x == 2 && y == 0) {

    *x1 = tx+xc*sx+sx  -offs;   *y1 = ty-yc*sy     -offs;
    *x2 = tx+xc*sx+sx  -offs;   *y2 = ty-yc*sy-sy+2*offs;
    *x3 = tx+xc*sx   +2*offs;   *y3 = ty-yc*sy     -offs;

  } else if (x == 0 && y == 2) {

    *x1 = tx+xc*sx     +offs;   *y1 = ty-yc*sy-sy  +offs;
    *x2 = tx+xc*sx     +offs;   *y2 = ty-yc*sy   -2*offs;
    *x3 = tx+xc*sx+sx-2*offs;   *y3 = ty-yc*sy-sy  +offs;

  } else if (x == 2 && y == 2) {

    *x1 = tx+xc*sx+sx  -offs;   *y1 = ty-yc*sy-sy  +offs;
    *x2 = tx+xc*sx   +2*offs;   *y2 = ty-yc*sy-sy  +offs;
    *x3 = tx+xc*sx+sx  -offs;   *y3 = ty-yc*sy   -2*offs;

  } else {
    bt_assert(0);
  }

  return 3;
}

void gridEditor_4_c::drawNormalTile(int x, int y, int z, int tx, int ty, int sx, int sy) {
  int x1, y1, x2, y2, x3, y3, x4, y4;

  if (getPolygon(x, y, z, tx, ty, sx, sy, &x1, &y1, &x2, &y2, &x3, &y3, &x4, &y4, 1) == 3)
    fl_polygon(x1, y1, x2, y2, x3, y3);
  else
    fl_polygon(x1, y1, x2, y2, x3, y3, x4, y4);
}

void gridEditor_4_c::drawVariableTile(int x, int y, int z, int tx, int ty, int sx, int sy) {
  int x1, y1, x2, y2, x3, y3, x4, y4;

  if (getPolygon(x, y, z, tx, ty, sx, sy, &x1, &y1, &x2, &y2, &x3, &y3, &x4, &y4, 3) == 3)
    fl_polygon(x1, y1, x2, y2, x3, y3);
  else
    fl_polygon(x1, y1, x2, y2, x3, y3, x4, y4);
}

void gridEditor_4_c::drawTileFrame(int x, int y, int z, int tx, int ty, int sx, int sy) {
  int x1, y1, x2, y2, x3, y3, x4, y4;

  if (getPolygon(x, y, z, tx, ty, sx, sy, &x1, &y1, &x2, &y2, &x3, &y3, &x4, &y4, 0) == 3)
    fl_loop(x1, y1, x2, y2, x3, y3);
  else
    fl_loop(x1, y1, x2, y2, x3, y3, x4, y4);
}

void gridEditor_4_c::drawTileColor(int x, int y, int z, int tx, int ty, int sx, int sy) {
  int x1, y1, x2, y2, x3, y3, x4, y4;

  if (getPolygon(x, y, z, tx, ty, sx, sy, &x1, &y1, &x2, &y2, &x3, &y3, &x4, &y4, 1) == 3)
  {
    x1 = (x1+x2)/2;
    x3 = (x3+x2)/2;

    y1 = (y1+y2)/2;
    y3 = (y3+y2)/2;

    fl_polygon(x1, y1, x2, y2, x3, y3);
  } else {
    x1 = (x1+x4)/2;
    x2 = (x2+x4)/2;
    x3 = (x3+x4)/2;

    y1 = (y1+y4)/2;
    y2 = (y2+y4)/2;
    y3 = (y3+y4)/2;

    fl_polygon(x1, y1, x2, y2, x3, y3, x4, y4);
  }
}

static bool findCommonEdge(int xa1, int ya1, int xa2, int ya2, int xa3, int ya3,
    int xb1, int yb1, int xb2, int yb2, int xb3, int yb3,
    int * xl1, int * yl1, int * xl2, int * yl2) {

  if (xa1 == xb1 && ya1 == yb1) {

    if ((xa2 == xb2 && ya2 == yb2) || (xa2 == xb3 && ya2 == yb3)) {

      *xl1 = xa1; *yl1 = ya1;
      *xl2 = xa2; *yl2 = ya2;
      return true;

    } else if ((xa3 == xb2 && ya3 == yb2) || (xa3 == xb3 && ya3 == yb3)) {

      *xl1 = xa1; *yl1 = ya1;
      *xl2 = xa3; *yl2 = ya3;
      return true;

    }

  } else if (xa1 == xb2 && ya1 == yb2) {

    if ((xa2 == xb1 && ya2 == yb1) || (xa2 == xb3 && ya2 == yb3)) {

      *xl1 = xa1; *yl1 = ya1;
      *xl2 = xa2; *yl2 = ya2;
      return true;

    } else if ((xa3 == xb1 && ya3 == yb1) || (xa3 == xb3 && ya3 == yb3)) {

      *xl1 = xa1; *yl1 = ya1;
      *xl2 = xa3; *yl2 = ya3;
      return true;

    }

  } else if (xa1 == xb3 && ya1 == yb3) {

    if ((xa2 == xb1 && ya2 == yb1) || (xa2 == xb2 && ya2 == yb2)) {

      *xl1 = xa1; *yl1 = ya1;
      *xl2 = xa2; *yl2 = ya2;
      return true;

    } else if ((xa3 == xb1 && ya3 == yb1) || (xa3 == xb2 && ya3 == yb2)) {

      *xl1 = xa1; *yl1 = ya1;
      *xl2 = xa3; *yl2 = ya3;
      return true;

    }
  } else if (xa2 == xb1 && ya2 == yb1) {

    if ((xa1 == xb2 && ya1 == yb2) || (xa1 == xb3 && ya1 == yb3)) {

      *xl1 = xa2; *yl1 = ya2;
      *xl2 = xa1; *yl2 = ya1;
      return true;

    } else if ((xa3 == xb2 && ya3 == yb2) || (xa3 == xb3 && ya3 == yb3)) {

      *xl1 = xa2; *yl1 = ya2;
      *xl2 = xa3; *yl2 = ya3;
      return true;

    }

  } else if (xa2 == xb2 && ya2 == yb2) {

    if ((xa1 == xb1 && ya1 == yb1) || (xa1 == xb3 && ya1 == yb3)) {

      *xl1 = xa1; *yl1 = ya1;
      *xl2 = xa2; *yl2 = ya2;
      return true;

    } else if ((xa3 == xb1 && ya3 == yb1) || (xa3 == xb3 && ya3 == yb3)) {

      *xl1 = xa2; *yl1 = ya2;
      *xl2 = xa3; *yl2 = ya3;
      return true;

    }

  } else if (xa2 == xb3 && ya2 == yb3) {

    if ((xa1 == xb1 && ya1 == yb1) || (xa1 == xb2 && ya1 == yb2)) {

      *xl1 = xa1; *yl1 = ya1;
      *xl2 = xa2; *yl2 = ya2;
      return true;

    } else if ((xa3 == xb1 && ya3 == yb1) || (xa3 == xb2 && ya3 == yb2)) {

      *xl1 = xa2; *yl1 = ya2;
      *xl2 = xa3; *yl2 = ya3;
      return true;

    }

  } else if (xa3 == xb1 && ya3 == yb1) {

    if ((xa1 == xb2 && ya1 == yb2) || (xa1 == xb3 && ya1 == yb3)) {

      *xl1 = xa3; *yl1 = ya3;
      *xl2 = xa1; *yl2 = ya1;
      return true;

    } else if ((xa2 == xb2 && ya2 == yb2) || (xa2 == xb3 && ya2 == yb3)) {

      *xl1 = xa2; *yl1 = ya2;
      *xl2 = xa3; *yl2 = ya3;
      return true;

    }

  } else if (xa3 == xb2 && ya3 == yb2) {

    if ((xa1 == xb1 && ya1 == yb1) || (xa1 == xb3 && ya1 == yb3)) {

      *xl1 = xa1; *yl1 = ya1;
      *xl2 = xa3; *yl2 = ya3;
      return true;

    } else if ((xa2 == xb1 && ya2 == yb1) || (xa2 == xb3 && ya2 == yb3)) {

      *xl1 = xa2; *yl1 = ya2;
      *xl2 = xa3; *yl2 = ya3;
      return true;

    }

  } else if (xa3 == xb3 && ya3 == yb3) {

    if ((xa1 == xb1 && ya1 == yb1) || (xa1 == xb2 && ya1 == yb2)) {

      *xl1 = xa1; *yl1 = ya1;
      *xl2 = xa3; *yl2 = ya3;
      return true;

    } else if ((xa2 == xb1 && ya2 == yb1) || (xa2 == xb2 && ya2 == yb2)) {

      *xl1 = xa2; *yl1 = ya2;
      *xl2 = xa3; *yl2 = ya3;
      return true;

    }
  }


  return false;
}

void gridEditor_4_c::drawTileCursor(int x, int y, int z, int tx, int ty, int sx, int sy) {

  voxel_c * space = puzzle->getShape(piecenumber);

  bool ins = inRegion(x, y);


  if (z%3 == 1)
  {
    int xc = intdiv_inf(x, 3);
    int yc = intdiv_inf(y, 3);

    if (ins ^ inRegion(x-3, y))
    {
      fl_line(tx+xc*sx-1, ty-yc*sy, tx+xc*sx-1, ty-yc*sy-sy);
      fl_line(tx+xc*sx+1, ty-yc*sy, tx+xc*sx+1, ty-yc*sy-sy);
    }
    if (ins ^ inRegion(x+3, y))
    {
      fl_line(tx+xc*sx+sx-1, ty-yc*sy, tx+xc*sx+sx-1, ty-yc*sy-sy);
      fl_line(tx+xc*sx+sx+1, ty-yc*sy, tx+xc*sx+sx+1, ty-yc*sy-sy);
    }
    if (ins ^ inRegion(x, y-3))
    {
      fl_line(tx+xc*sx, ty-yc*sy+1, tx+xc*sx+sx, ty-yc*sy+1);
      fl_line(tx+xc*sx, ty-yc*sy-1, tx+xc*sx+sx, ty-yc*sy-1);
    }
    if (ins ^ inRegion(x, y+3))
    {
      fl_line(tx+xc*sx, ty-yc*sy-sy+1, tx+xc*sx+sx, ty-yc*sy-sy+1);
      fl_line(tx+xc*sx, ty-yc*sy-sy-1, tx+xc*sx+sx, ty-yc*sy-sy-1);
    }

  }
  else
  {
    int nx, ny, nz, idx;
    idx = 0;

    int xd1, yd1, xd2, yd2, xd3, yd3, xd4, yd4;
    bt_assert2(getPolygon(x, y, z, tx, ty, sx, sy, &xd1, &yd1, &xd2, &yd2, &xd3, &yd3, &xd4, &yd4, 0) == 3);

    while (space->getNeighbor(idx, 0, x, y, z, &nx, &ny, &nz)) {
      if (nz == z && (ny <= y || (ny == y && nx < x)) ) {

        if (ins ^ inRegion(nx, ny)) {

          int xo1, yo1, xo2, yo2, xo3, yo3, xo4, yo4;
          bt_assert2(getPolygon(nx, ny, nz, tx, ty, sx, sy, &xo1, &yo1, &xo2, &yo2, &xo3, &yo3, &xo4, &yo4, 0) == 3);

          int xl1, yl1, xl2, yl2;
          if (findCommonEdge(xd1, yd1, xd2, yd2, xd3, yd3, xo1, yo1, xo2, yo2, xo3, yo3, &xl1, &yl1, &xl2, &yl2)) {

            fl_line(xl1-1, yl1, xl2-1, yl2);
            fl_line(xl1+1, yl1, xl2+1, yl2);
            fl_line(xl1, yl1-1, xl2, yl2-1);
            fl_line(xl1, yl1+1, xl2, yl2+1);
          }
        }
      }

      idx++;
    }

    idx = 0;

    while (space->getNeighbor(idx, 1, x, y, z, &nx, &ny, &nz)) {
      if (nz == z && (ny <= y || (ny == y && nx < x)) ) {

        if (ins ^ inRegion(nx, ny)) {

          int xo1, yo1, xo2, yo2, xo3, yo3, xo4, yo4;
          bt_assert2(getPolygon(nx, ny, nz, tx, ty, sx, sy, &xo1, &yo1, &xo2, &yo2, &xo3, &yo3, &xo4, &yo4, 0) == 3);

          int xl1, yl1, xl2, yl2;
          if (findCommonEdge(xd1, yd1, xd2, yd2, xd3, yd3, xo1, yo1, xo2, yo2, xo3, yo3, &xl1, &yl1, &xl2, &yl2)) {
            fl_line(xl1-1, yl1, xl2-1, yl2);
            fl_line(xl1+1, yl1, xl2+1, yl2);
            fl_line(xl1, yl1-1, xl2, yl2-1);
            fl_line(xl1, yl1+1, xl2, yl2+1);
          }
        }
      }

      idx++;
    }
  }
}

bool gridEditor_4_c::calcGridPosition(int x, int y, int z, int *gx, int *gy) {

  voxel_c * space = puzzle->getShape(piecenumber);

  int sx, sy, tx, ty;
  calcParameters(&sx, &sy, &tx, &ty);

  if (sx == 0 || sy == 0) return false;

  int by = (space->getY()+2)/3;

  x -= tx;
  y -= (ty-sy*by);

  int xp = x;
  int yp = y;

  x = intdiv_inf(x, sx);
  y = intdiv_inf(y, sy);

  xp -= x*sx;
  yp -= y*sy;

  y = by-y-1;

  // x and y now contains the square we are in, depending on that
  // we need to find out which triangle

  int mask = 0;
  if (xp<yp)    mask |= 1;
  if (xp+yp<sx) mask |= 2;

  if (z % 3 == 1) {

    x = 3*x+1;
    y = 3*y+1;

  } else if (((x+y+(z/3)) % 2 == 0)^(z%3==2)) {

    if ((mask & 2) == 0) {
      x = 3*x+2;
      y = 3*y;
    } else {
      x = 3*x;
      y = 3*y+2;
    }

  } else {

    if ((mask & 1) == 0) {
      x = 3*x+2;
      y = 3*y+2;
    } else {
      x = 3*x;
      y = 3*y;
    }
  }

  if (y >= (int)space->getY() || x >= (int)space->getX()) return false;

  *gx = x;
  *gy = y;

  return true;
}

