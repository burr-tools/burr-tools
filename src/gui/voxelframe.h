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
#ifndef __VOXEL_FRAME_H__
#define __VOXEL_FRAME_H__

#include "BlockList.h"

#include <FL/Fl_Gl_Window.H>
#include <FL/gl.h>

#include <vector>

class voxel_c;
class puzzle_c;
class problem_c;
class assembly_c;
class piecePositions_c;
class voxelDrawer_c;

class rotater_c;

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
class voxelFrame_c : public Fl_Gl_Window {

  public:

    voxelFrame_c(int x,int y,int w,int h);
    virtual ~voxelFrame_c(void);

    typedef enum {
      pieceColor,
      paletteColor,
      anaglyphColor,
      anaglyphColorL
    } colorMode;
    void setColorMode(colorMode color);

    typedef enum {
      normal,          // draw normal cubes with a grate at outer edges
      gridline,        // draw only the outer edges
      invisible        // draw nothing
    } drawingMode;
    void setDrawingMode(unsigned int nr, drawingMode mode);

    /* only active in single mode
     * the marker has 2 parts, a white part that shows the complete z layer
     * and a black part that is only drawn between the given coordinates
     * if you don't want the black part make x1 >= x2
     */
    void setMarker(int x1, int y1, int x2, int y2, int z, int markerType);
    void hideMarker(void);

    void useLightning(bool val) {
      _useLightning = val;
      redraw();
    }

    void showNothing(void);
    void showSingleShape(const puzzle_c * puz, unsigned int shapeNum);
    void showColors(const puzzle_c * puz, colorMode mode);
    void showAssembly(const problem_c * puz, unsigned int solNum);
    void updatePositions(piecePositions_c *shifting);
    void updatePositionsOverlap(piecePositions_c *shifting);
    void dimStaticPieces(piecePositions_c *shifting);
    void showAssemblerState(const problem_c * puz, const assembly_c * assm);
    void updateVisibility(PieceVisibility * pcvis);
    void showProblem(const puzzle_c * puz, unsigned int problem, unsigned int selShape);
    void showPlacement(const problem_c * puz, unsigned int piece, unsigned char trans, int x, int y, int z);

    // this value determines the scaling factor used to draw the cube.
    void setSize(double sz);
    double getSize(void) const { return size; }

    void setCallback(VoxelViewCallbacks *c = 0) { cb = c; }
    bool pickShape(int x, int y, unsigned int *shape, unsigned long *voxel, unsigned int *face);
    void setDrawer(voxelDrawer_c * dr);

    typedef enum {

      VFT_PS = 0,
      VFT_EPS = 1,
      VFT_TEX = 2,
      VFT_PDF = 3,
      VFT_SVG = 4,
      VFT_PGF = 5
    } VectorFiletype;

    void exportToVector(const char * fname, VectorFiletype vt);

    void setRotaterMethod(int method);

  private:

    voxelDrawer_c * drawer;

    assembly_c * curAssembly; // the currently shown assembly (if there is one)

    /* Draws the voxelspace. */
    void drawVoxelSpace();

    unsigned int addSpace(const voxel_c * vx);
    void clearSpaces(void);

    void setSpaceColor(unsigned int nr, float r, float g, float b, float a);
    void setSpaceColor(unsigned int nr, float a);

    void setSpacePosition(unsigned int nr, float x, float y, float z, float scale);

    typedef enum {
      ScaleRotateTranslate,      // for showing problems
      TranslateRoateScale,       // for showing pieces
      CenterTranslateRoateScale  // for showing disassembly
    } transformationType;
    transformationType trans;

    void setCenter(float x, float y, float z) {
      centerX = x;
      centerY = y;
      centerZ = z;
      redraw();
    }

    typedef struct {

      float r, g, b, a;
      const voxel_c * shape;
      drawingMode mode;
      float x, y, z, scale;
      bool dim;
      GLuint list;  // the display list for this shape 0 means no list defined

    } shapeInfo;

    typedef struct {
      float r, g, b;
    } colorInfo;

    std::vector<colorInfo> palette;

    /* the marker position */
    int mX1, mY1, mZ, mX2, mY2;
    int markerType;

    rotater_c * rotater;
    int rotMethod;
    double size;

    VoxelViewCallbacks * cb;

    std::vector<shapeInfo> shapes;

    colorMode colors;

    bool _showCoordinateSystem;

    float centerX, centerY, centerZ;

    bool _useLightning;

    // when picking shapes, this is the coordinate to use
    int pickx, picky;

    void draw();
    int handle(int event);
};

#endif
