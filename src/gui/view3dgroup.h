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
#ifndef __VIEW3D_GROUP_H__
#define __VIEW3D_GROUP_H__

#include "Layouter.h"

#include <FL/Fl_Group.H>
#include <FL/Fl_Slider.H>

class piecePositions_c;
class voxelFrame_c;

// the groups with the 3d view and the zoom slider
class LView3dGroup : public Fl_Group, public layoutable_c {

  voxelFrame_c * View3D;
  Fl_Slider * slider;

public:

  LView3dGroup(int x, int y, int w, int h);

  void cb_slider(void);

  double getZoom(void) { return slider->value(); }
  void setZoom(double v) { slider->value(v); cb_slider(); }

  void redraw(void);

  voxelFrame_c * getView(void) { return View3D; }

  virtual void getMinSize(int * w, int *h) const {
    *w = 40;
    *h = 40;
  }

  int handle(int event);
};

#endif
