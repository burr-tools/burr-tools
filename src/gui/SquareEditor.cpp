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

  currentZ = z;
  redraw();
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

  pieceVoxel_c * space = puzzle->getShape(piecenumber);

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

      switch(space->getState(x, space->getY()-y-1, currentZ)) {
      case pieceVoxel_c::VX_FILLED:
        fl_rectf(tx+x*s, ty+y*s, s, s, r, g, b);
        break;
      case pieceVoxel_c::VX_VARIABLE:
        fl_rectf(tx+x*s+3, ty+y*s+3, s-5, s-5, r, g, b);
        break;
      }

      if ((space->getState(x, space->getY()-y-1, currentZ) != pieceVoxel_c::VX_EMPTY) &&
          space->getColor(x, space->getY()-y-1, currentZ)) {

        puzzle->getColor(space->getColor(x, space->getY()-y-1, currentZ)-1, &r, &g, &b);
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
      x1 = tx+startX*s;
      x2 = tx+mX*s+s;
    } else {
      x2 = tx+startX*s+s;
      x1 = tx+mX*s;
    }

    if (startY > mY) {
      y1 = ty+(space->getY()-startY-1)*s;
      y2 = ty+(space->getY()-mY-1)*s+s;
    } else {
      y2 = ty+(space->getY()-startY-1)*s+s;
      y1 = ty+(space->getY()-mY-1)*s;
    }

    printf("rect %i %i %i %i\n", x1, y1, x2, y2);

    fl_color(labelcolor());
    fl_rect(x1-1, y1-1, x2-x1+3, y2-y1+3);
    fl_rect(x1+1, y1+1, x2-x1-1, y2-y1-1);
  }
}

bool SquareEditor::setLayer(unsigned int z, voxel_type v) {
  int x1, x2, y1, y2;

  pieceVoxel_c * space = puzzle->getShape(piecenumber);

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

  bool changed = false;

  for (int x = x1; x <= x2; x++)
    for (int y = y1; y <= y2; y++) {
      if (space->getState(x, y, z) != v) {
        changed = true;
        space->setState(x, y, z, v);
      }
      if (space->getColor(x, y, z) != currentColor) {
        changed = true;
        space->setColor(x, y, z, currentColor);
      }
    }


  return changed;
}


int SquareEditor::handle(int event) {

  if (piecenumber >= puzzle->shapeNumber())
    return 1;

  if (locked)
    return 1;

  pieceVoxel_c * space = puzzle->getShape(piecenumber);

  // if there is no valid space, we do nothing
  if ((space->getX() == 0) || (space->getY() == 0) || (space->getZ() == 0))
    return 1;

  switch(event) {
  case FL_RELEASE:
    {
      voxel_type vxNew = pieceVoxel_c::VX_EMPTY;
  
      switch (state) {
      case 2:
        vxNew = pieceVoxel_c::VX_EMPTY;
        break;
      case 3:
        vxNew = pieceVoxel_c::VX_FILLED;
        break;
      case 4:
        vxNew = pieceVoxel_c::VX_VARIABLE;
        break;
      }
  
      bool changed = false;
  
      if (_editAllLayers)
        for (unsigned int z = 0; z < space->getZ(); z++)
          changed |= setLayer(z, vxNew);
      else
        changed = setLayer(currentZ, vxNew);
  
      if (changed) {
        redraw();
        callbackReason = RS_CHANGESQUARE;
        do_callback();
      }
  
      state = 0;
    }
    break;
  case FL_PUSH:
    state = 1;
    // fallthrou
  case FL_DRAG:
  case FL_MOVE:
    {
      int s, tx, ty;
      calcParameters(&s, &tx, &ty);

      if ((Fl::event_x() < tx) || (Fl::event_y() < ty))
        break;

      unsigned int x = Fl::event_x() - tx;
      unsigned int y = Fl::event_y() - ty;

      x /= s;
      y /= s;

      // if we are outside the valid range, exit
      if ((x >= space->getX()) || (y >= space->getY())) {
        inside = false;
        mX = -1;
        break;
      }

      y = space->getY() - y - 1;

      inside = true;

      if ((event == FL_MOVE) || (event == FL_DRAG)) {

        // we move the mouse, if the new position is different from the saved one,
        // do a callback
        if ((x != mX) || (y != mY) || (currentZ != mZ)) {
          mX = x;
          mY = y;
          mZ = currentZ;
          redraw();
          callbackReason = RS_MOUSEMOVE;
          do_callback();
        }
      } else {
  
        // if we just pressed the button find out what to do while
        // the button is pressed
        if (state == 1) {
          startX = mX;
          startY = mY;
          printf("start\n");
          if (Fl::event_button() == 1)
            state = (space->getState(x, y, currentZ) == pieceVoxel_c::VX_FILLED) ? 2 : 3;
          else
            state = (space->getState(x, y, currentZ) == pieceVoxel_c::VX_VARIABLE) ? 2 : 4;

          redraw();
        }
      }
      break;
    }
  case FL_LEAVE:
    inside = false;
    callbackReason = RS_MOUSEMOVE;
    do_callback();
    break;
  }

  return 1;
}

