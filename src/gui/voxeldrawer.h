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
#ifndef __VOXEL_DRAWER_H__
#define __VOXEL_DRAWER_H__

#ifdef WIN32
#include <windows.h>
#endif

#include "BlockList.h"

#include <FL/Fl_Gl_Window.H>

#include <vector>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

class voxel_c;
class puzzle_c;
class assembly_c;
class piecePositions_c;

class arcBall_c;

/* this callback class defines 2 functions that are called, when
 * the draw function is called in VoxelView
 */
class VoxelViewCallbacks {
  public:

    virtual ~VoxelViewCallbacks(void) {}

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

/** this class draws a 3d view of a voxel space.
 * there are 2 modes:
 * single piece: here the value of PieceNumber gives the piecenumber
 * multipiece: here the value of the voxel gives the piecenumber
 *
 * single piece mode is extremely simple, only one parameter: the piece number
 * cubes are either empty, filled or variable, variable cubes
 * are drawn as a ...
 *
 * single cube allows to place markers: mark the plane, and additionally one single
 * voxel. The marked plane is always the z-plane.
 *
 * multi mode is a bit more complex. Here each peace can be shifted by a vector of 3
 * real values to make shifting pieces possible
 *
 * additionally pieces can be transparent
 */
class voxelDrawer_c : public Fl_Gl_Window {

private:

  /* Draws the voxelspace. */
  void drawVoxelSpace();

public:

  voxelDrawer_c(int x,int y,int w,int h);
  virtual ~voxelDrawer_c(void);

  virtual void drawData(void);

  unsigned int addSpace(const voxel_c * vx);
  void clearSpaces(void);

  unsigned int spaceNumber(void);

  void setSpaceColor(unsigned int nr, float r, float g, float b, float a);
  void setSpaceColor(unsigned int nr, float a);

  virtual void recalcSpaceCoordinates(float * x, float * y, float * z);

  void setSpacePosition(unsigned int nr, float x, float y, float z, float scale);
  void setSpaceDim(unsigned int nr, bool dim);

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

  typedef enum {
    ScaleRotateTranslate,      // for showing problems
    TranslateRoateScale,       // for showing pieces
    CenterTranslateRoateScale  // for showing disassembly
  } transformationType;

  void setTransformationType(transformationType type);

  /* some editing tools */
  enum {
    TOOL_MIRROR_X = 1,
    TOOL_MIRROR_Y = 2,
    TOOL_MIRROR_Z = 4,
    TOOL_STACK_X = 8,
    TOOL_STACK_Y = 16,
    TOOL_STACK_Z = 32
  };

  /* only active in single mode
   * the marker has 2 parts, a white part that shows the complete z layer
   * and a black part that is only drawn between the given coordinates
   * if you don't want the black part make x1 >= x2
   */
  void setMarker(int x1, int y1, int x2, int y2, int z, int markerType);
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

protected:

  typedef struct {

    float r, g, b, a;
    const voxel_c * shape;
    drawingMode mode;
    float x, y, z, scale;
    bool dim;

  } shapeInfo;

  typedef struct {
    float r, g, b;
  } colorInfo;

  std::vector<colorInfo> palette;

  /* the marker position */
  int mX1, mY1, mZ, mX2, mY2;
  int markerType;

private:

  arcBall_c * arcBall;
  bool doUpdates;
  double size;

  VoxelViewCallbacks * cb;

  std::vector<shapeInfo> shapes;

  transformationType trans;
  colorMode colors;

  bool _showCoordinateSystem;

  float centerX, centerY, centerZ;

  bool _useLightning;

  virtual void drawCursor(unsigned int sx, unsigned int sy, unsigned int sz) = 0;

  virtual void calculateSize(const voxel_c * shape, float * x, float * y, float * z) = 0;

  /* this matrix is used to change the look of the basic unit */
  GLfloat transformMatrix[16];

  // set to true, when the grid type changed and the transformation matrix is not yet updated
  bool _gtChanged;

public:

  void showSingleShape(const puzzle_c * puz, unsigned int shapeNum);
  void showProblem(const puzzle_c * puz, unsigned int probNum, unsigned int selShape);
  void showColors(const puzzle_c * puz, bool show);
  void showAssembly(const puzzle_c * puz, unsigned int probNum, unsigned int solNum);
  void showAssemblerState(const puzzle_c * puz, unsigned int probNum, const assembly_c * assm);
  void showPlacement(const puzzle_c * puz, unsigned int probNum, unsigned int piece, unsigned char trans, int x, int y, int z);
  void updatePositions(piecePositions_c *shifting);
  void updateVisibility(PieceVisibility * pcvis);
  void dimStaticPieces(piecePositions_c *shifting);

  /* this function is called whenever the gridType changed, so that the drawer can update
   * internal structures. Only parameters may have changed, but not the type itself
   */
  virtual void gridTypeChanged(void);

  /* this function sets a transformations matrix that is used to change the appearance of the
   * basic unit
   */
  void setTransformationMatrix(GLfloat m[16]);


  virtual void drawFrame(const voxel_c * space, int x, int y, int z, float edge) = 0;
  virtual void drawNormalVoxel(const voxel_c * space, int x, int y, int z, float alpha, float edge) = 0;
  virtual void drawVariableMarkers(const voxel_c * space, int x, int y, int z) = 0;


  void drawGridTriangle(double x0, double y0, double z0,
      double v1x, double v1y, double v1z,
      double v2x, double v2y, double v2z, int diag);
  void drawGridRect(double x0, double y0, double z0,
      double v1x, double v1y, double v1z,
      double v2x, double v2y, double v2z, int diag);

  bool inRegion(int x, int y, int z, int x1, int x2, int y1, int y2, int z1, int z2, int sx, int sy, int sz, int mode);

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

  arcBall_c * getArcBall(void) { return arcBall; }
  const arcBall_c * getArcBall(void) const { return arcBall; }

  void setCallback(VoxelViewCallbacks *c = 0) { cb = c; }
};

#endif
