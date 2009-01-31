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
#ifndef __GRID_EDITOR_H__
#define __GRID_EDITOR_H__

#include <FL/Fl.H>
#include <FL/Fl_Widget.H>

class puzzle_c;

/**
 * this widget allows to edit voxel spaces. It shows one Z-Layer of the space as a grid
 * of spheres and each of these squares can be toggled between the state filled, variable, empty
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

  void setTask(enTask t) { task = t; }

  /* some editing tools these might change with the inherited classes
   * but right now we keep it in here and have just one set of editing tools
   */
  enum {
    TOOL_MIRROR_X = 1,
    TOOL_MIRROR_Y = 2,
    TOOL_MIRROR_Z = 4,
    TOOL_STACK_X = 8,
    TOOL_STACK_Y = 16,
    TOOL_STACK_Z = 32
  };

  void activateTool(int tool) { activeTools |= tool; }
  void deactivateTool(int tool) { activeTools &= ~tool; }
  void setTool(int tool) { activeTools = tool; }

  /* edit types */
  enum {
    EDT_SINGLE,
    EDT_RUBBER
  };

  void setEditType(int type) { editType = type; }

protected:

  puzzle_c * puzzle;

  // the edit state
  int state;

  // the current edited layer
  unsigned int currentZ;

  // the number of the piece, this is used to colourize the squares
  unsigned int piecenumber;

  // current position of the mouse cursor
  int mX, mY, mZ;
  int startX, startY;

  // area of the marker
  int markX1, markX2, markY1, markY2;

  // is the mouse inside the widget?
  bool inside;

  int callbackReason;

  // the constraint colour to use
  unsigned int currentColor;

  enTask task;
  unsigned char activeTools;
  int editType;

  bool setLayer(unsigned int zv);
  int handle(int event);
  void draw();

protected:

  /* tool function to calculate, if the given voxel coordinate x;y is inside
   * the specified area
   */
  bool inRegion(int x, int y);

public:

  gridEditor_c(int x, int y, int w, int h, puzzle_c * p) : Fl_Widget(x, y, w, h), puzzle(p), state(0), currentZ(0), piecenumber(0), mX(0xFFFF), mY(0xFFFF), mZ(0xFFFF), startX(0), startY(0), inside(false), callbackReason(0), currentColor(0), task(TSK_SET), activeTools(0), editType(0) {}

  // sets the z layer to edit the value is clamped to valid values
  void setZ(unsigned int z);

  // get the current Z value
  unsigned int getZ(void) { return currentZ; }

  // sets the colour to use for editing voxels
  void setColor(unsigned int col) { currentColor = col; }

  // sets the voxel space to edit, the widget doesn't take over the space
  // the voxelspace must not be deleted while this is set here
  void setPuzzle(puzzle_c * p, unsigned int piecenum);
  void clearPuzzle();

  // get the mouse position so that the cursor can be shown in 3d view
  bool getMouse(void) { return inside; }
  int getMouseX1(void) { return (startX < mX)?(startX):(mX); }
  int getMouseY1(void) { return (startY < mY)?(startY):(mY); }
  int getMouseX2(void) { return (startX > mX)?(startX):(mX); }
  int getMouseY2(void) { return (startY > mY)?(startY):(mY); }
  int getMouseZ(void) { return mZ; }

  // find out the reason why this widget called the callback
  enum {
    RS_MOUSEMOVE,     // the mouse moved, the cursor must be updated
    RS_CHANGESQUARE   // something was edited, 3D view must be redrawn
  };

  int getReason(void) { return callbackReason; }

  /* each grid editor has to provide this function
   * is calculates the grid position gx and gy inside the voxel
   * for the given screen position x, y and the current z layer
   *
   * return false, if the position can not be calculated, (because
   * of division by zero, or so) true, if you can
   */
  virtual bool calcGridPosition(int x, int y, int z, int *gx, int *gy) = 0;

  /* calculate the x and sy scaling and the x and y translation for
   * the current shape so that it fits into the available space and
   * is well centred, the returned values will be given
   * to all drawing functions so that they do know where to paint
   */
  virtual void calcParameters(int *sx, int *sy, int *tx, int *ty) = 0;

  /* the following function all do draw a little bit of one tile
   * The first draws the colour part of a normal tile, this should fill
   * out the tile completely, the 2nd draws the colour part of a variable tile
   * this should leave 3 pixels empty to the edge of the tile. Both functions
   * do not draw the frame around the tile, just the inside. You don't need
   * to take care of the colour, just draw
   *
   * If follows the frame drawing function. Just draw a frame around the tile
   * keep in mind to properly merge with the neighbours otherwise the frames might
   * be 2 pixels thick
   *
   * Then we have the tile colour. Here you draw the assigned colour constraint colour
   * marker. This should be placed within one corner of the tile. Just draw it and
   * don't worry for the colour
   *
   * Finally the cursor. Right now its completely up to you do completely draw
   * it. Eventually I will provide a better way to do that
   */
  virtual void drawNormalTile(int x, int y, int z, int tx, int ty, int sx, int sy) = 0;
  virtual void drawVariableTile(int x, int y, int z, int tx, int ty, int sx, int sy) = 0;
  virtual void drawTileFrame(int x, int y, int z, int tx, int ty, int sx, int sy) = 0;
  virtual void drawTileColor(int x, int y, int z, int tx, int ty, int sx, int sy) = 0;
  virtual void drawTileCursor(int x, int y, int z, int tx, int ty, int sx, int sy) = 0;

private:

  bool setRecursive(unsigned char tools, int x, int y, int z);

};

#endif

