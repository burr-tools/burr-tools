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
#include "grideditor_3.h"

#include "../lib/voxel.h"
#include "../lib/puzzle.h"

#include "../tools/intdiv.h"

#include <FL/fl_draw.H>

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

  int xc = (x+1000) / 5 -200;
  int yc = (y+1000) / 5 -200;

  x -= 5*xc;
  y -= 5*yc;
  z -= 5*((z+1000)/5-200);

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

void gridEditor_3_c::drawTileCursor(int x, int y, int z, int tx, int ty, int sx, int sy) {

  voxel_c * space = puzzle->getShape(piecenumber);

  bool ins = inRegion(x, y);

  int nx, ny, nz, idx;
  idx = 0;

  int xd1, yd1, xd2, yd2, xd3, yd3;
  getTriangle(x, y, z, tx, ty, sx, sy, &xd1, &yd1, &xd2, &yd2, &xd3, &yd3, 0);

  while (space->getNeighbor(idx, 0, x, y, z, &nx, &ny, &nz)) {
    if (nz == z && (ny <= y || (ny == y && nx < x)) ) {

      if (ins ^ inRegion(nx, ny)) {

        int xo1, yo1, xo2, yo2, xo3, yo3;
        getTriangle(nx, ny, nz, tx, ty, sx, sy, &xo1, &yo1, &xo2, &yo2, &xo3, &yo3, 0);

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

        int xo1, yo1, xo2, yo2, xo3, yo3;
        getTriangle(nx, ny, nz, tx, ty, sx, sy, &xo1, &yo1, &xo2, &yo2, &xo3, &yo3, 0);

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

  x = intdiv_inf(x, sx);
  y = intdiv_inf(y, sy);

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

