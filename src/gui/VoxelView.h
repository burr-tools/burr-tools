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
#ifndef __VOXEL_VIEW_H__
#define __VOXEL_VIEW_H__

#include <FL/Fl_Gl_Window.H>

#include "VoxelDrawer.h"
#include "ArcBall.h"

class VoxelView : public Fl_Gl_Window, public VoxelDrawer
{

private:

  ArcBall_c * arcBall;
  bool doUpdates;
  double size;

public:

  VoxelView(int x,int y,int w,int h,const char *l=0);
  ~VoxelView(void) { delete arcBall; }

  void draw();
  int handle(int event);

  // if more complex updates are done, this can avoid doing
  // a screen update each time
  void update(bool doIt);

  // this value determines the scaling factor used to draw the cube.
  void setSize(double sz);
  double getSize(void) const { return size; }

  virtual void addRotationTransformation(void);
  virtual void updateRequired(void);

  ArcBall_c * getArcBall(void) { return arcBall; }
  const ArcBall_c * getArcBall(void) const { return arcBall; }
};

#endif
