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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "symmetries_lattice.h"

#include "latticegridinfo.h"
#include "gridtype.h"
#include "voxel.h"
#include "bt_assert.h"

#include <bit>
#include <memory>

symmetries_lattice_c::symmetries_lattice_c(const latticeGridInfo_c & info) : inf(info), n(info.transformations()) {

  bt_assert(n <= 64);
  const unsigned int groups = inf.numGroups();
  sym.resize(groups);
  unified.resize(groups);
  unique.resize(groups);
  mini.resize(groups * n);

  // the inverse of each transformation
  std::vector<unsigned int> invOf(n, 0);
  for (unsigned int r = 0; r < n; r++)
    for (unsigned int ri = 0; ri < n; ri++)
      if (inf.product(r, ri) == 0) {
        invOf[r] = ri;
        break;
      }

  for (unsigned int g = 0; g < groups; g++) {
    const uint64_t s = inf.groupSet(g);
    sym[g] = s;
    if (!s) continue;  // not a group of this grid

    // unified: x with "r, x, r^-1" in s for some r (tabs_0/generator_0.cpp, outputCompleteSymmetries)
    uint64_t u = s;
    for (unsigned int r = 1; r < n; r++)
      for (unsigned int x = 0; x < n; x++) {
        unsigned char a = inf.product(r, x);
        if (a == TND) continue;
        a = inf.product(a, invOf[r]);
        if (a != TND && ((s >> a) & 1)) u |= uint64_t(1) << x;
      }
    unified[g] = u;

    // unique (outputUniqueSymmetries): the first transformation of each orientation
    uint64_t covered = 0, out = 0;
    for (unsigned int r = 0; r < n; r++)
      if (!((covered >> r) & 1)) {
        out |= uint64_t(1) << r;
        for (unsigned int r2 = 0; r2 < n; r2++)
          if ((s >> r2) & 1) {
            const unsigned char p = inf.product(r2, r);
            if (p != TND) covered |= uint64_t(1) << p;
          }
      }
    unique[g] = out;

    // minimize (ssss): the smallest t with "g2, then t" == trans for some g2 in s
    for (unsigned int trans = 0; trans < n; trans++) {
      unsigned char m = (unsigned char)trans;
      for (unsigned int t = 0; t < trans && m == trans; t++)
        for (unsigned int t2 = 0; t2 < n; t2++)
          if (((s >> t2) & 1) && inf.product(t2, t) == trans) {
            m = (unsigned char)t;
            break;
          }
      mini[g * n + trans] = m;
    }
  }
}

unsigned int symmetries_lattice_c::getNumTransformations(void) const { return inf.rotations(); }
unsigned int symmetries_lattice_c::getNumTransformationsMirror(void) const { return n; }

bool symmetries_lattice_c::symmetrieContainsTransformation(symmetries_t s, unsigned int t) const {
  bt_assert(s < sym.size() && sym[s]);
  bt_assert(t < n);
  return (sym[s] >> t) & 1;
}

bool symmetries_lattice_c::isTransformationUnique(symmetries_t s, unsigned int t) const {
  bt_assert(s < sym.size() && sym[s]);
  bt_assert(t < n);
  return (unique[s] >> t) & 1;
}

unsigned char symmetries_lattice_c::transAdd(unsigned char t1, unsigned char t2) const {
  bt_assert(t1 < n);
  bt_assert(t2 < n);
  return inf.product(t1, t2);
}

unsigned char symmetries_lattice_c::minimizeTransformation(symmetries_t s, unsigned char trans) const {
  bt_assert(s < sym.size() && sym[s]);
  bt_assert(trans < n);
  return mini[s * n + trans];
}

unsigned int symmetries_lattice_c::countSymmetryIntersection(symmetries_t res, symmetries_t s2) const {
  bt_assert(res < sym.size() && sym[res]);
  bt_assert(s2 < sym.size() && sym[s2]);
  return (unsigned int)std::popcount(unified[res] & sym[s2]);
}

bool symmetries_lattice_c::symmetriesLeft(symmetries_t resultSym, symmetries_t s2) const {
  bt_assert(resultSym < sym.size() && sym[resultSym]);
  bt_assert(s2 < sym.size() && sym[s2]);
  return (sym[resultSym] & unified[s2] & ~uint64_t(1)) != 0;
}

bool symmetries_lattice_c::symmetryContainsMirror(symmetries_t s) const {
  bt_assert(s < sym.size() && sym[s]);
  return (sym[s] >> inf.rotations()) != 0;
}

uint64_t symmetries_lattice_c::selfSet(const voxel_c * pp) const {
  uint64_t s = 1;
  for (unsigned int t = 1; t < n; t++) {
    std::unique_ptr<voxel_c> v(pp->getGridType()->getVoxel(pp));
    if (v->transform(t) && pp->identicalInBB(v.get()))
      s |= uint64_t(1) << t;
  }
  return s;
}

bool symmetries_lattice_c::symmetryKnown(const voxel_c * pp) const {
  const uint64_t s = selfSet(pp);
  for (uint64_t g : sym)
    if (g == s) return true;
  return false;
}

symmetries_t symmetries_lattice_c::calculateSymmetry(const voxel_c * pp) const {
  bt_assert(pp);
  const uint64_t s = selfSet(pp);
  for (unsigned int g = 0; g < sym.size(); g++)
    if (sym[g] == s) return (symmetries_t)g;
  bt_te("symmetries_lattice_c::calculateSymmetry: the shape's symmetries are none of NG's groups");
}
