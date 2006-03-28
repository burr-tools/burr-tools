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
#ifndef __GRID_EDITOR_0_H__
#define __GRID_EDITOR_0_H__

#include "grideditor.h"

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
class gridEditor_0_c : public gridEditor_c {

private:

  // calculate the size of the grid cells and the
  // top right corner position
  void calcParameters(int *s, int *tx, int *ty);

  void calcGridPosition(int x, int y, int *gx, int *gy);

protected:

  void draw();

public:

  gridEditor_0_c(int x, int y, int w, int h, puzzle_c * p) : gridEditor_c(x, y, w, h, p) {}
};

#endif
