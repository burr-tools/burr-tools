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
#include "symmetries_1.h"

#include "voxel_1.h"

#include "bt_assert.h"

#include "tabs_1/tablesizes.inc"

/* this matrix contains the concatenation of 2 transformations
 * if you first transform the piece around t1 and then around t2
 * you can as well transform around transMult[t1][t2]
 */
static const unsigned int transMult[NUM_TRANSFORMATIONS_MIRROR][NUM_TRANSFORMATIONS_MIRROR] = {
#include "tabs_1/transmult.inc"
};

/* this array contains all possible symmetry groups, meaning bitmasks with exactly the bits set
 * that correspond to transformations that reorient the piece so that it looks identical
 */
static const unsigned long long symmetries[NUM_SYMMETRY_GROUPS] = {
#include "tabs_1/symmetries.inc"
};

static const unsigned long long unifiedSymmetries[NUM_SYMMETRY_GROUPS] = {
#include "tabs_1/unifiedsym.inc"
};

/* this matrix lets you calculate the orientation with the smallest number that results in an identical looking
 * shape. This requires us to know the symmetry group
 */
static const unsigned char transformationMinimizer[NUM_SYMMETRY_GROUPS][NUM_TRANSFORMATIONS_MIRROR] = {
#include "tabs_1/transformmini.inc"
};

static const unsigned long long uniqueSymmetries[NUM_SYMMETRY_GROUPS] = {
#include "tabs_1/uniquesym.inc"
};

symmetries_1_c::symmetries_1_c(void) {
}

unsigned int symmetries_1_c::getNumTransformations(void) const { return NUM_TRANSFORMATIONS; }
unsigned int symmetries_1_c::getNumTransformationsMirror(void) const { return NUM_TRANSFORMATIONS_MIRROR; }

bool symmetries_1_c::symmetryContainsMirror(symmetries_t sym) const {
  bt_assert(sym < NUM_SYMMETRY_GROUPS);
  return (symmetries[sym] & ~((1 << getNumTransformations()) - 1)) != 0;
}

unsigned char symmetries_1_c::transAdd(unsigned char t1, unsigned char t2) const {
  bt_assert(t1 < NUM_TRANSFORMATIONS_MIRROR);
  bt_assert(t2 < NUM_TRANSFORMATIONS_MIRROR);
  return transMult[t1][t2];
}

bool symmetries_1_c::symmetrieContainsTransformation(symmetries_t s, unsigned int t) const {

  bt_assert(s < NUM_SYMMETRY_GROUPS);
  bt_assert(t < NUM_TRANSFORMATIONS_MIRROR);

  return ((symmetries[s] & ((unsigned long long)1 << t)) != 0);
}

unsigned char symmetries_1_c::minimizeTransformation(symmetries_t s, unsigned char trans) const {

  bt_assert(s < NUM_SYMMETRY_GROUPS);
  bt_assert(trans < NUM_TRANSFORMATIONS_MIRROR);

  return transformationMinimizer[s][trans];
}

bool symmetries_1_c::isTransformationUnique(symmetries_t s, unsigned int t) const {

  bt_assert(s < NUM_SYMMETRY_GROUPS);
  bt_assert(t < NUM_TRANSFORMATIONS_MIRROR);

  return ((uniqueSymmetries[s] & ((unsigned long long)1 << t)) != 0);
}

unsigned int symmetries_1_c::countSymmetryIntersection(symmetries_t res, symmetries_t s2) const {

  bt_assert(res < NUM_SYMMETRY_GROUPS);
  bt_assert(s2 < NUM_SYMMETRY_GROUPS);

  unsigned long long s = unifiedSymmetries[res] & symmetries[s2];

  s -= ((s >> 1) & 0x5555555555555555ll);
  s = (((s >> 2) & 0x3333333333333333ll) + (s & 0x3333333333333333ll));
  s = (((s >> 4) + s) & 0x0f0f0f0f0f0f0f0fll);
  s += (s >> 8);
  s += (s >> 16);
  s += (s >> 32);
  return (unsigned int)(s & 0x3f);
}

bool symmetries_1_c::symmetriesLeft(symmetries_t resultSym, symmetries_t s2) const {

  bt_assert(resultSym < NUM_SYMMETRY_GROUPS);
  bt_assert(s2 < NUM_SYMMETRY_GROUPS);

  return symmetries[resultSym] & unifiedSymmetries[s2] & ~((unsigned long long)1);
}

bool symmetries_1_c::symmetryKnown(const voxel_c * pp) const {
  unsigned long long s = 1;
  int i;

  for (int j = 1; j < NUM_TRANSFORMATIONS_MIRROR; j++) {
    voxel_1_c v(pp);
    bt_assert2(v.transform(j));
    if (pp->identicalInBB(&v))
      s |= ((unsigned long long)1) << j;
  }

  for (i = 0; i < NUM_SYMMETRY_GROUPS; i++)
    if (symmetries[i] == s)
      break;

  return symmetries[i] == s;
}

symmetries_t symmetries_1_c::calculateSymmetry(const voxel_c *pp) const {

  bt_assert(pp);

#ifndef NDEBUG
  bt_assert(symmetryKnown(pp));
#endif

  /* this is auto-generated code, the tool to create this code is in tester.cpp function
   * makeSymmetryTree(0, 0);
   */
#include "tabs_1/symcalc.inc"
}
