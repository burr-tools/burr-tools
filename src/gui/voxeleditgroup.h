/* Burr Solver
 * Copyright (C) 2003-2007  Andreas Röver
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
#ifndef __VOXEL_EDIT_GROUP_H__
#define __VOXEL_EDIT_GROUP_H__

#include "BlockList.h"
#include "grideditor.h"

#include "Layouter.h"

#include <FL/Fl_Group.H>

class guiGridType_c;
class piecePositions_c;

class Fl_Slider;

class LineSpacer;

// the group for the square editor including the coloured marker and the slider for the z axis
class VoxelEditGroup_c : public Fl_Group, public layoutable_c {

  gridEditor_c * sqedit;
  Fl_Slider * zselect;
  LineSpacer * space;

public:

  VoxelEditGroup_c(int x, int y, int w, int h, puzzle_c * puzzle, const guiGridType_c * ggt);

  void newGridType(const guiGridType_c * ggt, puzzle_c * puzzle);

  void draw();

  void cb_Zselect(Fl_Slider* o);

  void setZ(unsigned int val);
  int getZ(void) { return sqedit->getZ(); }

  void cb_Sqedit(void) { do_callback(this, user_data()); }

  int getReason(void) { return sqedit->getReason(); }

  bool getMouse(void) { return sqedit->getMouse(); }

  int getMouseX1(void) { return sqedit->getMouseX1(); }
  int getMouseY1(void) { return sqedit->getMouseY1(); }
  int getMouseX2(void) { return sqedit->getMouseX2(); }
  int getMouseY2(void) { return sqedit->getMouseY2(); }
  int getMouseZ(void) { return sqedit->getMouseZ(); }

  void setPuzzle(puzzle_c * puzzle, unsigned int num);

  void clearPuzzle(void) {
    sqedit->clearPuzzle();
  }

  void setColor(unsigned int num) {
    sqedit->setColor(num);
  }

  void deactivate(void) {
    sqedit->deactivate();
  }

  void activate(void) {
    sqedit->activate();
  }

  void editSymmetries(int syms) {
    sqedit->setTool(syms);
  }

  void editChoice(gridEditor_c::enTask c) {
    sqedit->setTask(c);
  }

  void editType(int type) {
    sqedit->setEditType(type);
  }

  virtual void getMinSize(int *width, int *height) const {
    *width = 40;
    *height = 20;
  }
};

#endif
