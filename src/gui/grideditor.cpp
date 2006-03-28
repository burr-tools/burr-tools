#include "grideditor.h"
#include "pieceColor.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <FL/fl_draw.h>

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
  if ((piecenum < puzzle->shapeNumber()) && (puzzle->getShape(piecenum)->getZ() <= currentZ))
    currentZ = puzzle->getShape(piecenum)->getZ() - 1;

  if (currentZ < 0) currentZ = 0;

  redraw();
}

void gridEditor_c::clearPuzzle() {
  piecenumber = puzzle->shapeNumber();
  redraw();
}

// this function work in the same as the cursor inside function.
// It sets a (group of) voxels depending in the active tools by cesoursively
// calling itself
// tools contains the active tools, taks, what to do with the voxels
static bool setRecursive(voxel_c * s, unsigned char tools, int x, int y, int z, gridEditor_c::enTask task, unsigned int currentColor) {

  bool changed = false;

  if (tools == 0) {

    // no further tool is active, so we set the current voxel
    voxel_type v = voxel_c::VX_EMPTY;

    switch (task) {
    case gridEditor_c::TSK_SET:
      v = voxel_c::VX_FILLED;
      break;
    case gridEditor_c::TSK_VAR:
      v = voxel_c::VX_VARIABLE;
      break;
    default:
      break;
    }

    // on all other tasks but the color changing one, we need to set the state of the voxel
    if ((task != gridEditor_c::TSK_COLOR) && (s->getState(x, y, z) != v)) {
      changed = true;
      s->setState(x, y, z, v);

      // when emptying a cube, also clear the color away
      if (v == voxel_c::VX_EMPTY)
        s->setColor(x, y, z, 0);
    }

    // this is for the color change task
    if ((s->getState(x, y, z) != voxel_c::VX_EMPTY) && (s->getColor(x, y, z) != currentColor)) {
      changed = true;
      s->setColor(x, y, z, currentColor);
    }
  } else if (tools & gridEditor_c::TOOL_MIRROR_X) {
    // the mirror tools are active, call resursively with both possible coordinates
    changed |= setRecursive(s, tools & ~gridEditor_c::TOOL_MIRROR_X, x, y, z, task, currentColor);
    changed |= setRecursive(s, tools & ~gridEditor_c::TOOL_MIRROR_X, s->getX()-x-1, y, z, task, currentColor);
  } else if (tools & gridEditor_c::TOOL_MIRROR_Y) {
    changed |= setRecursive(s, tools & ~gridEditor_c::TOOL_MIRROR_Y, x, y, z, task, currentColor);
    changed |= setRecursive(s, tools & ~gridEditor_c::TOOL_MIRROR_Y, x, s->getY()-y-1, z, task, currentColor);
  } else if (tools & gridEditor_c::TOOL_MIRROR_Z) {
    changed |= setRecursive(s, tools & ~gridEditor_c::TOOL_MIRROR_Z, x, y, z, task, currentColor);
    changed |= setRecursive(s, tools & ~gridEditor_c::TOOL_MIRROR_Z, x, y, s->getZ()-z-1, task, currentColor);
  } else {
    // the column modes are active, this part must be at the end, because is
    // doesn't mask out the tool bits but clears all of them at once
    //
    // all 3 column tools need to be handled at once because otherwise we wouldnt handle
    // just the columns at the current position but all rows of all columns or so if more than
    // one columns tool is active
    if (tools & gridEditor_c::TOOL_STACK_X)
      for (unsigned int xp = 0; xp < s->getX(); xp++)
        changed |= setRecursive(s, 0, xp, y, z, task, currentColor);
    if (tools & gridEditor_c::TOOL_STACK_Y)
      for (unsigned int yp = 0; yp < s->getY(); yp++)
        changed |= setRecursive(s, 0, x, yp, z, task, currentColor);
    if (tools & gridEditor_c::TOOL_STACK_Z)
      for (unsigned int zp = 0; zp < s->getZ(); zp++)
        changed |= setRecursive(s, 0, x, y, zp, task, currentColor);
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
        changed |= setRecursive(space, activeTools, x, y, z, (state == 1) ? (task) : (TSK_RESET), currentColor);

  return changed;
}

int gridEditor_c::handle(int event) {

  // no valid shape, nothing to do
  if (piecenumber >= puzzle->shapeNumber())
    return 0;

  // not active, nothing to do. Normally we wouldn't require this
  // but mouse move events are still delivered and this would unnesessarily
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
      calcGridPosition(Fl::event_x(), Fl::event_y(), &x, &y);

      // check, if the current position is inside the grid, only if so carry out action, we wouldnt
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

    // fallthrough
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

    // fallthrough
  case FL_DRAG:
  case FL_MOVE:
    {
      /* find out where the mouse cursor is */
      int x, y;
      calcGridPosition(Fl::event_x(), Fl::event_y(), &x, &y);

      // clip the coordinates to the size of the space
      if (x < 0) x = 0;
      if (y < 0) y = 0;
      if (x >= (long)space->getX()) x = space->getX() - 1;
      if (y >= (long)space->getY()) y = space->getY() - 1;

      if (event == FL_PUSH || event == FL_MOVE || event == FL_ENTER) {

        // the startup events, save the current cursor position for the
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

  // if there is no voxelspace or the space is of volumn 0 return
  if ((space->getX() == 0) || (space->getY() == 0) || (space->getZ() == 0))
    return;

  // get the backgroud color, used for the dimmed squared if the layer below
  unsigned char bgr, bgg, bgb;
  Fl::get_color(color(), bgr, bgg, bgb);

  int sx, sy, tx, ty;
  calcParameters(&sx, &sy, &tx, &ty);

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
        fl_color(r, g, b);
        drawNormalTile(x, y, tx, ty, sx, sy);
        break;
      case voxel_c::VX_VARIABLE:
        fl_color(r, g, b);
        drawVariableTile(x, y, tx, ty, sx, sy);
        break;
      default:
        // for empty squares we check the layer below and if the square below is
        // not empty draw a very dimmed square
        if ((currentZ > 0) && (space->getState(x, space->getY()-y-1, currentZ-1) != voxel_c::VX_EMPTY)) {
          fl_color(((int)bgr*5+r)/6, ((int)bgg*5+g)/6, ((int)bgb*5+b)/6);
          drawNormalTile(x, y, tx, ty, sx, sy);
        }
      }

      // if the voxel is not empty and has a color assigned, draw a marker in the
      // upper left corner with the color of the constraint color
      if ((space->getState(x, space->getY()-y-1, currentZ) != voxel_c::VX_EMPTY) &&
          space->getColor(x, space->getY()-y-1, currentZ)) {

        puzzle->getColor(space->getColor(x, space->getY()-y-1, currentZ)-1, &r, &g, &b);
        fl_color(r, g, b);
        drawTileColor(x, y, tx, ty, sx, sy);
      }

      // the color for the grid lines
      if (active())
        fl_color(labelcolor());
      else
        fl_color(color());

      // draw the rectangle around the square, this will be the grid
      drawTileFrame(x, y, tx, ty, sx, sy);
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
        for (unsigned int y = 0; y <= space->getY(); y++)
          drawTileCursor(x, y, x1, y1, x2, y2, tx, ty, sx, sy);
    }
  }
}

// this function finds out if a given square is inside the selected region
// this check includes the symmetric and column edit modes
//
// it works resursive. mode contains the yet to check symmetries and columns
bool gridEditor_c::inRegion(int x, int y, int x1, int x2, int y1, int y2, int sx, int sy, int mode) {

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

