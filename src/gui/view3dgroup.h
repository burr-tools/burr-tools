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
#ifndef __VIEW3D_GROUP_H__
#define __VIEW3D_GROUP_H__

#include "voxelframe.h"
#include "Layouter.h"

#include <FL/Fl_Group.H>
#include <FL/Fl_Slider.H>

class guiGridType_c;
class piecePositions_c;

// the groups with the 3d view and the zoom slider
class LView3dGroup : public Fl_Group, public layoutable_c {

  voxelFrame_c * View3D;
  Fl_Slider * slider;

public:

  LView3dGroup(int x, int y, int w, int h, const guiGridType_c * ggt);

  void newGridType(const guiGridType_c * ggt);

  void cb_slider(void);

  void showNothing(void) { View3D->clearSpaces(); }
  void showSingleShape(const puzzle_c * puz, unsigned int shapeNum);
  void showProblem(const puzzle_c * puz, unsigned int probNum, unsigned int selShape);
  void showAssembly(const puzzle_c * puz, unsigned int probNum, unsigned int solNum);
  void showPlacement(const puzzle_c * puz, unsigned int probNum, unsigned int piece, unsigned char trans, int x, int y, int z);
  void showAssemblerState(const puzzle_c * puz, unsigned int probNum, const assembly_c * assm) {
    View3D->showAssemblerState(puz, probNum, assm);
  }

  void updatePositions(piecePositions_c *shifting);
  void updateVisibility(PieceVisibility * pcvis);
  void showColors(const puzzle_c * puz, voxelFrame_c::colorMode mode);

  void setMarker(int x1, int y1, int x2, int y2, int z, int type) { View3D->setMarker(x1, y1, x2, y2, z, type); }
  void hideMarker(void) { View3D->hideMarker(); }
  void useLightning(bool val) { View3D->useLightning(val); }

  double getZoom(void) { return slider->value(); }
  void setZoom(double v) { slider->value(v); cb_slider(); }

  void redraw(void) { View3D->redraw(); }

  voxelFrame_c * getView(void) { return View3D; }

  virtual void getMinSize(int * w, int *h) const {
    *w = 40;
    *h = 40;
  }
};

#endif
