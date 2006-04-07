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
      hy = sy - hz;
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

      hy = sy - hy;
      hz = sz - hz;

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
      hz = sz - t;

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
      hx = sx - hz;
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

      hx = sx - hx;
      hz = sz - hz;

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
      hz = sz - t;
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
      hx = sx - t;

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

      hx = sx - hx;
      hy = sy - hy;
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
      hy = sy - hx;
      hx = t;
    }
    break;
  }
  symmetries = symmetryInvalid();
}

void voxel_0_c::getHotspot(unsigned char trans, int * x, int * y, int * z) const {

  const symmetries_c * sym = gt->getSymmetries();

  int tx = hx;
  int ty = hy;
  int tz = hz;
  int tsx = sx;
  int tsy = sy;
  int tsz = sz;
  int t;

  if (trans >= sym->getNumTransformations()) {
    tx = sx - tx;
    trans -= sym->getNumTransformations();
  }

  for (int i = 0; i < sym->rotx(trans); i++) {
    t = ty;
    ty = tsz - tz;
    tz = t;
    t = tsy;
    tsy = tsz;
    tsz = t;
  }

  for (int i = 0; i < sym->roty(trans); i++) {
    t = tx;
    tx = tsz - tz;
    tz = t;
    t = tsx;
    tsx = tsz;
    tsz = t;
  }

  for (int i = 0; i < sym->rotz(trans); i++) {
    t = ty;
    ty = tx;
    tx = tsy - t;
    t = tsx;
    tsx = tsy;
    tsy = t;
  }

  *x = tx;
  *y = ty;
  *z = tz;
}

void voxel_0_c::getBoundingBox(unsigned char trans, int * x1, int * y1, int * z1, int * x2, int * y2, int * z2) const {

  const symmetries_c * sym = gt->getSymmetries();

  int t1x = bx1;
  int t1y = by1;
  int t1z = bz1;
  int t2x = bx2;
  int t2y = by2;
  int t2z = bz2;

  int tsx = sx;
  int tsy = sy;
  int tsz = sz;
  int t;

  if (trans >= sym->getNumTransformations()) {
    t1x = sx - 1 - t1x;
    t2x = sx - 1 - t2x;
    trans -= sym->getNumTransformations();
  }

  for (int i = 0; i < sym->rotx(trans); i++) {
    t = t1y;
    t1y = tsz - 1 - t1z;
    t1z = t;

    t = t2y;
    t2y = tsz - 1 - t2z;
    t2z = t;

    t = tsy;
    tsy = tsz;
    tsz = t;
  }

  for (int i = 0; i < sym->roty(trans); i++) {
    t = t1x;
    t1x = tsz - 1 - t1z;
    t1z = t;

    t = t2x;
    t2x = tsz - 1 - t2z;
    t2z = t;

    t = tsx;
    tsx = tsz;
    tsz = t;
  }

  for (int i = 0; i < sym->rotz(trans); i++) {
    t = t1y;
    t1y = t1x;
    t1x = tsy - 1 - t;

    t = t2y;
    t2y = t2x;
    t2x = tsy - 1 - t;

    t = tsx;
    tsx = tsy;
    tsy = t;
  }

#define MIN(a,b) (a<b)?(a):(b)
#define MAX(a,b) (a>b)?(a):(b)

  if (x1) *x1 = MIN(t1x, t2x);
  if (x2) *x2 = MAX(t1x, t2x);

  if (y1) *y1 = MIN(t1y, t2y);
  if (y2) *y2 = MAX(t1y, t2y);

  if (z1) *z1 = MIN(t1z, t2z);
  if (z2) *z2 = MAX(t1z, t2z);

#undef MIN
#undef MAX
}

void voxel_0_c::transformPoint(int * x, int * y, int * z, unsigned int trans) const {
  const symmetries_c * sym = gt->getSymmetries();

  if (trans >= sym->getNumTransformations()) {
    *x = -(*x);
    trans -= sym->getNumTransformations();
  }

  for (int i = 0; i < sym->rotx(trans); i++) {
    int tmp = *y;
    *y = - (*z);
    *z = tmp;
  }

  for (int i = 0; i < sym->roty(trans); i++) {
    int tmp = *z;
    *z = *x;
    *x = -tmp;
  }

  for (int i = 0; i < sym->rotz(trans); i++) {
    int tmp = *x;
    *x = -(*y);
    *y = tmp;
  }
}

