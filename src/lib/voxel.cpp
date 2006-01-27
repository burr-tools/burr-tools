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


#include "puzzle.h"

#include "symmetries.h"

/** @ file
 * The Implementation of the voxel space class.
 */

#include <string.h>
#include <stdio.h>

#include <xmlwrapp/attributes.h>

using namespace std;

voxel_c::voxel_c(unsigned int x, unsigned int y, unsigned int z, voxel_type init, voxel_type outs) : sx(x), sy(y), sz(z), voxels(x*y*z), outside(outs), hx(0), hy(0), hz(0), name(0) {

  space = new voxel_type[voxels];
  bt_assert(space);

  memset(space, init, voxels);

  if (init == 0)
    bx1 = bx2 = by1 = by2 = bz1 = bz2 = 0;
  else {
    bx1 = by1 = bz1 = 0;
    bx2 = x-1;
    by2 = y-1;
    bz2 = z-1;
  }

  doRecalc = true;

  symmetries = symmetryInvalid();
}

voxel_c::voxel_c(const voxel_c & orig, unsigned int transformation) : sx(orig.sx), sy(orig.sy), sz(orig.sz),
voxels(orig.voxels), hx(orig.hx), hy(orig.hy), hz(orig.hz), name(0) {

  space = new voxel_type[voxels];
  bt_assert(space);

  memcpy(space, orig.space, voxels);

  bx1 = orig.bx1;
  bx2 = orig.bx2;
  by1 = orig.by1;
  by2 = orig.by2;
  bz1 = orig.bz1;
  bz2 = orig.bz2;

  outside = orig.outside;

  doRecalc = true;

  transform(transformation);

  symmetries = symmetryInvalid();
}

voxel_c::voxel_c(const voxel_c * orig, unsigned int transformation) : sx(orig->sx), sy(orig->sy), sz(orig->sz),
voxels(orig->voxels), hx(orig->hx), hy(orig->hy), hz(orig->hz), name(0) {

  space = new voxel_type[voxels];
  bt_assert(space);

  memcpy(space, orig->space, voxels);

  bx1 = orig->bx1;
  bx2 = orig->bx2;
  by1 = orig->by1;
  by2 = orig->by2;
  bz1 = orig->bz1;
  bz2 = orig->bz2;

  outside = orig->outside;

  doRecalc = true;

  transform(transformation);

  symmetries = symmetryInvalid();
}

voxel_c::~voxel_c() {
  delete [] space;

  if (name) delete [] name;
}

void voxel_c::recalcBoundingBox(void) {

  if (!doRecalc)
    return;

  bx1 = by1 = bz1 = sx+sy+sz;
  bx2 = by2 = bz2 = 0;

  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++)
        if (get(x, y, z) != outside) {
          if (x < bx1) bx1 = x;
          if (x > bx2) bx2 = x;

          if (y < by1) by1 = y;
          if (y > by2) by2 = y;

          if (z < bz1) bz1 = z;
          if (z > bz2) bz2 = z;
        }
}


bool voxel_c::operator ==(const voxel_c & op) const {

  if (sx != op.sx) return false;
  if (sy != op.sy) return false;
  if (sz != op.sz) return false;

  for (unsigned int i = 0; i < voxels; i++)
    if (space[i] != op.space[i])
      return false;

  return true;
}

bool voxel_c::identicalInBB(const voxel_c * op) const {

  if (bx2-bx1 != op->bx2-op->bx1) return false;
  if (by2-by1 != op->by2-op->by1) return false;
  if (bz2-bz1 != op->bz2-op->bz1) return false;

  for (unsigned int x = bx1; x <= bx2; x++)
    for (unsigned int y = by1; y <= by2; y++)
      for (unsigned int z = bz1; z <= bz2; z++)
        if (get(x, y, z) != op->get(x-bx1+op->bx1, y-by1+op->by1, z-bz1+op->bz1))
          return false;

  return true;
}



void voxel_c::rotatex(int by) {

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

void voxel_c::rotatey(int by) {

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

void voxel_c::rotatez(int by) {

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

void voxel_c::getHotspot(unsigned char trans, int * x, int * y, int * z) const {

  int tx = hx;
  int ty = hy;
  int tz = hz;
  int tsx = sx;
  int tsy = sy;
  int tsz = sz;
  int t;

  if (trans >= NUM_TRANSFORMATIONS) {
    tx = sx - 1 - tx;
    trans -= NUM_TRANSFORMATIONS;
  }

  for (int i = 0; i < rotx(trans); i++) {
    t = ty;
    ty = tsz - 1 - tz;
    tz = t;
    t = tsy;
    tsy = tsz;
    tsz = t;
  }

  for (int i = 0; i < roty(trans); i++) {
    t = tx;
    tx = tsz - 1 - tz;
    tz = t;
    t = tsx;
    tsx = tsz;
    tsz = t;
  }

  for (int i = 0; i < rotz(trans); i++) {
    t = ty;
    ty = tx;
    tx = tsy - 1 - t;
    t = tsx;
    tsx = tsy;
    tsy = t;
  }

  *x = tx;
  *y = ty;
  *z = tz;
}


void voxel_c::minimize(voxel_type val) {

  unsigned int x1, x2, y1, y2, z1, z2;

  x1 = y1 = z1 = sx+sy+sz;
  x2 = y2 = z2 = 0;

  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++)
        if (get(x, y, z) != val) {
          if (x < x1) x1 = x;
          if (x > x2) x2 = x;

          if (y < y1) y1 = y;
          if (y > y2) y2 = y;

          if (z < z1) z1 = z;
          if (z > z2) z2 = z;
        }

  // check for empty cube and do nothing in that case
  if (x1 > x2)
    return;

  if ((x1 != 0) || (y1 != 0) || (z1 != 0) || (x2 != sx-1) || (y2 != sy-1) || (z2 != sz-1)) {
    if (x1 > x2) {
      sx = sy = sz = 0;
      if (space) {
        delete [] space;
        space = 0;
      }
    }

    voxel_type * s2 = new voxel_type[(x2-x1+1) * (y2-y1+1) * (z2-z1+1)];

    for (unsigned int x = x1; x <= x2; x++)
      for (unsigned int y = y1; y <= y2; y++)
        for (unsigned int z = z1; z <= z2; z++)
          s2[(x-x1) + (x2-x1+1) * ((y-y1) + (y2-y1+1) * (z-z1))] = get(x, y, z);

    delete [] space;
    space = s2;

    sx = x2-x1+1;
    sy = y2-y1+1;
    sz = z2-z1+1;
    voxels = sx*sy*sz;

    recalcBoundingBox();
    hx -= x1;
    hy -= y1;
    hz -= z1;
  }
}

void voxel_c::resize(unsigned int nsx, unsigned int nsy, unsigned int nsz, voxel_type filler) {
  voxel_type * s2 = new voxel_type[nsx*nsy*nsz];
  memset(s2, filler, nsx*nsy*nsz);

  unsigned int mx = (sx < nsx) ? sx : nsx;
  unsigned int my = (sy < nsy) ? sy : nsy;
  unsigned int mz = (sz < nsz) ? sz : nsz;

  for (unsigned int x = 0; x < mx; x++)
    for (unsigned int y = 0; y < my; y++)
      for (unsigned int z = 0; z < mz; z++)
        s2[x + nsx * (y + nsy * z)] = get(x, y, z);

  delete [] space;
  space = s2;

  sx = nsx;
  sy = nsy;
  sz = nsz;
  voxels = sx*sy*sz;

  recalcBoundingBox();
}

unsigned int voxel_c::count(voxel_type val) const {
  unsigned int count = 0;
  for (unsigned int i = 0; i < getXYZ(); i++)
    if (get(i) == val)
      count ++;
  return count;
}

unsigned int voxel_c::countState(int state) const {
  unsigned int count = 0;
  for (unsigned int i = 0; i < getXYZ(); i++)
    if ((get(i) & 3) == state)
      count ++;
  return count;
}

void voxel_c::mirrorX(void) {

  doRecalc = false;

  for (unsigned int x = 0; x < sx/2; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++) {
        voxel_type tmp = get(x, y, z);
        set(x, y, z, get(sx-x-1, y, z));
        set(sx-x-1, y, z, tmp);
      }

  doRecalc = true;

  unsigned int t = bx1;

  bx1 = sx - 1 - bx2;
  bx2 = sx - 1 - t;

  hx = sx - 1 - hx;

  symmetries = symmetryInvalid();
}

void voxel_c::mirrorY(void) {

  doRecalc = false;

  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy/2; y++)
      for (unsigned int z = 0; z < sz; z++) {
        voxel_type tmp = get(x, y, z);
        set(x, y, z, get(x, sy-y-1, z));
        set(x, sy-y-1, z, tmp);
      }

  doRecalc = true;

  unsigned int t = by1;

  by1 = sy - 1 - by2;
  by2 = sy - 1 - t;

  hy = sy - 1 - hy;

  symmetries = symmetryInvalid();
}

void voxel_c::mirrorZ(void) {

  doRecalc = false;

  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz/2; z++) {
        voxel_type tmp = get(x, y, z);
        set(x, y, z, get(x, y, sz-z-1));
        set(x, y, sz-z-1, tmp);
      }

  doRecalc = true;

  unsigned int t = bz1;

  bz1 = sz - 1 - bz2;
  bz2 = sz - 1 - t;

  hz = sz - 1 - sz;

  symmetries = symmetryInvalid();
}

void voxel_c::translate(int dx, int dy, int dz, voxel_type filler) {
  voxel_type * s2 = new voxel_type[sx*sy*sz];
  memset(s2, filler, sx*sy*sz);

  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++)
        if ((x+dx >= 0) && (x+dx < sx) &&
            (y+dy >= 0) && (y+dy < sy) &&
            (z+dz >= 0) && (z+dz < sz))
          s2[(x+dx)+sx*((y+dy)+sy*(z+dz))] = get(x, y, z);

  delete [] space;
  space = s2;

  if (dx + bx1 <= 0) bx1 = 0; else if (dx + bx1 >= sx) bx1 = sx-1; else bx1 += dx;
  if (dy + by1 <= 0) by1 = 0; else if (dy + by1 >= sy) by1 = sy-1; else by1 += dy;
  if (dz + bz1 <= 0) bz1 = 0; else if (dz + bz1 >= sz) bz1 = sz-1; else bz1 += dz;

  if (dx + bx2 <= 0) bx2 = 0; else if (dx + bx2 >= sx) bx2 = sx-1; else bx2 += dx;
  if (dy + by2 <= 0) by2 = 0; else if (dy + by2 >= sy) by2 = sy-1; else by2 += dy;
  if (dz + bz2 <= 0) bz2 = 0; else if (dz + bz2 >= sz) bz2 = sz-1; else bz2 += dz;

  hx += dx;
  hy += dy;
  hz += dz;
}

bool voxel_c::connected(char type, bool inverse, voxel_type value) const {

  /* union find algorithm:
   * 1. put all voxels that matter in an own set
   * 2. unify all sets whose voxels are neibors
   * 3. check if all voxels are in the same set
   */

  /* allocate enough space for all voxels */
  int * tree = new int[voxels];


  /* initialize tree */
  for (unsigned int i = 0; i < voxels; i++)
    tree[i] = 0;

  /* merge all neigboring voxels */
  for (unsigned int x = 1; x < sx; x++)
    for (unsigned int y = 1; y < sy; y++)
      for (unsigned int z = 1; z < sz; z++)
        if ((inverse && (get(x, y, z) != value)) ||
            (!inverse && (get(x, y, z) == value))) {

          int root1 = x+sx*(y+sy*z);
          while (tree[root1]) root1 = tree[root1];


          if ((inverse && (get(x-1, y, z) != value)) ||
              (!inverse && (get(x-1, y, z) == value))) {

            int root2 = (x-1)+sx*(y+sy*z);
            while (tree[root2]) root2 = tree[root2];

            if (root1 != root2) tree[root2] = root1;
          }

          if ((inverse && (get(x, y-1, z) != value)) ||
              (!inverse && (get(x, y-1, z) == value))) {

            int root2 = x+sx*((y-1)+sy*z);
            while (tree[root2]) root2 = tree[root2];

            if (root1 != root2) tree[root2] = root1;
          }

          if ((inverse && (get(x, y, z-1) != value)) ||
              (!inverse && (get(x, y, z-1) == value))) {

            int root2 = x+sx*(y+sy*(z-1));
            while (tree[root2]) root2 = tree[root2];

            if (root1 != root2) tree[root2] = root1;
          }

          if (type > 0) {
            if ((inverse && (get(x-1, y, z-1) != value)) ||
                (!inverse && (get(x-1, y, z-1) == value))) {

              int root2 = (x-1)+sx*(y+sy*(z-1));
              while (tree[root2]) root2 = tree[root2];

              if (root1 != root2) tree[root2] = root1;
            }

            if ((inverse && (get(x-1, y-1, z) != value)) ||
                (!inverse && (get(x-1, y-1, z) == value))) {

              int root2 = (x-1)+sx*((y-1)+sy*z);
              while (tree[root2]) root2 = tree[root2];

              if (root1 != root2) tree[root2] = root1;
            }

            if ((inverse && (get(x, y-1, z-1) != value)) ||
                (!inverse && (get(x, y-1, z+1) == value))) {

              int root2 = x+sx*((y-1)+sy*(z-1));
              while (tree[root2]) root2 = tree[root2];

              if (root1 != root2) tree[root2] = root1;
            }
          }

          if (type > 1) {
            if ((inverse && (get(x-1, y-1, z-1) != value)) ||
                (!inverse && (get(x-1, y-1, z-1) == value))) {

              int root2 = (x-1)+sx*(y+sy*(z-1));
              while (tree[root2]) root2 = tree[root2];

              if (root1 != root2) tree[root2] = root1;
            }
          }
        }


  int root = -1;

  /* finally check, if all voxels are in the same set */
  { for (unsigned int x = 1; x < sx; x++)
      for (unsigned int y = 1; y < sy; y++)
        for (unsigned int z = 1; z < sz; z++)
          if ((inverse && (get(x, y, z) != value)) ||
              (!inverse && (get(x, y, z) == value))) {
            if (root == -1) {
              root = x+sx*(y+sy*z);
              while (tree[root]) root = tree[root];

            } else {

              int root2 = x+sx*(y+sy*z);
              while (tree[root2]) root2 = tree[root2];

              if (root2 != root) {
                delete [] tree;
                return false;
              }
            }
          }
  }

  delete [] tree;
  return true;
}

void voxel_c::copy(const voxel_c * orig) {

  delete [] space;

  space = new voxel_type [orig->getXYZ()];

  memcpy(space, orig->space, orig->getXYZ());

  sx = orig->sx;
  sy = orig->sy;
  sz = orig->sz;

  voxels = orig->voxels;

  bx1 = orig->bx1;
  bx2 = orig->bx2;
  by1 = orig->by1;
  by2 = orig->by2;
  bz1 = orig->bz1;
  bz2 = orig->bz2;

  symmetries = orig->symmetries;

  // we don't copy the name intentionally because the name is supposed to
  // be unique
  if (name) delete [] name;
  name = 0;
}

bool voxel_c::neighbour(unsigned int p, voxel_type val) const {

  unsigned int x = p % sx;
  unsigned int y = ((p - x) / sx) % sy;
  unsigned int z = (((p - x) / sx) - y) / sy;

  bt_assert(x + sx * (y + sy * z) == p);

  if ((x > 0   ) && (space[p-1] == val)) return true;
  if ((x < sx-1) && (space[p+1] == val)) return true;

  if ((y > 0   ) && (space[p-sx] == val)) return true;
  if ((y < sy-1) && (space[p+sx] == val)) return true;

  if ((z > 0   ) && (space[p-sx*sy] == val)) return true;
  if ((z < sz-1) && (space[p+sx*sy] == val)) return true;

  return false;
}

void voxel_c::transform(unsigned int nr) {

  bt_assert(nr < NUM_TRANSFORMATIONS_MIRROR);

  if (nr >= NUM_TRANSFORMATIONS) {
    mirrorX();
    nr -= NUM_TRANSFORMATIONS;
  }

  rotatex(rotx(nr));
  rotatey(roty(nr));
  rotatez(rotz(nr));
}

symmetries_t voxel_c::selfSymmetries(void) const {

  if (isSymmetryInvalid(symmetries)) {

    ((voxel_c*)this)->symmetries = symmetryCalcuation(this);
  }

  return symmetries;
}

void voxel_c::minimizePiece(void) {

  unsigned int x1, x2, y1, y2, z1, z2;

  x1 = y1 = z1 = getXYZ();
  x2 = y2 = z2 = 0;

  for (unsigned int x = 0; x < getX(); x++)
    for (unsigned int y = 0; y < getY(); y++)
      for (unsigned int z = 0; z < getZ(); z++)
        if (getState(x, y, z) != VX_EMPTY) {
          if (x < x1) x1 = x;
          if (x > x2) x2 = x;

          if (y < y1) y1 = y;
          if (y > y2) y2 = y;

          if (z < z1) z1 = z;
          if (z > z2) z2 = z;
        }

  // check for empty cube and do nothing in that case
  if (x1 > x2)
    return;

  if ((x1 != 0) || (y1 != 0) || (z1 != 0) || (x2 != getX()-1) || (y2 != getY()-1) || (z2 != getZ()-1)) {

    translate(-x1, -y1, -z1, 0);
    resize(x2-x1+1, y2-y1+1, z2-z1+1, 0);
  }
}

void voxel_c::actionOnSpace(int action, bool inside) {

  if (!getX() || !getY() ||!getZ())
    return;

  for (unsigned int x = 0; x < getX(); x++)
    for (unsigned int y = 0; y < getY(); y++)
      for (unsigned int z = 0; z < getZ(); z++)
        if (getState(x, y, z) != VX_EMPTY) {
          if (inside ^
              ((getState2(x-1, y, z) == VX_EMPTY) || (getState2(x+1, y, z) == VX_EMPTY) ||
               (getState2(x, y-1, z) == VX_EMPTY) || (getState2(x, y+1, z) == VX_EMPTY) ||
               (getState2(x, y, z-1) == VX_EMPTY) || (getState2(x, y, z+1) == VX_EMPTY)
              )
             )
            switch(action) {
              case ACT_FIXED: setState(x, y, z, VX_FILLED); break;
              case ACT_VARIABLE: setState(x, y, z, VX_VARIABLE); break;
              case ACT_DECOLOR: setColor(x, y, z, 0); break;
            }
        }
}

xml::node voxel_c::save(void) const {

  xml::node nd("voxel");

  char tmp[50];

  snprintf(tmp, 50, "%i", sx);
  nd.get_attributes().insert("x", tmp);

  snprintf(tmp, 50, "%i", sy);
  nd.get_attributes().insert("y", tmp);

  snprintf(tmp, 50, "%i", sz);
  nd.get_attributes().insert("z", tmp);

  /* save the hotspot, but only when it is not zero */
  if (hx) {
    snprintf(tmp, 50, "%i", hx);
    nd.get_attributes().insert("hx", tmp);
  }
  if (hy) {
    snprintf(tmp, 50, "%i", hy);
    nd.get_attributes().insert("hy", tmp);
  }
  if (hz) {
    snprintf(tmp, 50, "%i", hz);
    nd.get_attributes().insert("hz", tmp);
  }

  if (name)
    nd.get_attributes().insert("name", name);

  // this might allow us to later add another format
  nd.get_attributes().insert("type", "0");

  std::string cont;

  for (unsigned int i = 0; i < getXYZ(); i++)
    switch(getState(i)) {
    case VX_EMPTY:
      cont += "_";
      break;
    case VX_VARIABLE:
      cont += "+";
      if (getColor(i)) {
        bt_assert(getColor(i) < 100);
        if (getColor(i) > 9)
          cont += ('0' + (getColor(i) / 10));
        cont += ('0' + (getColor(i) % 10));
      }
      break;
    case VX_FILLED:
      cont += "#";
      if (getColor(i)) {
        bt_assert(getColor(i) < 100);
        if (getColor(i) > 9)
          cont += ('0' + (getColor(i) / 10));
        cont += ('0' + (getColor(i) % 10));
      }
      break;
    }

  nd.set_content(cont.c_str());

  return nd;
}

voxel_c::voxel_c(const xml::node & node) : hx(0), hy(0), hz(0), name(0) {

  // we must have a real node and the following attributes
  if ((node.get_type() != xml::node::type_element) ||
      (strcmp(node.get_name(), "voxel") != 0))
    throw load_error("not the right type of node for a voxel space", node);

  if (node.get_attributes().find("x") == node.get_attributes().end())
    throw load_error("piece Voxel with no attribut 'x' encountered", node);
  if (node.get_attributes().find("y") == node.get_attributes().end())
    throw load_error("piece Voxel with no attribut 'y' encountered", node);
  if (node.get_attributes().find("z") == node.get_attributes().end())
    throw load_error("piece Voxel with no attribut 'z' encountered", node);
  if (node.get_attributes().find("type") == node.get_attributes().end())
    throw load_error("piece Voxel with no attribut 'type' encountered", node);

  skipRecalcBoundingBox(true);

  // set to the correct size
  sx = atoi(node.get_attributes().find("x")->get_value());
  sy = atoi(node.get_attributes().find("y")->get_value());
  sz = atoi(node.get_attributes().find("z")->get_value());
  voxels = sx*sy*sz;

  if (node.get_attributes().find("hx") != node.get_attributes().end())
    hx = atoi(node.get_attributes().find("hx")->get_value());
  if (node.get_attributes().find("hy") != node.get_attributes().end())
    hy = atoi(node.get_attributes().find("hy")->get_value());
  if (node.get_attributes().find("hz") != node.get_attributes().end())
    hz = atoi(node.get_attributes().find("hz")->get_value());
  if (node.get_attributes().find("name") != node.get_attributes().end()) {
    int i = strlen(node.get_attributes().find("name")->get_value());
    // we don't use names longer than 1000 characters that should be more
    // than enough and will prevent allocating too long strings
    if (i > 1000) i = 1000;
    name = new char[i+1];
    strncpy(name, node.get_attributes().find("name")->get_value(), i+1);
  }

  space = new voxel_type[voxels];
  bt_assert(space);

  unsigned int type = atoi(node.get_attributes().find("type")->get_value());

  const char * c = node.get_content();
  unsigned int idx = 0;
  unsigned int color = 0;

  if (c) {

    if (type != 0)
      throw load_error("piece Voxel with type not equal to 0 encountetred", node);

    while (*c) {
      switch(*c) {
      case '#':
        setState(idx++, VX_FILLED);
        color = 0;
        break;
      case '+':
        setState(idx++, VX_VARIABLE);
        color = 0;
        break;
      case '_':
        setState(idx++, VX_EMPTY);
        color = 0;
        break;
      case '0': color = color * 10 + 0; break;
      case '1': color = color * 10 + 1; break;
      case '2': color = color * 10 + 2; break;
      case '3': color = color * 10 + 3; break;
      case '4': color = color * 10 + 4; break;
      case '5': color = color * 10 + 5; break;
      case '6': color = color * 10 + 6; break;
      case '7': color = color * 10 + 7; break;
      case '8': color = color * 10 + 8; break;
      case '9': color = color * 10 + 9; break;
      default:
        throw load_error("unrecognized character in piece voxel space", node);
      }

      if (idx > 0)
        setColor(idx-1, color);

      c++;
      if (idx > getXYZ())
        throw load_error("too many voxels defined in voxelspace", node);
    }
    if (idx < getXYZ())
      throw load_error("not enough voxels defined in voxelspace", node);
  }

  setOutside(VX_EMPTY);
  skipRecalcBoundingBox(false);
}


void voxel_c::setName(const char * n) {

  /* delete the old name */
  if (name)
    delete [] name;
  name = 0;

  if (n && n[0]) {
    name = new char [strlen(n)+1];
    strcpy(name, n);
  }
}
