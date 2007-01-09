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
#include "voxel_2.h"

#include <math.h>

double rotationMatrices[120][9] = {
#include "tabs_2/rotmatrix.inc"
};

void voxel_2_c::rotatex(int by) {

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
      hy = sy - hz - 1;
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

      hy = sy - hy - 1;
      hz = sz - hz - 1;

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
      hz = sz - t - 1;

      delete [] space;
      space = s;
    }

    break;
  }
  symmetries = symmetryInvalid();
}

void voxel_2_c::rotatey(int by) {

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
      hx = sx - hz - 1;
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

      hx = sx - hx - 1;
      hz = sz - hz - 1;

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
      hz = sz - t - 1;
    }
    break;
  }
  symmetries = symmetryInvalid();
}

void voxel_2_c::rotatez(int by) {

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
      hx = sx - t - 1;

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

      hx = sx - hx - 1;
      hy = sy - hy - 1;
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
      hy = sy - hx - 1;
      hx = t;
    }
    break;
  }
  symmetries = symmetryInvalid();
}

bool voxel_2_c::transform(unsigned int nr) {

  bt_assert(nr < 240);

  if (nr >= 120) {
    mirrorX();
    nr -= 120;
  }

  int minx = 100000;
  int miny = 100000;
  int minz = 100000;
  int maxx = -100000;
  int maxy = -100000;
  int maxz = -100000;

  bool first = true;
  double shx, shy, shz;
  shx = shy = shz = 0;

  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++) {

        if (((x+y+z) & 1) == 0) {

          if (!isEmpty(x, y, z)) {

            double xp = x * sqrt(0.5);
            double yp = y * sqrt(0.5);
            double zp = z * sqrt(0.5);

            // rotate using the rotation matrices from the top

            double xpn = rotationMatrices[nr][0]*xp + rotationMatrices[nr][1]*yp + rotationMatrices[nr][2]*zp;
            double ypn = rotationMatrices[nr][3]*xp + rotationMatrices[nr][4]*yp + rotationMatrices[nr][5]*zp;
            double zpn = rotationMatrices[nr][6]*xp + rotationMatrices[nr][7]*yp + rotationMatrices[nr][8]*zp;

            xpn /= sqrt(0.5);
            ypn /= sqrt(0.5);
            zpn /= sqrt(0.5);

            if (first) {
              shx = xpn;
              shy = ypn;
              shz = zpn;
              xpn = ypn = zpn = 0;
              first = false;
            } else {
              xpn -= shx;
              ypn -= shy;
              zpn -= shz;
            }

            int xn = (int)round(xpn);
            int yn = (int)round(ypn);
            int zn = (int)round(zpn);

            // check errors, it should be small enough, so the calculated
            // new position should fall on a grid position
            if ((fabs(xpn-xn) > 0.01) ||
                (fabs(ypn-yn) > 0.01) ||
                (fabs(zpn-zn) > 0.01)) {
              return false;
            }

            // is should fall on a valid grid position
            if (((xn+yn+zn) & 1) != 0) {
              return false;
            }

            if (xn > maxx) maxx = xn;
            if (yn > maxy) maxy = yn;
            if (zn > maxz) maxz = zn;

            if (xn < minx) minx = xn;
            if (yn < miny) miny = yn;
            if (zn < minz) minz = zn;
          }
        }
      }

  if ((minx+miny+minz) & 1)
    minx--;

  int nsx = maxx-minx+1;
  int nsy = maxy-miny+1;
  int nsz = maxz-minz+1;

  int voxelsn = nsx*nsy*nsz;

  voxel_type *s = new voxel_type[voxelsn];
  memset(s, outside, voxelsn);

  bool hotspot_done = false;

  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++) {

        if (((x+y+z) & 1) == 0) {

          if (!isEmpty(x, y, z)) {

            double xp = x * sqrt(0.5);
            double yp = y * sqrt(0.5);
            double zp = z * sqrt(0.5);

            // rotate using the rotation matrices from the top

            double xpn = rotationMatrices[nr][0]*xp + rotationMatrices[nr][1]*yp + rotationMatrices[nr][2]*zp;
            double ypn = rotationMatrices[nr][3]*xp + rotationMatrices[nr][4]*yp + rotationMatrices[nr][5]*zp;
            double zpn = rotationMatrices[nr][6]*xp + rotationMatrices[nr][7]*yp + rotationMatrices[nr][8]*zp;

            xpn /= sqrt(0.5);
            ypn /= sqrt(0.5);
            zpn /= sqrt(0.5);

            xpn -= shx;
            ypn -= shy;
            zpn -= shz;

            int xn = (int)round(xpn);
            int yn = (int)round(ypn);
            int zn = (int)round(zpn);

            if ((x == hx) && (y == hy) && (z == hz) && (!hotspot_done)) {
              hx = xn-minx;
              hy = yn-miny;
              hz = zn-minz;
              hotspot_done = true;
            }

            s[(xn-minx) + nsx*((yn-miny) + nsy*(zn-minz))] = space[x + sx*(y + sy*z)];
          }
        }
      }

  sx = nsx;
  sy = nsy;
  sz = nsz;

  delete [] space;
  space = s;

  voxels = voxelsn;

  recalcBoundingBox();

  return true;
}

void voxel_2_c::mirrorX(void) {

  voxel_c::mirrorX();

  if (!(sx & 1)) {
    if (bx1 > 0)
      translate(-1, 0, 0, 0);
    else if (bx2 < sx-1)
      translate(1, 0, 0, 0);
    else if (by1 > 0)
      translate(0, -1, 0, 0);
    else if (by2 < sy-1)
      translate(0, 1, 0, 0);
    else {
      resize(sx+1, sy, sz, 0);
      translate(1, 0, 0, 0);
    }
  }
}

void voxel_2_c::mirrorY(void) {
  voxel_c::mirrorY();

  if (sy & 1) {
    if (bx1 > 0)
      translate(-1, 0, 0, 0);
    else if (bx2 < sx-1)
      translate(1, 0, 0, 0);
    else if (by1 > 0)
      translate(0, -1, 0, 0);
    else if (by2 < sy-1)
      translate(0, 1, 0, 0);
    else {
      resize(sx+1, sy, sz, 0);
      translate(1, 0, 0, 0);
    }
  }
}

void voxel_2_c::mirrorZ(void) {
  voxel_c::mirrorZ();

  if (sz & 1) {
    if (bx1 > 0)
      translate(-1, 0, 0, 0);
    else if (bx2 < sx-1)
      translate(1, 0, 0, 0);
    else if (by1 > 0)
      translate(0, -1, 0, 0);
    else if (by2 < sy-1)
      translate(0, 1, 0, 0);
    else if (bz1 > 0)
      translate(0, 0, -1, 0);
    else if (bz2 < sz-1)
      translate(0, 0, 1, 0);
    else {
      resize(sx+1, sy, sz, 0);
      translate(1, 0, 0, 0);
    }
  }
}

void voxel_2_c::transformPoint(int * x, int * y, int * z, unsigned int trans) const {

  bt_assert(((*x+*y+*z) & 1) == 0);

  double xp = *x * sqrt(0.5);
  double yp = *y * sqrt(0.5);
  double zp = *z * sqrt(0.5);

  double xpn = rotationMatrices[trans][0]*xp + rotationMatrices[trans][1]*yp + rotationMatrices[trans][2]*zp;
  double ypn = rotationMatrices[trans][3]*xp + rotationMatrices[trans][4]*yp + rotationMatrices[trans][5]*zp;
  double zpn = rotationMatrices[trans][6]*xp + rotationMatrices[trans][7]*yp + rotationMatrices[trans][8]*zp;

  xpn /= sqrt(0.5);
  ypn /= sqrt(0.5);
  zpn /= sqrt(0.5);

  int xn = (int)round(xpn);
  int yn = (int)round(ypn);
  int zn = (int)round(zpn);

  // is should fall on a valid grid position
  bt_assert((fabs(xpn-xn) < 0.01) && (fabs(ypn-yn) < 0.01) && (fabs(zpn-zn) < 0.01));
  bt_assert((((xn+yn+zn) & 1) == 0));

  *x = xn;
  *y = yn;
  *z = zn;
}

bool voxel_2_c::getNeighbor(unsigned int idx, unsigned int typ, int x, int y, int z, int * xn, int *yn, int *zn) const {

  // spheres have only one type of neighbour
  switch (typ) {
    case 0:
      switch (idx) {
        case  0: *xn = x-1; *yn = y-1; *zn = z;   break;
        case  1: *xn = x-1; *yn = y+1; *zn = z;   break;
        case  2: *xn = x+1; *yn = y-1; *zn = z;   break;
        case  3: *xn = x+1; *yn = y+1; *zn = z;   break;
        case  4: *xn = x-1; *yn = y;   *zn = z-1; break;
        case  5: *xn = x-1; *yn = y;   *zn = z+1; break;
        case  6: *xn = x+1; *yn = y;   *zn = z-1; break;
        case  7: *xn = x+1; *yn = y;   *zn = z+1; break;
        case  8: *xn = x;   *yn = y-1; *zn = z-1; break;
        case  9: *xn = x;   *yn = y-1; *zn = z+1; break;
        case 10: *xn = x;   *yn = y+1; *zn = z-1; break;
        case 11: *xn = x;   *yn = y+1; *zn = z+1; break;
        default: return false;
      }
      break;
    default:
      return false;
  }
  return true;
}

