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
#ifndef __SYMMETRIES_LATTICE_H__
#define __SYMMETRIES_LATTICE_H__

#include "symmetries.h"

#include <cstdint>
#include <vector>

class latticeGridInfo_c;

/**
 * Symmetries of a described grid (step G3 of
 * design/2026-11-19-ng-merge-plan.md).
 *
 * A symmetry group (symmetries_t) is NG's group number, one of its 136
 * subgroups of the 60 transformations; only the ones made of this grid's
 * transformations occur. The tables the native grids have generated
 * (tabs_N/generator_N.cpp) are computed here from the grid's product
 * table, with the same definitions:
 *
 * - unique: going through the transformations in order, the first of each
 *   orientation the shape can have (t and "first g, then t" for g in the
 *   group give the same shape);
 * - minimize: the smallest transformation giving the same shape;
 * - unified: every transformation that is a symmetry of some turned copy
 *   of the shape (the conjugates of the group).
 */
class symmetries_lattice_c : public symmetries_c {

  public:

    explicit symmetries_lattice_c(const latticeGridInfo_c & info);

    unsigned int getNumTransformations(void) const override;
    unsigned int getNumTransformationsMirror(void) const override;
    bool symmetrieContainsTransformation(symmetries_t s, unsigned int t) const override;
    bool isTransformationUnique(symmetries_t s, unsigned int t) const override;
    unsigned char transAdd(unsigned char t1, unsigned char t2) const override;
    unsigned char minimizeTransformation(symmetries_t s, unsigned char trans) const override;
    unsigned int countSymmetryIntersection(symmetries_t resultSym, symmetries_t s2) const override;
    bool symmetriesLeft(symmetries_t resultSym, symmetries_t s2) const override;
    bool symmetryContainsMirror(symmetries_t sym) const override;
    symmetries_t calculateSymmetry(const voxel_c * pp) const override;
    bool symmetryKnown(const voxel_c * pp) const override;

  private:

    const latticeGridInfo_c & inf;
    unsigned int n;   // transformations with mirror

    // per NG group: its transformations, the unified set, the unique set
    std::vector<uint64_t> sym, unified, unique;
    // per group and transformation: the minimised transformation
    std::vector<unsigned char> mini;

    uint64_t selfSet(const voxel_c * pp) const;
};

#endif
