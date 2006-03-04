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

#include "ArcBall.h"

#include "../lib/puzzle.h"

/* this callback class defines 2 functions that are called, when
 * the draw function is called in VoxelView
 */
class VoxelViewCallbacks {
  public:

    /* this function gets called before the the setup of the camera
     * and rotation of the object. If you return true
     * here the draw function will NOT setup its own camera and rotation
     * but you can continue using the light setup as its done before
     * this function is called
     */
    virtual bool PreDraw(void) { return false; }

    /* this is called AFTER the data has been drawn
     */
    virtual void PostDraw(void) { }
};

class PiecePositions;
class PieceVisibility;

/* this is a voxel drawer that paints its information into
 * an Fl_Gl_Window and that has handling for
 * dragging the contents with the mouse via an arcball
 */
class VoxelView : public Fl_Gl_Window
{

private:

  ArcBall_c * arcBall;
  bool doUpdates;
  double size;

  VoxelViewCallbacks * cb;

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

  void addRotationTransformation(void);
  void updateRequired(void);

  ArcBall_c * getArcBall(void) { return arcBall; }
  const ArcBall_c * getArcBall(void) const { return arcBall; }

  virtual void drawData(void) {}

  void setCallback(VoxelViewCallbacks *c = 0) { cb = c; }


  virtual void showSingleShape(const puzzle_c * puz, unsigned int shapeNum) = 0;
  virtual void showProblem(const puzzle_c * puz, unsigned int probNum, unsigned int selShape) = 0;
  virtual void showColors(const puzzle_c * puz, bool show) = 0;
  virtual void showAssembly(const puzzle_c * puz, unsigned int probNum, unsigned int solNum) = 0;
  virtual void showAssemblerState(const puzzle_c * puz, unsigned int probNum, const assembly_c * assm) = 0;
  virtual void showPlacement(const puzzle_c * puz, unsigned int probNum, unsigned int piece, unsigned char trans, int x, int y, int z) = 0;
  virtual void updatePositions(PiecePositions *shifting) = 0;
  virtual void updateVisibility(PieceVisibility * pcvis) = 0;
  virtual void dimStaticPieces(PiecePositions *shifting) = 0;
};

#endif
