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
#include "voxel_3.h"

bool voxel_3_c::transform(unsigned int nr) {

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

  return voxel_0_c::transform(nr);
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

  // the following table was created with a helper function
  // and the edge detection code in the voxeldrawer_3
  static const int edgeNeighbors[24][14+1][3] = {
    { {0, 1, 2}, // cube position
      {-1, -3, 0}, {-1, 1, -1}, {-1, 1, 1}, {-2, -1, 0}, {-2, -2, 0}, {0, -3, 0}, {0, 2, 0},
      {1, -2, 0}, {1, 1, -2}, {1, 1, 2}, {2, -1, -1}, {2, -1, 1}, {2, 0, -2}, {2, 0, 2}
    },
    { {0, 2, 1}, // cube position
      {-1, -1, 1}, {-1, 0, -3}, {-1, 1, 1}, {-2, 0, -1}, {-2, 0, -2}, {0, 0, -3}, {0, 0, 2},
      {1, -2, 1}, {1, 0, -2}, {1, 2, 1}, {2, -1, -1}, {2, -2, 0}, {2, 1, -1}, {2, 2, 0}
    },
    { {0, 2, 3}, // cube position
      {-1, -1, -1}, {-1, 0, 3}, {-1, 1, -1}, {-2, 0, 1}, {-2, 0, 2}, {0, 0, -2}, {0, 0, 3},
      {1, -2, -1}, {1, 0, 2}, {1, 2, -1}, {2, -1, 1}, {2, -2, 0}, {2, 1, 1}, {2, 2, 0}
    },
    { {0, 3, 2}, // cube position
      {-1, -1, -1}, {-1, -1, 1}, {-1, 3, 0}, {-2, 1, 0}, {-2, 2, 0}, {0, -2, 0}, {0, 3, 0},
      {1, -1, -2}, {1, -1, 2}, {1, 2, 0}, {2, 0, -2}, {2, 0, 2}, {2, 1, -1}, {2, 1, 1}
    },
    { {1, 0, 2}, // cube position
      {-1, -2, 0}, {-1, 2, -1}, {-1, 2, 1}, {-2, -2, 0}, {-2, 1, 0}, {-3, -1, 0}, {-3, 0, 0},
      {0, 2, -2}, {0, 2, 2}, {1, -1, -1}, {1, -1, 1}, {1, 1, -2}, {1, 1, 2}, {2, 0, 0}
    },
    { {1, 2, 0}, // cube position
      {-1, -1, 2}, {-1, 0, -2}, {-1, 1, 2}, {-2, 0, -2}, {-2, 0, 1}, {-3, 0, -1}, {-3, 0, 0},
      {0, -2, 2}, {0, 2, 2}, {1, -1, -1}, {1, -2, 1}, {1, 1, -1}, {1, 2, 1}, {2, 0, 0}
    },
    { {2, 0, 1}, // cube position
      {-1, -1, 1}, {-1, 2, -1}, {-2, 1, 1}, {-2, 2, 0}, {0, -1, -3}, {0, -2, -1}, {0, -2, -2},
      {0, 0, -3}, {0, 0, 2}, {0, 1, -2}, {1, -1, 1}, {1, 2, -1}, {2, 1, 1}, {2, 2, 0}
    },
    { {2, 0, 3}, // cube position
      {-1, -1, -1}, {-1, 2, 1}, {-2, 1, -1}, {-2, 2, 0}, {0, -1, 3}, {0, -2, 1}, {0, -2, 2},
      {0, 0, -2}, {0, 0, 3}, {0, 1, 2}, {1, -1, -1}, {1, 2, 1}, {2, 1, -1}, {2, 2, 0}
    },
    { {2, 1, 0}, // cube position
      {-1, -1, 2}, {-1, 1, -1}, {-2, 0, 2}, {-2, 1, 1}, {0, -1, -2}, {0, -2, -2}, {0, -2, 1},
      {0, -3, -1}, {0, -3, 0}, {0, 2, 0}, {1, -1, 2}, {1, 1, -1}, {2, 0, 2}, {2, 1, 1}
    },
    { {2, 3, 0}, // cube position
      {-1, -1, -1}, {-1, 1, 2}, {-2, -1, 1}, {-2, 0, 2}, {0, -2, 0}, {0, 1, -2}, {0, 2, -2},
      {0, 2, 1}, {0, 3, -1}, {0, 3, 0}, {1, -1, -1}, {1, 1, 2}, {2, -1, 1}, {2, 0, 2}
    },
    { {3, 0, 2}, // cube position
      {-1, -1, -1}, {-1, -1, 1}, {-1, 1, -2}, {-1, 1, 2}, {-2, 0, 0}, {0, 2, -2}, {0, 2, 2},
      {1, -2, 0}, {1, 2, -1}, {1, 2, 1}, {2, -2, 0}, {2, 1, 0}, {3, -1, 0}, {3, 0, 0}
    },
    { {3, 2, 0}, // cube position
      {-1, -1, -1}, {-1, -2, 1}, {-1, 1, -1}, {-1, 2, 1}, {-2, 0, 0}, {0, -2, 2}, {0, 2, 2},
      {1, -1, 2}, {1, 0, -2}, {1, 1, 2}, {2, 0, -2}, {2, 0, 1}, {3, 0, -1}, {3, 0, 0}
    },
    { {1, 2, 4}, // cube position
      {-1, -1, -2}, {-1, 0, 2}, {-1, 1, -2}, {-2, 0, -1}, {-2, 0, 2}, {-3, 0, 0}, {-3, 0, 1},
      {0, -2, -2}, {0, 2, -2}, {1, -1, 1}, {1, -2, -1}, {1, 1, 1}, {1, 2, -1}, {2, 0, 0},
    },
    { {1, 4, 2}, // cube position
      {-1, -2, -1}, {-1, -2, 1}, {-1, 2, 0}, {-2, -1, 0}, {-2, 2, 0}, {-3, 0, 0}, {-3, 1, 0},
      {0, -2, -2}, {0, -2, 2}, {1, -1, -2}, {1, -1, 2}, {1, 1, -1}, {1, 1, 1}, {2, 0, 0},
    },
    { {2, 1, 4}, // cube position
      {-1, -1, -2}, {-1, 1, 1}, {-2, 0, -2}, {-2, 1, -1}, {0, -1, 2}, {0, -2, -1}, {0, -2, 2},
      {0, -3, 0}, {0, -3, 1}, {0, 2, 0}, {1, -1, -2}, {1, 1, 1}, {2, 0, -2}, {2, 1, -1},
    },
    { {2, 3, 4}, // cube position
      {-1, -1, 1}, {-1, 1, -2}, {-2, -1, -1}, {-2, 0, -2}, {0, -2, 0}, {0, 1, 2}, {0, 2, -1},
      {0, 2, 2}, {0, 3, 0}, {0, 3, 1}, {1, -1, 1}, {1, 1, -2}, {2, -1, -1}, {2, 0, -2},
    },
    { {2, 4, 1}, // cube position
      {-1, -2, -1}, {-1, 1, 1}, {-2, -1, 1}, {-2, -2, 0}, {0, -1, -2}, {0, 0, -3}, {0, 0, 2},
      {0, 1, -3}, {0, 2, -1}, {0, 2, -2}, {1, -2, -1}, {1, 1, 1}, {2, -1, 1}, {2, -2, 0},
    },
    { {2, 4, 3}, // cube position
      {-1, -2, 1}, {-1, 1, -1}, {-2, -1, -1}, {-2, -2, 0}, {0, -1, 2}, {0, 0, -2}, {0, 0, 3},
      {0, 1, 3}, {0, 2, 1}, {0, 2, 2}, {1, -2, 1}, {1, 1, -1}, {2, -1, -1}, {2, -2, 0},
    },
    { {3, 2, 4}, // cube position
      {-1, -1, 1}, {-1, -2, -1}, {-1, 1, 1}, {-1, 2, -1}, {-2, 0, 0}, {0, -2, -2}, {0, 2, -2},
      {1, -1, -2}, {1, 0, 2}, {1, 1, -2}, {2, 0, -1}, {2, 0, 2}, {3, 0, 0}, {3, 0, 1},
    },
    { {3, 4, 2}, // cube position
      {-1, -1, -2}, {-1, -1, 2}, {-1, 1, -1}, {-1, 1, 1}, {-2, 0, 0}, {0, -2, -2}, {0, -2, 2},
      {1, -2, -1}, {1, -2, 1}, {1, 2, 0}, {2, -1, 0}, {2, 2, 0}, {3, 0, 0}, {3, 1, 0},
    },
    { {4, 1, 2}, // cube position
      {-1, -2, 0}, {-1, 1, -2}, {-1, 1, 2}, {-2, -1, -1}, {-2, -1, 1}, {-2, 0, -2}, {-2, 0, 2},
      {0, -3, 0}, {0, 2, 0}, {1, -3, 0}, {1, 1, -1}, {1, 1, 1}, {2, -1, 0}, {2, -2, 0},
    },
    { {4, 2, 1}, // cube position
      {-1, -2, 1}, {-1, 0, -2}, {-1, 2, 1}, {-2, -1, -1}, {-2, -2, 0}, {-2, 1, -1}, {-2, 2, 0},
      {0, 0, -3}, {0, 0, 2}, {1, -1, 1}, {1, 0, -3}, {1, 1, 1}, {2, 0, -1}, {2, 0, -2},
    },
    { {4, 2, 3}, // cube position
      {-1, -2, -1}, {-1, 0, 2}, {-1, 2, -1}, {-2, -1, 1}, {-2, -2, 0}, {-2, 1, 1}, {-2, 2, 0},
      {0, 0, -2}, {0, 0, 3}, {1, -1, -1}, {1, 0, 3}, {1, 1, -1}, {2, 0, 1}, {2, 0, 2},
    },
    { {4, 3, 2}, // cube position
      {-1, -1, -2}, {-1, -1, 2}, {-1, 2, 0}, {-2, 0, -2}, {-2, 0, 2}, {-2, 1, -1}, {-2, 1, 1},
      {0, -2, 0}, {0, 3, 0}, {1, -1, -1}, {1, -1, 1}, {1, 3, 0}, {2, 1, 0}, {2, 2, 0},
    },
  };

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

        default:
          return false;
      }

      break;

    case 1:

      if (idx >= 14) return false;

      for (int i = 0; i < 24; i++)
        if (edgeNeighbors[i][0][0] == xs &&
            edgeNeighbors[i][0][1] == ys &&
            edgeNeighbors[i][0][2] == zs) {
          *xn = x + edgeNeighbors[i][idx+1][0];
          *yn = y + edgeNeighbors[i][idx+1][1];
          *zn = z + edgeNeighbors[i][idx+1][2];
          return true;
        }

      return false;
  }

  return false;
}

void voxel_3_c::scale(unsigned int amount) {

  /* the coordinates of the valid voxels */
  static int voxelCoords[24][3] = {
    { 2, 1, 0 }, { 1, 2, 0 }, { 3, 2, 0 }, { 2, 3, 0 },
    { 2, 0, 1 }, { 0, 2, 1 }, { 4, 2, 1 }, { 2, 4, 1 },
    { 1, 0, 2 }, { 3, 0, 2 }, { 0, 1, 2 }, { 4, 1, 2 }, { 0, 3, 2 }, { 4, 3, 2 }, { 1, 4, 2 }, { 3, 4, 2 },
    { 2, 0, 3 }, { 0, 2, 3 }, { 4, 2, 3 }, { 2, 4, 3 },
    { 2, 1, 4 }, { 1, 2, 4 }, { 3, 2, 4 }, { 2, 3, 4 }
  };

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
  for (unsigned int voxel1 = 0; voxel1 < 24; voxel1++) {
    unsigned int x = voxelCoords[voxel1][0];
    unsigned int y = voxelCoords[voxel1][1];
    unsigned int z = voxelCoords[voxel1][2];

    /* this bitmask represents the position of the current voxel
     * relative to the 6 planes */
    int planeMask = 0;
    for (int i = 0; i < 6; i++)
      if ((x-2.5)*planes[i][0] +
          (y-2.5)*planes[i][1] +
          (z-2.5)*planes[i][2] < 0)
        planeMask |= 1<<i;

    /* go through all voxels in the target block */
    for (unsigned int amx = 0; amx < amount; amx++)
      for (unsigned int amy = 0; amy < amount; amy++)
        for (unsigned int amz = 0; amz < amount; amz++)

          for (unsigned int voxel2 = 0; voxel2 < 24; voxel2++) {

            unsigned int ax = 5*amx + voxelCoords[voxel2][0];
            unsigned int ay = 5*amy + voxelCoords[voxel2][1];
            unsigned int az = 5*amz + voxelCoords[voxel2][2];

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

bool voxel_3_c::scaleDown(unsigned char /*by*/, bool /*action*/) {

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

  if (nsx % 5) nsx += 5-nsx%5;
  if (nsy % 5) nsy += 5-nsy%5;
  if (nsz % 5) nsz += 5-nsz%5;

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

  /* make sure that the new size is a multiple of 5 */
  if (sx%5 || sy%5 || sz%5)
    resize(sx+(5-sx%5)%5, sy+(5-sy%5)%5, sz+(5-sz%5)%5, 0);
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

bool voxel_3_c::identicalInBB(const voxel_c * op, bool includeColors) const {
  return bx1 % 5 == op->boundX1() % 5 && by1 % 5 == op->boundY1() % 5 && bz1 % 5 == op->boundZ1() % 5 && voxel_c::identicalInBB(op, includeColors);
}

