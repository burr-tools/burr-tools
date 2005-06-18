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


#ifdef WIN32
#include <windows.h>
#endif

#include <FL/Fl.H>
#include <FL/Fl_Gl_Window.H>

#include "../lib/voxel.h"
#include "../lib/puzzle.h"

#include "ArcBall.h"
#include "DisasmToMoves.h"

#include <vector>

/** this class draws a 3d view of a voxel space.
 * there are 2 modes:
 * single piece: here the value of PieceNumber gives the piecenumber
 * multipiece: here the value of the voxelgives the piecenumber
 *
 * single piece mode is extremely simple, only one parameter: the piece number
 * cubes are either empty, filled or variable, variable cubes
 * are drawn as a ...
 *
 * single cube allows to place markers: mark the plane, and additionally one single
 * voxel. the marked plane is always the z-plane.
 *
 * multi mode is a bit more complex. here each peace can be shifted by a vector of 3
 * reals to make shifting pieces possible
 *
 * additionally pieces can be transparent
 */

//#define DEBUG

#ifndef DEBUG
class VoxelView : public Fl_Gl_Window
#else
class VoxelView : public Fl_Widget
#endif
{


private:

  /* Draws the voxelspace. */
  void drawVoxelSpace();

  const assemblyVoxel_c * asmSpace;
  const pieceVoxel_c * pcSpace;
  voxel_type pieceNumber;

  PiecePositions * shiftArray;
  char * visArray;
  int arraySize;
  int * colArray;

  ArcBall_c * arcBall;

  double size;

public:

  VoxelView(int x,int y,int w,int h,const char *l=0);
  // this value determines the scaling factor used to draw the cube.
  void setSize(double sz) {
    size = sz;
    redraw();
  }

  void draw();
  int handle(int event);

  /* sets the voxel space and the piecenumber and sets single mode */
  void setVoxelSpace(const pieceVoxel_c *sp, int pn);

  /* sets a new voxel space and the necessary parameters and
   * activates multi mode
   *
   * the parameters are pointers to arrays. they are read each time
   * the image is redrawn, so you can change them and the image
   * will replect that as soon as it is redrawn
   *
   * set shiftarray only active in multi mode
   * this array defines by how much a piece is
   * shifted in the 3 directions. for the first
   * piece the first 3 values, the 2nd piece are the next 3 values, ...
   * numPieces is used for sanity check, it must contain the number
   * of pieces inside the multi display, so the number of entries
   * in the array must be numPieces*3
   * pieces with grater number are supposed to be unshifted
   *
   * sets the visibility of each piece, if 0 piece isnormal,
   * 1 is only outlined
   * 2 is completely invisible
   *
   * pieces outside range are normal
   */
  void setVoxelSpace(const puzzle_c * puz, unsigned int prob, unsigned int sol, PiecePositions * pos, char * vArray, int numPieces, int * colors);


public:
  /* ------ new interface -------- */

  void addSpace(const pieceVoxel_c * vx);
  void clearSpaces(void);

  unsigned int spaceNumber(void);

  void setSpaceColor(unsigned int nr, unsigned char r, unsigned char g, unsigned char b);
  void setSpacePosition(unsigned int nr, float x, float y, float z);

  typedef enum {
    normal,          // draw normal cubes with a grate at outer edges
    gridline,        // draw only the outer edges
    invisible        // draw nothing
  } drawingMode;

  typedef enum {
    pieceColor,
    paletteColor
  } colorMode;

  void setDrawingMode(unsigned int nr, drawingMode mode, colorMode color);

  void setScaling(float factor);

  typedef enum {
    ScaleRotateTranslate,     // for showing problems
    TranslateRoateScale       // for showing pieces and disassembly
  } transformationType;

  void setTransformationType(transformationType type);

  /* only active in single mode */
  void setMarker(int x, int y, int z) {
    markerType = true;
    mX = x;
    mY = y;
    mZ = z;
  }

  void hideMarker(void) { markerType = false; }

private:

  typedef struct {

    unsigned int r, g, b;
    pieceVoxel_c * shape;
    drawingMode mode;
    float x, y, z;

  } shapeInfo;

  float scale;

  /* the marker position */
  int mX, mY, mZ;
  bool markerType;

  std::vector<shapeInfo> shapes;

  transformationType trans;
  colorMode colors;

};

