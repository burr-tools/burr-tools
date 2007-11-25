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
#include "voxel_3.h"

#include "tabs_0/tablesizes.inc"

static const int rotationMatrices[NUM_TRANSFORMATIONS_MIRROR][9] = {
#include "tabs_0/rotmatrix.inc"
};

bool voxel_3_c::transform(unsigned int nr) {

  bt_assert(nr < NUM_TRANSFORMATIONS_MIRROR);

  // the first thing to do here is to ensure that all 3 dimensions are a multiple of 5
  {
    int sx = getX();
    int sy = getY();
    int sz = getZ();

    sx += 4; sx -= sx % 5;
    sy += 4; sy -= sy % 5;
    sz += 4; sz -= sz % 5;

    resize(sx, sy, sz, 0);
  }

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
  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++) {
        tx = rotationMatrices[nr][0]*x + rotationMatrices[nr][1]*y + rotationMatrices[nr][2]*z + shx;
        ty = rotationMatrices[nr][3]*x + rotationMatrices[nr][4]*y + rotationMatrices[nr][5]*z + shy;
        tz = rotationMatrices[nr][6]*x + rotationMatrices[nr][7]*y + rotationMatrices[nr][8]*z + shz;

        bt_assert(tx >= 0);
        bt_assert(ty >= 0);
        bt_assert(tz >= 0);

        s[tx + nsx*(ty + nsy*tz)] = space[x + sx*(y + sy*z)];
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

void voxel_3_c::getHotspot(unsigned char trans, int * x, int * y, int * z) const {

  bt_assert(trans < NUM_TRANSFORMATIONS_MIRROR);

  int tsx = rotationMatrices[trans][0]*(sx-1) + rotationMatrices[trans][1]*(sy-1) + rotationMatrices[trans][2]*(sz-1);
  int tsy = rotationMatrices[trans][3]*(sx-1) + rotationMatrices[trans][4]*(sy-1) + rotationMatrices[trans][5]*(sz-1);
  int tsz = rotationMatrices[trans][6]*(sx-1) + rotationMatrices[trans][7]*(sy-1) + rotationMatrices[trans][8]*(sz-1);

  int thx = rotationMatrices[trans][0]*hx + rotationMatrices[trans][1]*hy + rotationMatrices[trans][2]*hz;
  int thy = rotationMatrices[trans][3]*hx + rotationMatrices[trans][4]*hy + rotationMatrices[trans][5]*hz;
  int thz = rotationMatrices[trans][6]*hx + rotationMatrices[trans][7]*hy + rotationMatrices[trans][8]*hz;

  if (tsx < 0) thx -= tsx;
  if (tsy < 0) thy -= tsy;
  if (tsz < 0) thz -= tsz;

  *x = thx;
  *y = thy;
  *z = thz;
}

void voxel_3_c::getBoundingBox(unsigned char trans, int * x1, int * y1, int * z1, int * x2, int * y2, int * z2) const {

  bt_assert(trans < NUM_TRANSFORMATIONS_MIRROR);

  int ttsx = rotationMatrices[trans][0]*(sx-1) + rotationMatrices[trans][1]*(sy-1) + rotationMatrices[trans][2]*(sz-1);
  int ttsy = rotationMatrices[trans][3]*(sx-1) + rotationMatrices[trans][4]*(sy-1) + rotationMatrices[trans][5]*(sz-1);
  int ttsz = rotationMatrices[trans][6]*(sx-1) + rotationMatrices[trans][7]*(sy-1) + rotationMatrices[trans][8]*(sz-1);

  int tbx1 = rotationMatrices[trans][0]*bx1 + rotationMatrices[trans][1]*by1 + rotationMatrices[trans][2]*bz1;
  int tby1 = rotationMatrices[trans][3]*bx1 + rotationMatrices[trans][4]*by1 + rotationMatrices[trans][5]*bz1;
  int tbz1 = rotationMatrices[trans][6]*bx1 + rotationMatrices[trans][7]*by1 + rotationMatrices[trans][8]*bz1;

  int tbx2 = rotationMatrices[trans][0]*bx2 + rotationMatrices[trans][1]*by2 + rotationMatrices[trans][2]*bz2;
  int tby2 = rotationMatrices[trans][3]*bx2 + rotationMatrices[trans][4]*by2 + rotationMatrices[trans][5]*bz2;
  int tbz2 = rotationMatrices[trans][6]*bx2 + rotationMatrices[trans][7]*by2 + rotationMatrices[trans][8]*bz2;

  if (ttsx < 0) {
    tbx1 -= ttsx;
    tbx2 -= ttsx;
  }
  if (ttsy < 0) {
    tby1 -= ttsy;
    tby2 -= ttsy;
  }
  if (ttsz < 0) {
    tbz1 -= ttsz;
    tbz2 -= ttsz;
  }

#define MIN(a,b) ((a<b)?(a):(b))
#define MAX(a,b) ((a>b)?(a):(b))

  if (x1) *x1 = MIN(tbx1, tbx2);
  if (x2) *x2 = MAX(tbx1, tbx2);

  if (y1) *y1 = MIN(tby1, tby2);
  if (y2) *y2 = MAX(tby1, tby2);

  if (z1) *z1 = MIN(tbz1, tbz2);
  if (z2) *z2 = MAX(tbz1, tbz2);

#undef MIN
#undef MAX
}

void voxel_3_c::transformPoint(int * x, int * y, int * z, unsigned int trans) const {

  bt_assert(trans < NUM_TRANSFORMATIONS_MIRROR);

  int sx = *x;
  int sy = *y;
  int sz = *z;

  *x = rotationMatrices[trans][0]*sx + rotationMatrices[trans][1]*sy + rotationMatrices[trans][2]*sz;
  *y = rotationMatrices[trans][3]*sx + rotationMatrices[trans][4]*sy + rotationMatrices[trans][5]*sz;
  *z = rotationMatrices[trans][6]*sx + rotationMatrices[trans][7]*sy + rotationMatrices[trans][8]*sz;
}


static void coordinateToggling(int a, int b, int as, int bs, int * an, int * bn) {

  if (a == 0) {      if (b == 1) { *an = as+1; *bn = bs-1; }
                     else        { *an = as+1; *bn = bs+1; } }
  else if (a == 1) { if (b == 0) { *an = as-1; *bn = bs+1; }
                     else        { *an = as-1; *bn = bs-1; } }
  else if (a == 3) { if (b == 0) { *an = as+1; *bn = bs+1; }
                     else        { *an = as+1; *bn = bs-1; } }
  else {             if (b == 1) { *an = as-1; *bn = bs-1; }
                     else        { *an = as-1; *bn = bs+1; } }
}

bool voxel_3_c::getNeighbor(unsigned int idx, unsigned int typ, int x, int y, int z, int * xn, int *yn, int *zn) const {

  int xc = (x+100)/5-20;
  int yc = (y+100)/5-20;
  int zc = (z+100)/5-20;

  int xs = x - 5*xc;
  int ys = y - 5*yc;
  int zs = z - 5*zc;

  *xn = x;
  *yn = y;
  *zn = z;

  switch (typ) {

    case 0:

      switch (idx) {

        case 0:
          if (xs == 0 || xs == 4) {

            if (ys == 2) *yn = y-1; else *yn = 5*yc+2;
            if (zs == 2) *zn = z-1; else *zn = 5*zc+2;

          } else if (ys == 0 || ys == 4) {

            if (xs == 2) *xn = x-1; else *xn = 5*xc+2;
            if (zs == 2) *zn = z-1; else *zn = 5*zc+2;

          } else {

            if (xs == 2) *xn = x-1; else *xn = 5*xc+2;
            if (ys == 2) *yn = y-1; else *yn = 5*yc+2;

          }
          return true;

        case 1:
          if (xs == 0 || xs == 4) {

            if (ys == 2) *yn = y+1; else *yn = 5*yc+2;
            if (zs == 2) *zn = z+1; else *zn = 5*zc+2;

          } else if (ys == 0 || ys == 4) {

            if (xs == 2) *xn = x+1; else *xn = 5*xc+2;
            if (zs == 2) *zn = z+1; else *zn = 5*zc+2;

          } else {

            if (xs == 2) *xn = x+1; else *xn = 5*xc+2;
            if (ys == 2) *yn = y+1; else *yn = 5*yc+2;

          }
          return true;

        case 2:
          if (xs == 0) { *xn = x-1; }
          if (xs == 4) { *xn = x+1; }
          if (ys == 0) { *yn = y-1; }
          if (ys == 4) { *yn = y+1; }
          if (zs == 0) { *zn = z-1; }
          if (zs == 4) { *zn = z+1; }
          return true;

        case 3:
          if (xs == 2) { coordinateToggling(ys, zs, y, z, yn, zn); }
          if (ys == 2) { coordinateToggling(xs, zs, x, z, xn, zn); }
          if (zs == 2) { coordinateToggling(xs, ys, x, y, xn, yn); }
          return true;
      }
  }

  return false;
}

void voxel_3_c::scale(unsigned int amount) {

  /* the 6 cutting planes */
  static int planes[6][3] = {
    {1, 1, 0}, {1, -1, 0},
    {1, 0, 1}, {1, 0, -1},
    {0, 1, 1}, {0, 1, -1}};

  unsigned int nsx = ((sx+4)/5)*amount*5;
  unsigned int nsy = ((sy+4)/5)*amount*5;
  unsigned int nsz = ((sz+4)/5)*amount*5;
  voxel_type * s2 = new voxel_type[nsx*nsy*nsz];
  memset(s2, outside, nsx*nsy*nsz);

  // we scale each 5x5x5 block

  unsigned int bsx = ((sx+4)/5);
  unsigned int bsy = ((sy+4)/5);
  unsigned int bsz = ((sz+4)/5);

  /* for each voxel within a 5x5x5 block */
  for (unsigned int x = 0; x < 5; x++)
    for (unsigned int y = 0; y < 5; y++)
      for (unsigned int z = 0; z < 5; z++)

        if (validCoordinate(x, y, z)) {

          /* this bitmask represents the position of the current voxel
           * relative to the 6 planes */
          int planeMask = 0;
          for (int i = 0; i < 6; i++)
            if ((x-2.5)*planes[i][0] +
                (y-2.5)*planes[i][1] +
                (z-2.5)*planes[i][2] < 0)
              planeMask |= 1<<i;

          /* go through all voxels in the target block */
          for (unsigned int ax = 0; ax < 5*amount; ax++)
            for (unsigned int ay = 0; ay < 5*amount; ay++)
              for (unsigned int az = 0; az < 5*amount; az++)
                if (validCoordinate(ax, ay, az)) {

                  bool useVoxel = true;

                  for (int i = 0; i < 6; i++)
                    if (((ax-2.5*amount)*planes[i][0] +
                          (ay-2.5*amount)*planes[i][1] +
                          (az-2.5*amount)*planes[i][2] < 0) != (((planeMask & (1<<i)) != 0))) {
                      useVoxel = false;
                      break;
                    }

                  if (useVoxel)

                    /* for each block */
                    for (unsigned int bx = 0; bx < bsx; bx++)
                      for (unsigned int by = 0; by < bsy; by++)
                        for (unsigned int bz = 0; bz < bsz; bz++)
                          if (!isEmpty2(5*bx+x, 5*by+y, 5*bz+z))
                            s2[(bx*amount*5+ax) + nsx * ((by*amount*5+ay) + nsy * (bz*amount*5+az))] = get(5*bx+x, 5*by+y, 5*bz+z);

                }
        }

  delete [] space;
  space = s2;

  sx = nsx;
  sy = nsy;
  sz = nsz;

  hx = hy = hz = 0;

  voxels = sx*sy*sz;

  recalcBoundingBox();
}

bool voxel_3_c::scaleDown(unsigned char by, bool action) {

  return false;
}

void voxel_3_c::resizeInclude(int & px, int & py, int & pz) {

  int nsx = getX();
  int nsy = getY();
  int nsz = getZ();
  int tx = 0;
  int ty = 0;
  int tz = 0;

  while (px < 0) {

    nsx += 5;
    tx += 5;
    px += 5;
  }
  while (py < 0) {

    nsy += 5;
    ty += 5;
    py += 5;
  }
  while (pz < 0) {

    nsz += 5;
    tz += 5;
    pz += 5;
  }
  if (px >= (int)getX()) nsx += (px-getX()+1);
  if (py >= (int)getY()) nsy += (py-getY()+1);
  if (pz >= (int)getZ()) nsz += (pz-getZ()+1);

  resize(nsx, nsy, nsz, 0);
  translate(tx, ty, tz, 0);
}

void voxel_3_c::minimizePiece(void) {

  // we must make sure that the lower corner is
  // shifted back to the same modulo 6 position that it had been on

  int x = bx1 % 5;
  int y = by1 % 5;
  int z = bz1 % 5;

  voxel_c::minimizePiece();

  if (x || y || z) {
    resize(sx+x, sy+y, sz+z, 0);
    translate(x, y, z, 0);
  }
}

bool voxel_3_c::validCoordinate(int x, int y, int z) const {

  x %= 5;
  y %= 5;
  z %= 5;

  if (x > 2) x = 4-x;
  if (y > 2) y = 4-y;
  if (z > 2) z = 4-z;

  if (x != 0 && y != 0 && z != 0) return false;
  if (x != 1 && y != 1 && z != 1) return false;
  if (x != 2 && y != 2 && z != 2) return false;

  return true;
}

