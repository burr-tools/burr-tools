/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
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

#include "voxel.h"

#include "../tools/xml.h"

#include "../halfedge/polyhedron.h"
#include "../halfedge/vector3.h"
#include "../halfedge/modifiers.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/** \mainpage The BurrTools Library documentation
 *
 * \section Introduction
 *
 * This documentation tries to achieve two goals: it tries to explain how to
 * use the BurrTools Library and it also explains the algorithms used and generally
 * how things are implemented and done inside the library.
 *
 * I hope this will help everyone to understand the inner workings of this
 * complex piece of software.
 *
 * To get started it is best to begin reading the userguide. This document will explain
 * basic concepts of the whole software, even though this is done in a GUI centric way.
 * Next read the information for the voxel voxel_c classes.
 * Then continue to the assembly assembly_c. The assemblers assembler_c are probably not
 * necessary to read. They contain too much complicated stuff. Then read the disassembly
 * disassembly_c class documentation. Finally the grid type gridType_c class will
 * glue things together.
 *
 * You should now be able to use the library.
 *
 * If you want to understand how things work you will need to read the documentation for the
 * assembler \ref assembler_c and disassembler \ref disassembler_c classes.
 */

/** \file voxel.cpp
 * Contains the implementation for the voxel base class
 */

/** \class voxel_c
 *
 * For the transformation of a voxel space all the grid types use transformation matrices
 * that are in the corresponding tabs_x directory in the file rotmatrix.inc
 *
 * These tables contain all rotation matrices for all orientations of the grid. More
 * in \ref transformationDetails
 *
 * The base class of the voxel space is not suitable for any grid because it misses
 * functions. The derived classes implement those missing functions.
 *
 * The glasses for the tetrahedral-octahedral and for the rhombic grid are derived
 * from the cube grid (voxel_0_c) because they just superimpose another larger grid
 * onto the standard cube grid to achieve their goals
 */

/// the value that signifies uninitialised values for the hotspot and bounding box cache
#define BBHSCACHE_UNINIT -30000
/// the value means that this entry for the bounding box hotspot cache is not defines
/// due to an undefined orientation of the piece
#define BBHSCACHE_NOT_DEF -30001

voxel_c::voxel_c(unsigned int x, unsigned int y, unsigned int z, const gridType_c * g, voxel_type init) : gt(g), sx(x), sy(y), sz(z), voxels(x*y*z), hx(0), hy(0), hz(0), weight(1) {

  space = new voxel_type[voxels];
  bt_assert(space);
  bt_assert(gt);

  memset(space, init, voxels);

  if (init == 0) {
    bx2 = by2 = bz2 = 0;
    bx1 = x-1;
    by1 = y-1;
    bz1 = z-1;
  } else {
    bx1 = by1 = bz1 = 0;
    bx2 = x-1;
    by2 = y-1;
    bz2 = z-1;
  }

  doRecalc = true;

  symmetries = symmetryInvalid();

  BbHsCache = new int[9*gt->getSymmetries()->getNumTransformationsMirror()];

  for (unsigned int i = 0; i < gt->getSymmetries()->getNumTransformationsMirror(); i++)
    BbHsCache[9*i+0] = BbHsCache[9*i+3] = BBHSCACHE_UNINIT;
}

voxel_c::voxel_c(const voxel_c & orig) : gt(orig.gt), sx(orig.sx), sy(orig.sy), sz(orig.sz),
voxels(orig.voxels), hx(orig.hx), hy(orig.hy), hz(orig.hz), weight(orig.weight) {

  space = new voxel_type[voxels];
  bt_assert(space);

  memcpy(space, orig.space, voxels);

  bx1 = orig.bx1;
  bx2 = orig.bx2;
  by1 = orig.by1;
  by2 = orig.by2;
  bz1 = orig.bz1;
  bz2 = orig.bz2;

  doRecalc = true;

  symmetries = symmetryInvalid();

  BbHsCache = new int[9*gt->getSymmetries()->getNumTransformationsMirror()];

  for (unsigned int i = 0; i < gt->getSymmetries()->getNumTransformationsMirror(); i++)
    BbHsCache[9*i+0] = BbHsCache[9*i+3] = BBHSCACHE_UNINIT;
}

voxel_c::voxel_c(const voxel_c * orig) : gt(orig->gt), sx(orig->sx), sy(orig->sy), sz(orig->sz),
voxels(orig->voxels), hx(orig->hx), hy(orig->hy), hz(orig->hz), weight(orig->weight) {

  space = new voxel_type[voxels];
  bt_assert(space);

  memcpy(space, orig->space, voxels);

  bx1 = orig->bx1;
  bx2 = orig->bx2;
  by1 = orig->by1;
  by2 = orig->by2;
  bz1 = orig->bz1;
  bz2 = orig->bz2;

  doRecalc = true;

  symmetries = symmetryInvalid();

  BbHsCache = new int[9*gt->getSymmetries()->getNumTransformationsMirror()];

  for (unsigned int i = 0; i < gt->getSymmetries()->getNumTransformationsMirror(); i++)
    BbHsCache[9*i+0] = BbHsCache[9*i+3] = BBHSCACHE_UNINIT;
}

voxel_c::~voxel_c() {
  delete [] space;
  delete [] BbHsCache;
}

void voxel_c::recalcBoundingBox(void) {

  if (!doRecalc)
    return;

  bx1 = by1 = bz1 = sx+sy+sz;
  bx2 = by2 = bz2 = 0;

  bool empty = true;

  unsigned int index = 0;

  for (unsigned int z = 0; z < sz; z++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int x = 0; x < sx; x++) {
        if ((space[index] & 3) != VX_EMPTY) {
          if (x < bx1) bx1 = x;
          if (x > bx2) bx2 = x;

          if (y < by1) by1 = y;
          if (y > by2) by2 = y;

          if (z < bz1) bz1 = z;
          if (z > bz2) bz2 = z;

          empty = false;
        } else {
          space[index] = 0;  // clear away all colours that might be left
        }
        index++;
      }

  if (empty)
    bx1 = by1 = bz1 = bx2 = by2 = bz2 = 0;

  /* we also clear the bounding box and hotspot cache */
  for (unsigned int i = 0; i < gt->getSymmetries()->getNumTransformationsMirror(); i++)
    BbHsCache[9*i+0] = BbHsCache[9*i+3] = BBHSCACHE_UNINIT;
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

bool voxel_c::identicalInBB(const voxel_c * op, bool includeColors) const {

  if (bx2-bx1 != op->bx2-op->bx1) return false;
  if (by2-by1 != op->by2-op->by1) return false;
  if (bz2-bz1 != op->bz2-op->bz1) return false;

  for (unsigned int x = bx1; x <= bx2; x++)
    for (unsigned int y = by1; y <= by2; y++)
      for (unsigned int z = bz1; z <= bz2; z++)
        if (includeColors) {
          if (get(x, y, z) != op->get(x-bx1+op->bx1, y-by1+op->by1, z-bz1+op->bz1))
            return false;
        } else {
          if (getState(x, y, z) != op->getState(x-bx1+op->bx1, y-by1+op->by1, z-bz1+op->bz1))
            return false;
        }


  return true;
}

bool voxel_c::identicalWithRots(const voxel_c * op, bool includeMirror, bool includeColors) const {

  const symmetries_c * sym = gt->getSymmetries();

  unsigned int maxTrans = includeMirror ? sym->getNumTransformationsMirror() : sym->getNumTransformations();

  for (unsigned int t = 0; t < maxTrans; t++) {
    voxel_c * v = gt->getVoxel(op);

    if (v->transform(t) && identicalInBB(v, includeColors)) {
      delete v;
      return true;
    }

    delete v;
  }

  return false;
}

unsigned char voxel_c::getMirrorTransform(const voxel_c * op) const {

  const symmetries_c * sym = gt->getSymmetries();

  for (unsigned int t = sym->getNumTransformations(); t < sym->getNumTransformationsMirror(); t++) {
    voxel_c * v = gt->getVoxel(this);

    if (v->transform(t) && v->identicalInBB(op, true)) {
      delete v;
      return t;
    }

    delete v;
  }

  return 0;
}

bool voxel_c::getHotspot(unsigned char trans, int * x, int * y, int * z) const {

  bt_assert(trans < gt->getSymmetries()->getNumTransformationsMirror());
  bt_assert(x && y && z);

  /* if the cache values don't exist calculate them */
  if (BbHsCache[9*trans] == BBHSCACHE_UNINIT) {

    /* this version always works, but also is quite slow
    */
    voxel_c * tmp = gt->getVoxel(this);

    if (!tmp->transform(trans))
    {
      BbHsCache[9*trans+0] = BBHSCACHE_NOT_DEF;
      BbHsCache[9*trans+3] = BBHSCACHE_NOT_DEF;
    }

    BbHsCache[9*trans+0] = tmp->getHx();
    BbHsCache[9*trans+1] = tmp->getHy();
    BbHsCache[9*trans+2] = tmp->getHz();

    delete tmp;
  }

  if (BbHsCache[9*trans] == BBHSCACHE_NOT_DEF)
  {
     return false;
  }
  else
  {
    *x = BbHsCache[9*trans+0];
    *y = BbHsCache[9*trans+1];
    *z = BbHsCache[9*trans+2];
    return true;
  }
}

bool voxel_c::getBoundingBox(unsigned char trans, int * x1, int * y1, int * z1, int * x2, int * y2, int * z2) const {

  bt_assert(trans < gt->getSymmetries()->getNumTransformationsMirror());

  /* if the cache values don't exist calculate them */
  if (BbHsCache[9*trans+3] == BBHSCACHE_UNINIT) {

    /* this version always works, but it is quite slow */
    voxel_c * tmp = gt->getVoxel(this);

    if (!tmp->transform(trans))
    {
      BbHsCache[9*trans+3] = BBHSCACHE_NOT_DEF;
      BbHsCache[9*trans+0] = BBHSCACHE_NOT_DEF;
    }
    else
    {
      BbHsCache[9*trans+3] = tmp->boundX1();
      BbHsCache[9*trans+4] = tmp->boundX2();
      BbHsCache[9*trans+5] = tmp->boundY1();
      BbHsCache[9*trans+6] = tmp->boundY2();
      BbHsCache[9*trans+7] = tmp->boundZ1();
      BbHsCache[9*trans+8] = tmp->boundZ2();
    }

    delete tmp;
  }

  if (BbHsCache[9*trans+3] == BBHSCACHE_NOT_DEF)
  {
    return false;
  }
  else
  {
    if (x1) *x1 = BbHsCache[9*trans+3];
    if (x2) *x2 = BbHsCache[9*trans+4];
    if (y1) *y1 = BbHsCache[9*trans+5];
    if (y2) *y2 = BbHsCache[9*trans+6];
    if (z1) *z1 = BbHsCache[9*trans+7];
    if (z2) *z2 = BbHsCache[9*trans+8];
    return true;
  }
}

void voxel_c::resize(unsigned int nsx, unsigned int nsy, unsigned int nsz, voxel_type filler) {

  // if size doesn't change, do nothing
  if (nsx == sx && nsy == sy && nsz == sz) return;

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

void voxel_c::scale(unsigned int /*amount*/, bool /*grid*/) {
  // do nothing by default
}

bool voxel_c::scaleDown(unsigned char /*by*/, bool /*action*/) {
  return false;
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

void voxel_c::translate(int dx, int dy, int dz, voxel_type filler) {
  voxel_type * s2 = new voxel_type[sx*sy*sz];
  memset(s2, filler, sx*sy*sz);

  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++)
        if (((int)x+dx >= 0) && ((int)x+dx < (int)sx) &&
            ((int)y+dy >= 0) && ((int)y+dy < (int)sy) &&
            ((int)z+dz >= 0) && ((int)z+dz < (int)sz))
          s2[(x+dx)+sx*((y+dy)+sy*(z+dz))] = get(x, y, z);

  delete [] space;
  space = s2;

  // initially I thought I could just shift the bounding box, but this doesn't work
  // as off piece voxels might have been shifted out making the shape smaller
  // and thus requiring a recalculation...
  recalcBoundingBox();

  hx += dx;
  hy += dy;
  hz += dz;
}

/** \page unionfinddetails Union Find Details
 *
 * Please read http://en.wikipedia.org/wiki/Union_find first.
 *
 * Now the method in the unionFind function is similar.
 * Each node in the tree corresponds to one voxel of the voxel space.
 * The tree array contains
 * all indices to the parent node of the tree. The tree is initialised with NULL
 * pinter equivalents (-1 in this case) then we loop over all voxels and all neighbours
 * of a voxel are found and the corresponding trees are unified.
 *
 * In the end all nodes that are somehow connected are within a single tree within the forest.
 *
 * The 2 functions that use this function now evaluate the tree forest in 2 ways:
 *
 * The \ref voxel_c::connected function will check if all non empty voxels in the space
 * are within the same tree
 *
 * The \ref voxel_c::fillHoles function uses the unionFind function differently. It lets
 * the function connect the empty voxels instead of the filled. Now we will find isolated
 * voids  because they are in separate trees.
 */

/** \ref unionfinddetails */
void voxel_c::unionFind(int * tree, char type, bool inverse, voxel_type value, bool outsideZ) const {

  /* union find algorithm:
   * 1. put all voxels that matter in an own set
   * 2. unify all sets whose voxels are neighbours
   * 3. check if all voxels are in the same set
   */

  /* Initialise tree */
  for (unsigned int i = 0; i < voxels+1; i++)
    tree[i] = -1;

  bool merge_outside = ((inverse && (value != 0)) || (!inverse && (value == 0)));

  /* merge all neighbouring voxels */
  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++)
        if (validCoordinate(x, y, z) &&

            ((inverse && (get(x, y, z) != value)) ||
             (!inverse && (get(x, y, z) == value)))) {

          int root1 = getIndex(x, y, z);
          while (tree[root1] >= 0) root1 = tree[root1];

          int curTyp = 0;

          while (curTyp <= type) {

            int x2, y2, z2;
            int idx = 0;
            while (getNeighbor(idx, curTyp, x, y, z, &x2, &y2, &z2)) {
              if ((x2 >= 0) && (x2 < (int)sx) && (y2 >= 0) && (y2 < (int)sy) && (z2 >= 0) && (z2 < (int)sz)) {
                if ((x2 < (int)x) || (y2 < (int)y) || (z2 < (int)z)) {
                  if ((inverse && (get(x2, y2, z2) != value)) ||
                      (!inverse && (get(x2, y2, z2) == value))) {

                    int root2 = getIndex(x2, y2, z2);
                    while (tree[root2] >= 0) root2 = tree[root2];

                    if (root1 != root2)
                      tree[root2] = root1;
                  }
                }
              } else if (merge_outside) {

                if (outsideZ || ((z2 >= 0) && (z2 < (int)sz))) {

                  int root2 = voxels;
                  while (tree[root2] >= 0) root2 = tree[root2];

                  if (root1 != root2)
                    tree[root2] = root1;
                }
              }
              idx++;
            }
            curTyp++;
          }
        }
}

bool voxel_c::connected(char type, bool inverse, voxel_type value, bool outsideZ) const {

  /* allocate enough space for all voxels plus one for the outside */
  int * tree = new int[voxels+1];

  unionFind(tree, type, inverse, value, outsideZ);

  int root = -1;

  bool merge_outside = ((inverse && (value != 0)) || (!inverse && (value == 0)));

  /* finally check, if all voxels are in the same set */
  { for (unsigned int x = 0; x < sx; x++)
      for (unsigned int y = 0; y < sy; y++)
        for (unsigned int z = 0; z < sz; z++)
          if (validCoordinate(x, y, z) &&
              ((inverse && (get(x, y, z) != value)) ||
               (!inverse && (get(x, y, z) == value)))) {
            if (root == -1) {
              root = getIndex(x, y, z);
              while (tree[root] >= 0) root = tree[root];

            } else {

              int root2 = getIndex(x, y, z);
              while (tree[root2] >= 0) root2 = tree[root2];

              if (root2 != root) {
                delete [] tree;
                return false;
              }
            }
          }

    if ((root != -1) && merge_outside) {
      int root2 = voxels;
      while (tree[root2] >= 0) root2 = tree[root2];

      if (root2 != root) {
        delete [] tree;
        return false;
      }
    }
  }

  delete [] tree;
  return true;
}

void voxel_c::fillHoles(char type) {

  /* allocate enough space for all voxels plus one for the outside */
  int * tree = new int[voxels+1];

  unionFind(tree, type, true, VX_FILLED, true);

  int root = -1;

  root = tree[voxels];
  while (tree[root] >= 0) root = tree[root];

  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++)
        if (validCoordinate(x, y, z) &&
            get(x, y, z) != VX_FILLED)  {
          int root2 = getIndex(x, y, z);
          while (tree[root2] >= 0) root2 = tree[root2];

          if (root2 != root) {
            set(x, y, z, VX_FILLED);
          }
        }


  delete [] tree;
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
  name = "";

  weight = orig->weight;
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

symmetries_t voxel_c::selfSymmetries(void) const {

  // if we have not calculated the symmetries, yet we calculate it
  if (isSymmetryInvalid(symmetries))
  {
    symmetries = gt->getSymmetries()->calculateSymmetry(this);
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

void voxel_c::actionOnSpace(VoxelAction action, bool inside) {

  if (!getX() || !getY() ||!getZ())
    return;

  for (unsigned int x = 0; x < getX(); x++)
    for (unsigned int y = 0; y < getY(); y++)
      for (unsigned int z = 0; z < getZ(); z++)
        if (getState(x, y, z) != VX_EMPTY) {
          bool neighborEmpty = false;

          int idx = 0;
          int nx, ny, nz;

          while (!neighborEmpty && getNeighbor(idx, 0, x, y, z, &nx, &ny, &nz)) {
            neighborEmpty |= (getState2(nx, ny, nz) == VX_EMPTY);
            idx++;
          }

          if (inside ^ neighborEmpty)
            switch(action) {
              case ACT_FIXED: setState(x, y, z, VX_FILLED); break;
              case ACT_VARIABLE: setState(x, y, z, VX_VARIABLE); break;
              case ACT_DECOLOR: setColor(x, y, z, 0); break;
            }
        }
}

void voxel_c::save(xmlWriter_c & xml) const {

  xml.newTag("voxel");

  xml.newAttrib("x", sx);
  xml.newAttrib("y", sy);
  xml.newAttrib("z", sz);

  if (hx) xml.newAttrib("hx", hx);
  if (hy) xml.newAttrib("hy", hy);
  if (hz) xml.newAttrib("hz", hz);

  if (weight != 1)
    xml.newAttrib("weight", weight);

  if (name.length())
    xml.newAttrib("name", name);

  // this might allow us to later add another format
  xml.newAttrib("type", 0);

  std::ostream & str = xml.addContent();

  for (unsigned int i = 0; i < getXYZ(); i++)
  {
    // output state
    switch(getState(i))
    {
      case VX_EMPTY:    str << "_"; break;
      case VX_FILLED:   str << "#"; break;
      case VX_VARIABLE: str << "+"; break;
    }

    // output colour postfix, but only for nonempty voxels
    switch(getState(i))
    {
      case VX_VARIABLE:
      case VX_FILLED:
        // output colour, only when colour is not zero
        if (getColor(i))
          str << getColor(i);
        break;

      default:
        break;
    }
  }

  xml.endTag("voxel");
}

voxel_c::voxel_c(xmlParser_c & pars, const gridType_c * g) : gt(g), hx(0), hy(0), hz(0), weight(1)
{
  pars.require(xmlParser_c::START_TAG, "voxel");

  skipRecalcBoundingBox(true);

  std::string szStr;

  szStr = pars.getAttributeValue("x");
  if (!szStr.length())
    pars.exception("voxel space requires 'x' attribute");
  sx = atoi(szStr.c_str());

  szStr = pars.getAttributeValue("y");
  if (!szStr.length())
    pars.exception("voxel space requires 'y' attribute");
  sy = atoi(szStr.c_str());

  szStr = pars.getAttributeValue("z");
  if (!szStr.length())
    pars.exception("voxel space requires 'z' attribute");
  sz = atoi(szStr.c_str());

  szStr = pars.getAttributeValue("type");
  if (!szStr.length())
    pars.exception("voxel space requires 'type' attribute");

  unsigned int type = atoi(szStr.c_str());

  // set to the correct size
  voxels = sx*sy*sz;

  szStr = pars.getAttributeValue("hx");
  hx = atoi(szStr.c_str());
  szStr = pars.getAttributeValue("hy");
  hy = atoi(szStr.c_str());
  szStr = pars.getAttributeValue("hz");
  hz = atoi(szStr.c_str());

  name = pars.getAttributeValue("name");

  szStr = pars.getAttributeValue("weight");
  if (szStr != "")
    weight = atoi(szStr.c_str());

  space = new voxel_type[voxels];

  if (pars.next() != xmlParser_c::TEXT)
    pars.exception("voxel space requires content");

  std::string c = pars.getText();

  unsigned int idx = 0;
  unsigned int color = 0;

  if (c.length())
  {
    if (type != 0)
      pars.exception("unknown voxel type");

    for (unsigned int pos = 0; pos < c.length(); pos++)
    {
      switch (c[pos])
      {
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
        default : pars.exception("unrecognised character in piece voxel space"); break;
      }

      if (color > 63)
        pars.exception("constraint color too big > 63");

      if (idx > 0)
        setColor(idx-1, color);

      if (idx > getXYZ())
        pars.exception("too many voxels defined for voxelspace");
    }
    if (idx < getXYZ())
      pars.exception("not enough voxels defined for voxelspace");
  }

  symmetries = symmetryInvalid();
  BbHsCache = new int[9*gt->getSymmetries()->getNumTransformationsMirror()];

  skipRecalcBoundingBox(false);

  pars.next();
  pars.require(xmlParser_c::END_TAG, "voxel");
}

void voxel_c::setHotspot(int x, int y, int z) {
  hx = x; hy = y; hz = z;

  for (unsigned int i = 0; i < gt->getSymmetries()->getNumTransformationsMirror(); i++)
    BbHsCache[9*i+0] = BBHSCACHE_UNINIT;
}

void voxel_c::initHotspot(void) {
  setHotspot(0, 0, 0);
}

bool voxel_c::indexToXYZ(unsigned int index, unsigned int *x, unsigned int *y, unsigned int *z) const {

  *x = index % sx;
  index -= *x;
  index /= sx;

  *y = index % sy;
  index -= *y;
  index /= sy;

  *z = index;

  return *z < sz;
}

bool voxel_c::unionintersect(
      const voxel_c * va, int xa, int ya, int za,
      const voxel_c * vb, int xb, int yb, int zb
      ) {

  /* first make sure that the 2 given voxels bounding boxes do overlap
   * if they don't we don't need to to anything
   */
  if (va->bx1+xa > vb->bx2+xb || va->bx2+xa < vb->bx1+xb) return false;
  if (va->by1+ya > vb->by2+yb || va->by2+ya < vb->by1+yb) return false;
  if (va->bz1+za > vb->bz2+zb || va->bz2+za < vb->bz1+zb) return false;

  /* first make sure wa can accommodate the complete 2nd voxel space */
  bool do_resize = false;
  int nx = sx;
  int ny = sy;
  int nz = sz;

  if (xa+(int)va->sx > nx) { nx = xa+(int)va->sx; do_resize = true; }
  if (ya+(int)va->sy > ny) { ny = ya+(int)va->sy; do_resize = true; }
  if (za+(int)va->sz > nz) { nz = za+(int)va->sz; do_resize = true; }

  if (xb+(int)vb->sx > nx) { nx = xb+(int)vb->sx; do_resize = true; }
  if (yb+(int)vb->sy > ny) { ny = yb+(int)vb->sy; do_resize = true; }
  if (zb+(int)vb->sz > nz) { nz = zb+(int)vb->sz; do_resize = true; }

  if (do_resize)
    resize(nx, ny, nz, VX_EMPTY);

  bool result = false;

  for (unsigned int x = 0; x < sx; x++)
    for (unsigned int y = 0; y < sy; y++)
      for (unsigned int z = 0; z < sz; z++)
        if (va->getState2(x-xa, y-ya, z-za) == VX_FILLED &&
            vb->getState2(x-xb, y-yb, z-zb) == VX_FILLED) {
          set(x, y, z, VX_FILLED);
          result = true;
        }

  return result;
}

Polyhedron * voxel_c::getMeshInternal(double bevel, double offset, bool fast) const
{
  Polyhedron * res = new Polyhedron();

  vertexList_c vl(res);
  std::vector<int> face3(3);
  std::vector<int> face4(4);
  std::vector<float> faceCorners;
  std::vector<float> faceCorners2;

  // make sure the 2 parameters are positive or zero and have a valid minimum size
  if (bevel < 1e-5) bevel = 0;
  if (offset < 1e-5) offset = 0;

  for (unsigned int z = 0; z < getZ(); z++)
    for (unsigned int y = 0; y < getY(); y++)
      for (unsigned int x = 0; x < getX(); x++)
        if (!isEmpty(x, y, z))
        {
          int n;
          int nx, ny, nz;

          // we skip generating this voxel for the fast case,
          // when the voxel to generate has no empty neighbours
          if (fast)
          {
            bool hasEmptyN = false;

            n = 0;

            while (getNeighbor(n, 0, x, y, z, &nx, &ny, &nz))
            {
              if (isEmpty2(nx, ny, nz))
              {
                hasEmptyN = true;
                break;
              }
              n++;
            }

            if (!hasEmptyN)
            {
              continue;
            }
          }

          uint32_t type = (((x+y+z) & 1) == 0) ? FF_COLOR_LIGHT : 0;
          uint32_t idx = getIndex(x, y, z);

          // first t the voxel polyhedron that is fixed

          n = 1;

          if (bevel > 0)
          {
            do {
              faceCorners.clear();
              getConnectionFace(x, y, z, -n, bevel, offset, faceCorners);

              Face * f = 0;

              if (faceCorners.size() == 9)
              {
                face3[0] = vl.get(faceCorners[0], faceCorners[1], faceCorners[2]);
                face3[1] = vl.get(faceCorners[3], faceCorners[4], faceCorners[5]);
                face3[2] = vl.get(faceCorners[6], faceCorners[7], faceCorners[8]);
                f = res->addFace(face3);
              }
              else if (faceCorners.size() == 12)
              {
                face4[0] = vl.get(faceCorners[0], faceCorners[1], faceCorners[2]);
                face4[1] = vl.get(faceCorners[3], faceCorners[4], faceCorners[5]);
                face4[2] = vl.get(faceCorners[6], faceCorners[7], faceCorners[8]);
                face4[3] = vl.get(faceCorners[9], faceCorners[10], faceCorners[11]);
                f = res->addFace(face4);
              }
              else if (faceCorners.size() == 0)
              {
              }
              else
              {
                bt_assert(0);
              }

              n++;

              if (f)
              {
                f->_flags = FF_WIREFRAME | type | FF_BEVEL_FACE;
                f->_fb_index = idx;
                f->_fb_face = -1;
                f->_color = 0;
              f->_color = getColor(x, y, z);
              }

            } while (faceCorners.size() > 0);
          }

          n = 0;

          while (getNeighbor(n, 0, x, y, z, &nx, &ny, &nz))
          {
            if (isEmpty2(nx, ny, nz))
            {
              faceCorners.clear();
              getConnectionFace(x, y, z, n, bevel, offset, faceCorners);

              Face * f = 0;

              if (faceCorners.size() == 9)
              {
                face3[0] = vl.get(faceCorners[0], faceCorners[1], faceCorners[2]);
                face3[1] = vl.get(faceCorners[3], faceCorners[4], faceCorners[5]);
                face3[2] = vl.get(faceCorners[6], faceCorners[7], faceCorners[8]);
                f = res->addFace(face3);
              }
              else if (faceCorners.size() == 12)
              {
                face4[0] = vl.get(faceCorners[0], faceCorners[1], faceCorners[2]);
                face4[1] = vl.get(faceCorners[3], faceCorners[4], faceCorners[5]);
                face4[2] = vl.get(faceCorners[6], faceCorners[7], faceCorners[8]);
                face4[3] = vl.get(faceCorners[9], faceCorners[10], faceCorners[11]);
                f = res->addFace(face4);
              }
              else
              {
                bt_assert(0);
              }

              f->_flags = type;
              if (isVariable(x, y, z)) f->_flags |= FF_VARIABLE_MARK;
              f->_fb_index = idx;
              f->_fb_face = n;
              f->_color = getColor(x, y, z);
            }
            else
            {
              if ((!fast) && (offset > 0) && ((nx > (int)x) || (nx == (int)x && ny > (int)y) || (nx == (int)x && ny == (int)y && nz > (int)z)))
              {
                // add connection prisms

                // first find out which neighbour we are relative to our neighbour n

                int n2 = 0;
                bool found = false;
                int mx, my, mz;

                while (getNeighbor(n2, 0, nx, ny, nz, &mx, &my, &mz))
                {
                  if ((mx == (int)x) && (my == (int)y) && (mz == (int)z))
                  {
                    found = true;
                    break;
                  }
                  n2++;
                }

                bt_assert(found);

                faceCorners.clear();
                getConnectionFace(x, y, z, n, bevel, offset, faceCorners);

                faceCorners2.clear();
                getConnectionFace(nx, ny, nz, n2, bevel, offset, faceCorners2);

                bt_assert(faceCorners.size() == faceCorners2.size());
                bt_assert(faceCorners.size() % 3 == 0);

                int corners = faceCorners.size() / 3;

                for (int i = 0; i < corners; i++)
                {
                  int f1c1 = 3*i;
                  int f1c2 = 3*((i+1) % corners);
                  int f2c1 = 3*((-i+corners) % corners);
                  int f2c2 = 3*((-i+corners-1) % corners);

                  face4[0] = vl.get(faceCorners[f1c1+0],  faceCorners[f1c1+1],  faceCorners[f1c1+2]);
                  face4[1] = vl.get(faceCorners[f1c2+0],  faceCorners[f1c2+1],  faceCorners[f1c2+2]);
                  face4[2] = vl.get(faceCorners2[f2c2+0], faceCorners2[f2c2+1], faceCorners2[f2c2+2]);
                  face4[3] = vl.get(faceCorners2[f2c1+0], faceCorners2[f2c1+1], faceCorners2[f2c1+2]);

                  Face * f = res->addFace(face4);

                  f->_flags = FF_WIREFRAME | type | FF_OFFSET_FACE;
                  f->_fb_index = idx;
                  f->_fb_face = -1;
                  f->_color = 0;
              f->_color = getColor(x, y, z);
                }
              }
            }
            n++;
          }
        }

  if (!fast)
  {
    res->finalize();
  }

  return res;
}

Polyhedron * voxel_c::getMesh(double bevel, double offset) const
{
  return getMeshInternal(bevel, offset, false);
}

Polyhedron * voxel_c::getDrawingMesh(void) const
{
  return getMeshInternal(0.03, 0.005, true);
}

Polyhedron * voxel_c::getWireframeMesh(void) const
{
  Polyhedron * p = getMeshInternal(0.03, 0.005, false);
  fillPolyhedronHoles(*p, 1);
  return p;
}


