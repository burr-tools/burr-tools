/* Burr Solver
 * Copyright (C) 2003-2005  Andreas R�ver
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

#include "../lib/voxel.h"
#include "../lib/puzzle.h"

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
#ifndef __VOXEL_DRAWER__
#define __VOXEL_DRAWER__

class VoxelDrawer {

private:

  /* Draws the voxelspace. */
  void drawVoxelSpace();

public:

  VoxelDrawer(void);
  virtual ~VoxelDrawer(void) {}

  void drawData(void);

  unsigned int addSpace(const voxel_c * vx);
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

  /* only active in single mode
   * the marker has 2 parts, a white part that shows the complete z layer
   * and a black part that is only drawn between the given coordinates
   * if you don't want the black part make x1 >= x2
   */
  void setMarker(int x1, int y1, int x2, int y2, int z);
  void hideMarker(void);

  void showCoordinateSystem(bool show) { _showCoordinateSystem = show; updateRequired(); }

  void setCenter(float x, float y, float z) {
    centerX = x;
    centerY = y;
    centerZ = z;
    updateRequired();
  }

  void clearPalette(void) { palette.clear(); }
  void addPaletteEntry(float r, float g, float b);

  void useLightning(bool val) {
    _useLightning = val;
    updateRequired();
  }

  virtual void addRotationTransformation(void) {}
  virtual void updateRequired(void) {}

private:

  typedef struct {

    float r, g, b, a;
    const voxel_c * shape;
    drawingMode mode;
    float x, y, z, scale;

  } shapeInfo;

  float scale;

  /* the marker position */
  int mX1, mY1, mZ, mX2, mY2;
  bool markerType;

  std::vector<shapeInfo> shapes;

  transformationType trans;
  colorMode colors;

  bool _showCoordinateSystem;

  float centerX, centerY, centerZ;

  typedef struct {
    float r, g, b;
  } colorInfo;

  std::vector<colorInfo> palette;

  bool _useLightning;
};

#endif