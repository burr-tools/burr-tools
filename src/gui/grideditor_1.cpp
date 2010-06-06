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
#include "grideditor_1.h"

#include "../lib/voxel.h"
#include "../lib/puzzle.h"

#include "../tools/intdiv.h"

#include <FL/fl_draw.H>

#include <math.h>

#define HEIGHT 0.8660254     // sqrt(3)/2

// this function calculates the size of the squares and the starting position
// for the grid inside the available space of the widget
void gridEditor_1_c::calcParameters(int *s, int *s2, int *tx, int *ty) {

  int xx = puzzle->getShape(piecenumber)->getX();
  int yy = puzzle->getShape(piecenumber)->getY();

  // calculate the size of the squares
  int sx = (w() > 2) ? (int)((w()-1) / ((1+xx)/2.0)) : 0;
  int sy = (h() > 2) ? (h()-1) / (yy) : 0;

  if (sx*HEIGHT < sy) {
    // use x as base

    // make sx even, so that we have the point of the triangle in the centre
    if (sx & 1) sx--;

  } else {
    // use height as base for calculation

    sx = (int)(sy/HEIGHT + 0.5);
    if (sx & 1) sx--;
  }

  if (sx > 30) sx = 30;

  *s = sx;
  *s2 = (int)(sx * HEIGHT + 0.5);

  *tx = x()       + ((int)(w() - (1+(xx-1)/2.0) * (*s) - 1)) / 2;
  *ty = y()+h()-1 - (h() - yy * (*s2) - 1) / 2;
}

void gridEditor_1_c::drawNormalTile(int x, int y, int, int tx, int ty, int s, int s2) {

  int x1, y1, x2, y2, x3, y3;

  /* find out the coordinates of the 3 corners of the triangle */
  if ((x+y) & 1) {
    // triangle with base at the top

    x1 = tx+s*x/2;
    y1 = ty-s2-y*s2;
    x2 = x1+s;
    y2 = y1;
    x3 = x1+s/2;
    y3 = y1+s2;

  } else {

    x1 = tx+s*x/2;
    y1 = ty-y*s2;
    x2 = x1+s;
    y2 = y1;
    x3 = x1+s/2;
    y3 = y1-s2;
  }
  fl_polygon(x1, y1, x2, y2, x3, y3);
}

void gridEditor_1_c::drawVariableTile(int x, int y, int, int tx, int ty, int s, int s2) {
  int x1, y1, x2, y2, x3, y3;
  int x1v, y1v, x2v, y2v, x3v, y3v;

  /* find out the coordinates of the 3 corners of the triangle */
  if ((x+y) & 1) {
    // triangle with base at the top

    x1 = tx+s*x/2;
    y1 = ty-s2-y*s2;
    x2 = x1+s;
    y2 = y1;
    x3 = x1+s/2;
    y3 = y1+s2;

    y1v = y2v = y1 + 3;
    x1v = x1 + 5;
    x2v = x2 - 5;
    x3v = x3;
    y3v = y3 - 6;

  } else {

    x1 = tx+s*x/2;
    y1 = ty-y*s2;
    x2 = x1+s;
    y2 = y1;
    x3 = x1+s/2;
    y3 = y1-s2;

    y1v = y2v = y1 - 3;
    x1v = x1 + 5;
    x2v = x2 - 5;
    x3v = x3;
    y3v = y3 + 6;
  }
  fl_polygon(x1v, y1v, x2v, y2v, x3v, y3v);
}

void gridEditor_1_c::drawTileFrame(int x, int y, int, int tx, int ty, int s, int s2) {
  int x1, y1, x2, y2, x3, y3;

  /* find out the coordinates of the 3 corners of the triangle */
  if ((x+y) & 1) {
    // triangle with base at the top

    x1 = tx+s*x/2;
    y1 = ty-s2-y*s2;
    x2 = x1+s;
    y2 = y1;
    x3 = x1+s/2;
    y3 = y1+s2;

  } else {

    x1 = tx+s*x/2;
    y1 = ty-y*s2;
    x2 = x1+s;
    y2 = y1;
    x3 = x1+s/2;
    y3 = y1-s2;
  }
  fl_loop(x1, y1, x2, y2, x3, y3);
}

void gridEditor_1_c::drawTileColor(int x, int y, int, int tx, int ty, int s, int s2) {
  int x1, y1, x2, y2, x3, y3;

  /* find out the coordinates of the 3 corners of the triangle */
  if ((x+y) & 1) {
    // triangle with base at the top

    x1 = tx+s*x/2;
    y1 = ty-s2-y*s2;
    x2 = x1+s;
    y2 = y1;
    x3 = x1+s/2;
    y3 = y1+s2;

  } else {

    x1 = tx+s*x/2;
    y1 = ty-y*s2;
    x2 = x1+s;
    y2 = y1;
    x3 = x1+s/2;
    y3 = y1-s2;
  }
  fl_polygon(x3, y3, (x1+x3)/2, (y1+y3)/2, (x2+x3)/2, (y2+y3)/2);
}

void gridEditor_1_c::drawTileCursor(int x, int y, int, int tx, int ty, int sx, int sy) {

  int xl1, yl1, xl2, yl2, xl3, yl3;

  if ((x+y) & 1) {
    // triangle with base at the top

    xl1 = tx+sx*x/2;
    yl1 = ty-sy-y*sy;
    xl2 = xl1+sx;
    yl2 = yl1;
    xl3 = xl1+sx/2;
    yl3 = yl1+sy;

  } else {

    xl1 = tx+sx*x/2;
    yl1 = ty-y*sy;
    xl2 = xl1+sx;
    yl2 = yl1;
    xl3 = xl1+sx/2;
    yl3 = yl1-sy;

  }

  bool ins = inRegion(x, y);

  if ((((x+y)&1)) && (ins ^ inRegion(x, y+1))) {
    fl_line(xl1, yl1+1, xl2, yl2+1);
    fl_line(xl1, yl1-1, xl2, yl2-1);
  }

  if ((!((x+y)&1)) && (ins ^ inRegion(x, y-1))) {
    fl_line(xl1, yl1+1, xl2, yl2+1);
    fl_line(xl1, yl1-1, xl2, yl2-1);
  }

  if (ins ^ inRegion(x-1, y)) {
    fl_line(xl1+1, yl1, xl3+1, yl3);
    fl_line(xl1-1, yl1, xl3-1, yl3);
  }
}

bool gridEditor_1_c::calcGridPosition(int x, int y, int, int *gx, int *gy) {

  voxel_c * space = puzzle->getShape(piecenumber);

  int s, s2, tx, ty;
  calcParameters(&s, &s2, &tx, &ty);

  if (s == 0 || s2 == 0) return false;

  x -= tx;
  y -= (ty-space->getY()*s2);

  int xp;
  int yp = intdiv_inf(y, s2);

  int yf = y - yp*s2;

  if ((yp+space->getY()) & 1) {

    x -= (int)((s2-yf) / sqrt(3)+0.5);

    xp = intdiv_inf(x, s);

    if ((1.0*(x - xp*s)/s + (1.0*(s2-yf)/s2)) > 1.0)
      xp = 2*xp + 1;
    else
      xp = 2*xp;

  } else {

    x -= (int)(yf/sqrt(3)+0.5);

    xp = intdiv_inf(x, s);

    if (1.0*(x - xp*s)/s + (1.0*yf/s2) > 1.0)
      xp = 2*xp + 1;
    else
      xp = 2*xp;
  }

  *gx = xp;
  *gy = space->getY()-yp-1;

  return true;
}

