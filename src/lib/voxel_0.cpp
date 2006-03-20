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
#include "voxel_0.h"

void voxel_0_c::rotatex(int by) {

  by &= 3;

  switch(by) {
  case 0:
    break;
  case 1:
    {
      int tmp = sy;
      sy = sz;
      sz = tmp;

      voxel_type *s = new voxel_type[voxels];

      for (unsigned int x = 0; x < sx; x++)
        for (unsigned int y = 0; y < sy; y++)
          for (unsigned int z = 0; z < sz; z++)
            s[x+sx*(y+sy*z)] = space[x+sx*(z+sz*(sy-y-1))];

      unsigned int t = by1;
      by1 = sy - 1 - bz2;
      bz2 = by2;
      by2 = sy - 1 - bz1;
      bz1 = t;

      t = hy;
      hy = sy - 1 - hz;
      hz = t;

      delete [] space;
      space = s;
    }
    break;
  case 2:
    {
      voxel_type *s = new voxel_type[voxels];

      for (unsigned int x = 0; x < sx; x++)
        for (unsigned int y = 0; y < sy; y++)
          for (unsigned int z = 0; z < sz; z++)
            s[x+sx*(y+sy*z)] = space[x+sx*((sy-y-1)+sy*(sz-z-1))];

      unsigned int t = by1;
      by1 = sy - 1 - by2;
      by2 = sy - 1 - t;

      t = bz1;
      bz1 = sz - 1 - bz2;
      bz2 = sz - 1 - t;

      hy = sy - 1 - hy;
      hz = sz - 1 - hz;

      delete [] space;
      space = s;
    }

    break;
  case 3:
    {
      int tmp = sy;
      sy = sz;
      sz = tmp;

      voxel_type *s = new voxel_type[voxels];

      for (unsigned int x = 0; x < sx; x++)
        for (unsigned int y = 0; y < sy; y++)
          for (unsigned int z = 0; z < sz; z++)
            s[x+sx*(y+sy*z)] = space[x+sx*((sz-z-1)+sz*y)];

      unsigned int t = by1;
      by1 = bz1;
      bz1 = sz - 1 - by2;
      by2 = bz2;
      bz2 = sz - 1 - t;

      t = hy;
      hy = hz;
      hz = sz - 1 - t;

      delete [] space;
      space = s;
    }

    break;
  }
  symmetries = symmetryInvalid();
}

void voxel_0_c::rotatey(int by) {

  by &= 3;

  switch(by) {
  case 0:
    break;
  case 1:
    {
      int tmp = sx;
      sx = sz;
      sz = tmp;

      voxel_type *s = new voxel_type[voxels];

      for (unsigned int x = 0; x < sx; x++)
        for (unsigned int y = 0; y < sy; y++)
          for (unsigned int z = 0; z < sz; z++)
            s[x+sx*(y+sy*z)] = space[z+sz*(y+sy*(sx-x-1))];

      delete [] space;
      space = s;

      unsigned int t = bx1;

      bx1 = sx - 1 - bz2;
      bz2 = bx2;
      bx2 = sx - 1 - bz1;
      bz1 = t;

      t = hx;
      hx = sx - 1 - hz;
      hz = t;

    }
    break;
  case 2:
    {
      voxel_type *s = new voxel_type[voxels];

      for (unsigned int x = 0; x < sx; x++)
        for (unsigned int y = 0; y < sy; y++)
          for (unsigned int z = 0; z < sz; z++)
            s[x+sx*(y+sy*z)] = space[(sx-x-1)+sx*(y+sy*(sz-z-1))];

      delete [] space;
      space = s;

      unsigned int t = bx1;

      bx1 = sx - 1 - bx2;
      bx2 = sx - 1 - t;

      t = bz1;
      bz1 = sz - 1 - bz2;
      bz2 = sz - 1 - t;

      hx = sx - 1 - hx;
      hz = sz - 1 - hz;

    }
    break;
  case 3:
    {
      int tmp = sx;
      sx = sz;
      sz = tmp;

      voxel_type *s = new voxel_type[voxels];

      for (unsigned int x = 0; x < sx; x++)
        for (unsigned int y = 0; y < sy; y++)
          for (unsigned int z = 0; z < sz; z++)
            s[x+sx*(y+sy*z)] = space[(sz-z-1)+sz*(y+sy*x)];

      delete [] space;
      space = s;

      unsigned int t = bx1;

      bx1 = bz1;
      bz1 = sz - 1 - bx2;
      bx2 = bz2;
      bz2 = sz - 1 - t;

      t = hx;
      hx = hz;
      hz = sz - 1 - t;

    }
    break;
  }
  symmetries = symmetryInvalid();
}

void voxel_0_c::rotatez(int by) {

  by &= 3;

  switch(by) {
  case 0:
    break;
  case 1:
    {
      int tmp = sy;
      sy = sx;
      sx = tmp;

      voxel_type *s = new voxel_type[voxels];

      for (unsigned int x = 0; x < sx; x++)
        for (unsigned int y = 0; y < sy; y++)
          for (unsigned int z = 0; z < sz; z++)
            s[x+sx*(y+sy*z)] = space[y+sy*((sx-x-1)+sx*z)];

      delete [] space;
      space = s;

      unsigned int t = by1;

      by1 = bx1;
      bx1 = sx - 1 - by2;
      by2 = bx2;
      bx2 = sx - 1 - t;

      t = hy;
      hy = hx;
      hx = sx - 1 - t;

    }
    break;
  case 2:
    {
      voxel_type *s = new voxel_type[voxels];

      for (unsigned int x = 0; x < sx; x++)
        for (unsigned int y = 0; y < sy; y++)
          for (unsigned int z = 0; z < sz; z++)
            s[x+sx*(y+sy*z)] = space[(sx-x-1)+sx*((sy-y-1)+sy*z)];

      delete [] space;
      space = s;

      unsigned int t = by1;

      by1 = sy - 1 - by2;
      by2 = sy - 1 - t;

      t = bx1;
      bx1 = sx - 1 - bx2;
      bx2 = sx - 1 - t;

      hx = sx - 1 - hx;
      hy = sy - 1 - hy;
    }
    break;
  case 3:
    {
      int tmp = sy;
      sy = sx;
      sx = tmp;

      voxel_type *s = new voxel_type[voxels];

      for (unsigned int x = 0; x < sx; x++)
        for (unsigned int y = 0; y < sy; y++)
          for (unsigned int z = 0; z < sz; z++)
            s[x+sx*(y+sy*z)] = space[(sy-y-1)+sy*(x+sx*z)];

      delete [] space;
      space = s;

      unsigned int t = by1;

      by1 = sy - 1 - bx2;
      bx2 = by2;
      by2 = sy - 1 - bx1;
      bx1 = t;

      t = hy;
      hy = sy - 1 - hx;
      hx = t;
    }
    break;
  }
  symmetries = symmetryInvalid();
}

