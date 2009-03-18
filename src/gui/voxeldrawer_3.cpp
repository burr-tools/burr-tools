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

#include "voxeldrawer_3.h"

#include "../lib/voxel.h"

#include "../tools/intdiv.h"

bool voxelDrawer_3_c::getTetrahedron(int x, int y, int z,
    int *x1, int *y1, int *z1,
    int *x2, int *y2, int *z2,
    int *x3, int *y3, int *z3,
    int *x4, int *y4, int *z4) {


  int xc = intdiv_inf(x, 5);
  int yc = intdiv_inf(y, 5);
  int zc = intdiv_inf(z, 5);

  int xs = x - 5*xc;
  int ys = y - 5*yc;
  int zs = z - 5*zc;

  /* the center is always used */
  *x1 = *y1 = *z1 = 1;

  /* the snd point is in the center of the face, where the voxel is within its 5x5x5 grid */
  *x2 = *y2 = *z2 = 1;
  if (xs == 0 || xs == 4)      *x2 = xs/2;
  else if (ys == 0 || ys == 4) *y2 = ys/2;
  else if (zs == 0 || zs == 4) *z2 = zs/2;
  else return false;

  /* the last 2 points are in the 2 corners closest to the point */
  if (xs == 2) {
    *x3 = 0;
    *x4 = 2;

    *y3 = *y4 = (ys<2)?0:2;
    *z3 = *z4 = (zs<2)?0:2;
  } else if (ys == 2) {
    *y3 = 0;
    *y4 = 2;

    *x3 = *x4 = (xs<2)?0:2;
    *z3 = *z4 = (zs<2)?0:2;
  } else if (zs == 2) {
    *z3 = 0;
    *z4 = 2;

    *x3 = *x4 = (xs<2)?0:2;
    *y3 = *y4 = (ys<2)?0:2;
  } else return false;

  *x1 += 2*xc; *x2 += 2*xc; *x3 += 2*xc; *x4 += 2*xc;
  *y1 += 2*yc; *y2 += 2*yc; *y3 += 2*yc; *y4 += 2*yc;
  *z1 += 2*zc; *z2 += 2*zc; *z3 += 2*zc; *z4 += 2*zc;

  /* final check for invalid voxels */
  if (xs > 2) xs = 4-xs;
  if (ys > 2) ys = 4-ys;
  if (zs > 2) zs = 4-zs;

  if (xs != 0 && ys != 0 && zs != 0) return false;
  if (xs != 1 && ys != 1 && zs != 1) return false;

  return true;
}

void voxelDrawer_3_c::calculateSize(const voxel_c * shape, float * x, float * y, float * z) {
  *x = 2*((shape->getX()+4)/5);
  *y = 2*((shape->getY()+4)/5);
  *z = 2*((shape->getZ()+4)/5);
}

void voxelDrawer_3_c::recalcSpaceCoordinates(float * x, float * y, float * z) {
  *x *= 0.4;
  *y *= 0.4;
  *z *= 0.4;
}

