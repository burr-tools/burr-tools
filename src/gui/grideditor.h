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
#ifndef __GRID_EDITOR_H__
#define __GRID_EDITOR_H__

#include <FL/Fl.H>
#include <FL/Fl_Widget.h>

class puzzle_c;

/**
 * this widget allows to edit voxel spaces. It shows one Z-Layer of the space as a grid
 * of spheres and each of these quares can be toggled between the state filled, variable, empty
 *
 * the callback is issued on on the following occasions:
 *  - Mouse moves inside the area of the widget (for the 3d view to show the cursor
 *  - Changes of voxel states
 *
 *  this is by faaaaar the most ugly code in the whole project, this really needs a clean rewrite
 */
class gridEditor_c : public Fl_Widget {

public:

  /* sets the task of what to do next */
  typedef enum {
    TSK_SET,
    TSK_VAR,
    TSK_RESET,
    TSK_COLOR
  } enTask;

  /* some editing tools */
  enum {
    TOOL_MIRROR_X = 1,
    TOOL_MIRROR_Y = 2,
    TOOL_MIRROR_Z = 4,
    TOOL_STACK_X = 8,
    TOOL_STACK_Y = 16,
    TOOL_STACK_Z = 32
  };

  /* edit types */
  enum {
    EDT_SINGLE,
    EDT_RUBBER
  };

protected:

  puzzle_c * puzzle;

  // the edit state
  int state;

  // the current edited layer
  unsigned int currentZ;

  // the number of the piece, this is used to colorize the squares
  unsigned int piecenumber;

  // current position of the mouse cursor
  int mX, mY, mZ;
  int startX, startY;

  // is the mouse inside the widget?
  bool inside;

  int callbackReason;

  // the constraint color to use
  unsigned int currentColor;

  enTask task;
  unsigned char activeTools;
  int editType;

  int handle(int event);

public:

  gridEditor_c(int x, int y, int w, int h, puzzle_c * p) : Fl_Widget(x, y, w, h), puzzle(p), state(0), currentZ(0), piecenumber(0), mX(0xFFFF), mY(0xFFFF), mZ(0xFFFF), startX(0), startY(0), inside(false), callbackReason(0), currentColor(0), task(TSK_SET), activeTools(0), editType(0) {}

  // sets the z layer to edit the value is clamped to valid values
  void setZ(unsigned int z);

  // get the current Z value
  unsigned int getZ(void) { return currentZ; }

  // sets the color to use for editing voxels
  void setColor(unsigned int col) {
    currentColor = col;
  }

  // sets the voxel space to edit, the widget doesn't take over the space
  // the voxelspace must not be deleted while this is set here
  void setPuzzle(puzzle_c * p, unsigned int piecenum);
  void clearPuzzle();

  void setTask(enTask t) { task = t; }

  // get the mouse mosition so that the cursor can be shown in 3d view
  bool getMouse(void) { return inside; }
  int getMouseX1(void) { return (startX < mX)?(startX):(mX); }
  int getMouseY1(void) { return (startY < mY)?(startY):(mY); }
  int getMouseX2(void) { return (startX > mX)?(startX):(mX); }
  int getMouseY2(void) { return (startY > mY)?(startY):(mY); }
  int getMouseZ(void) { return mZ; }

  // find out the reason why this widget called the callback
  enum {
    RS_MOUSEMOVE,
    RS_CHANGESQUARE
  };

  int getReason(void) { return callbackReason; }

  void activateTool(int tool) { activeTools |= tool; }
  void deactivateTool(int tool) { activeTools &= ~tool; }
  void setTool(int tool) { activeTools = tool; }

  void setEditType(int type) { editType = type; }

  /* each grid editor has to provide this function
   * is calculates the grid position gx and gy inside the voxel
   * for the given screen position x, y
   */
  virtual void calcGridPosition(int x, int y, int *gx, int *gy) = 0;

  bool setLayer(unsigned int zv);

};

#endif

