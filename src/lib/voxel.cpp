/* Burr Solver
 * Copyright (C) 2003-2005  Andreas Röver
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

using namespace std;

voxel_c::voxel_c(int x, int y, int z, voxel_type init) : sx(x), sy(y), sz(z), voxels(x*y*z) {

  space = new voxel_type[voxels];
  assert(space);

  memset(space, init, voxels);
}

voxel_c::voxel_c(const voxel_c & orig, unsigned int transformation) : sx(orig.sx), sy(orig.sy), sz(orig.sz), voxels(orig.voxels) {

  space = new voxel_type[voxels];
  assert(space);

  memcpy(space, orig.space, voxels);

  transform(transformation);
}

voxel_c::voxel_c(const voxel_c * orig, unsigned int transformation) : sx(orig->sx), sy(orig->sy), sz(orig->sz), voxels(orig->voxels) {

  space = new voxel_type[voxels];
  assert(space);

  memcpy(space, orig->space, voxels);

  transform(transformation);
}

voxel_c::voxel_c(istream * str) {

  *str >> sx >> sy >> sz;

  voxels = sx*sy*sz;

  if ((sx < 0) || (sy < 0) || (sz < 0) || (voxels > 1000000))
    throw load_error("swong size for voxel space");

  space = new voxel_type[voxels];
  assert(space);

  char c;

  for (int j = 0; j < voxels; j++) {
    *str >> c;

    switch (c) {
    case '_': set(j, 0); break;
    case '#': set(j, 1); break;
    case '+': set(j, 2); break;
    case '\n':
      while (j < voxels) {
        set(j, 0);
        j++;
      }
      return;
    default:
      throw load_error("not allowed character in voxel space definition");
    }
  }
}

voxel_c::~voxel_c() {
  delete [] space;
}

void voxel_c::print(char base) const {
  for (int z = 0; z < sz; z++) {
    printf(" +");
    for (int x = 0; x < sx; x++)
      printf("-");
    printf("+");
  }
  printf("\n");

  for (int y = 0; y < sy; y++) {
    for (int z = 0; z < sz; z++) {
      printf(" +");
      for (int x = 0; x < sx; x++)
        if (get(x, y, z) != 0)
          printf("%c", base + get(x, y, z)-1);
        else
          printf(" ");
      printf("+");
    }
    printf("\n");
  }

  { for (int z = 0; z < sz; z++) {
      printf(" +");
      for (int x = 0; x < sx; x++)
        printf("-");
      printf("+");
    }
  }
  printf("\n");
}

void assemblyVoxel_c::print(void) const {
  for (int z = 0; z < getZ(); z++) {
    printf(" +");
    for (int x = 0; x < getX(); x++)
      printf("-");
    printf("+");
  }
  printf("\n");

  for (int y = 0; y < getY(); y++) {
    for (int z = 0; z < getZ(); z++) {
      printf(" +");
      for (int x = 0; x < getX(); x++)
        if (get(x, y, z) != VX_EMPTY)
          printf("%c", 'a' + get(x, y, z));
        else
          printf(" ");
      printf("+");
    }
    printf("\n");
  }

  { for (int z = 0; z < getZ(); z++) {
      printf(" +");
      for (int x = 0; x < getX(); x++)
        printf("-");
      printf("+");
    }
  }
  printf("\n");
}

void voxel_c::save(ostream * str) const {

  static unsigned char VoxelTranslate[] = "_#+";

  *str << sx << " " << sy << " " << sz << " ";

  for (int j = 0; j < voxels; j++)
    *str << VoxelTranslate[get(j)];

  *str << endl;
}


bool voxel_c::operator ==(const voxel_c & op) const {

  if (sx != op.sx) return false;
  if (sy != op.sy) return false;
  if (sz != op.sz) return false;

  for (int i = 0; i < voxels; i++)
    if (space[i] != op.space[i])
      return false;

  return true;
}


void voxel_c::rotatex() {

  int tmp = sy;
  sy = sz;
  sz = tmp;

  voxel_type *s = new voxel_type[voxels];

  for (int x = 0; x < sx; x++)
    for (int y = 0; y < sy; y++)
      for (int z = 0; z < sz; z++)
        s[x+sx*(y+sy*z)] = space[x+sx*(z+sz*(sy-y-1))];

  delete [] space;
  space = s;
}

void voxel_c::rotatey() {

  int tmp = sx;
  sx = sz;
  sz = tmp;

  voxel_type *s = new voxel_type[voxels];

  for (int x = 0; x < sx; x++)
    for (int y = 0; y < sy; y++)
      for (int z = 0; z < sz; z++)
        s[x+sx*(y+sy*z)] = space[z+sz*(y+sy*(sx-x-1))];

  delete [] space;
  space = s;
}

void voxel_c::rotatez() {

  int tmp = sy;
  sy = sx;
  sx = tmp;

  voxel_type *s = new voxel_type[voxels];

  for (int x = 0; x < sx; x++)
    for (int y = 0; y < sy; y++)
      for (int z = 0; z < sz; z++)
        s[x+sx*(y+sy*z)] = space[y+sy*((sx-x-1)+sx*z)];

  delete [] space;
  space = s;
}

void voxel_c::minimize(voxel_type val) {

  int x1, x2, y1, y2, z1, z2;

  x1 = y1 = z1 = sx+sy+sz;
  x2 = y2 = z2 = 0;

  for (int x = 0; x < sx; x++)
    for (int y = 0; y < sy; y++)
      for (int z = 0; z < sz; z++)
        if (get(x, y, z) != val) {
          if (x < x1) x1 = x;
          if (x > x2) x2 = x;

          if (y < y1) y1 = y;
          if (y > y2) y2 = y;

          if (z < z1) z1 = z;
          if (z > z2) z2 = z;
        }

  if ((x1 != 0) || (y1 != 0) || (z1 != 0) || (x2 != sx-1) || (y2 != sy-1) || (z2 != sz-1)) {
    if (x1 > x2) {
      sx = sy = sz = 0;
      if (space) {
        delete [] space;
        space = 0;
      }
    }

    voxel_type * s2 = new voxel_type[(x2-x1+1) * (y2-y1+1) * (z2-z1+1)];

    for (int x = x1; x <= x2; x++)
      for (int y = y1; y <= y2; y++)
        for (int z = z1; z <= z2; z++)
          s2[(x-x1) + (x2-x1+1) * ((y-y1) + (y2-y1+1) * (z-z1))] = get(x, y, z);

    delete [] space;
    space = s2;

    sx = x2-x1+1;
    sy = y2-y1+1;
    sz = z2-z1+1;
    voxels = sx*sy*sz;
  }
}

void voxel_c::resize(int nsx, int nsy, int nsz, voxel_type filler) {
  voxel_type * s2 = new voxel_type[nsx*nsy*nsz];
  memset(s2, filler, nsx*nsy*nsz);

  int mx = (sx < nsx) ? sx : nsx;
  int my = (sy < nsy) ? sy : nsy;
  int mz = (sz < nsz) ? sz : nsz;

  for (int x = 0; x < mx; x++)
    for (int y = 0; y < my; y++)
      for (int z = 0; z < mz; z++)
        s2[x + nsx * (y + nsy * z)] = get(x, y, z);

  delete [] space;
  space = s2;

  sx = nsx;
  sy = nsy;
  sz = nsz;
  voxels = sx*sy*sz;
}

unsigned int voxel_c::count(voxel_type val) const {
  unsigned int count = 0;
  for (int i = 0; i < getXYZ(); i++)
    if (get(i) == val)
      count ++;
  return count;
}

unsigned int pieceVoxel_c::countState(int state) const {
  unsigned int count = 0;
  for (int i = 0; i < getXYZ(); i++)
    if ((get(i) & 3) == state)
      count ++;
  return count;
}

void voxel_c::mirrorX(void) {

  for (int x = 0; x < sx/2; x++)
    for (int y = 0; y < sy; y++)
      for (int z = 0; z < sz; z++) {
        voxel_type tmp = get(x, y, z);
        set(x, y, z, get(sx-x-1, y, z));
        set(sx-x-1, y, z, tmp);
      }

}

void voxel_c::mirrorY(void) {
  for (int x = 0; x < sx; x++)
    for (int y = 0; y < sy/2; y++)
      for (int z = 0; z < sz; z++) {
        voxel_type tmp = get(x, y, z);
        set(x, y, z, get(x, sy-y-1, z));
        set(x, sy-y-1, z, tmp);
      }
}

void voxel_c::mirrorZ(void) {
  for (int x = 0; x < sx; x++)
    for (int y = 0; y < sy; y++)
      for (int z = 0; z < sz/2; z++) {
        voxel_type tmp = get(x, y, z);
        set(x, y, z, get(x, y, sz-z-1));
        set(x, y, sz-z-1, tmp);
      }
}

void voxel_c::translate(int dx, int dy, int dz, voxel_type filler) {
  voxel_type * s2 = new voxel_type[sx*sy*sz];
  memset(s2, filler, sx*sy*sz);

  for (int x = 0; x < sx; x++)
    for (int y = 0; y < sy; y++)
      for (int z = 0; z < sz; z++)
        if ((x+dx >= 0) && (x+dx < sx) &&
            (y+dy >= 0) && (y+dy < sy) &&
            (z+dz >= 0) && (z+dz < sz))
          s2[(x+dx)+sx*((y+dy)+sy*(z+dz))] = get(x, y, z);

  delete [] space;
  space = s2;
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
  for (int i = 0; i < voxels; i++)
    tree[i] = 0;

  /* merge all neigboring voxels */
  for (int x = 1; x < sx; x++)
    for (int y = 1; y < sy; y++)
      for (int z = 1; z < sz; z++)
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
  { for (int x = 1; x < sx; x++)
      for (int y = 1; y < sy; y++)
        for (int z = 1; z < sz; z++)
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
}

bool voxel_c::neighbour(int p, voxel_type val) const {

  int x = p % sx;
  int y = ((p - x) / sx) % sy;
  int z = (((p - x) / sx) - y) / sy;

  assert(x + sx * (y + sy * z) == p);

  if ((x > 0   ) && (space[p-1] == val)) return true;
  if ((x < sx-1) && (space[p+1] == val)) return true;

  if ((y > 0   ) && (space[p-sx] == val)) return true;
  if ((y < sy-1) && (space[p+sx] == val)) return true;

  if ((z > 0   ) && (space[p-sx*sy] == val)) return true;
  if ((z < sz-1) && (space[p+sx*sy] == val)) return true;

  return false;
}

void voxel_c::transform(unsigned int nr) {

  assert(nr < 48);

  int i;

  if (nr >= 24) {
    mirrorX();
    nr -= 24;
  }

  for (i = 0; i < rotx(nr); i++) rotatex();
  for (i = 0; i < roty(nr); i++) rotatey();
  for (i = 0; i < rotz(nr); i++) rotatez();
}

symmetries_t voxel_c::selfSymmetries(void) const {

  symmetries_t result = 1;

  for (int i = 1; i < 48; i++) {

    voxel_c * rot = new voxel_c(this, i);

    if (*rot == *this)
      result |= ((symmetries_t)1 << i);

    delete rot;
  }

  return result;
}

void pieceVoxel_c::minimizePiece(void) {

  unsigned int x1, x2, y1, y2, z1, z2;

  x1 = y1 = z1 = getXYZ();
  x2 = y2 = z2 = 0;

  for (int x = 0; x < getX(); x++)
    for (int y = 0; y < getY(); y++)
      for (int z = 0; z < getZ(); z++)
        if (getState(x, y, z) != VX_EMPTY) {
          if (x < x1) x1 = x;
          if (x > x2) x2 = x;

          if (y < y1) y1 = y;
          if (y > y2) y2 = y;

          if (z < z1) z1 = z;
          if (z > z2) z2 = z;
        }

  if ((x1 != 0) || (y1 != 0) || (z1 != 0) || (x2 != getX()-1) || (y2 != getY()-1) || (z2 != getZ()-1)) {

    translate(-x1, -y1, -z1, 0);
    resize(x2-x1+1, y2-y1+1, z2-z1+1, 0);
  }
}


void pieceVoxel_c::makeInsideHoly(void) {

  for (int x = 1; x < getX()-1; x++)
    for (int y = 1; y < getY()-1; y++)
      for (int z = 1; z < getZ()-1; z++)
        if (getState(x, y, z) != VX_EMPTY) {
          if ((getState(x-1, y, z) == VX_EMPTY) || (getState(x+1, y, z) == VX_EMPTY) ||
              (getState(x, y-1, z) == VX_EMPTY) || (getState(x, y+1, z) == VX_EMPTY) ||
              (getState(x, y, z-1) == VX_EMPTY) || (getState(x, y, z+1) == VX_EMPTY))
            setState(x, y, z, VX_FILLED);
          else
            setState(x, y, z, VX_VARIABLE);
        }
}

