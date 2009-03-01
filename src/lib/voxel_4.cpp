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
#include "voxel_4.h"

/**
 * special transformation function for this grid.
 *
 * All the function has to do is to make sure that the grid is a multiple
 * size of 6 and then it can use the brick transformation function
 */
bool voxel_4_c::transform(unsigned int nr) {

  // the first thing to do here is to ensure that all 3 dimensions are a multiple of 6
  {
    int sx = getX();
    int sy = getY();
    int sz = getZ();

    sx += 5; sx -= sx % 6;
    sy += 5; sy -= sy % 6;
    sz += 5; sz -= sz % 6;

    resize(sx, sy, sz, 0);
  }

  return voxel_0_c::transform(nr);
}

bool voxel_4_c::getNeighbor(unsigned int idx, unsigned int typ, int x, int y, int z, int * xn, int *yn, int *zn) const {

  /* index 0, position of voxel
   * index 1, cube type (position even or not)
   * index 3, first entry voxel, other entries neighbors
   * index 4, x,y,z coordinate
   */

  static const int faceNeighbors[5][2][5][3] = {
    {
      { {1, 1, 1}, /* cube position */ {1, -1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, 1, 1} },
      { {1, 1, 1}, /* cube position */ {-1, -1, -1}, {1, 1, -1}, {1, -1, 1}, {-1, 1, 1} }
    },
    {
      { {2, 0, 0}, /* cube position */ {-1, 1, 1}, {0, 0, -1}, {1, 0, 0}, {0, -1, 0} },
      { {0, 0, 0}, /* cube position */ {1, 1, 1}, {0, 0, -1}, {0, -1, 0}, {-1, 0, 0} }
    },
    {
      { {0, 2, 0}, /* cube position */ {1, -1, 1}, {0, 0, -1}, {0, 1, 0}, {-1, 0, 0} },
      { {2, 2, 0}, /* cube position */ {-1, -1, 1}, {0, 0, -1}, {1, 0, 0}, {0, 1, 0} }
    },
    {
      { {0, 0, 2}, /* cube position */ {0, -1, 0}, {-1, 0, 0}, {1, 1, -1}, {0, 0, 1} },
      { {2, 0, 2}, /* cube position */ {0, -1, 0}, {1, 0, 0}, {-1, 1, -1}, {0, 0, 1} }
    },
    {
      { {2, 2, 2}, /* cube position */ {1, 0, 0}, {0, 1, 0}, {-1, -1, -1}, {0, 0, 1} },
      { {0, 2, 2}, /* cube position */ {-1, 0, 0}, {0, 1, 0}, {1, -1, -1}, {0, 0, 1} }
    },
  };

  // the following table was created with a helper function
  // and the edge detection code in the voxeldrawer_a
  static const int edgeNeighbors[24][18+1][3] = {
    { {0, 0, 2}, /* cube position */
      {0, -1, 1}, {0, 2, -2}, {-1, 0, 1}, {-1, -1, 0}, {1, 1, 2}, {1, -2, -1},
      {-1, 2, -2}, {2, 0, -2}, {-2, 1, -1}, {2, -1, -2}, {2, 2, 0}, {2, 2, 1},
    },
    { {0, 2, 0}, /* cube position */
      {0, 1, -1}, {0, -2, 2}, {-1, 0, -1}, {-1, 1, 0}, {1, -1, -2}, {1, 2, 1},
      {-1, -2, 2}, {2, 0, 2}, {-2, -1, 1}, {2, 1, 2}, {2, -2, 0}, {2, -2, -1},
    },
    { {1, 1, 1}, /* cube position */
      {0, 0, 3}, {0, 0, -3}, {0, -3, 0}, {0, 3, 0}, {-1, -1, 2}, {-1, 1, -2},
      {1, -1, -2}, {1, 1, 2}, {-1, -2, 1}, {-1, 2, -1}, {1, -2, -1}, {1, 2, 1},
      {-2, -1, 1}, {-2, 1, -1}, {2, -1, -1}, {2, 1, 1}, {-3, 0, 0}, {3, 0, 0},
    },
    { {2, 0, 0}, /* cube position */
      {0, -1, -1}, {0, 2, 2}, {1, 0, -1}, {1, -1, 0}, {-1, 1, -2}, {-1, -2, 1},
      {1, 2, 2}, {-2, 0, 2}, {2, 1, 1}, {-2, -1, 2}, {-2, 2, 0}, {-2, 2, -1},
    },
    { {2, 2, 2}, /* cube position */
      {0, 1, 1}, {0, -2, -2}, {1, 0, 1}, {1, 1, 0}, {-1, -1, 2}, {-1, 2, -1},
      {1, -2, -2}, {-2, 0, -2}, {2, -1, -1}, {-2, 1, -2}, {-2, -2, 0}, {-2, -2, 1},
    },


    { {3, 0, 0}, /* cube position */
      {0, -1, -1}, {0, 2, 2}, {-1, 0, -1}, {-1, -1, 0}, {1, 1, -2}, {1, -2, 1},
      {-1, 2, 2}, {2, 0, 2}, {-2, 1, 1}, {2, -1, 2}, {2, 2, 0}, {2, 2, -1},
    },
    { {3, 2, 2}, /* cube position */
      {0, 1, 1}, {0, -2, -2}, {-1, 0, 1}, {-1, 1, 0}, {1, -1, 2}, {1, 2, -1},
      {-1, -2, -2}, {2, 0, -2}, {-2, -1, -1}, {2, 1, -2}, {2, -2, 0}, {2, -2, 1},
    },
    { {4, 1, 1}, /* cube position */
      {0, 0, 3}, {0, 0, -3}, {0, -3, 0}, {0, 3, 0}, {-1, -1, -2}, {-1, 1, 2},
      {1, -1, 2}, {1, 1, -2}, {-1, -2, -1}, {-1, 2, 1}, {1, -2, 1}, {1, 2, -1},
      {-2, -1, -1}, {-2, 1, 1}, {2, -1, 1}, {2, 1, -1}, {-3, 0, 0}, {3, 0, 0},
    },
    { {5, 0, 2}, /* cube position */
      {0, -1, 1}, {0, 2, -2}, {1, 0, 1}, {1, -1, 0}, {-1, 1, 2}, {-1, -2, -1},
      {1, 2, -2}, {-2, 0, -2}, {2, 1, -1}, {-2, -1, -2}, {-2, 2, 0}, {-2, 2, 1},
    },
    { {5, 2, 0}, /* cube position */
      {0, 1, -1}, {0, -2, 2}, {1, 0, -1}, {1, 1, 0}, {-1, -1, -2}, {-1, 2, 1},
      {1, -2, 2}, {-2, 0, 2}, {2, -1, 1}, {-2, 1, 2}, {-2, -2, 0}, {-2, -2, -1},
    }
  };

  int xc = (x+60) / 3 -20;   // this is supposed to make shure we round towards -inf. It works up to -100
  int yc = (y+60) / 3 -20;
  int zc = (z+60) / 3 -20;

  int xs = x - 3*xc;
  int ys = y - 3*yc;
  int zs = z - 3*zc;

  int cubeType = (xc+yc+zc) & 1;

  switch (typ) {

    case 0:

      if (idx >= 4) return false;

      for (int i = 0; i < 5; i++)
        if (faceNeighbors[i][cubeType][0][0] == xs &&
            faceNeighbors[i][cubeType][0][1] == ys &&
            faceNeighbors[i][cubeType][0][2] == zs) {
          *xn = x + faceNeighbors[i][cubeType][idx+1][0];
          *yn = y + faceNeighbors[i][cubeType][idx+1][1];
          *zn = z + faceNeighbors[i][cubeType][idx+1][2];
          return true;
        }

      return false;

    case 1:

      if (xs == 1 && ys == 1 && zs == 1) {
        if (idx >= 18) return false;
      } else {
        if (idx >= 12) return false;
      }

      if (cubeType) xs+= 3;

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

/**
 * scale up.
 *
 * The idea is to use cutting planes that cut the cube into its parts
 * the first thing is to find out on which side of each plane a voxel is
 * and then use the same planes scaled up for the upscaled space.
 */
void voxel_4_c::scale(unsigned int amount) {

  /* the coordinates of the valid voxels */
  static int voxelCoords[2][5][3] = {
    { { 1, 1, 1 }, { 0, 2, 0 }, { 2, 0, 0 }, { 0, 0, 2 }, { 2, 2, 2 } },
    { { 1, 1, 1 }, { 0, 0, 0 }, { 2, 2, 0 }, { 2, 0, 2 }, { 0, 2, 2 } },
  };

  /* the 2x4 cutting planes */
  static int planes[2][4][3] = {
    { {-1, 1, -1}, {1, -1, -1}, {-1, -1, 1}, {1, 1, 1} },
    { {1, 1, -1}, {-1, -1, -1}, {-1, 1, 1}, {1, -1, 1} },
  };

  unsigned int nsx = ((sx+2)/3)*amount*3;
  unsigned int nsy = ((sy+2)/3)*amount*3;
  unsigned int nsz = ((sz+2)/3)*amount*3;
  voxel_type * s2 = new voxel_type[nsx*nsy*nsz];
  memset(s2, VX_EMPTY, nsx*nsy*nsz);

  // we scale each 3x3x3 block

  unsigned int bsx = ((sx+2)/3);
  unsigned int bsy = ((sy+2)/3);
  unsigned int bsz = ((sz+2)/3);

  for (unsigned bx = 0; bx < bsx; bx++)
    for (unsigned by = 0; by < bsy; by++)
      for (unsigned bz = 0; bz < bsz; bz++) {

        /* blocktype for the source block */
        unsigned int blockType = (bx+by+bz) & 1;

        /* for all voxels of the source block */
        for (unsigned int voxels1 = 0; voxels1 < 5; voxels1++) {

          int x = voxelCoords[blockType][voxels1][0];
          int y = voxelCoords[blockType][voxels1][1];
          int z = voxelCoords[blockType][voxels1][2];

          if (get2(3*bx+x, 3*by+y, 3*bz+z)) {

            /* calculate relative position of the source voxel
             * to the cutting planes
             */
            int planeMask = 0;
            for (int i = 0; i < 4; i++) {
              if ((x-1)*planes[blockType][i][0] +
                  (y-1)*planes[blockType][i][1] +
                  (z-1)*planes[blockType][i][2] <= 1)
                planeMask |= 1<<i;
            }

            /* for all possible destination blocks of that source block */
            for (unsigned int amx = 0; amx < amount; amx++)
              for (unsigned int amy = 0; amy < amount; amy++)
                for (unsigned int amz = 0; amz < amount; amz++) {

                  /* find out the block type of that block */
                  unsigned int blockType2 = (amount*(bx+by+bz)+amx+amy+amz) & 1;

                  /* for all voxels of that block */
                  for (unsigned int voxel2 = 0; voxel2 < 5; voxel2++) {

                    /* find out the coordinates of that voxel */
                    int ax = 3*amx + voxelCoords[blockType2][voxel2][0];
                    int ay = 3*amy + voxelCoords[blockType2][voxel2][1];
                    int az = 3*amz + voxelCoords[blockType2][voxel2][2];

                    bool useVoxel = true;

                    for (int i = 0; i < 4; i++) {
                      if (((ax-1.5*amount+0.5)*planes[blockType][i][0] +
                           (ay-1.5*amount+0.5)*planes[blockType][i][1] +
                           (az-1.5*amount+0.5)*planes[blockType][i][2] < (1.5*amount-0.5)) != (((planeMask & (1<<i)) != 0))) {

                        useVoxel = false;
                        break;
                      }
                    }

                    if (useVoxel) {
                      s2[(bx*amount*3+ax) + nsx * ((by*amount*3+ay) + nsy * (bz*amount*3+az))] = get(3*bx+x, 3*by+y, 3*bz+z);
                    }
                  }
                }
          }
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

/**
 * Scale down voxel space
 *
 * Not implemented
 */
bool voxel_4_c::scaleDown(unsigned char /*by*/, bool /*action*/) {

  return false;
}

/**
 * resize space to include a certain coordinate.
 *
 * maybe changing the coordinate itself in the process.
 * make sure that we add a multiple of 6, when we add at the base
 * that's all there is to it
 */
void voxel_4_c::resizeInclude(int & px, int & py, int & pz) {

  int nsx = getX();
  int nsy = getY();
  int nsz = getZ();
  int tx = 0;
  int ty = 0;
  int tz = 0;

  while (px < 0) {

    nsx += 6;
    tx += 6;
    px += 6;
  }
  while (py < 0) {

    nsy += 6;
    ty += 6;
    py += 6;
  }
  while (pz < 0) {

    nsz += 6;
    tz += 6;
    pz += 6;
  }
  if (px >= (int)getX()) nsx += (px-getX()+1);
  if (py >= (int)getY()) nsy += (py-getY()+1);
  if (pz >= (int)getZ()) nsz += (pz-getZ()+1);

  if (nsx % 6) nsx += 6-nsx%6;
  if (nsy % 6) nsy += 6-nsy%6;
  if (nsz % 6) nsz += 6-nsz%6;

  resize(nsx, nsy, nsz, 0);
  translate(tx, ty, tz, 0);
}

void voxel_4_c::minimizePiece(void) {

  // we must make sure that the lower corner is
  // shifted back to the same modulo 6 position that it had been on

  int x = bx1 % 6;
  int y = by1 % 6;
  int z = bz1 % 6;

  voxel_c::minimizePiece();

  if (x || y || z) {
    resize(sx+x, sy+y, sz+z, 0);
    translate(x, y, z, 0);
  }

  /* make sure that the new size is a multiple of 3 */
  if (sx%3 || sy%3 || sz%3)
    resize(sx+(3-sx%3)%3, sy+(3-sy%3)%3, sz+(3-sz%3)%3, 0);
}

bool voxel_4_c::validCoordinate(int x, int y, int z) const {

  int xc = (x+60)/3-20;
  int yc = (y+60)/3-20;
  int zc = (z+60)/3-20;

  int xs = x - 3*xc;
  int ys = y - 3*yc;
  int zs = z - 3*zc;

  if (xs == 1 && ys == 1 && zs == 1) return true;

  if ((xc + yc + zc) & 1) {

    return ((xs == 0 && ys == 0 && zs == 0) ||
            (xs == 2 && ys == 2 && zs == 0) ||
            (xs == 0 && ys == 2 && zs == 2) ||
            (xs == 2 && ys == 0 && zs == 2));

  } else {

    return ((xs == 2 && ys == 0 && zs == 0) ||
            (xs == 0 && ys == 2 && zs == 0) ||
            (xs == 2 && ys == 2 && zs == 2) ||
            (xs == 0 && ys == 0 && zs == 2));

  }
}

bool voxel_4_c::identicalInBB(const voxel_c * op, bool includeColors) const {

  // only when the lower corner of the bounding box is within the right box type and within that 3x3x3 box within the
  // same place can we have an identical shape
  return bx1 % 3 == op->boundX1() % 3 && by1 % 3 == op->boundY1() % 3 && bz1 % 3 == op->boundZ1() % 3 &&
    ((bx1/3 + by1/3 + bz1/3) & 1) == ((op->boundX1()/3 + op->boundY1()/3 + op->boundZ1()/3) & 1) &&
    voxel_c::identicalInBB(op, includeColors);
}

bool voxel_4_c::onGrid(int x, int y, int z) const {

  // the shape doesn't fit, when not within the 3 raster or the cube parity doesn't match
  return x%3 == 0 && y%3 == 0 && z%3 == 0 && (((x/3 + y/3 + z/3) & 1) == 0);
}

