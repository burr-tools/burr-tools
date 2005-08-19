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

  unsigned int addSpace(const pieceVoxel_c * vx);
  void clearSpaces(void);

  unsigned int spaceNumber(void);

  void setSpaceColor(unsigned int nr, float r, float g, float b, float a);
  void setSpaceColor(unsigned int nr, float a);
  void setSpacePosition(unsigned int nr, float x, float y, float z, float scale);

  typedef enum {
    normal,          // draw normal cubes with a grate at outer edges
    gridline,        // draw only the outer edges
    invisible        // draw nothing
  } drawingMode;

  typedef enum {
    pieceColor,
    paletteColor
  } colorMode;

  void setDrawingMode(unsigned int nr, drawingMode mode);
  void setColorMode(colorMode color);

  void setScaling(float factor);

  typedef enum {
    ScaleRotateTranslate,      // for showing problems
    TranslateRoateScale,       // for showing pieces
    CenterTranslateRoateScale  // for showing disassembly
  } transformationType;

  void setTransformationType(transformationType type);

  /* only active in single mode */
  void setMarker(int x, int y, int z);
  void hideMarker(void);

  // if more complex updates are done, this can avoid doing
  // a screen update each time
  void update(bool doIt);

  void showCoordinateSystem(bool show) { _showCoordinateSystem = show; redraw(); }

  void setCenter(float x, float y, float z) {
    centerX = x;
    centerY = y;
    centerZ = z;
    redraw();
  }

  void clearPalette(void) { palette.clear(); }
  void addPaletteEntry(float r, float g, float b);

private:

  typedef struct {

    float r, g, b, a;
    const pieceVoxel_c * shape;
    drawingMode mode;
    float x, y, z, scale;

  } shapeInfo;

  float scale;

  /* the marker position */
  int mX, mY, mZ;
  bool markerType;

  std::vector<shapeInfo> shapes;

  transformationType trans;
  colorMode colors;

  bool _showCoordinateSystem;

  bool doUpdates;

  float centerX, centerY, centerZ;


  typedef struct {
    float r, g, b;
  } colorInfo;

  std::vector<colorInfo> palette;
};

