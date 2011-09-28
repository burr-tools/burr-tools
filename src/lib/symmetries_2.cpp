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
#include "symmetries_2.h"

#include "voxel_2.h"

#include "bt_assert.h"
#include "bitfield.h"

#include "tabs_2/tablesizes.inc"

/* this matrix contains the concatenation of 2 transformations
 * if you first transform the piece around t1 and then around t2
 * you can as well transform around transMult[t1][t2]
 */
static const unsigned char transMult[NUM_TRANSFORMATIONS_MIRROR][NUM_TRANSFORMATIONS_MIRROR] = {
#include "tabs_2/transmult.inc"
};

/* this array contains all possible symmetry groups, meaning bitmasks with exactly the bits set
 * that correspond to transformations that reorient the piece so that it looks identical
 */
static const bitfield_c<NUM_TRANSFORMATIONS_MIRROR> symmetries[NUM_SYMMETRY_GROUPS] = {
#include "tabs_2/symmetries.inc"
};

static const bitfield_c<NUM_TRANSFORMATIONS_MIRROR> unifiedSymmetries[NUM_SYMMETRY_GROUPS] = {
#include "tabs_2/unifiedsym.inc"
};

/* this matrix lets you calculate the orientation with the smallest number that results in an identical looking
 * shape. This requires us to know the symmetry group
 */
static const unsigned char transformationMinimizer[NUM_SYMMETRY_GROUPS][NUM_TRANSFORMATIONS_MIRROR] = {
#include "tabs_2/transformmini.inc"
};

static const bitfield_c<NUM_TRANSFORMATIONS_MIRROR> uniqueSymmetries[NUM_SYMMETRY_GROUPS] = {
#include "tabs_2/uniquesym.inc"
};

symmetries_2_c::symmetries_2_c(void) {
}

unsigned int symmetries_2_c::getNumTransformations(void) const { return NUM_TRANSFORMATIONS; }
unsigned int symmetries_2_c::getNumTransformationsMirror(void) const { return NUM_TRANSFORMATIONS_MIRROR; }

bool symmetries_2_c::symmetryContainsMirror(symmetries_t sym) const {
  bt_assert(sym < NUM_SYMMETRY_GROUPS);

  bitfield_c<NUM_TRANSFORMATIONS_MIRROR>s = symmetries[sym];

  for (int i = 0; i < NUM_TRANSFORMATIONS; i++)
    s.reset(i);

  return s.notNull();
}

unsigned char symmetries_2_c::transAdd(unsigned char t1, unsigned char t2) const {
  bt_assert(t1 < NUM_TRANSFORMATIONS_MIRROR);
  bt_assert(t2 < NUM_TRANSFORMATIONS_MIRROR);
  bt_assert(transMult[t1][t2] < NUM_TRANSFORMATIONS_MIRROR);
  return transMult[t1][t2];
}

bool symmetries_2_c::symmetrieContainsTransformation(symmetries_t s, unsigned int t) const {

  bt_assert(s < NUM_SYMMETRY_GROUPS);
  bt_assert(t < NUM_TRANSFORMATIONS_MIRROR);

  return symmetries[s].get(t);
}

unsigned char symmetries_2_c::minimizeTransformation(symmetries_t s, unsigned char trans) const {

  bt_assert(s < NUM_SYMMETRY_GROUPS);
  bt_assert(trans < NUM_TRANSFORMATIONS_MIRROR);

  return transformationMinimizer[s][trans];
}

bool symmetries_2_c::isTransformationUnique(symmetries_t s, unsigned int t) const {

  bt_assert(s < NUM_SYMMETRY_GROUPS);
  bt_assert(t < NUM_TRANSFORMATIONS_MIRROR);

  return uniqueSymmetries[s].get(t);
}

unsigned int symmetries_2_c::countSymmetryIntersection(symmetries_t res, symmetries_t s2) const {

  bt_assert(res < NUM_SYMMETRY_GROUPS);
  bt_assert(s2 < NUM_SYMMETRY_GROUPS);

  bitfield_c<NUM_TRANSFORMATIONS_MIRROR> s = unifiedSymmetries[res] & symmetries[s2];

  return s.countbits();
}

bool symmetries_2_c::symmetriesLeft(symmetries_t resultSym, symmetries_t s2) const {

  bt_assert(resultSym < NUM_SYMMETRY_GROUPS);
  bt_assert(s2 < NUM_SYMMETRY_GROUPS);

  bitfield_c<NUM_TRANSFORMATIONS_MIRROR>s = symmetries[resultSym] & unifiedSymmetries[s2];

  s.reset(0);

  return s.notNull();
}

bool symmetries_2_c::symmetryKnown(const voxel_c * pp) const {

  int i;
  bitfield_c<NUM_TRANSFORMATIONS_MIRROR> s;

  s.clear();
  s.set(0);

  for (int j = 1; j < NUM_TRANSFORMATIONS_MIRROR; j++) {
    voxel_2_c v(pp);
    if (v.transform(j) && pp->identicalInBB(&v))
      s.set(j);
  }

  for (i = 0; i < NUM_SYMMETRY_GROUPS; i++)
    if (symmetries[i] == s)
      break;

  return symmetries[i] == s;
}

symmetries_t symmetries_2_c::calculateSymmetry(const voxel_c *pp) const {

  bt_assert(pp);

#ifndef NDEBUG
  bt_assert(symmetryKnown(pp));
#endif

  /* this is auto-generated code, the tool to create this code is in tester.cpp function
   * makeSymmetryTree(0, 0);
   */
#include "tabs_2/symcalc.inc"
}
