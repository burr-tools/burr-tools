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
#include "VoxelDrawer.h"

#include "pieceColor.h"

#include "../lib/voxel.h"
#include "../lib/puzzle.h"
#include "../lib/assembly.h"

#include <math.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

// this is used to shift one side of the cubes so that they slightly differ
// from the side of the next cube, so that (in case of frames) the sides
// are clearly separated and dont't interlock when drawing
#define MY 0.005f

VoxelDrawer::VoxelDrawer(int x,int y,int w,int h,const char *l) :
  VoxelView(x, y, w, h, l), markerType(-1),
  colors(pieceColor), _useLightning(true)
{
};

// draws a wireframe box depending on the neibors
static void drawFrame(const voxel_c * space, int x, int y, int z, float edge) {

  if (fabs(edge) < 0.00001) return;

  glBegin(GL_QUADS);

  if (space->isEmpty2(x, y, z-1)) {
    glNormal3f( 0.0f, 0.0f, -1.0f);

    if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y, z-1)) { glVertex3f(x+0, y+0, z); glVertex3f(x+edge, y+0, z); glVertex3f(x+edge, y+1, z); glVertex3f(x+0, y+1, z); }
    if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y, z-1)) { glVertex3f(x+1, y+0, z); glVertex3f(x+(1-edge), y+0, z); glVertex3f(x+(1-edge), y+1, z); glVertex3f(x+1, y+1, z); }
    if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x, y-1, z-1)) { glVertex3f(x+0, y+0, z); glVertex3f(x+0, y+edge, z); glVertex3f(x+1, y+edge, z); glVertex3f(x+1, y+0, z); }
    if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x, y+1, z-1)) { glVertex3f(x+0, y+1, z); glVertex3f(x+0, y+(1-edge), z); glVertex3f(x+1, y+(1-edge), z); glVertex3f(x+1, y+1, z); }
  }
  if (space->isEmpty2(x-1, y, z)) {
    glNormal3f( -1.0f, 0.0f, 0.0f);

    if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x-1, y-1, z)) { glVertex3f(x, y+0, z+0); glVertex3f(x, y+edge, z+0); glVertex3f(x, y+edge, z+1); glVertex3f(x, y+0, z+1); }
    if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x-1, y+1, z)) { glVertex3f(x, y+1, z+0); glVertex3f(x, y+1, z+1); glVertex3f(x, y+(1-edge), z+1); glVertex3f(x, y+(1-edge), z+0); }
    if (space->isEmpty2(x, y, z-1) || !space->isEmpty2(x-1, y, z-1)) { glVertex3f(x, y+0, z+edge); glVertex3f(x, y+0, z+0); glVertex3f(x, y+1, z+0); glVertex3f(x, y+1, z+edge); }
    if (space->isEmpty2(x, y, z+1) || !space->isEmpty2(x-1, y, z+1)) { glVertex3f(x, y+0, z+1); glVertex3f(x, y+0, z+(1-edge)); glVertex3f(x, y+1, z+(1-edge)); glVertex3f(x, y+1, z+1); }
  }
  if (space->isEmpty2(x+1, y, z)) {
    glNormal3f( 1.0f, 0.0f, 0.0f);

    if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x+1, y-1, z)) { glVertex3f(x+1-MY, y+0, z+0); glVertex3f(x+1-MY, y+0, z+1); glVertex3f(x+1-MY, y+edge, z+1); glVertex3f(x+1-MY, y+edge, z+0); }
    if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x+1, y+1, z)) { glVertex3f(x+1-MY, y+(1-edge), z+0); glVertex3f(x+1-MY, y+(1-edge), z+1); glVertex3f(x+1-MY, y+1, z+1); glVertex3f(x+1-MY, y+1, z+0); }
    if (space->isEmpty2(x, y, z-1) || !space->isEmpty2(x+1, y, z-1)) { glVertex3f(x+1-MY, y+1, z+edge); glVertex3f(x+1-MY, y+1, z+0); glVertex3f(x+1-MY, y+0, z+0); glVertex3f(x+1-MY, y+0, z+edge); }
    if (space->isEmpty2(x, y, z+1) || !space->isEmpty2(x+1, y, z+1)) { glVertex3f(x+1-MY, y+1, z+1); glVertex3f(x+1-MY, y+1, z+(1-edge)); glVertex3f(x+1-MY, y+0, z+(1-edge)); glVertex3f(x+1-MY, y+0, z+1); }
  }
  if (space->isEmpty2(x, y, z+1)) {
    glNormal3f( 0.0f, 0.0f, 1.0f);

    if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y, z+1)) { glVertex3f(x+edge, y+0, z+1-MY); glVertex3f(x+edge, y+1, z+1-MY); glVertex3f(x+0, y+1, z+1-MY); glVertex3f(x+0, y+0, z+1-MY); }
    if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y, z+1)) { glVertex3f(x+1, y+0, z+1-MY); glVertex3f(x+1, y+1, z+1-MY); glVertex3f(x+(1-edge), y+1, z+1-MY); glVertex3f(x+(1-edge), y+0, z+1-MY); }
    if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x, y-1, z+1)) { glVertex3f(x+0, y+edge, z+1-MY); glVertex3f(x+0, y+0, z+1-MY); glVertex3f(x+1, y+0, z+1-MY); glVertex3f(x+1, y+edge, z+1-MY); }
    if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x, y+1, z+1)) { glVertex3f(x+0, y+1, z+1-MY); glVertex3f(x+0, y+(1-edge), z+1-MY); glVertex3f(x+1, y+(1-edge), z+1-MY); glVertex3f(x+1, y+1, z+1-MY); }
  }
  if (space->isEmpty2(x, y-1, z)) {
    glNormal3f( 0.0f, -1.0f, 0.0f);

    if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y-1, z)) { glVertex3f(x+edge, y, z+0); glVertex3f(x+0, y, z+0); glVertex3f(x+0, y, z+1); glVertex3f(x+edge, y, z+1); }
    if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y-1, z)) { glVertex3f(x+1, y, z+0); glVertex3f(x+(1-edge), y, z+0); glVertex3f(x+(1-edge), y, z+1); glVertex3f(x+1, y, z+1); }
    if (space->isEmpty2(x, y, z-1) || !space->isEmpty2(x, y-1, z-1)) { glVertex3f(x+0, y, z+edge); glVertex3f(x+1, y, z+edge); glVertex3f(x+1, y, z+0); glVertex3f(x+0, y, z+0); }
    if (space->isEmpty2(x, y, z+1) || !space->isEmpty2(x, y-1, z+1)) { glVertex3f(x+0, y, z+1); glVertex3f(x+1, y, z+1); glVertex3f(x+1, y, z+(1-edge)); glVertex3f(x+0, y, z+(1-edge)); }
  }
  if (space->isEmpty2(x, y+1, z)) {
    glNormal3f( 0.0f, 1.0f, 0.0f);

    if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y+1, z)) { glVertex3f(x+edge, y+1-MY, z+0); glVertex3f(x+edge, y+1-MY, z+1); glVertex3f(x+0, y+1-MY, z+1); glVertex3f(x+0, y+1-MY, z+0); }
    if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y+1, z)) { glVertex3f(x+1, y+1-MY, z+0); glVertex3f(x+1, y+1-MY, z+1); glVertex3f(x+(1-edge), y+1-MY, z+1); glVertex3f(x+(1-edge), y+1-MY, z+0); }
    if (space->isEmpty2(x, y, z-1) || !space->isEmpty2(x, y+1, z-1)) { glVertex3f(x+0, y+1-MY, z+edge); glVertex3f(x+0, y+1-MY, z+0); glVertex3f(x+1, y+1-MY, z+0); glVertex3f(x+1, y+1-MY, z+edge); }
    if (space->isEmpty2(x, y, z+1) || !space->isEmpty2(x, y+1, z+1)) { glVertex3f(x+0, y+1-MY, z+1); glVertex3f(x+0, y+1-MY, z+(1-edge)); glVertex3f(x+1, y+1-MY, z+(1-edge)); glVertex3f(x+1, y+1-MY, z+1); }
  }

  glEnd();
}

// draws a box with borders depending on the neibor boxes
static void drawBox(const voxel_c * space, int x, int y, int z, float alpha, float edge) {

  GLfloat a0, b0, a1, b1;

  glBegin(GL_QUADS);
  if (space->isEmpty2(x, y, z-1)) {
    glNormal3f( 0.0f, 0.0f, -1.0f);

    if (!space->isEmpty2(x-1, y, z) && space->isEmpty2(x-1, y, z-1)) a0 = 0; else a0 = edge;
    if (!space->isEmpty2(x+1, y, z) && space->isEmpty2(x+1, y, z-1)) a1 = 1; else a1 = (1-edge);
    if (!space->isEmpty2(x, y-1, z) && space->isEmpty2(x, y-1, z-1)) b0 = 0; else b0 = edge;
    if (!space->isEmpty2(x, y+1, z) && space->isEmpty2(x, y+1, z-1)) b1 = 1; else b1 = (1-edge);

    glVertex3f(x+a0, y+b0, z); glVertex3f(x+a0, y+b1, z); glVertex3f(x+a1, y+b1, z); glVertex3f(x+a1, y+b0, z);
  }
  if (space->isEmpty2(x-1, y, z)) {
    glNormal3f( -1.0f, 0.0f, 0.0f);

    if (!space->isEmpty2(x, y-1, z) && space->isEmpty2(x-1, y-1, z)) a0 = 0; else a0 = edge;
    if (!space->isEmpty2(x, y+1, z) && space->isEmpty2(x-1, y+1, z)) a1 = 1; else a1 = (1-edge);
    if (!space->isEmpty2(x, y, z-1) && space->isEmpty2(x-1, y, z-1)) b0 = 0; else b0 = edge;
    if (!space->isEmpty2(x, y, z+1) && space->isEmpty2(x-1, y, z+1)) b1 = 1; else b1 = (1-edge);

    glVertex3f(x, y+a0, z+b0); glVertex3f(x, y+a0, z+b1); glVertex3f(x, y+a1, z+b1); glVertex3f(x, y+a1, z+b0);
  }
  if (space->isEmpty2(x+1, y, z)) {
    glNormal3f( 1.0f, 0.0f, 0.0f);

    if (!space->isEmpty2(x, y-1, z) && space->isEmpty2(x+1, y-1, z)) a0 = 0; else a0 = edge;
    if (!space->isEmpty2(x, y+1, z) && space->isEmpty2(x+1, y+1, z)) a1 = 1; else a1 = (1-edge);
    if (!space->isEmpty2(x, y, z-1) && space->isEmpty2(x+1, y, z-1)) b0 = 0; else b0 = edge;
    if (!space->isEmpty2(x, y, z+1) && space->isEmpty2(x+1, y, z+1)) b1 = 1; else b1 = (1-edge);

    glVertex3f(x+1-MY, y+a1, z+b0); glVertex3f(x+1-MY, y+a1, z+b1); glVertex3f(x+1-MY, y+a0, z+b1); glVertex3f(x+1-MY, y+a0, z+b0);
  }
  if (space->isEmpty2(x, y, z+1)) {
    glNormal3f( 0.0f, 0.0f, 1.0f);

    if (!space->isEmpty2(x-1, y, z) && space->isEmpty2(x-1, y, z+1)) a0 = 0; else a0 = edge;
    if (!space->isEmpty2(x+1, y, z) && space->isEmpty2(x+1, y, z+1)) a1 = 1; else a1 = (1-edge);
    if (!space->isEmpty2(x, y-1, z) && space->isEmpty2(x, y-1, z+1)) b0 = 0; else b0 = edge;
    if (!space->isEmpty2(x, y+1, z) && space->isEmpty2(x, y+1, z+1)) b1 = 1; else b1 = (1-edge);

    glVertex3f(x+a0, y+b0, z+1-MY); glVertex3f(x+a0, y+b1, z+1-MY); glVertex3f(x+a1, y+b1, z+1-MY); glVertex3f(x+a1, y+b0, z+1-MY);
  }
  if (space->isEmpty2(x, y-1, z)) {
    glNormal3f( 0.0f, -1.0f, 0.0f);

    if (!space->isEmpty2(x-1, y, z) && space->isEmpty2(x-1, y-1, z)) a0 = 0; else a0 = edge;
    if (!space->isEmpty2(x+1, y, z) && space->isEmpty2(x+1, y-1, z)) a1 = 1; else a1 = (1-edge);
    if (!space->isEmpty2(x, y, z-1) && space->isEmpty2(x, y-1, z-1)) b0 = 0; else b0 = edge;
    if (!space->isEmpty2(x, y, z+1) && space->isEmpty2(x, y-1, z+1)) b1 = 1; else b1 = (1-edge);

    glVertex3f(x+a0, y  , z+b0); glVertex3f(x+a1, y  , z+b0); glVertex3f(x+a1, y  , z+b1); glVertex3f(x+a0, y  , z+b1);
  }
  if (space->isEmpty2(x, y+1, z)) {
    glNormal3f( 0.0f, 1.0f, 0.0f);

    if (!space->isEmpty2(x-1, y, z) && space->isEmpty2(x-1, y+1, z)) a0 = 0; else a0 = edge;
    if (!space->isEmpty2(x+1, y, z) && space->isEmpty2(x+1, y+1, z)) a1 = 1; else a1 = (1-edge);
    if (!space->isEmpty2(x, y, z-1) && space->isEmpty2(x, y+1, z-1)) b0 = 0; else b0 = edge;
    if (!space->isEmpty2(x, y, z+1) && space->isEmpty2(x, y+1, z+1)) b1 = 1; else b1 = (1-edge);

    glVertex3f(x+a0, y+1-MY, z+b0); glVertex3f(x+a0, y+1-MY, z+b1); glVertex3f(x+a1, y+1-MY, z+b1); glVertex3f(x+a1, y+1-MY, z+b0);
  }

  glEnd();

  glColor4f(0, 0, 0, alpha);

  drawFrame(space, x, y, z, edge);
}

// draw a bube that is smaller than 1
static void drawCube(const voxel_c * space, int x, int y, int z) {
  glBegin(GL_QUADS);
  glNormal3f( 0.0f, 0.0f, -1.0f);
  glVertex3f(x+0.2, y+0.2, z-MY); glVertex3f(x+0.2, y+0.8, z-MY); glVertex3f(x+0.8, y+0.8, z-MY); glVertex3f(x+0.8, y+0.2, z-MY);
  glNormal3f( -1.0f, 0.0f, 0.0f);
  glVertex3f(x-MY, y+0.2, z+0.2); glVertex3f(x-MY, y+0.2, z+0.8); glVertex3f(x-MY, y+0.8, z+0.8); glVertex3f(x-MY, y+0.8, z+0.2);
  glNormal3f( 1.0f, 0.0f, 0.0f);
  glVertex3f(x+1+MY, y+0.8, z+0.2); glVertex3f(x+1+MY, y+0.8, z+0.8); glVertex3f(x+1+MY, y+0.2, z+0.8); glVertex3f(x+1+MY, y+0.2, z+0.2);
  glNormal3f( 0.0f, 0.0f, 1.0f);
  glVertex3f(x+0.2, y+0.2, z+1+MY); glVertex3f(x+0.2, y+0.8, z+1+MY); glVertex3f(x+0.8, y+0.8, z+1+MY); glVertex3f(x+0.8, y+0.2, z+1.02);
  glNormal3f( 0.0f, -1.0f, 0.0f);
  glVertex3f(x+0.2, y-MY, z+0.2); glVertex3f(x+0.8, y-MY, z+0.2); glVertex3f(x+0.8, y-MY, z+0.8); glVertex3f(x+0.2, y-MY, z+0.8);
  glNormal3f( 0.0f, 1.0f, 0.0f);
  glVertex3f(x+0.2, y+1+MY, z+0.2); glVertex3f(x+0.2, y+1+MY, z+0.8); glVertex3f(x+0.8, y+1+MY, z+0.8); glVertex3f(x+0.8, y+1+MY, z+0.2);
  glEnd();
}

static void drawRect(int x0, int y0, int z0,
                     int v1x, int v1y, int v1z,
                     int v2x, int v2y, int v2z, bool type, int diag) {

  bt_assert((v1x >= 0) && (v1y >= 0) && (v1z >= 0));
  bt_assert((v2x >= 0) && (v2y >= 0) && (v2z >= 0));

  glBegin(GL_LINES);
  glVertex3f(x0, y0, z0); glVertex3f(x0+v1x, y0+v1y, z0+v1z);
  glVertex3f(x0+v1x, y0+v1y, z0+v1z); glVertex3f(x0+v1x+v2x, y0+v1y+v2y, z0+v1z+v2z);
  glVertex3f(x0+v1x+v2x, y0+v1y+v2y, z0+v1z+v2z); glVertex3f(x0+v2x, y0+v2y, z0+v2z);
  glVertex3f(x0+v2x, y0+v2y, z0+v2z); glVertex3f(x0, y0, z0);

  int state1 = 0;
  int state2 = 0;

  if (type) {

    float x1 = x0;
    float y1 = y0;
    float z1 = z0;

    float x2 = x0;
    float y2 = y0;
    float z2 = z0;

    int xe = x0 + v1x + v2x;
    int ye = y0 + v1y + v2y;
    int ze = z0 + v1z + v2z;

    while ((x1 < xe) || (y1 < ye) || (z1 < ze)) {
      // v1=(x1, y1, z1) first goes along vector1 =(v1x, v1y, v1z) and then along vector2

      if (state1 == 0) {
        if (v1x) x1 += 1.0/diag;
        if (v1y) y1 += 1.0/diag;
        if (v1z) z1 += 1.0/diag;

        if ((v1x) && (x1 >= x0+v1x) || (v1y) && (y1 >= y0+v1y) || (v1z) && (z1 >= z0+v1z))
          state1 = 1;
      } else {

        if (v2x) x1 += 1.0/diag;
        if (v2y) y1 += 1.0/diag;
        if (v2z) z1 += 1.0/diag;
      }

      if (state2 == 0) {
        if (v2x) x2 += 1.0/diag;
        if (v2y) y2 += 1.0/diag;
        if (v2z) z2 += 1.0/diag;

        if ((v2x) && (x2 >= x0+v2x) || (v2y) && (y2 >= y0+v2y) || (v2z) && (z2 >= z0+v2z))
          state2 = 1;
      } else {

        if (v1x) x2 += 1.0/diag;
        if (v1y) y2 += 1.0/diag;
        if (v1z) z2 += 1.0/diag;
      }

      glVertex3f(x1, y1, z1); glVertex3f(x2, y2, z2);
    }

  } else {

    float x1 = x0 + v1x;
    float y1 = y0 + v1y;
    float z1 = z0 + v1z;

    float x2 = x0 + v1x;
    float y2 = y0 + v1y;
    float z2 = z0 + v1z;

    int xe = x0 + v2x;
    int ye = y0 + v2y;
    int ze = z0 + v2z;

    while ((x1 < xe) || (y1 < ye) || (z1 < ze)) {
      // v1=(x1, y1, z1) first goes along vector1 =(v1x, v1y, v1z) and then along vector2

      if (state1 == 0) {
        if (v1x) x1 -= 1.0/diag;
        if (v1y) y1 -= 1.0/diag;
        if (v1z) z1 -= 1.0/diag;

        if ((v1x) && (x1 <= x0) || (v1y) && (y1 <= y0) || (v1z) && (z1 <= z0))
          state1 = 1;
      } else {

        if (v2x) x1 += 1.0/diag;
        if (v2y) y1 += 1.0/diag;
        if (v2z) z1 += 1.0/diag;
      }

      if (state2 == 0) {
        if (v2x) x2 += 1.0/diag;
        if (v2y) y2 += 1.0/diag;
        if (v2z) z2 += 1.0/diag;

        if ((v2x) && (x2 >= x0+v2x+v1x) || (v2y) && (y2 >= y0+v2y+v1y) || (v2z) && (z2 >= z0+v2z+v1z))
          state2 = 1;
      } else {

        if (v1x) x2 -= 1.0/diag;
        if (v1y) y2 -= 1.0/diag;
        if (v1z) z2 -= 1.0/diag;
      }

      glVertex3f(x1, y1, z1); glVertex3f(x2, y2, z2);
    }
  }

  glEnd();
}

// this function finds out if a given square is inside the selected region
// this check includes the symmetric and comulmn edit modes
static bool inRegion(int x, int y, int z, int x1, int x2, int y1, int y2, int z1, int z2, int sx, int sy, int sz, int mode) {

  if ((x < 0) || (y < 0) || (z < 0) || (x >= sx) || (y >= sy) || (z >= sz)) return false;

  if (mode == 0)
    return (x1 <= x) && (x <= x2) && (y1 <= y) && (y <= y2) && (z1 <= z) && (z <= z2);
  if (mode == VoxelDrawer::TOOL_STACK_Y)
    return (x1 <= x) && (x <= x2) && (z1 <= z) && (z <= z2);
  if (mode == VoxelDrawer::TOOL_STACK_X)
    return (y1 <= y) && (y <= y2) && (z1 <= z) && (z <= z2);
  if (mode == VoxelDrawer::TOOL_STACK_Z)
    return (x1 <= x) && (x <= x2) && (y1 <= y) && (y <= y2);

  if (mode == VoxelDrawer::TOOL_STACK_X + VoxelDrawer::TOOL_STACK_Y)
    return ((x1 <= x) && (x <= x2) || (y1 <= y) && (y <= y2)) && ((z1 <= z) && (z <= z2));
  if (mode == VoxelDrawer::TOOL_STACK_X + VoxelDrawer::TOOL_STACK_Z)
    return ((x1 <= x) && (x <= x2) || (z1 <= z) && (z <= z2)) && ((y1 <= y) && (y <= y2));
  if (mode == VoxelDrawer::TOOL_STACK_Y + VoxelDrawer::TOOL_STACK_Z)
    return ((y1 <= y) && (y <= y2) || (y1 <= y) && (y <= y2)) && ((x1 <= x) && (x <= x2));

  if (mode == VoxelDrawer::TOOL_STACK_X + VoxelDrawer::TOOL_STACK_Y + VoxelDrawer::TOOL_STACK_Z)
    return ((x1 <= x) && (x <= x2) && (y1 <= y) && (y <= y2) ||
        (x1 <= x) && (x <= x2) && (z1 <= z) && (z <= z2) ||
        (y1 <= y) && (y <= y2) && (z1 <= z) && (z <= z2));

  if (mode & VoxelDrawer::TOOL_MIRROR_X)
    return inRegion(x, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~VoxelDrawer::TOOL_MIRROR_X) ||
      inRegion(sx-x-1, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~VoxelDrawer::TOOL_MIRROR_X);

  if (mode & VoxelDrawer::TOOL_MIRROR_Y)
    return inRegion(x, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~VoxelDrawer::TOOL_MIRROR_Y) ||
      inRegion(x, sy-y-1, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~VoxelDrawer::TOOL_MIRROR_Y);

  if (mode & VoxelDrawer::TOOL_MIRROR_Z)
    return inRegion(x, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~VoxelDrawer::TOOL_MIRROR_Z) ||
      inRegion(x, y, sz-z-1, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~VoxelDrawer::TOOL_MIRROR_Z);

  return false;
}

void VoxelDrawer::drawVoxelSpace() {

  glShadeModel(GL_FLAT);

  for (unsigned int run = 0; run < 2; run++) {
    for (unsigned int piece = 0; piece < shapes.size(); piece++) {

      if (shapes[piece].a == 0)
        continue;

      // in run 0 we only paint opaque objects and in run 1 only transparent ones
      // this lets the transparent objects be always in front of the others
      if ((run == 0) && (shapes[piece].a != 1)) continue;
      if ((run == 1) && (shapes[piece].a == 1)) continue;

      glPushMatrix();

      switch(trans) {
      case ScaleRotateTranslate:
        glTranslatef(shapes[piece].x - shapes[piece].shape->getHx(),
                     shapes[piece].y - shapes[piece].shape->getHy(),
                     shapes[piece].z - shapes[piece].shape->getHz());
        glScalef(shapes[piece].scale, shapes[piece].scale, shapes[piece].scale);
        addRotationTransformation();
        glTranslatef(shapes[piece].shape->getX()/-2.0, shapes[piece].shape->getY()/-2.0, shapes[piece].shape->getZ()/-2.0);
        break;
      case TranslateRoateScale:
        addRotationTransformation();
        glTranslatef(shapes[piece].x - shapes[piece].shape->getHx(),
                     shapes[piece].y - shapes[piece].shape->getHy(),
                     shapes[piece].z - shapes[piece].shape->getHz());
        glTranslatef(shapes[piece].shape->getX()/-2.0, shapes[piece].shape->getY()/-2.0, shapes[piece].shape->getZ()/-2.0);
        glScalef(shapes[piece].scale, shapes[piece].scale, shapes[piece].scale);
        break;
      case CenterTranslateRoateScale:
        addRotationTransformation();
        glTranslatef(shapes[piece].x - shapes[piece].shape->getHx(),
                     shapes[piece].y - shapes[piece].shape->getHy(),
                     shapes[piece].z - shapes[piece].shape->getHz());
        glTranslatef(-centerX, -centerY, -centerZ);
        glScalef(shapes[piece].scale, shapes[piece].scale, shapes[piece].scale);
        break;
      default:
        break;
      }

      if (_showCoordinateSystem) {
        if (_useLightning) glDisable(GL_LIGHTING);
        glDisable(GL_BLEND);
        glBegin(GL_LINES);
        glColor3f(1, 0, 0);
        glVertex3f(-1, -1, -1); glVertex3f(shapes[piece].shape->getX()+1, -1, -1);
        glColor3f(0, 0.75, 0);
        glVertex3f(-1, -1, -1); glVertex3f(-1, shapes[piece].shape->getY()+1, -1);
        glColor3f(0, 0, 1);
        glVertex3f(-1, -1, -1); glVertex3f(-1, -1, shapes[piece].shape->getZ()+1);
        glEnd();
        if (_useLightning) glEnable(GL_LIGHTING);
        glEnable(GL_BLEND);
      }

      for (unsigned int x = 0; x < shapes[piece].shape->getX(); x++)
        for (unsigned int y = 0; y < shapes[piece].shape->getY(); y++)
          for (unsigned int z = 0; z < shapes[piece].shape->getZ(); z++) {

            if (shapes[piece].shape->isEmpty(x, y , z))
              continue;

            float cr, cg, cb, ca;
            cr = cg = cb = 0;
            ca = 1;

            switch (colors) {
            case pieceColor:
              if ((x+y+z) & 1) {
                cr = lightPieceColor(shapes[piece].r);
                cg = lightPieceColor(shapes[piece].g);
                cb = lightPieceColor(shapes[piece].b);
                ca = shapes[piece].a;
              } else {
                cr = darkPieceColor(shapes[piece].r);
                cg = darkPieceColor(shapes[piece].g);
                cb = darkPieceColor(shapes[piece].b);
                ca = shapes[piece].a;
              }
              break;
            case paletteColor:
              unsigned int color = shapes[piece].shape->getColor(x, y, z);
              if ((color == 0) || (color - 1 >= palette.size())) {
                if ((x+y+z) & 1) {
                  cr = lightPieceColor(shapes[piece].r);
                  cg = lightPieceColor(shapes[piece].g);
                  cb = lightPieceColor(shapes[piece].b);
                  ca = shapes[piece].a;
                } else {
                  cr = darkPieceColor(shapes[piece].r);
                  cg = darkPieceColor(shapes[piece].g);
                  cb = darkPieceColor(shapes[piece].b);
                  ca = shapes[piece].a;
                }
              } else {
                cr = palette[color-1].r;
                cg = palette[color-1].g;
                cb = palette[color-1].b;
                ca = shapes[piece].a;
              }
            }

            if (shapes[piece].dim) {
              cr = 1 - (1 - cr) * 0.2;
              cg = 1 - (1 - cg) * 0.2;
              cb = 1 - (1 - cb) * 0.2;
            }

            glColor4f(cr, cg, cb, ca);

            switch (shapes[piece].mode) {
            case normal:
              if (shapes[piece].shape->getState(x, y , z) == voxel_c::VX_VARIABLE) {
                drawBox(shapes[piece].shape, x, y, z, shapes[piece].a, shapes[piece].dim ? 0 : 0.05);
                glColor4f(0, 0, 0, shapes[piece].a);
                drawCube(shapes[piece].shape, x, y, z);
              } else
                drawBox(shapes[piece].shape, x, y, z, shapes[piece].a, shapes[piece].dim ? 0 : 0.05);
              break;
            case gridline:
              drawFrame(shapes[piece].shape, x, y, z, 0.05);
              break;
            case invisible:
              break;
            }
          }

      // the marker should be only active, when only one shape is there
      // otherwise it's drawn for every shape
      if ((markerType >= 0) && (mX1 <= mX2) && (mY1 <= mY2)) {

        if (_useLightning) glDisable(GL_LIGHTING);
        glDisable(GL_BLEND);

        glColor4f(0, 0, 0, 1);

        // draw the cursor, this is done by iterating over all
        // cubes and checking for the 3 directions (in one direction only as the other
        // direction is done with the next cube), if there is a border in the cursor
        // between these 2 cubes, if so draw the cursor grid
        for (unsigned int x = 0; x <= shapes[piece].shape->getX(); x++)
          for (unsigned int y = 0; y <= shapes[piece].shape->getY(); y++)
            for (unsigned int z = 0; z <= shapes[piece].shape->getZ(); z++) {
              bool ins = inRegion(x, y, z, mX1, mX2, mY1, mY2, mZ, mZ,
                  shapes[piece].shape->getX(), shapes[piece].shape->getY(), shapes[piece].shape->getZ(), markerType);

              if (ins ^ inRegion(x-1, y, z, mX1, mX2, mY1, mY2, mZ, mZ,
                    shapes[piece].shape->getX(), shapes[piece].shape->getY(), shapes[piece].shape->getZ(), markerType)) {
                drawRect(x, y, z, 0, 1, 0, 0, 0, 1, false, 4);
              }

              if (ins ^ inRegion(x, y-1, z, mX1, mX2, mY1, mY2, mZ, mZ,
                    shapes[piece].shape->getX(), shapes[piece].shape->getY(), shapes[piece].shape->getZ(), markerType)) {
                drawRect(x, y, z, 1, 0, 0, 0, 0, 1, false, 4);
              }

              if (ins ^ inRegion(x, y, z-1, mX1, mX2, mY1, mY2, mZ, mZ,
                    shapes[piece].shape->getX(), shapes[piece].shape->getY(), shapes[piece].shape->getZ(), markerType)) {
                drawRect(x, y, z, 1, 0, 0, 0, 1, 0, false, 4);
              }
            }
        if (_useLightning) glEnable(GL_LIGHTING);
        glEnable(GL_BLEND);
      }

      glPopMatrix();
    }

    glDepthMask(GL_FALSE);
  }

  glDepthMask(GL_TRUE);
}

void VoxelDrawer::drawData(void) {

  glShadeModel(GL_FLAT);
  glEnable(GL_DEPTH_TEST);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glLineWidth(3);

  if (_useLightning)
    glEnable(GL_LIGHTING);
  else
    glDisable(GL_LIGHTING);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glPushMatrix();
  drawVoxelSpace();
  glPopMatrix();
}

unsigned int VoxelDrawer::addSpace(const voxel_c * vx) {
  shapeInfo i;

  i.r = i.g = i.b = 1;
  i.a = 1;
  i.shape = vx;

  i.mode = normal;

  i.x = i.y = i.z = 0;
  i.scale = 1;

  i.dim = false;

  shapes.push_back(i);

  updateRequired();

  return shapes.size()-1;
}

void VoxelDrawer::clearSpaces(void) {

  for (unsigned int i = 0; i < shapes.size(); i++)
    delete shapes[i].shape;

  shapes.clear();
  updateRequired();
}

unsigned int VoxelDrawer::spaceNumber(void) {
  return shapes.size();
}

void VoxelDrawer::setSpaceColor(unsigned int nr, float r, float g, float b, float a) {
  shapes[nr].r = r;
  shapes[nr].g = g;
  shapes[nr].b = b;
  shapes[nr].a = a;

  updateRequired();
}

void VoxelDrawer::setSpaceColor(unsigned int nr, float a) {
  shapes[nr].a = a;

  updateRequired();
}

void VoxelDrawer::setSpacePosition(unsigned int nr, float x, float y, float z, float scale) {
  shapes[nr].x = x;
  shapes[nr].y = y;
  shapes[nr].z = z;
  shapes[nr].scale = scale;

  updateRequired();
}

void VoxelDrawer::setSpaceDim(unsigned int nr, bool dim) {

  shapes[nr].dim = dim;

  updateRequired();
}


void VoxelDrawer::setDrawingMode(unsigned int nr, drawingMode mode) {
  shapes[nr].mode = mode;

  updateRequired();
}

void VoxelDrawer::setColorMode(colorMode color) {
  colors = color;

  updateRequired();
}

void VoxelDrawer::setTransformationType(transformationType type) {
  trans = type;

  updateRequired();
}

void VoxelDrawer::addPaletteEntry(float r, float g, float b) {

  colorInfo ci;

  ci.r = r;
  ci.g = g;
  ci.b = b;

  palette.push_back(ci);
}

void VoxelDrawer::setMarker(int x1, int y1, int x2, int y2, int z, int mT) {
  markerType = mT;
  mX1 = x1;
  mY1 = y1;
  mX2 = x2;
  mY2 = y2;
  mZ = z;
}

void VoxelDrawer::hideMarker(void) {
  markerType = -1;
}

void VoxelDrawer::showSingleShape(const puzzle_c * puz, unsigned int shapeNum) {

  hideMarker();
  clearSpaces();
  unsigned int num = addSpace(puz->getGridType()->getVoxel(puz->getShape(shapeNum)));

  setSpaceColor(num, pieceColorR(shapeNum), pieceColorG(shapeNum), pieceColorB(shapeNum), 1);

  setTransformationType(TranslateRoateScale);
  showCoordinateSystem(true);
}

void VoxelDrawer::showProblem(const puzzle_c * puz, unsigned int probNum, unsigned int selShape) {

  hideMarker();
  clearSpaces();

  if (probNum < puz->problemNumber()) {

    // first find out how to arrange the pieces:
    unsigned int square = 3;
    while (square * (square-2) < puz->probShapeNumber(probNum)) square++;

    unsigned int num;

    float diagonal = 0;

    // now find a scaling factor, so that all pieces fit into their square
    if (puz->probGetResult(probNum) < puz->shapeNumber()) {

      if (puz->probGetResultShape(probNum)->getDiagonal() > diagonal)
        diagonal = puz->probGetResultShape(probNum)->getDiagonal();
    }

    // check the selected shape
    if (selShape < puz->shapeNumber()) {

      if (puz->getShape(selShape)->getDiagonal() > diagonal)
        diagonal = puz->getShape(selShape)->getDiagonal();
    }

    for (unsigned int p = 0; p < puz->probShapeNumber(probNum); p++)
      if (puz->probGetShapeShape(probNum, p)->getDiagonal() > diagonal)
        diagonal = puz->probGetShapeShape(probNum, p)->getDiagonal();

    diagonal = sqrt(diagonal)/1.5;

    // now place the result shape
    if (puz->probGetResult(probNum) < puz->shapeNumber()) {

      num = addSpace(puz->getGridType()->getVoxel(puz->probGetResultShape(probNum)));
      setSpaceColor(num,
                            pieceColorR(puz->probGetResult(probNum)),
                            pieceColorG(puz->probGetResult(probNum)),
                            pieceColorB(puz->probGetResult(probNum)), 1);
      setSpacePosition(num,
                               0.5* (square*diagonal) * (1.0/square - 0.5),
                               0.5* (square*diagonal) * (0.5 - 1.0/square), -20, 1.0);
    }

    // now place the selected shape
    if (selShape < puz->shapeNumber()) {

      num = addSpace(puz->getGridType()->getVoxel(puz->getShape(selShape)));
      setSpaceColor(num,
                            pieceColorR(selShape),
                            pieceColorG(selShape),
                            pieceColorB(selShape), 1);
      setSpacePosition(num,
                               0.5* (square*diagonal) * (0.5 - 0.5/square),
                               0.5* (square*diagonal) * (0.5 - 0.5/square), -20, 0.5);
    }

    // and now the shapes
    int unsigned line = 2;
    int unsigned col = 0;
    for (unsigned int p = 0; p < puz->probShapeNumber(probNum); p++) {
      num = addSpace(puz->getGridType()->getVoxel(puz->probGetShapeShape(probNum, p)));

      setSpaceColor(num,
                            pieceColorR(puz->probGetShape(probNum, p)),
                            pieceColorG(puz->probGetShape(probNum, p)),
                            pieceColorB(puz->probGetShape(probNum, p)), 1);

      setSpacePosition(num,
                               0.5* (square*diagonal) * ((col+0.5)/square - 0.5),
                               0.5* (square*diagonal) * (0.5 - (line+0.5)/square),
                               -20, 0.5);

      col++;
      if (col == square) {
        col = 0;
        line++;
      }
    }

    setTransformationType(ScaleRotateTranslate);
    showCoordinateSystem(false);
  }
}

void VoxelDrawer::showColors(const puzzle_c * puz, bool show) {

  if (show) {

    clearPalette();
    for (unsigned int i = 0; i < puz->colorNumber(); i++) {
      unsigned char r, g, b;
      puz->getColor(i, &r, &g, &b);
      addPaletteEntry(r/255.0, g/255.0, b/255.0);
    }
    setColorMode(paletteColor);

  } else
    setColorMode(pieceColor);

}

void VoxelDrawer::showAssembly(const puzzle_c * puz, unsigned int probNum, unsigned int solNum) {

  hideMarker();
  clearSpaces();

  if ((probNum < puz->problemNumber()) &&
      (solNum < puz->probSolutionNumber(probNum))) {

    unsigned int num;

    const assembly_c * assm = puz->probGetAssembly(probNum, solNum);

    unsigned int piece = 0;

    // and now the shapes
    for (unsigned int p = 0; p < puz->probShapeNumber(probNum); p++)
      for (unsigned int q = 0; q < puz->probGetShapeCount(probNum, p); q++) {

        num = addSpace(puz->getGridType()->getVoxel(puz->probGetShapeShape(probNum, p), assm->getTransformation(piece)));

        setSpacePosition(num, assm->getX(piece), assm->getY(piece), assm->getZ(piece), 1);

        setSpaceColor(num,
                              pieceColorR(puz->probGetShape(probNum, p), q),
                              pieceColorG(puz->probGetShape(probNum, p), q),
                              pieceColorB(puz->probGetShape(probNum, p), q), 1);

        piece++;
      }

    setCenter(0.5*puz->probGetResultShape(probNum)->getX(),
                      0.5*puz->probGetResultShape(probNum)->getY(),
                      0.5*puz->probGetResultShape(probNum)->getZ()
                     );
    setTransformationType(CenterTranslateRoateScale);
    showCoordinateSystem(false);
  }
}

void VoxelDrawer::showAssemblerState(const puzzle_c * puz, unsigned int probNum, const assembly_c * assm) {

  hideMarker();
  clearSpaces();

  if (probNum < puz->problemNumber()) {

    unsigned int num;
    unsigned int piece = 0;

    // and now the shapes
    for (unsigned int p = 0; p < puz->probShapeNumber(probNum); p++)
      for (unsigned int q = 0; q < puz->probGetShapeCount(probNum, p); q++) {

        if (assm->getTransformation(piece) < 0xff) {

          num = addSpace(puz->getGridType()->getVoxel(puz->probGetShapeShape(probNum, p), assm->getTransformation(piece)));

          setSpacePosition(num, assm->getX(piece), assm->getY(piece), assm->getZ(piece), 1);

          setSpaceColor(num,
              pieceColorR(puz->probGetShape(probNum, p), q),
              pieceColorG(puz->probGetShape(probNum, p), q),
              pieceColorB(puz->probGetShape(probNum, p), q), 1);
        }

        piece++;
      }

    setCenter(0.5*puz->probGetResultShape(probNum)->getX(),
                      0.5*puz->probGetResultShape(probNum)->getY(),
                      0.5*puz->probGetResultShape(probNum)->getZ()
                     );
    setTransformationType(CenterTranslateRoateScale);
    showCoordinateSystem(false);
  }
}

void VoxelDrawer::showPlacement(const puzzle_c * puz, unsigned int probNum, unsigned int piece, unsigned char trans, int x, int y, int z) {

  clearSpaces();
  hideMarker();
  setTransformationType(CenterTranslateRoateScale);
  showCoordinateSystem(false);
  setCenter(0.5*puz->probGetResultShape(probNum)->getX(),
                    0.5*puz->probGetResultShape(probNum)->getY(),
                    0.5*puz->probGetResultShape(probNum)->getZ()
                   );

  int num;

  if (trans < puz->getGridType()->getSymmetries()->getNumTransformationsMirror()) {

    int shape = 0;
    unsigned int p = piece;
    while (p >= puz->probGetShapeCount(probNum, shape)) {
      p -= puz->probGetShapeCount(probNum, shape);
      shape++;
    }

    num = addSpace(puz->getGridType()->getVoxel(puz->probGetShapeShape(probNum, shape), trans));
    setSpacePosition(num, x, y, z, 1);
    setSpaceColor(num,
                          pieceColorR(puz->probGetShape(probNum, shape), p),
                          pieceColorG(puz->probGetShape(probNum, shape), p),
                          pieceColorB(puz->probGetShape(probNum, shape), p), 1);
    setDrawingMode(num, normal);
  }

  num = addSpace(puz->getGridType()->getVoxel(puz->probGetResultShape(probNum)));
  setSpaceColor(num,
                        pieceColorR(puz->probGetResult(probNum)),
                        pieceColorG(puz->probGetResult(probNum)),
                        pieceColorB(puz->probGetResult(probNum)), 1);
  setDrawingMode(num, gridline);
}


void VoxelDrawer::updatePositions(PiecePositions *shifting) {

  for (unsigned int p = 0; p < spaceNumber(); p++) {
    setSpacePosition(p, shifting->getX(p), shifting->getY(p), shifting->getZ(p), 1);
    setSpaceColor(p, shifting->getA(p));
  }
}

void VoxelDrawer::dimStaticPieces(PiecePositions *shifting) {

  for (unsigned int p = 0; p < spaceNumber(); p++) {
    setSpaceDim(p, !shifting->moving(p));
  }
}

void VoxelDrawer::updateVisibility(PieceVisibility * pcvis) {

  for (unsigned int p = 0; p < spaceNumber(); p++) {

    switch(pcvis->getVisibility(p)) {
    case 0:
      setDrawingMode(p, normal);
      break;
    case 1:
      setDrawingMode(p, gridline);
      break;
    case 2:
      setDrawingMode(p, invisible);
      break;
    }
  }
}
