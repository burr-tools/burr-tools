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

void SquareEditor::setVoxelSpace(pieceVoxel_c * newSpace, int piecenum) {

  space = newSpace;

  // check if the current z value is in valid regions
  if (space && (space->getZ() <= currentZ))
    currentZ = space->getZ()-1;

  if (currentZ < 0) currentZ = 0;

  piecenumber = piecenum;

  redraw();
}

void SquareEditor::setZ(int z) {

  // clamp the value to valid values
  if (z < 0) z = 0;
  if (z >= space->getZ()) z = space->getZ()-1;

  currentZ = z;
  redraw();
}

void SquareEditor::calcParameters(int *s, int *tx, int *ty) {

  // calculate the size of the squares
  int sx = (w()-1) / space->getX();
  int sy = (h()-1) / space->getY();

  *s = (sx < sy) ? sx : sy;

  if (*s > 20) *s = 20;

  *tx = x() + (w() - space->getX()*(*s) - 1) / 2;
  *ty = y() + (h() - space->getY()*(*s) - 1) / 2;
}

void SquareEditor::draw() {

  // draw the background
  fl_color(color());
  fl_rectf(x(), y(), w(), h());

  // if there is no voxelspace or the space is of volumn 0 return
  if (!space || (space->getX() == 0) || (space->getY() == 0) || (space->getZ() == 0))
    return;

  int s, tx, ty;
  calcParameters(&s, &tx, &ty);

  // the color for the grid lines
  fl_color(labelcolor());

  // the color for the squares
  int r, g, b;

  for (int x = 0; x < space->getX(); x++)
    for (int y = 0; y < space->getY(); y++) {
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
      fl_color(labelcolor());
      fl_rect(tx+x*s, ty+y*s, s+1, s+1);
    }
}

int SquareEditor::handle(int event) {

  // if there is no valid space, we do nothing
  if (!space || (space->getX() == 0) || (space->getY() == 0) || (space->getZ() == 0))
    return 1;

  switch(event) {
  case FL_RELEASE:
    state = 0;
    break;
  case FL_PUSH:
    state = 1;
    // fallthrou
  case FL_DRAG:
  case FL_MOVE:
    {
      int s, tx, ty;
      calcParameters(&s, &tx, &ty);

      int x = Fl::event_x() - tx;
      int y = Fl::event_y() - ty;

      if ((x < 0) || (y < 0))
        break;

      x /= s;
      y /= s;

      // if we are outside the valid range, exit
      if ((x >= space->getX()) || (y >= space->getY()))
        break;

      y = space->getY() - y - 1;

      if (event == FL_MOVE) {

        // we move the mouse, if the new position is different from the saved one,
        // do a callback
        if ((x != mX) || (y != mY) || (currentZ != mZ)) {
          mX = x;
          mY = y;
          mZ = currentZ;
          callbackReason = RS_MOUSEMOVE;
          do_callback();
        }
      } else {
  
        // if we just pressed the button find out what to do while
        // the button is pressed
        if (state == 1) {
          if (Fl::event_button() == 1)
            state = (space->getState(x, y, currentZ) == pieceVoxel_c::VX_FILLED) ? 2 : 3;
          else
            state = (space->getState(x, y, currentZ) == pieceVoxel_c::VX_VARIABLE) ? 2 : 4;
        }
  
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
  
        if (space->getState(x, y, currentZ) != vxNew) {
          space->setState(x, y, currentZ, vxNew);
          redraw();
          callbackReason = RS_CHANGESQUARE;
          do_callback();
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

