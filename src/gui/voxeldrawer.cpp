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
#include "voxeldrawer.h"

#include "grideditor.h"

#include "../lib/voxel.h"

#include <math.h>

#include <FL/gl.h>

void voxelDrawer_c::recalcSpaceCoordinates(float * /*x*/, float * /*y*/, float * /*z*/) {}

void drawGridRect(double x0, double y0, double z0,
                     double v1x, double v1y, double v1z,
                     double v2x, double v2y, double v2z, int diag) {

  glBegin(GL_LINES);
  glVertex3f(x0, y0, z0); glVertex3f(x0+v1x, y0+v1y, z0+v1z);
  glVertex3f(x0+v1x, y0+v1y, z0+v1z); glVertex3f(x0+v1x+v2x, y0+v1y+v2y, z0+v1z+v2z);
  glVertex3f(x0+v1x+v2x, y0+v1y+v2y, z0+v1z+v2z); glVertex3f(x0+v2x, y0+v2y, z0+v2z);
  glVertex3f(x0+v2x, y0+v2y, z0+v2z); glVertex3f(x0, y0, z0);

  int state1 = 0;
  int state2 = 0;


  float x1 = x0 + v1x;
  float y1 = y0 + v1y;
  float z1 = z0 + v1z;

  float x2 = x0 + v1x;
  float y2 = y0 + v1y;
  float z2 = z0 + v1z;

  float xe = x0 + v2x;
  float ye = y0 + v2y;
  float ze = z0 + v2z;

  while ((fabs(x1 - xe) > 0.01) || (fabs(y1 - ye) > 0.01) || (fabs(z1 - ze) > 0.01)) {
    // v1=(x1, y1, z1) first goes along vector1 =(v1x, v1y, v1z) and then along vector2

    if (state1 == 0) {
      x1 -= v1x/diag;
      y1 -= v1y/diag;
      z1 -= v1z/diag;

      if ((v1x) && (fabs(x1 - x0) < 0.01) ||
          (v1y) && (fabs(y1 - y0) < 0.01) ||
          (v1z) && (fabs(z1 - z0) < 0.01)) {
        state1 = 1;
      }
    } else {

      x1 += v2x/diag;
      y1 += v2y/diag;
      z1 += v2z/diag;
    }

    if (state2 == 0) {
      x2 += v2x/diag;
      y2 += v2y/diag;
      z2 += v2z/diag;

      if ((v2x) && (fabs(x2 - (x0+v2x+v1x)) < 0.01) ||
          (v2y) && (fabs(y2 - (y0+v2y+v1y)) < 0.01) ||
          (v2z) && (fabs(z2 - (z0+v2z+v1z)) < 0.01)) {
        state2 = 1;
      }
    } else {

      x2 -= v1x/diag;
      y2 -= v1y/diag;
      z2 -= v1z/diag;
    }

    glVertex3f(x1, y1, z1); glVertex3f(x2, y2, z2);
  }

  glEnd();
}

void drawGridTriangle(double x0, double y0, double z0,
                         double v1x, double v1y, double v1z,
                         double v2x, double v2y, double v2z, int diag) {

  glBegin(GL_LINES);
  glVertex3f(x0, y0, z0); glVertex3f(x0+v1x, y0+v1y, z0+v1z);
  glVertex3f(x0+v1x, y0+v1y, z0+v1z); glVertex3f(x0+v2x, y0+v2y, z0+v2z);
  glVertex3f(x0+v2x, y0+v2y, z0+v2z); glVertex3f(x0, y0, z0);

  float x1 = x0;
  float y1 = y0;
  float z1 = z0;

  float x2 = x0;
  float y2 = y0;
  float z2 = z0;

  float xe = x0 + v1x;
  float ye = y0 + v1y;
  float ze = z0 + v1z;

  while ((fabs(x1 - xe) > 0.01) || (fabs(y1 - ye) > 0.01) || (fabs(z1 - ze) > 0.01)) {

    x1 += v1x/diag;
    y1 += v1y/diag;
    z1 += v1z/diag;

    x2 += v2x/diag;
    y2 += v2y/diag;
    z2 += v2z/diag;

    glVertex3f(x1, y1, z1); glVertex3f(x2, y2, z2);
  }

  glEnd();
}

// this function finds out if a given square is inside the selected region
// this check includes the symmetric and column edit modes
bool inRegion(int x, int y, int z, int x1, int x2, int y1, int y2, int z1, int z2, int sx, int sy, int sz, int mode) {

  if ((x < 0) || (y < 0) || (z < 0) || (x >= sx) || (y >= sy) || (z >= sz)) return false;

  if (mode == 0)
    return (x1 <= x) && (x <= x2) && (y1 <= y) && (y <= y2) && (z1 <= z) && (z <= z2);
  if (mode == gridEditor_c::TOOL_STACK_Y)
    return (x1 <= x) && (x <= x2) && (z1 <= z) && (z <= z2);
  if (mode == gridEditor_c::TOOL_STACK_X)
    return (y1 <= y) && (y <= y2) && (z1 <= z) && (z <= z2);
  if (mode == gridEditor_c::TOOL_STACK_Z)
    return (x1 <= x) && (x <= x2) && (y1 <= y) && (y <= y2);

  if (mode == gridEditor_c::TOOL_STACK_X + gridEditor_c::TOOL_STACK_Y)
    return ((x1 <= x) && (x <= x2) || (y1 <= y) && (y <= y2)) && ((z1 <= z) && (z <= z2));
  if (mode == gridEditor_c::TOOL_STACK_X + gridEditor_c::TOOL_STACK_Z)
    return ((x1 <= x) && (x <= x2) || (z1 <= z) && (z <= z2)) && ((y1 <= y) && (y <= y2));
  if (mode == gridEditor_c::TOOL_STACK_Y + gridEditor_c::TOOL_STACK_Z)
    return ((y1 <= y) && (y <= y2) || (y1 <= y) && (y <= y2)) && ((x1 <= x) && (x <= x2));

  if (mode == gridEditor_c::TOOL_STACK_X + gridEditor_c::TOOL_STACK_Y + gridEditor_c::TOOL_STACK_Z)
    return ((x1 <= x) && (x <= x2) && (y1 <= y) && (y <= y2) ||
        (x1 <= x) && (x <= x2) && (z1 <= z) && (z <= z2) ||
        (y1 <= y) && (y <= y2) && (z1 <= z) && (z <= z2));

  if (mode & gridEditor_c::TOOL_MIRROR_X)
    return inRegion(x, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~gridEditor_c::TOOL_MIRROR_X) ||
      inRegion(sx-x-1, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~gridEditor_c::TOOL_MIRROR_X);

  if (mode & gridEditor_c::TOOL_MIRROR_Y)
    return inRegion(x, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~gridEditor_c::TOOL_MIRROR_Y) ||
      inRegion(x, sy-y-1, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~gridEditor_c::TOOL_MIRROR_Y);

  if (mode & gridEditor_c::TOOL_MIRROR_Z)
    return inRegion(x, y, z, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~gridEditor_c::TOOL_MIRROR_Z) ||
      inRegion(x, y, sz-z-1, x1, x2, y1, y2, z1, z2, sx, sy, sz, mode & ~gridEditor_c::TOOL_MIRROR_Z);

  return false;
}

/* draw a trianle, set the normal so that it is orthogonal and pointing away from p */
void drawTriangle(double x1, double y1, double z1,
    double x2, double y2, double z2,
    double x3, double y3, double z3, double px, double py, double pz) {

  double sx = x1;
  double sy = y1;
  double sz = z1;
  double v1x = x2-x1;
  double v1y = y2-y1;
  double v1z = z2-z1;
  double v2x = x3-x1;
  double v2y = y3-y1;
  double v2z = z3-z1;

  float nx = v1y*v2z - v1z*v2y;
  float ny = v1z*v2x - v1x*v2z;
  float nz = v1x*v2y - v1y*v2x;

  float l = sqrt(nx*nx+ny*ny+nz*nz);
  nx /= l;
  ny /= l;
  nz /= l;

  if (nx*(px-sx)+ny*(py-sy)+nz*(pz-sz) > 0) {
    nx = -nx;
    ny = -ny;
    nz = -nz;
  }

  glBegin(GL_TRIANGLES);
  glNormal3f(nx, ny, nz);
//  glColor3f(rand()*1.0/RAND_MAX, rand()*1.0/RAND_MAX, rand()*1.0/RAND_MAX);
  glVertex3f(x1, y1, z1); glVertex3f(x2, y2, z2); glVertex3f(x3, y3, z3);
  glEnd();
}

