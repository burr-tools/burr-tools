/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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

#include "voxeldrawer_4.h"

#include "../lib/voxel.h"

bool voxelDrawer_4_c::getTetrahedron(int x, int y, int z,
    int *x1, int *y1, int *z1,
    int *x2, int *y2, int *z2,
    int *x3, int *y3, int *z3,
    int *x4, int *y4, int *z4) {

  int xc = (x+60) / 3 -20;   // this is supposed to make shure we round towards -inf. It works up to -100
  int yc = (y+60) / 3 -20;
  int zc = (z+60) / 3 -20;

  int xs = x - 3*xc;
  int ys = y - 3*yc;
  int zs = z - 3*zc;

  if ((xc+yc+zc) & 1) {

    if (xs == 1 && ys == 1 && zs == 1) {
      *x1 = xc; *y1 = yc+1; *z1 = zc;
      *x2 = xc+1; *y2 = yc; *z2 = zc;
      *x3 = xc; *y3 = yc; *z3 = zc+1;
      *x4 = xc+1; *y4 = yc+1; *z4 = zc+1;

      return true;
    }

    if (xs == 0 && ys == 0 && zs == 0) {
      *x1 = xc; *y1 = yc+1; *z1 = zc;
      *x2 = xc+1; *y2 = yc; *z2 = zc;
      *x3 = xc; *y3 = yc; *z3 = zc+1;
      *x4 = xc; *y4 = yc; *z4 = zc;

      return true;
    }

    if (xs == 2 && ys == 2 && zs == 0) {
      *x1 = xc; *y1 = yc+1; *z1 = zc;
      *x2 = xc+1; *y2 = yc; *z2 = zc;
      *x3 = xc+1; *y3 = yc+1; *z3 = zc+1;
      *x4 = xc+1; *y4 = yc+1; *z4 = zc;

      return true;
    }

    if (xs == 0 && ys == 2 && zs == 2) {
      *x1 = xc; *y1 = yc+1; *z1 = zc+1;
      *x2 = xc; *y2 = yc+1; *z2 = zc;
      *x3 = xc; *y3 = yc; *z3 = zc+1;
      *x4 = xc+1; *y4 = yc+1; *z4 = zc+1;

      return true;
    }

    if (xs == 2 && ys == 0 && zs == 2) {
      *x1 = xc+1; *y1 = yc; *z1 = zc+1;
      *x2 = xc+1; *y2 = yc; *z2 = zc;
      *x3 = xc; *y3 = yc; *z3 = zc+1;
      *x4 = xc+1; *y4 = yc+1; *z4 = zc+1;

      return true;
    }


  } else {

    if (xs == 1 && ys == 1 && zs == 1) {
      *x1 = xc+1; *y1 = yc+1; *z1 = zc;
      *x2 = xc; *y2 = yc; *z2 = zc;
      *x3 = xc+1; *y3 = yc; *z3 = zc+1;
      *x4 = xc; *y4 = yc+1; *z4 = zc+1;

      return true;
    }

    if (xs == 0 && ys == 2 && zs == 0) {
      *x1 = xc; *y1 = yc; *z1 = zc;
      *x2 = xc+1; *y2 = yc+1; *z2 = zc;
      *x3 = xc; *y3 = yc+1; *z3 = zc+1;
      *x4 = xc; *y4 = yc+1; *z4 = zc;

      return true;
    }

    if (xs == 2 && ys == 0 && zs == 0) {
      *x1 = xc; *y1 = yc; *z1 = zc;
      *x2 = xc+1; *y2 = yc+1; *z2 = zc;
      *x3 = xc+1; *y3 = yc; *z3 = zc+1;
      *x4 = xc+1; *y4 = yc; *z4 = zc;

      return true;
    }

    if (xs == 0 && ys == 0 && zs == 2) {
      *x1 = xc; *y1 = yc; *z1 = zc+1;
      *x2 = xc; *y2 = yc; *z2 = zc;
      *x3 = xc+1; *y3 = yc; *z3 = zc+1;
      *x4 = xc; *y4 = yc+1; *z4 = zc+1;

      return true;
    }

    if (xs == 2 && ys == 2 && zs == 2) {
      *x1 = xc+1; *y1 = yc+1; *z1 = zc+1;
      *x2 = xc+1; *y2 = yc+1; *z2 = zc;
      *x3 = xc+1; *y3 = yc; *z3 = zc+1;
      *x4 = xc; *y4 = yc+1; *z4 = zc+1;

      return true;
    }

  }

  return false;
}

void voxelDrawer_4_c::calculateSize(const voxel_c * shape, float * x, float * y, float * z) {
  *x = (shape->getX()+2)/3;
  *y = (shape->getY()+2)/3;
  *z = (shape->getZ()+2)/3;
}

void voxelDrawer_4_c::recalcSpaceCoordinates(float * x, float * y, float * z) {
  *x /= 3;
  *y /= 3;
  *z /= 3;
}

