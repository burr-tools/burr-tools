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
#include "voxel_1.h"

#include <string.h>
#include <math.h>

void voxel_1_c::rotatex(int by) {

  by &= 1;

  if (by != 0) {

    // this works, when the vertical size is even

    unsigned int nsy = sy + (sy & 1);
    unsigned int nsx = sx;
    unsigned nsz = sz;

    voxel_type *s = new voxel_type[nsx*nsy*nsz];
    memset(s, outside, nsx*nsy*nsz);

    for (unsigned int x = 0; x < sx; x++)
      for (unsigned int z = 0; z < sz; z++)
        for (unsigned int y = 0; y < sy; y++)
          s[x+nsx*((nsy-y-1)+nsy*(nsz-z-1))] = space[x+sx*(y+sy*z)];

    delete [] space;
    space = s;
    sx = nsx;
    sy = nsy;
    sz = nsz;

    voxels = nsx*nsy*nsz;
  }

  symmetries = symmetryInvalid();
}

void voxel_1_c::rotatey(int by) {

  by &= 1;

  if (by != 0) {

    unsigned int nsy = sy;
    unsigned int nsx = sx + (1-(sx & 1));
    unsigned int nsz = sz;

    voxel_type *s = new voxel_type[nsx*nsy*nsz];
    memset(s, outside, nsx*nsy*nsz);

    for (unsigned int x = 0; x < sx; x++)
      for (unsigned int z = 0; z < sz; z++)
        for (unsigned int y = 0; y < sy; y++)
          s[(nsx-x-1)+nsx*(y+nsy*(nsz-z-1))] = space[x+sx*(y+sy*z)];

    delete [] space;
    space = s;
    sx = nsx;
    sy = nsy;
    sz = nsz;

    voxels = nsx*nsy*nsz;
  }

  symmetries = symmetryInvalid();
}

void voxel_1_c::rotatez(int by) {

  // this is the complicated case
  //
  // we first calculate the rotation center by looking for a point where the maximum distance of the other
  // used points is minimal
  // then we calculate the size required to hold all of the possible rotations
  // then we place the new rotation inside this new grid

  by &= 6;

  // the rotation center
  float rx, ry;
  int nsx = sx;
  int nsy = sy;
  int nsz = sz;

  // calculate rotation center and new grid size
  if (by != 0) {
  }

  // perform the rotations around the caluclated center
  voxel_type *s = new voxel_type[nsx*nsy*nsz];
  memset(s, outside, nsx*nsy*nsz);

  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++) {

      // calculete a point inside the current triangle
      float px = x*0.5+0.5 - rx;
      float py = (y+0.5)*sqrt(3)/2 - ry;

      // rotate arount z axis by 60 degree
      float tx =  cos(by*60*M_PI/180)*px + sin(by*60*M_PI/180)*py + rx;
      float ty = -sin(by*60*M_PI/180)*px + cos(by*60*M_PI/180)*py + ry;

      // find out the tile that we are on
      bt_assert(tx > 0);
      bt_assert(ty > 0);

      unsigned int tty = (unsigned int)(ty / (sqrt(3)/2));
      unsigned int ttx = 2 * (unsigned int)tx;

      for (unsigned int z = 0; z < nsz; z++)
        s[ttx + nsx*(y + nsy*z)] = space[x + sx*(y + sy*z)];
    }

  delete [] space;
  space = s;
  sx = nsx;
  sy = nsy;
  sz = nsz;

  voxels = nsx*nsy*nsz;

  symmetries = symmetryInvalid();
}

void voxel_1_c::minimizePiece(void) {

  // find a first voxel
  unsigned int fx, fy;
  fx = fy = 0;

  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++)
        if (!isEmpty(x, y, z)) {
          fx = x;
          fy = y;

          // break out of loops
          z = sz;
          y = sy;
          x = sx;
        }

  voxel_c::minimizePiece();

  // find the first voxel again
  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++)
        if (!isEmpty(x, y, z)) {

          // if the parity of that voxel changed, add an empty column at the left
          if ( ((fx+fy) & 1) != ((x+y) & 1) ) {
            resize(sx+1, sy, sz, 0);
            translate(1, 0, 0, 0);
          }

          // break out of loops
          z = sz;
          y = sy;
          x = sx;
        }
}

