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
#include "pieceColor.h"

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


// this function finds out if a given square is inside the selected region
// this check includes the symmetric and column edit modes
//
// it works resursive. mode contains the yet to check symmetries and columns
static bool inRegion(int x, int y, int x1, int x2, int y1, int y2, int sx, int sy, int mode) {

  // if we are outside the active shape we are not in a region
  if ((x < 0) || (y < 0) || (x >= sx) || (y >= sy)) return false;

  // these 2 modes ar of no interest, they only belong to the z layer
  mode &= ~ (gridEditor_c::TOOL_STACK_Z + gridEditor_c::TOOL_MIRROR_Z);

  if (mode == 0)
    // no mode bit set, so the given coordinate needs to be inside the selected area
    return (x1 <= x) && (x <= x2) && (y1 <= y) && (y <= y2);
  if (mode == gridEditor_c::TOOL_STACK_Y)
    // y stack active, so we need to be inside the x area
    return (x1 <= x) && (x <= x2);
  if (mode == gridEditor_c::TOOL_STACK_X)
    // x stack active, so we need to be inside the y area
    return (y1 <= y) && (y <= y2);
  if (mode == gridEditor_c::TOOL_STACK_X + gridEditor_c::TOOL_STACK_Y)
    // x and y stack active, we need to be either in the row or the column
    return (x1 <= x) && (x <= x2) || (y1 <= y) && (y <= y2);

  // symmetric modes, recursive call with the same coordinates and also
  // with the corresponding mirrored coordinate and the symmetry bit removed
  // from mode, so that in the functioncall the checks above work properly.
  if (mode & gridEditor_c::TOOL_MIRROR_X)
    return inRegion(x, y, x1, x2, y1, y2, sx, sy, mode & ~gridEditor_c::TOOL_MIRROR_X) ||
      inRegion(sx-x-1, y, x1, x2, y1, y2, sx, sy, mode & ~gridEditor_c::TOOL_MIRROR_X);

  if (mode & gridEditor_c::TOOL_MIRROR_Y)
    return inRegion(x, y, x1, x2, y1, y2, sx, sy, mode & ~gridEditor_c::TOOL_MIRROR_Y) ||
      inRegion(x, sy-y-1, x1, x2, y1, y2, sx, sy, mode & ~gridEditor_c::TOOL_MIRROR_Y);

  return false;
}

// this function calculates the size of the squares and the starting position
// for the grid inside the available space of the widget
void gridEditor_0_c::calcParameters(int *s, int *tx, int *ty) {

  // calculate the size of the squares
  int sx = (w() > 2) ? (w()-1) / puzzle->getShape(piecenumber)->getX() : 0;
  int sy = (h() > 2) ? (h()-1) / puzzle->getShape(piecenumber)->getY() : 0;

  *s = (sx < sy) ? sx : sy;

  if (*s > 20) *s = 20;

  *tx = x() + (w() - puzzle->getShape(piecenumber)->getX()*(*s) - 1) / 2;
  *ty = y() + (h() - puzzle->getShape(piecenumber)->getY()*(*s) - 1) / 2;
}

void gridEditor_0_c::draw() {

  // draw the background, as we don't cover the whole area
  fl_color(color());
  fl_rectf(x(), y(), w(), h());

  // no valid piece, nothing to draw
  if (piecenumber >= puzzle->shapeNumber())
    return;

  voxel_c * space = puzzle->getShape(piecenumber);

  // if there is no voxelspace or the space is of volumn 0 return
  if ((space->getX() == 0) || (space->getY() == 0) || (space->getZ() == 0))
    return;

  // get the backgroud color, used for the dimmed squared if the layer below
  unsigned char bgr, bgg, bgb;
  Fl::get_color(color(), bgr, bgg, bgb);

  int s, tx, ty;
  calcParameters(&s, &tx, &ty);

  // the color for the squares
  unsigned char r, g, b;

  for (unsigned int x = 0; x < space->getX(); x++)
    for (unsigned int y = 0; y < space->getY(); y++) {

      // apply the checkerboard pattern
      if ((x+y+currentZ) & 1) {
        r = int(255*darkPieceColor(pieceColorR(piecenumber)));
        g = int(255*darkPieceColor(pieceColorG(piecenumber)));
        b = int(255*darkPieceColor(pieceColorB(piecenumber)));
      } else {
        r = int(255*lightPieceColor(pieceColorR(piecenumber)));
        g = int(255*lightPieceColor(pieceColorG(piecenumber)));
        b = int(255*lightPieceColor(pieceColorB(piecenumber)));
      }

      // draw the square depending on the state
      switch(space->getState(x, space->getY()-y-1, currentZ)) {
      case voxel_c::VX_FILLED:
        fl_rectf(tx+x*s, ty+y*s, s, s, r, g, b);
        break;
      case voxel_c::VX_VARIABLE:
        fl_rectf(tx+x*s+3, ty+y*s+3, s-5, s-5, r, g, b);
        break;
      default:
        // for empty squares we check the layer below and if the square below is
        // not empty draw a very dimmed square
        if ((currentZ > 0) && (space->getState(x, space->getY()-y-1, currentZ-1) != voxel_c::VX_EMPTY))
          fl_rectf(tx+x*s, ty+y*s, s, s, ((int)bgr*5+r)/6, ((int)bgg*5+g)/6, ((int)bgb*5+b)/6);
      }

      // if the voxel is not empty and has a color assigned, draw a marker in the
      // upper left corner with the color of the constraint color
      if ((space->getState(x, space->getY()-y-1, currentZ) != voxel_c::VX_EMPTY) &&
          space->getColor(x, space->getY()-y-1, currentZ)) {

        puzzle->getColor(space->getColor(x, space->getY()-y-1, currentZ)-1, &r, &g, &b);
        fl_rectf(tx+x*s, ty+y*s, s/2, s/2, r, g, b);
      }

      // the color for the grid lines
      if (active())
        fl_color(labelcolor());
      else
        fl_color(color());

      // draw the rectangle around the square, this will be the grid
      fl_rect(tx+x*s, ty+y*s, s+1, s+1);
    }

  // if the cursor is inside the widget, we do need to draw the cursor
  if (inside && active()) {

    int x1, x2, y1, y2;

    // sort the marker points, so that x1 is always the smaller one
    if (startX < mX) {
      x1 = startX;
      x2 = mX;
    } else {
      x2 = startX;
      x1 = mX;
    }

    // clamp area
    if (x1 < 0) x1 = 0;
    if (x2 >= (int)space->getX()) x2 = (int)space->getX()-1;

    // same for y
    if (startY > mY) {
      y1 = startY;
      y2 = mY;
    } else {
      y2 = startY;
      y1 = mY;
    }

    if (y2 < 0) y2 = 0;
    if (y1 >= (int)space->getY()) y1 = (int)space->getY()-1;

    // flip y vertically, as everything is drawn vertially flipped
    y1 = space->getY() - y1 - 1;
    y2 = space->getY() - y2 - 1;

    if ((x1 <= x2) && (y1 <= y2)) {

      // ok, we have a valid range selected, now we need to check for
      // edit mode (symmetric modes, ...) and draw the right cursor for the current mode

      fl_color(labelcolor());

      // go over all grid lines and check, if the square on one side of the line is inside and
      // the other on the outside, if so draw the cursor line
      for (unsigned int x = 0; x <= space->getX(); x++)
        for (unsigned int y = 0; y <= space->getY(); y++) {
          bool ins = inRegion(x, y, x1, x2, y1, y2, space->getX(), space->getY(), activeTools);

          if (ins ^ inRegion(x, y-1, x1, x2, y1, y2, space->getX(), space->getY(), activeTools)) {
            fl_line(tx+s*x, ty+s*y+1, tx+s*x+s, ty+s*y+1);
            fl_line(tx+s*x, ty+s*y-1, tx+s*x+s, ty+s*y-1);
          }

          if (ins ^ inRegion(x-1, y, x1, x2, y1, y2, space->getX(), space->getY(), activeTools)) {
            fl_line(tx+s*x+1, ty+s*y, tx+s*x+1, ty+s*y+s);
            fl_line(tx+s*x-1, ty+s*y, tx+s*x-1, ty+s*y+s);
          }
        }
    }
  }
}

void gridEditor_0_c::calcGridPosition(int x, int y, int *gx, int *gy) {

  voxel_c * space = puzzle->getShape(piecenumber);

  int s, tx, ty;
  calcParameters(&s, &tx, &ty);

  x -= tx;
  y -= ty;

  x = floordiv(x, s);
  y = floordiv(y, s);

  *gx = x;
  *gy = space->getY() - y - 1;
}


