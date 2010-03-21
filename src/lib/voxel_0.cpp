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
#include "voxel_0.h"
#include <stdlib.h>

#include "tabs_0/tablesizes.inc"

/// this array contains all rotation and mirror rotation matrices for the cubic \ref grid
static const int rotationMatrices[NUM_TRANSFORMATIONS_MIRROR][9] = {
#include "tabs_0/rotmatrix.inc"
};

/** \page grid Grids in BurrTools
 *
 * Grids in BurrTools define how the space is separated into the different volume
 * elements (voxels). There are many different ways how to do that.
 *
 * BurrTools poses a few restrictions on how those grids can look:
 *
 * - The grids must be periodical in all directions (so no Penrose Tiling)
 * - The number of transformations must be limited (this is a problem with the sphere grid)
 */

/** the transformation function for cubic grids
 *
 * The nice thing about the cubic grid is that we can easily calculate how big the
 * grid will be after the transformation, we can also easily calculate the new
 * bounding box and the now position for the hotspot.
 *
 * This makes this transformation function faster than the other 2 variations for
 * spheres and triangles
 */
bool voxel_0_c::transform(unsigned int nr) {

  bt_assert(nr < NUM_TRANSFORMATIONS_MIRROR);

  // first transform the corner opposite the 0,0,0 corner to find the final size
  // and the required shifts

  int tx = rotationMatrices[nr][0]*(sx-1) + rotationMatrices[nr][1]*(sy-1) + rotationMatrices[nr][2]*(sz-1);
  int ty = rotationMatrices[nr][3]*(sx-1) + rotationMatrices[nr][4]*(sy-1) + rotationMatrices[nr][5]*(sz-1);
  int tz = rotationMatrices[nr][6]*(sx-1) + rotationMatrices[nr][7]*(sy-1) + rotationMatrices[nr][8]*(sz-1);

  // calculate the amount the rotated coodinates must be shifted up
  int shx = (tx < 0) ? -tx : 0;
  int shy = (ty < 0) ? -ty : 0;
  int shz = (tz < 0) ? -tz : 0;

  // calculate new size
  int nsx = abs(tx)+1;
  int nsy = abs(ty)+1;
  int nsz = abs(tz)+1;

  voxel_type * s = new voxel_type[nsx*nsy*nsz];
  memset(s, VX_EMPTY, nsx*nsy*nsz);

  unsigned int index = 0;
  for (unsigned int z = 0; z < sz; z++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int x = 0; x < sx; x++) {
        if (space[index]) {
          tx = rotationMatrices[nr][0]*x + rotationMatrices[nr][1]*y + rotationMatrices[nr][2]*z + shx;
          ty = rotationMatrices[nr][3]*x + rotationMatrices[nr][4]*y + rotationMatrices[nr][5]*z + shy;
          tz = rotationMatrices[nr][6]*x + rotationMatrices[nr][7]*y + rotationMatrices[nr][8]*z + shz;

          bt_assert(tx >= 0);
          bt_assert(ty >= 0);
          bt_assert(tz >= 0);

          s[tx + nsx*(ty + nsy*tz)] = space[index];
        }
        index++;
      }

  delete [] space;
  space = s;

  sx = nsx;
  sy = nsy;
  sz = nsz;

  // update hotspot
  int thx = rotationMatrices[nr][0]*hx + rotationMatrices[nr][1]*hy + rotationMatrices[nr][2]*hz + shx;
  int thy = rotationMatrices[nr][3]*hx + rotationMatrices[nr][4]*hy + rotationMatrices[nr][5]*hz + shy;
  int thz = rotationMatrices[nr][6]*hx + rotationMatrices[nr][7]*hy + rotationMatrices[nr][8]*hz + shz;

  hx = thx;
  hy = thy;
  hz = thz;

  // update bounding box
  int tbx1 = rotationMatrices[nr][0]*bx1 + rotationMatrices[nr][1]*by1 + rotationMatrices[nr][2]*bz1 + shx;
  int tby1 = rotationMatrices[nr][3]*bx1 + rotationMatrices[nr][4]*by1 + rotationMatrices[nr][5]*bz1 + shy;
  int tbz1 = rotationMatrices[nr][6]*bx1 + rotationMatrices[nr][7]*by1 + rotationMatrices[nr][8]*bz1 + shz;

  int tbx2 = rotationMatrices[nr][0]*bx2 + rotationMatrices[nr][1]*by2 + rotationMatrices[nr][2]*bz2 + shx;
  int tby2 = rotationMatrices[nr][3]*bx2 + rotationMatrices[nr][4]*by2 + rotationMatrices[nr][5]*bz2 + shy;
  int tbz2 = rotationMatrices[nr][6]*bx2 + rotationMatrices[nr][7]*by2 + rotationMatrices[nr][8]*bz2 + shz;

#define MIN(a,b) ((a<b)?(a):(b))
#define MAX(a,b) ((a>b)?(a):(b))

  bx1 = MIN(tbx1, tbx2);
  bx2 = MAX(tbx1, tbx2);

  by1 = MIN(tby1, tby2);
  by2 = MAX(tby1, tby2);

  bz1 = MIN(tbz1, tbz2);
  bz2 = MAX(tbz1, tbz2);

#undef MIN
#undef MAX

  symmetries = symmetryInvalid();

  return true;
}

void voxel_0_c::transformPoint(int * x, int * y, int * z, unsigned int trans) const {

  bt_assert(trans < NUM_TRANSFORMATIONS_MIRROR);

  int sx = *x;
  int sy = *y;
  int sz = *z;

  *x = rotationMatrices[trans][0]*sx + rotationMatrices[trans][1]*sy + rotationMatrices[trans][2]*sz;
  *y = rotationMatrices[trans][3]*sx + rotationMatrices[trans][4]*sy + rotationMatrices[trans][5]*sz;
  *z = rotationMatrices[trans][6]*sx + rotationMatrices[trans][7]*sy + rotationMatrices[trans][8]*sz;
}

bool voxel_0_c::getNeighbor(unsigned int idx, unsigned int typ, int x, int y, int z, int * xn, int *yn, int *zn) const {

  switch (typ) {
    case 0:
      switch (idx) {
        case 0: *xn = x-1; *yn = y;   *zn = z;   break;
        case 1: *xn = x;   *yn = y-1; *zn = z;   break;
        case 2: *xn = x  ; *yn = y;   *zn = z-1; break;
        case 3: *xn = x+1; *yn = y;   *zn = z;   break;
        case 4: *xn = x  ; *yn = y+1; *zn = z;   break;
        case 5: *xn = x  ; *yn = y;   *zn = z+1; break;
        default: return false;
      }
      break;
    case 1:
      switch (idx) {
        case  0: *xn = x-1; *yn = y-1; *zn = z;   break;
        case  1: *xn = x-1; *yn = y+1; *zn = z;   break;
        case  2: *xn = x+1; *yn = y+1; *zn = z;   break;
        case  3: *xn = x+1; *yn = y-1; *zn = z;   break;
        case  4: *xn = x-1; *yn = y;   *zn = z-1; break;
        case  5: *xn = x-1; *yn = y;   *zn = z+1; break;
        case  6: *xn = x+1; *yn = y;   *zn = z+1; break;
        case  7: *xn = x+1; *yn = y;   *zn = z-1; break;
        case  8: *xn = x;   *yn = y-1; *zn = z-1; break;
        case  9: *xn = x;   *yn = y-1; *zn = z+1; break;
        case 10: *xn = x;   *yn = y+1; *zn = z+1; break;
        case 11: *xn = x;   *yn = y+1; *zn = z-1; break;
        default: return false;
      }
      break;
    case 2:
      switch (idx) {
        case 0: *xn = x-1; *yn = y-1; *zn = z-1; break;
        case 1: *xn = x-1; *yn = y-1; *zn = z+1; break;
        case 2: *xn = x-1; *yn = y+1; *zn = z-1; break;
        case 3: *xn = x-1; *yn = y+1; *zn = z+1; break;
        case 4: *xn = x+1; *yn = y-1; *zn = z-1; break;
        case 5: *xn = x+1; *yn = y-1; *zn = z+1; break;
        case 6: *xn = x+1; *yn = y+1; *zn = z-1; break;
        case 7: *xn = x+1; *yn = y+1; *zn = z+1; break;
        default: return false;
      }
      break;
  }
  return true;
}

void voxel_0_c::scale(unsigned int amount, bool grid)
{
  voxel_type * s2 = new voxel_type[sx*amount*sy*amount*sz*amount];

  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++)
        for (unsigned int ax = 0; ax < amount; ax++)
          for (unsigned int ay = 0; ay < amount; ay++)
            for (unsigned int az = 0; az < amount; az++)
              if (!grid)
                s2[(x*amount+ax) + (sx*amount) * ((y*amount+ay) + (sy*amount) * (z*amount+az))] = get(x, y, z);
              else
              {
                if (!isEmpty(x, y, z) &&
                      (
                        (ax == 0        && isEmpty2(x-1, y, z)) ||
                        (ax == amount-1 && isEmpty2(x+1, y, z)) ||
                        (ay == 0        && isEmpty2(x, y-1, z)) ||
                        (ay == amount-1 && isEmpty2(x, y+1, z)) ||
                        (az == 0        && isEmpty2(x, y, z-1)) ||
                        (az == amount-1 && isEmpty2(x, y, z+1)) ||
                        ((ax == 0 || ax == amount-1) && (ay == 0 || ay == amount-1 || az == 0 || az == amount-1)) ||
                        ((ay == 0 || ay == amount-1) && (ax == 0 || ax == amount-1 || az == 0 || az == amount-1)) ||
                        ((az == 0 || az == amount-1) && (ax == 0 || ax == amount-1 || ay == 0 || ay == amount-1))
                      )
                   )

                    s2[(x*amount+ax) + (sx*amount) * ((y*amount+ay) + (sy*amount) * (z*amount+az))] = get(x, y, z);
                else
                    s2[(x*amount+ax) + (sx*amount) * ((y*amount+ay) + (sy*amount) * (z*amount+az))] = 0;
              }

  delete [] space;
  space = s2;

  sx *= amount;
  sy *= amount;
  sz *= amount;

  voxels = sx*sy*sz;

  hx = hy = hz = 0;

  recalcBoundingBox();
}

bool voxel_0_c::scaleDown(unsigned char by, bool action) {

  if (by < 2) return true;
  if (sx < by || sy < by || sz < by) return false;

  for (unsigned int shx = 0; shx < by; shx++)
    for (unsigned int shy = 0; shy < by; shy++)
      for (unsigned int shz = 0; shz < by; shz++) {

        bool problem = false;

        for (int x = 0; x < (int)sx/by+1; x++)
          for (int y = 0; y < (int)sy/by+1; y++)
            for (int z = 0; z < (int)sz/by+1; z++)

              for (unsigned int cx = 0; cx < by; cx++)
                for (unsigned int cy = 0; cy < by; cy++)
                  for (unsigned int cz = 0; cz < by; cz++)

                    problem |= get2(x*by-shx, y*by-shy, z*by-shz) != get2(x*by-shx+cx, y*by-shy+cy, z*by-shz+cz);

        if (!problem) {

          if (action) {

            // we don't need to include the +1 in the sizes
            // as we've done for the check as these voxels are
            // definitively empty
            unsigned int nsx = sx/by;
            unsigned int nsy = sy/by;
            unsigned int nsz = sz/by;

            voxel_type * s2 = new voxel_type[nsx*nsy*nsz];

            for (unsigned int x = 0; x < nsx; x++)
              for (unsigned int y = 0; y < nsy; y++)
                for (unsigned int z = 0; z < nsz; z++)
                  s2[x + nsx * (y + nsy * z)] = get2(x*by, y*by, z*by);

            delete [] space;
            space = s2;

            sx = nsx;
            sy = nsy;
            sz = nsz;

            voxels = sx*sy*sz;

            recalcBoundingBox();
          }

          return true;
        }
      }

  return false;
}

void voxel_0_c::resizeInclude(int & px, int & py, int & pz) {

  int nsx = getX();
  int nsy = getY();
  int nsz = getZ();
  int tx = 0;
  int ty = 0;
  int tz = 0;

  if (px < 0) {

    nsx -= px;
    tx -= px;
  }
  if (py < 0) {

    nsy -= py;
    ty -= py;
  }
  if (pz < 0) {

    nsz -= pz;
    tz -= pz;
  }
  if (px >= (int)getX()) nsx += (px-getX()+1);
  if (py >= (int)getY()) nsy += (py-getY()+1);
  if (pz >= (int)getZ()) nsz += (pz-getZ()+1);

  resize(nsx, nsy, nsz, 0);
  translate(tx, ty, tz, 0);

  px += tx;
  py += ty;
  pz += tz;
}

bool voxel_0_c::validCoordinate(int /*x*/, int /*y*/, int /*z*/) const {
  return true;
}

bool voxel_0_c::onGrid(int /*x*/, int /*y*/, int /*z*/) const
{
  return true;
}

