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
#include "symmetries_2.h"

#include "voxel_2.h"

#include "bt_assert.h"
#include "bitfield.h"

#include "tabs_2/tablesizes.inc"

/* this matrix contains the concatenation of 2 transformations
 * if you first transform the piece around t1 and then around t2
 * you can as well transform around transMult[t1][t2]
 */
static const unsigned int transMult[NUM_TRANSFORMATIONS_MIRROR][NUM_TRANSFORMATIONS_MIRROR] = {
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

symmetries_2_c::symmetries_2_c(const gridType_c * g) : gt(g) {
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

symmetries_t symmetries_2_c::symmetryCalcuation(const voxel_c *pp) const {

  bt_assert(pp);

  int i;
  bitfield_c<NUM_TRANSFORMATIONS_MIRROR> s;

#ifndef NDEBUG

  /* this is debug code that checks, if we really have all symmetry groups included
   * it should be finally removed some day in the future
   */
  for (int t = NUM_TRANSFORMATIONS_MIRROR-1; t >= 0; t--) {

    voxel_2_c p(pp);

    if (!p.transform(t))
      continue;

    s.clear();
    s.set(0);

    for (int j = 1; j < NUM_TRANSFORMATIONS_MIRROR; j++) {
      voxel_2_c v(&p);
      if (v.transform(j) && p.identicalInBB(&v))
        s.set(j);
    }

    for (i = 0; i < NUM_SYMMETRY_GROUPS; i++)
      if (symmetries[i] == s)
        break;

    if (symmetries[i] != s) {
      char txt[500];
      int idx = snprintf(txt, 500, "could not find ");
      s.print(txt+idx, 500-idx);
      break;
    }
  }

  /* if we can not find the current symmetry group in our list
   * lets create the groups for all possible orientation of the piece
   * as they might differ for different initial orientations
   */
  if (symmetries[i] != s) {

    for (int j = 1; j < NUM_TRANSFORMATIONS_MIRROR; j++) {
      voxel_2_c p(pp);

      if (!p.transform(j)) continue;

      s.clear();
      s.set(0);

      for (int k = 1; k < NUM_TRANSFORMATIONS_MIRROR; k++) {
        voxel_2_c v(p);
        if (v.transform(k) && p.identicalInBB(&v))
          s.set(k);
      }

      char txt[150];
      s.print(txt, 150);
      bt_assert_line(txt);
    }

    bt_assert(s == symmetries[i]);
  }

#endif

  /* this is autogenerated code, the tool to create this code is in tester.cpp function
   * makeSymmetryTree(0, 0);
   */
#if 1
#include "tabs_2/symcalc.inc"
#endif

  return i;
}
