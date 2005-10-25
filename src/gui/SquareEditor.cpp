/* Burr Solver
 * Copyright (C) 2003-2005  Andreas Röver
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


#include "SquareEditor.h"
#include "pieceColor.h"

#include <FL/fl_draw.h>

void SquareEditor::setPuzzle(puzzle_c * p, unsigned int piecenum) {

  puzzle = p;
  piecenumber = piecenum;

  // check if the current z value is in valid regions
  if ((piecenum < puzzle->shapeNumber()) && (puzzle->getShape(piecenum)->getZ() <= currentZ))
    currentZ = puzzle->getShape(piecenum)->getZ() - 1;

  if (currentZ < 0) currentZ = 0;

  redraw();
}

void SquareEditor::setZ(unsigned int z) {

  // clamp the value to valid values
  if (z >= puzzle->getShape(piecenumber)->getZ()) z = puzzle->getShape(piecenumber)->getZ()-1;

  if (z != currentZ) {

    currentZ = z;
    mZ = puzzle->getShape(piecenumber)->getZ()-z-1;
    startX = mX = puzzle->getShape(piecenumber)->getX();
    startY = mY = puzzle->getShape(piecenumber)->getY();
    inside = true;
    redraw();
    callbackReason = RS_MOUSEMOVE;
    do_callback();
  }

}

void SquareEditor::calcParameters(int *s, int *tx, int *ty) {

  // calculate the size of the squares
  int sx = (w() > 2) ? (w()-1) / puzzle->getShape(piecenumber)->getX() : 0;
  int sy = (h() > 2) ? (h()-1) / puzzle->getShape(piecenumber)->getY() : 0;

  *s = (sx < sy) ? sx : sy;

  if (*s > 20) *s = 20;

  *tx = x() + (w() - puzzle->getShape(piecenumber)->getX()*(*s) - 1) / 2;
  *ty = y() + (h() - puzzle->getShape(piecenumber)->getY()*(*s) - 1) / 2;
}

void SquareEditor::draw() {

  // draw the background
  fl_color(color());
  fl_rectf(x(), y(), w(), h());

  if (piecenumber >= puzzle->shapeNumber())
    return;

  voxel_c * space = puzzle->getShape(piecenumber);

  // if there is no voxelspace or the space is of volumn 0 return
  if ((space->getX() == 0) || (space->getY() == 0) || (space->getZ() == 0))
    return;

  int s, tx, ty;
  calcParameters(&s, &tx, &ty);

  // the color for the grid lines
  if (locked)
    fl_color(color());
  else
    fl_color(labelcolor());

  // the color for the squares
  unsigned char r, g, b;

  for (unsigned int x = 0; x < space->getX(); x++)
    for (unsigned int y = 0; y < space->getY(); y++) {
      if ((x+y+currentZ) & 1) {
        r = int(255*darkPieceColor(pieceColorR(piecenumber)));
        g = int(255*darkPieceColor(pieceColorG(piecenumber)));
        b = int(255*darkPieceColor(pieceColorB(piecenumber)));
      } else {
        r = int(255*lightPieceColor(pieceColorR(piecenumber)));
        g = int(255*lightPieceColor(pieceColorG(piecenumber)));
        b = int(255*lightPieceColor(pieceColorB(piecenumber)));
      }

      switch(space->getState(x, space->getY()-y-1, space->getZ()-currentZ-1)) {
      case voxel_c::VX_FILLED:
        fl_rectf(tx+x*s, ty+y*s, s, s, r, g, b);
        break;
      case voxel_c::VX_VARIABLE:
        fl_rectf(tx+x*s+3, ty+y*s+3, s-5, s-5, r, g, b);
        break;
      }

      if ((space->getState(x, space->getY()-y-1, space->getZ()-currentZ-1) != voxel_c::VX_EMPTY) &&
          space->getColor(x, space->getY()-y-1, space->getZ()-currentZ-1)) {

        puzzle->getColor(space->getColor(x, space->getY()-y-1, space->getZ()-currentZ-1)-1, &r, &g, &b);
        fl_rectf(tx+x*s, ty+y*s, s/2, s/2, r, g, b);
      }

      if (locked)
        fl_color(color());
      else
        fl_color(labelcolor());
      fl_rect(tx+x*s, ty+y*s, s+1, s+1);
    }

  // when we do something we need to draw a frame around the
  // squares that get edited
  if (state) {

    int x1, x2, y1, y2;

    if (startX < mX) {
      x1 = startX;
      x2 = mX;
    } else {
      x2 = startX;
      x1 = mX;
    }

    if (x1 < 0) x1 = 0;
    if (x2 > (int)space->getX()-1) x2 = (int)space->getX()-1;

    x1 = tx + s*x1;
    x2 = tx + s*x2 + s;

    if (startY > mY) {
      y1 = startY;
      y2 = mY;
    } else {
      y2 = startY;
      y1 = mY;
    }

    if (y2 < 0) y2 = 0;
    if (y1 > (int)space->getY()-1) y1 = (int)space->getY()-1;

    y1 = ty + s*(space->getY()-y1-1);
    y2 = ty + s*(space->getY()-y2-1) + s;

    if ((x1 < x2) && (y1 < y2)) {

      fl_color(labelcolor());
      fl_rect(x1-1, y1-1, x2-x1+3, y2-y1+3);
      fl_rect(x1+1, y1+1, x2-x1-1, y2-y1-1);
    }
  }
}

bool SquareEditor::setLayer(unsigned int z) {
  int x1, x2, y1, y2;

  voxel_c * space = puzzle->getShape(piecenumber);

  if (mX < startX) {
    x1 = mX;
    x2 = startX;
  } else {
    x2 = mX;
    x1 = startX;
  }

  if (mY < startY) {
    y1 = mY;
    y2 = startY;
  } else {
    y2 = mY;
    y1 = startY;
  }

  if (x1 < 0) x1 = 0;
  if (x2 > (int)space->getX()-1) x2 = (int)space->getX()-1;
  if (y1 < 0) y1 = 0;
  if (y2 > (int)space->getY()-1) y2 = (int)space->getY()-1;

  voxel_type v;

  switch (task) {
  case TSK_RESET:
    v = voxel_c::VX_EMPTY;
    break;
  case TSK_SET:
    v = voxel_c::VX_FILLED;
    break;
  case TSK_VAR:
    v = voxel_c::VX_VARIABLE;
    break;
  }

  bool changed = false;

  for (int x = x1; x <= x2; x++)
    for (int y = y1; y <= y2; y++)
      if ((x >= 0) && (y >= 0) && (x < (int)space->getX()) && (y < (int)space->getY())) {
        if ((task != TSK_COLOR) && (space->getState(x, y, z) != v)) {
          changed = true;
          space->setState(x, y, z, v);
        }
        if ((space->getState(x, y, z) != voxel_c::VX_EMPTY) && (space->getColor(x, y, z) != currentColor)) {
          changed = true;
          space->setColor(x, y, z, currentColor);
        }
      }

  return changed;
}


int SquareEditor::handle(int event) {

  if (piecenumber >= puzzle->shapeNumber())
    return 0;

  if (locked)
    return 0;

  voxel_c * space = puzzle->getShape(piecenumber);

  // if there is no valid space, we do nothing
  if ((space->getX() == 0) || (space->getY() == 0) || (space->getZ() == 0))
    return 0;

  switch(event) {
  case FL_RELEASE:
    {
      state = 0;

      bool changed = false;

      if (_editAllLayers)
        for (unsigned int z = 0; z < space->getZ(); z++)
          changed |= setLayer(z);
      else
        changed = setLayer(space->getZ()-currentZ-1);

      if (changed) {
        callbackReason = RS_CHANGESQUARE;
        do_callback();
      }

      redraw();
    }

    return 1;
  case FL_PUSH:
    inside = true;
    state = 1;
    // fallthrough
  case FL_DRAG:
  case FL_MOVE:
    {
      /* find out where the mouse cursor is */
      int s, tx, ty;
      calcParameters(&s, &tx, &ty);

      int x = Fl::event_x() - tx;
      int y = Fl::event_y() - ty;

      x /= s;
      y /= s;

      y = space->getY() - y - 1;

      if (event == FL_PUSH) {
        mX = startX = x;
        mY = startY = y;
        mZ = space->getZ()-currentZ-1;
        redraw();
        callbackReason = RS_MOUSEMOVE;
        do_callback();

      } else {

        // we move the mouse, if the new position is different from the saved one,
        // do a callback
        if ((x != mX) || (y != mY) || ((int)space->getZ()-currentZ-1 != mZ)) {

          mX = x;
          mY = y;
          mZ = space->getZ()-currentZ-1;

          redraw();
          callbackReason = RS_MOUSEMOVE;
          do_callback();
        }
      }
    }
    return 1;
  case FL_LEAVE:
    inside = false;
    callbackReason = RS_MOUSEMOVE;
    do_callback();
    return 1;
    break;
  }

  return 0;
}

