/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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

#include "grideditor.h"
#include "piececolor.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <FL/fl_draw.H>

void gridEditor_c::setZ(unsigned int z) {

  // clamp the value to valid values
  if (z >= puzzle->getShape(piecenumber)->getZ()) z = puzzle->getShape(piecenumber)->getZ()-1;

  if (z != currentZ) {

    mZ = currentZ = z;
    redraw();
    callbackReason = RS_MOUSEMOVE;
    do_callback();
  }

}

void gridEditor_c::setPuzzle(puzzle_c * p, unsigned int piecenum) {

  puzzle = p;
  piecenumber = piecenum;

  // check if the current z value is in valid regions
  if ((piecenum < puzzle->shapeNumber()) && (puzzle->getShape(piecenum)->getZ() <= currentZ)) {
    bt_assert(puzzle->getShape(piecenum)->getZ() >= 1);
    currentZ = puzzle->getShape(piecenum)->getZ() - 1;
  }

  redraw();
}

void gridEditor_c::clearPuzzle() {
  piecenumber = puzzle->shapeNumber();
  redraw();
}

// this function work in the same as the cursor inside function.
// It sets a (group of) voxels depending in the active tools by recursively
// calling itself
// tools contains the active tools, task, what to do with the voxels
bool gridEditor_c::setRecursive(unsigned char tools, int x, int y, int z) {

  bool changed = false;
  voxel_c * space = puzzle->getShape(piecenumber);

  if (tools == 0) {

    // no further tool is active, so we set the current voxel

    // but first we check, if the current coordinates are valid
    // if not don't change anything
    if (space->validCoordinate(x, y, z)) {

      voxel_type v = voxel_c::VX_EMPTY;

      enTask todo = (state == 1) ? (task) : (TSK_RESET);

      switch (todo) {
        case gridEditor_c::TSK_SET:
          v = voxel_c::VX_FILLED;
          break;
        case gridEditor_c::TSK_VAR:
          v = voxel_c::VX_VARIABLE;
          break;
        default:
          break;
      }

      // on all other tasks but the colour changing one, we need to set the state of the voxel
      if ((todo != gridEditor_c::TSK_COLOR) && (space->getState(x, y, z) != v)) {
        changed = true;
        space->setState(x, y, z, v);

        // when emptying a cube, also clear the colour away
        if (v == voxel_c::VX_EMPTY)
          space->setColor(x, y, z, 0);
      }

      // this is for the colour change task
      if ((space->getState(x, y, z) != voxel_c::VX_EMPTY) && (space->getColor(x, y, z) != currentColor)) {
        changed = true;
        space->setColor(x, y, z, currentColor);
      }
    }
  } else if (tools & gridEditor_c::TOOL_MIRROR_X) {
    // the mirror tools are active, call recursively with both possible coordinates
    changed |= setRecursive(tools & ~gridEditor_c::TOOL_MIRROR_X, x, y, z);
    changed |= setRecursive(tools & ~gridEditor_c::TOOL_MIRROR_X, space->getX()-x-1, y, z);
  } else if (tools & gridEditor_c::TOOL_MIRROR_Y) {
    changed |= setRecursive(tools & ~gridEditor_c::TOOL_MIRROR_Y, x, y, z);
    changed |= setRecursive(tools & ~gridEditor_c::TOOL_MIRROR_Y, x, space->getY()-y-1, z);
  } else if (tools & gridEditor_c::TOOL_MIRROR_Z) {
    changed |= setRecursive(tools & ~gridEditor_c::TOOL_MIRROR_Z, x, y, z);
    changed |= setRecursive(tools & ~gridEditor_c::TOOL_MIRROR_Z, x, y, space->getZ()-z-1);
  } else {
    // the column modes are active, this part must be at the end, because is
    // doesn't mask out the tool bits but clears all of them at once
    //
    // all 3 column tools need to be handled at once because otherwise we wouldn't handle
    // just the columns at the current position but all rows of all columns or so if more than
    // one columns tool is active
    if (tools & gridEditor_c::TOOL_STACK_X)
      for (unsigned int xp = 0; xp < space->getX(); xp++)
        changed |= setRecursive(0, xp, y, z);
    if (tools & gridEditor_c::TOOL_STACK_Y)
      for (unsigned int yp = 0; yp < space->getY(); yp++)
        changed |= setRecursive(0, x, yp, z);
    if (tools & gridEditor_c::TOOL_STACK_Z)
      for (unsigned int zp = 0; zp < space->getZ(); zp++)
        changed |= setRecursive(0, x, y, zp);
  }

  return changed;
}

bool gridEditor_c::setLayer(unsigned int z) {
  int x1, x2, y1, y2;

  voxel_c * space = puzzle->getShape(piecenumber);

  // sort start and end
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

  // clamp values
  if (x1 < 0) x1 = 0;
  if (x2 > (int)space->getX()-1) x2 = (int)space->getX()-1;
  if (y1 < 0) y1 = 0;
  if (y2 > (int)space->getY()-1) y2 = (int)space->getY()-1;

  bool changed = false;

  // loop over the area and set all voxels
  for (int x = x1; x <= x2; x++)
    for (int y = y1; y <= y2; y++)
      if ((x >= 0) && (y >= 0) && (x < (int)space->getX()) && (y < (int)space->getY()))
        // depending on the state we either do the current active task, or clear
        changed |= setRecursive(activeTools, x, y, z);

  if (changed)
    space->initHotspot();

  return changed;
}

int gridEditor_c::handle(int event) {

  // no valid shape, nothing to do
  if (piecenumber >= puzzle->shapeNumber())
    return 0;

  // not active, nothing to do. Normally we wouldn't require this
  // but mouse move events are still delivered and this would unnecessarily
  // update the cursor
  if (!active())
    return 0;

  voxel_c * space = puzzle->getShape(piecenumber);

  // if there is no valid space, we do nothing
  if ((space->getX() == 0) || (space->getY() == 0) || (space->getZ() == 0))
    return 0;

  switch(event) {
  case FL_FOCUS:
    return 1;

  case FL_RELEASE:
    {
      // mouse released, update the rubberband area

      int x, y;
      if (!calcGridPosition(Fl::event_x(), Fl::event_y(), currentZ, &x, &y)) break;

      // check, if the current position is inside the grid, only if so carry out action, we don't
      // need to to this if we are not in rubberband modus, but it doesn't hurt either
      if (0 <= x && x < (long)space->getX() && 0 <= y && y < (long)space->getY() && setLayer(currentZ)) {
        callbackReason = RS_CHANGESQUARE;
        do_callback();
      }

      state = 0;

      redraw();
    }

    return 1;
  case FL_ENTER:
    // we want to get mouse movement events
    Fl::belowmouse(this);

    // fall through
  case FL_PUSH:

    // the mouse is inside the widget, so draw the cursor
    inside = true;

    // we save the mouse button in state, so that we can do different actions
    // depending on the mouse button
    state = Fl::event_button();

    if (event == FL_PUSH) {
      // take the focus to make the input lines in the main window take over their value
      set_visible_focus();
      take_focus();
      clear_visible_focus();
    }

    // fall through
  case FL_DRAG:
  case FL_MOVE:
    {
      /* find out where the mouse cursor is */
      int x, y;
      if (!calcGridPosition(Fl::event_x(), Fl::event_y(), currentZ, &x, &y)) break;

      // clip the coordinates to the size of the space
      if (x < 0) x = 0;
      if (y < 0) y = 0;
      if (x >= (long)space->getX()) x = space->getX() - 1;
      if (y >= (long)space->getY()) y = space->getY() - 1;

      if (event == FL_PUSH || event == FL_MOVE || event == FL_ENTER) {

        // the start-up events, save the current cursor position for the
        // rubber band
        mX = startX = x;
        mY = startY = y;
        mZ = currentZ;

        redraw();
        callbackReason = RS_MOUSEMOVE;
        do_callback();

      } else {

        // we move the mouse, if the new position is different from the saved one,
        // do a callback
        if ((x != (long)mX) || (y != (long)mY) || ((long)currentZ != (long)mZ)) {

          mX = x;
          mY = y;
          mZ = currentZ;

          // if we are _not_ in ruberband mode, we always keep the start
          // position for the rubberband at the current coordinate
          if (editType == EDT_SINGLE) {
            startX = mX;
            startY = mY;
          }

          redraw();
          callbackReason = RS_MOUSEMOVE;
          do_callback();
        }
      }

      // if we are not in rubberband mode, we need to update the voxelspace
      if ((event == FL_DRAG || event == FL_PUSH) && (editType == EDT_SINGLE))
        if (setLayer(currentZ)) {
          callbackReason = RS_CHANGESQUARE;
          redraw();
          do_callback();
        }

    }
    return 1;
  case FL_LEAVE:

    // the mouse leaves the widget space, remove cursor
    inside = false;
    redraw();
    callbackReason = RS_MOUSEMOVE;
    do_callback();
    return 1;
    break;
  }

  return 0;
}

void gridEditor_c::draw() {

  // draw the background, as we don't cover the whole area
  fl_color(color());
  fl_rectf(x(), y(), w(), h());

  // no valid piece, nothing to draw
  if (piecenumber >= puzzle->shapeNumber())
    return;

  voxel_c * space = puzzle->getShape(piecenumber);

  // if there is no voxelspace or the space is of column 0 return
  if ((space->getX() == 0) || (space->getY() == 0) || (space->getZ() == 0))
    return;

  // get the background colour, used for the dimmed squared if the layer below
  unsigned char bgr, bgg, bgb;
  Fl::get_color(color(), bgr, bgg, bgb);

  int sx, sy, tx, ty;
  calcParameters(&sx, &sy, &tx, &ty);

  // the colour for the squares
  unsigned char r, g, b;

  for (unsigned int x = 0; x < space->getX(); x++)
    for (unsigned int y = 0; y < space->getY(); y++) {

      if (!space->validCoordinate(x, y, currentZ-1))
        continue;

      // apply the chequerboard pattern
      if ((x+y+currentZ) & 1) {
        r = int(255*darkPieceColor(pieceColorR(piecenumber)));
        g = int(255*darkPieceColor(pieceColorG(piecenumber)));
        b = int(255*darkPieceColor(pieceColorB(piecenumber)));
      } else {
        r = int(255*lightPieceColor(pieceColorR(piecenumber)));
        g = int(255*lightPieceColor(pieceColorG(piecenumber)));
        b = int(255*lightPieceColor(pieceColorB(piecenumber)));
      }

      // for empty squares we check the layer below and if the square below is
      // not empty draw a very dimmed square
      if ((currentZ > 0) && (space->getState(x, y, currentZ-1) != voxel_c::VX_EMPTY)) {
        fl_color(((int)bgr*5+r)/6, ((int)bgg*5+g)/6, ((int)bgb*5+b)/6);
        drawNormalTile(x, y, currentZ-1, tx, ty, sx, sy);
      }
    }

  for (unsigned int x = 0; x < space->getX(); x++)
    for (unsigned int y = 0; y < space->getY(); y++) {

      if (!space->validCoordinate(x, y, currentZ))
        continue;

      // apply the chequerboard pattern
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
      switch(space->getState(x, y, currentZ)) {
      case voxel_c::VX_FILLED:
        fl_color(r, g, b);
        drawNormalTile(x, y, currentZ, tx, ty, sx, sy);
        break;
      case voxel_c::VX_VARIABLE:
        fl_color(r, g, b);
        drawVariableTile(x, y, currentZ, tx, ty, sx, sy);
        break;
      }

      // if the voxel is not empty and has a colour assigned, draw a marker in the
      // upper left corner with the colour of the constraint colour
      if ((space->getState(x, y, currentZ) != voxel_c::VX_EMPTY) &&
          space->getColor(x, y, currentZ)) {

        puzzle->getColor(space->getColor(x, y, currentZ)-1, &r, &g, &b);
        fl_color(r, g, b);
        drawTileColor(x, y, currentZ, tx, ty, sx, sy);
      }

      // the colour for the grid lines
      if (active())
        fl_color(labelcolor());
      else
        fl_color(color());

      // draw the rectangle around the square, this will be the grid
      drawTileFrame(x, y, currentZ, tx, ty, sx, sy);
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
    if (startY < mY) {
      y1 = startY;
      y2 = mY;
    } else {
      y2 = startY;
      y1 = mY;
    }

    if (y2 < 0) y2 = 0;
    if (y1 >= (int)space->getY()) y1 = (int)space->getY()-1;

    if ((x1 <= x2) && (y1 <= y2)) {

      // OK, we have a valid range selected, now we need to check for
      // edit mode (symmetric modes, ...) and draw the right cursor for the current mode

      fl_color(labelcolor());

      markX1 = x1; markY1 = y1;
      markX2 = x2; markY2 = y2;

      // go over all grid lines and check, if the square on one side of the line is inside and
      // the other on the outside, if so draw the cursor line
      for (unsigned int x = 0; x <= space->getX(); x++)
        for (unsigned int y = 0; y <= space->getY(); y++)
          if (space->validCoordinate(x, y, currentZ))
            drawTileCursor(x, y, currentZ, tx, ty, sx, sy);
    }
  }
}

// this function finds out if a given square is inside the selected region
// this check includes the symmetric and column edit modes
//
// it works recursive. Mode contains the yet to check symmetries and columns
static bool inRegionRec(int x, int y, int x1, int x2, int y1, int y2, int sx, int sy, int mode) {

  // if we are outside the active shape we are not in a region
  if ((x < 0) || (y < 0) || (x >= sx) || (y >= sy)) return false;

  // these 2 modes are of no interest, they only belong to the z layer
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
  // from mode, so that in the function call the checks above work properly.
  if (mode & gridEditor_c::TOOL_MIRROR_X)
    return inRegionRec(x, y, x1, x2, y1, y2, sx, sy, mode & ~gridEditor_c::TOOL_MIRROR_X) ||
      inRegionRec(sx-x-1, y, x1, x2, y1, y2, sx, sy, mode & ~gridEditor_c::TOOL_MIRROR_X);

  if (mode & gridEditor_c::TOOL_MIRROR_Y)
    return inRegionRec(x, y, x1, x2, y1, y2, sx, sy, mode & ~gridEditor_c::TOOL_MIRROR_Y) ||
      inRegionRec(x, sy-y-1, x1, x2, y1, y2, sx, sy, mode & ~gridEditor_c::TOOL_MIRROR_Y);

  return false;
}

bool gridEditor_c::inRegion(int x, int y) {
  voxel_c * space = puzzle->getShape(piecenumber);
  return space->validCoordinate(x, y, currentZ) && inRegionRec(x, y, markX1, markX2, markY1, markY2, space->getX(), space->getY(), activeTools);
}

