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


#ifndef __SQUAREEDITOR_H__
#define __SQUAREEDITOR_H__

#include <FL/Fl.H>
#include <FL/Fl_Widget.h>

#include "../lib/voxel.h"

/**
 * this widget allows to edit voxel spaces. It shows one Z-Layer of the space as a grid
 * of spheres and each of these quares can be toggled between the state filled, variable, empty
 *
 * the callback is issued on on the following occasions:
 *  - Mouse moves inside the area of the widget (for the 3d view to show the cursor
 *  - Changes of voxel states
 */
class SquareEditor : public Fl_Widget {

private:

  // the current voxel space that is worked on
  voxel_c * space;

  // the current edited layer
  int currentZ;

  // the number of the piece, this is used to colorize the squares
  int piecenumber;

  // the edit state
  int state;

  // current position of the mouse cursor
  int mX, mY, mZ;

  // is the mouse inside the widget?
  bool inside;

  // calculate the size of the grid cells and the
  // top right corner position
  void calcParameters(int *s, int *tx, int *ty);

  int callbackReason;

protected:

  void draw();

public:

  SquareEditor(int x, int y, int w, int h, const char *label = 0) : Fl_Widget(x, y, w, h, label), space(0), currentZ(0), state(0), mX(-1), mY(-1), mZ(-1), inside(false) {}

  // sets the voxel space to edit, the widget doesn't take over the space
  // the voxelspace must not be deleted while this is set here
  void setVoxelSpace(voxel_c * newSpace, int piecenum);

  // sets the z layer to edit the value is clamped to valid values
  void setZ(int z);

  // get the current Z value
  int getZ(void) { return currentZ; }

  int handle(int event);

  // get the mouse mosition so that the cursor can be shown in 3d view
  bool getMouse(void) { return inside; }
  int getMouseX(void) { return mX; }
  int getMouseY(void) { return mY; }
  int getMouseZ(void) { return mZ; }

  // find out the reason why this widget called the callback
  enum {
    RS_MOUSEMOVE,
    RS_CHANGESQUARE
  };

  int getReason(void) { return callbackReason; }
};

#endif
