/* Burr Solver
 * Copyright (C) 2003-2007  Andreas Röver
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

#include "voxeldrawer_0.h"

#include "piececolor.h"

#include "../lib/voxel.h"

#include <math.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

// this is used to shift one side of the cubes so that they slightly differ
// from the side of the next cube, so that (in case of frames) the sides
// are clearly separated and don't interlock when drawing
#define MY 0.005f

// draws a wire frame box depending on the neighbours
void voxelDrawer_0_c::drawFrame(const voxel_c * space, int x, int y, int z, float edge) {

  if (fabs(edge) < 0.00001) return;


  glPushName(2);
  if (space->isEmpty2(x, y, z-1)) {
    glBegin(GL_QUADS);
    glNormal3f( 0.0f, 0.0f, -1.0f);

    if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y, z-1)) { glVertex3f(x+0, y+0, z); glVertex3f(x+edge, y+0, z); glVertex3f(x+edge, y+1, z); glVertex3f(x+0, y+1, z); }
    if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y, z-1)) { glVertex3f(x+1, y+0, z); glVertex3f(x+(1-edge), y+0, z); glVertex3f(x+(1-edge), y+1, z); glVertex3f(x+1, y+1, z); }
    if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x, y-1, z-1)) { glVertex3f(x+0, y+0, z); glVertex3f(x+0, y+edge, z); glVertex3f(x+1, y+edge, z); glVertex3f(x+1, y+0, z); }
    if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x, y+1, z-1)) { glVertex3f(x+0, y+1, z); glVertex3f(x+0, y+(1-edge), z); glVertex3f(x+1, y+(1-edge), z); glVertex3f(x+1, y+1, z); }
    glEnd();
  }

  glLoadName(0);
  if (space->isEmpty2(x-1, y, z)) {
    glBegin(GL_QUADS);
    glNormal3f( -1.0f, 0.0f, 0.0f);

    if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x-1, y-1, z)) { glVertex3f(x, y+0, z+0); glVertex3f(x, y+edge, z+0); glVertex3f(x, y+edge, z+1); glVertex3f(x, y+0, z+1); }
    if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x-1, y+1, z)) { glVertex3f(x, y+1, z+0); glVertex3f(x, y+1, z+1); glVertex3f(x, y+(1-edge), z+1); glVertex3f(x, y+(1-edge), z+0); }
    if (space->isEmpty2(x, y, z-1) || !space->isEmpty2(x-1, y, z-1)) { glVertex3f(x, y+0, z+edge); glVertex3f(x, y+0, z+0); glVertex3f(x, y+1, z+0); glVertex3f(x, y+1, z+edge); }
    if (space->isEmpty2(x, y, z+1) || !space->isEmpty2(x-1, y, z+1)) { glVertex3f(x, y+0, z+1); glVertex3f(x, y+0, z+(1-edge)); glVertex3f(x, y+1, z+(1-edge)); glVertex3f(x, y+1, z+1); }
    glEnd();
  }

  glLoadName(3);
  if (space->isEmpty2(x+1, y, z)) {
    glBegin(GL_QUADS);
    glNormal3f( 1.0f, 0.0f, 0.0f);

    if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x+1, y-1, z)) { glVertex3f(x+1-MY, y+0, z+0); glVertex3f(x+1-MY, y+0, z+1); glVertex3f(x+1-MY, y+edge, z+1); glVertex3f(x+1-MY, y+edge, z+0); }
    if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x+1, y+1, z)) { glVertex3f(x+1-MY, y+(1-edge), z+0); glVertex3f(x+1-MY, y+(1-edge), z+1); glVertex3f(x+1-MY, y+1, z+1); glVertex3f(x+1-MY, y+1, z+0); }
    if (space->isEmpty2(x, y, z-1) || !space->isEmpty2(x+1, y, z-1)) { glVertex3f(x+1-MY, y+1, z+edge); glVertex3f(x+1-MY, y+1, z+0); glVertex3f(x+1-MY, y+0, z+0); glVertex3f(x+1-MY, y+0, z+edge); }
    if (space->isEmpty2(x, y, z+1) || !space->isEmpty2(x+1, y, z+1)) { glVertex3f(x+1-MY, y+1, z+1); glVertex3f(x+1-MY, y+1, z+(1-edge)); glVertex3f(x+1-MY, y+0, z+(1-edge)); glVertex3f(x+1-MY, y+0, z+1); }
    glEnd();
  }

  glLoadName(5);
  if (space->isEmpty2(x, y, z+1)) {
    glBegin(GL_QUADS);
    glNormal3f( 0.0f, 0.0f, 1.0f);

    if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y, z+1)) { glVertex3f(x+edge, y+0, z+1-MY); glVertex3f(x+edge, y+1, z+1-MY); glVertex3f(x+0, y+1, z+1-MY); glVertex3f(x+0, y+0, z+1-MY); }
    if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y, z+1)) { glVertex3f(x+1, y+0, z+1-MY); glVertex3f(x+1, y+1, z+1-MY); glVertex3f(x+(1-edge), y+1, z+1-MY); glVertex3f(x+(1-edge), y+0, z+1-MY); }
    if (space->isEmpty2(x, y-1, z) || !space->isEmpty2(x, y-1, z+1)) { glVertex3f(x+0, y+edge, z+1-MY); glVertex3f(x+0, y+0, z+1-MY); glVertex3f(x+1, y+0, z+1-MY); glVertex3f(x+1, y+edge, z+1-MY); }
    if (space->isEmpty2(x, y+1, z) || !space->isEmpty2(x, y+1, z+1)) { glVertex3f(x+0, y+1, z+1-MY); glVertex3f(x+0, y+(1-edge), z+1-MY); glVertex3f(x+1, y+(1-edge), z+1-MY); glVertex3f(x+1, y+1, z+1-MY); }
    glEnd();
  }

  glLoadName(1);
  if (space->isEmpty2(x, y-1, z)) {
    glBegin(GL_QUADS);
    glNormal3f( 0.0f, -1.0f, 0.0f);

    if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y-1, z)) { glVertex3f(x+edge, y, z+0); glVertex3f(x+0, y, z+0); glVertex3f(x+0, y, z+1); glVertex3f(x+edge, y, z+1); }
    if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y-1, z)) { glVertex3f(x+1, y, z+0); glVertex3f(x+(1-edge), y, z+0); glVertex3f(x+(1-edge), y, z+1); glVertex3f(x+1, y, z+1); }
    if (space->isEmpty2(x, y, z-1) || !space->isEmpty2(x, y-1, z-1)) { glVertex3f(x+0, y, z+edge); glVertex3f(x+1, y, z+edge); glVertex3f(x+1, y, z+0); glVertex3f(x+0, y, z+0); }
    if (space->isEmpty2(x, y, z+1) || !space->isEmpty2(x, y-1, z+1)) { glVertex3f(x+0, y, z+1); glVertex3f(x+1, y, z+1); glVertex3f(x+1, y, z+(1-edge)); glVertex3f(x+0, y, z+(1-edge)); }
    glEnd();
  }

  glLoadName(4);
  if (space->isEmpty2(x, y+1, z)) {
    glBegin(GL_QUADS);
    glNormal3f( 0.0f, 1.0f, 0.0f);

    if (space->isEmpty2(x-1, y, z) || !space->isEmpty2(x-1, y+1, z)) { glVertex3f(x+edge, y+1-MY, z+0); glVertex3f(x+edge, y+1-MY, z+1); glVertex3f(x+0, y+1-MY, z+1); glVertex3f(x+0, y+1-MY, z+0); }
    if (space->isEmpty2(x+1, y, z) || !space->isEmpty2(x+1, y+1, z)) { glVertex3f(x+1, y+1-MY, z+0); glVertex3f(x+1, y+1-MY, z+1); glVertex3f(x+(1-edge), y+1-MY, z+1); glVertex3f(x+(1-edge), y+1-MY, z+0); }
    if (space->isEmpty2(x, y, z-1) || !space->isEmpty2(x, y+1, z-1)) { glVertex3f(x+0, y+1-MY, z+edge); glVertex3f(x+0, y+1-MY, z+0); glVertex3f(x+1, y+1-MY, z+0); glVertex3f(x+1, y+1-MY, z+edge); }
    if (space->isEmpty2(x, y, z+1) || !space->isEmpty2(x, y+1, z+1)) { glVertex3f(x+0, y+1-MY, z+1); glVertex3f(x+0, y+1-MY, z+(1-edge)); glVertex3f(x+1, y+1-MY, z+(1-edge)); glVertex3f(x+1, y+1-MY, z+1); }
    glEnd();
  }


  glPopName();
}

// draws a box with borders depending on the neighbour boxes
void voxelDrawer_0_c::drawNormalVoxel(const voxel_c * space, int x, int y, int z, float alpha, float edge) {

  GLfloat a0, b0, a1, b1;

  glPushName(2);
  if (space->isEmpty2(x, y, z-1)) {
    glBegin(GL_QUADS);
    glNormal3f( 0.0f, 0.0f, -1.0f);

    if (!space->isEmpty2(x-1, y, z) && space->isEmpty2(x-1, y, z-1)) a0 = 0; else a0 = edge;
    if (!space->isEmpty2(x+1, y, z) && space->isEmpty2(x+1, y, z-1)) a1 = 1; else a1 = (1-edge);
    if (!space->isEmpty2(x, y-1, z) && space->isEmpty2(x, y-1, z-1)) b0 = 0; else b0 = edge;
    if (!space->isEmpty2(x, y+1, z) && space->isEmpty2(x, y+1, z-1)) b1 = 1; else b1 = (1-edge);

    glVertex3f(x+a0, y+b0, z); glVertex3f(x+a0, y+b1, z); glVertex3f(x+a1, y+b1, z); glVertex3f(x+a1, y+b0, z);
    glEnd();
  }

  glLoadName(0);
  if (space->isEmpty2(x-1, y, z)) {
    glBegin(GL_QUADS);
    glNormal3f( -1.0f, 0.0f, 0.0f);

    if (!space->isEmpty2(x, y-1, z) && space->isEmpty2(x-1, y-1, z)) a0 = 0; else a0 = edge;
    if (!space->isEmpty2(x, y+1, z) && space->isEmpty2(x-1, y+1, z)) a1 = 1; else a1 = (1-edge);
    if (!space->isEmpty2(x, y, z-1) && space->isEmpty2(x-1, y, z-1)) b0 = 0; else b0 = edge;
    if (!space->isEmpty2(x, y, z+1) && space->isEmpty2(x-1, y, z+1)) b1 = 1; else b1 = (1-edge);

    glVertex3f(x, y+a0, z+b0); glVertex3f(x, y+a0, z+b1); glVertex3f(x, y+a1, z+b1); glVertex3f(x, y+a1, z+b0);
    glEnd();
  }

  glLoadName(3);
  if (space->isEmpty2(x+1, y, z)) {
    glBegin(GL_QUADS);
    glNormal3f( 1.0f, 0.0f, 0.0f);

    if (!space->isEmpty2(x, y-1, z) && space->isEmpty2(x+1, y-1, z)) a0 = 0; else a0 = edge;
    if (!space->isEmpty2(x, y+1, z) && space->isEmpty2(x+1, y+1, z)) a1 = 1; else a1 = (1-edge);
    if (!space->isEmpty2(x, y, z-1) && space->isEmpty2(x+1, y, z-1)) b0 = 0; else b0 = edge;
    if (!space->isEmpty2(x, y, z+1) && space->isEmpty2(x+1, y, z+1)) b1 = 1; else b1 = (1-edge);

    glVertex3f(x+1-MY, y+a1, z+b0); glVertex3f(x+1-MY, y+a1, z+b1); glVertex3f(x+1-MY, y+a0, z+b1); glVertex3f(x+1-MY, y+a0, z+b0);
    glEnd();
  }

  glLoadName(5);
  if (space->isEmpty2(x, y, z+1)) {
    glBegin(GL_QUADS);
    glNormal3f( 0.0f, 0.0f, 1.0f);

    if (!space->isEmpty2(x-1, y, z) && space->isEmpty2(x-1, y, z+1)) a0 = 0; else a0 = edge;
    if (!space->isEmpty2(x+1, y, z) && space->isEmpty2(x+1, y, z+1)) a1 = 1; else a1 = (1-edge);
    if (!space->isEmpty2(x, y-1, z) && space->isEmpty2(x, y-1, z+1)) b0 = 0; else b0 = edge;
    if (!space->isEmpty2(x, y+1, z) && space->isEmpty2(x, y+1, z+1)) b1 = 1; else b1 = (1-edge);

    glVertex3f(x+a0, y+b0, z+1-MY); glVertex3f(x+a0, y+b1, z+1-MY); glVertex3f(x+a1, y+b1, z+1-MY); glVertex3f(x+a1, y+b0, z+1-MY);
    glEnd();
  }

  glLoadName(1);
  if (space->isEmpty2(x, y-1, z)) {
    glBegin(GL_QUADS);
    glNormal3f( 0.0f, -1.0f, 0.0f);

    if (!space->isEmpty2(x-1, y, z) && space->isEmpty2(x-1, y-1, z)) a0 = 0; else a0 = edge;
    if (!space->isEmpty2(x+1, y, z) && space->isEmpty2(x+1, y-1, z)) a1 = 1; else a1 = (1-edge);
    if (!space->isEmpty2(x, y, z-1) && space->isEmpty2(x, y-1, z-1)) b0 = 0; else b0 = edge;
    if (!space->isEmpty2(x, y, z+1) && space->isEmpty2(x, y-1, z+1)) b1 = 1; else b1 = (1-edge);

    glVertex3f(x+a0, y  , z+b0); glVertex3f(x+a1, y  , z+b0); glVertex3f(x+a1, y  , z+b1); glVertex3f(x+a0, y  , z+b1);
    glEnd();
  }

  glLoadName(4);
  if (space->isEmpty2(x, y+1, z)) {
    glBegin(GL_QUADS);
    glNormal3f( 0.0f, 1.0f, 0.0f);

    if (!space->isEmpty2(x-1, y, z) && space->isEmpty2(x-1, y+1, z)) a0 = 0; else a0 = edge;
    if (!space->isEmpty2(x+1, y, z) && space->isEmpty2(x+1, y+1, z)) a1 = 1; else a1 = (1-edge);
    if (!space->isEmpty2(x, y, z-1) && space->isEmpty2(x, y+1, z-1)) b0 = 0; else b0 = edge;
    if (!space->isEmpty2(x, y, z+1) && space->isEmpty2(x, y+1, z+1)) b1 = 1; else b1 = (1-edge);

    glVertex3f(x+a0, y+1-MY, z+b0); glVertex3f(x+a0, y+1-MY, z+b1); glVertex3f(x+a1, y+1-MY, z+b1); glVertex3f(x+a1, y+1-MY, z+b0);
    glEnd();
  }

  glColor4f(0, 0, 0, alpha);

  glPopName();

  drawFrame(space, x, y, z, edge);
}

// draw a cube that is smaller than 1
void voxelDrawer_0_c::drawVariableMarkers(const voxel_c * space, int x, int y, int z) {


  glPushName(2);
  if (space->isEmpty2(x, y, z-1)) {
    glBegin(GL_QUADS);
    glNormal3f( 0.0f, 0.0f, -1.0f);
    glVertex3f(x+0.2, y+0.2, z-MY); glVertex3f(x+0.2, y+0.8, z-MY); glVertex3f(x+0.8, y+0.8, z-MY); glVertex3f(x+0.8, y+0.2, z-MY);
    glEnd();
  }

  glLoadName(0);
  if (space->isEmpty2(x-1, y, z)) {
    glBegin(GL_QUADS);
    glNormal3f( -1.0f, 0.0f, 0.0f);
    glVertex3f(x-MY, y+0.2, z+0.2); glVertex3f(x-MY, y+0.2, z+0.8); glVertex3f(x-MY, y+0.8, z+0.8); glVertex3f(x-MY, y+0.8, z+0.2);
    glEnd();
  }

  glLoadName(3);
  if (space->isEmpty2(x+1, y, z)) {
    glBegin(GL_QUADS);
    glNormal3f( 1.0f, 0.0f, 0.0f);
    glVertex3f(x+1+MY, y+0.8, z+0.2); glVertex3f(x+1+MY, y+0.8, z+0.8); glVertex3f(x+1+MY, y+0.2, z+0.8); glVertex3f(x+1+MY, y+0.2, z+0.2);
    glEnd();
  }

  glLoadName(5);
  if (space->isEmpty2(x, y, z+1)) {
    glBegin(GL_QUADS);
    glNormal3f( 0.0f, 0.0f, 1.0f);
    glVertex3f(x+0.2, y+0.2, z+1+MY); glVertex3f(x+0.2, y+0.8, z+1+MY); glVertex3f(x+0.8, y+0.8, z+1+MY); glVertex3f(x+0.8, y+0.2, z+1.02);
    glEnd();
  }

  glLoadName(1);
  if (space->isEmpty2(x, y-1, z)) {
    glBegin(GL_QUADS);
    glNormal3f( 0.0f, -1.0f, 0.0f);
    glVertex3f(x+0.2, y-MY, z+0.2); glVertex3f(x+0.8, y-MY, z+0.2); glVertex3f(x+0.8, y-MY, z+0.8); glVertex3f(x+0.2, y-MY, z+0.8);
    glEnd();
  }

  glLoadName(4);
  if (space->isEmpty2(x, y+1, z)) {
    glBegin(GL_QUADS);
    glNormal3f( 0.0f, 1.0f, 0.0f);
    glVertex3f(x+0.2, y+1+MY, z+0.2); glVertex3f(x+0.2, y+1+MY, z+0.8); glVertex3f(x+0.8, y+1+MY, z+0.8); glVertex3f(x+0.8, y+1+MY, z+0.2);
    glEnd();
  }

  glPopName();
}

void voxelDrawer_0_c::drawCursor(const voxel_c * /*space*/, unsigned int sx, unsigned int sy, unsigned int sz) {

#if 0

  // draw the cursor, this is done by iterating over all
  // cubes and checking for the 3 directions (in one direction only as the other
  // direction is done with the next cube), if there is a border in the cursor
  // between these 2 cubes, if so draw the cursor grid
  for (unsigned int x = 0; x <= sx; x++)
    for (unsigned int y = 0; y <= sy; y++)
      for (unsigned int z = 0; z <= sz; z++) {
        bool ins = inRegion(x, y, z, mX1, mX2, mY1, mY2, mZ, mZ, sx, sy, sz, markerType);

        if (ins ^ inRegion(x-1, y, z, mX1, mX2, mY1, mY2, mZ, mZ, sx, sy, sz, markerType)) {
          drawGridRect(x, y, z, 0, 1, 0, 0, 0, 1, 4);
        }

        if (ins ^ inRegion(x, y-1, z, mX1, mX2, mY1, mY2, mZ, mZ, sx, sy, sz, markerType)) {
          drawGridRect(x, y, z, 1, 0, 0, 0, 0, 1, 4);
        }

        if (ins ^ inRegion(x, y, z-1, mX1, mX2, mY1, mY2, mZ, mZ, sx, sy, sz, markerType)) {
          drawGridRect(x, y, z, 1, 0, 0, 0, 1, 0, 4);
        }
      }
#endif
}

void voxelDrawer_0_c::calculateSize(const voxel_c * shape, float * x, float * y, float * z) {
  *x = shape->getX();
  *y = shape->getY();
  *z = shape->getZ();
}

