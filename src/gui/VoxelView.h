#ifdef WIN32
#include <windows.h>
#endif

#include <FL/Fl.H>
#include <FL/Fl_Gl_Window.H>

#include "../lib/voxel.h"

#include "ArcBall.h"

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

  /* function to set the color for one piece with the given number */
  void setColor(voxel_type piece, int p, float alpha);

  /* Draws the voxelspace. */
  void drawVoxelSpace();

  const voxel_c * space;
  voxel_type pieceNumber;

  /* defines the mod the widget works in, see drawVoxelSpace */
  enum {
    singleMode,
    multiMode
  } mode;

  float* shiftArray;
  char * visArray;
  int arraySize;
  int * colArray;

  /* the marker position */
  int mX, mY, mZ;
  bool markerType;

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
  void setVoxelSpace(const voxel_c *sp, int pn);

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
  void setVoxelSpace(const voxel_c *sp, float * shArray, char * vArray, int numPieces, int * colors);

  /* only active in single mode */
  void setMarker(int x, int y, int z) {
    markerType = true;
    mX = x;
    mY = y;
    mZ = z;
  }

  void hideMarker(void) { markerType = false; }
};

