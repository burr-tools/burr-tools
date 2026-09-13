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
#ifndef __SYMMETRIES_0_H__
#define __SYMMETRIES_0_H__

#include "symmetries.h"

class gridType_c;

/** this is the symmetries class for cubes and cube based grids (like rhombic) */
class symmetries_0_c : public symmetries_c {

  public:

    symmetries_0_c(void);

    unsigned int getNumTransformations(void) const override;
    unsigned int getNumTransformationsMirror(void) const override;
    bool symmetrieContainsTransformation(symmetries_t s, unsigned int t) const override;
    unsigned char transAdd(unsigned char t1, unsigned char t2) const override;
    unsigned char minimizeTransformation(symmetries_t s, unsigned char trans) const override;
    unsigned int countSymmetryIntersection(symmetries_t resultSym, symmetries_t s2) const override;
    bool symmetriesLeft(symmetries_t resultSym, symmetries_t s2) const override;
    symmetries_t calculateSymmetry(const voxel_c * pp) const override;
    bool symmetryContainsMirror(symmetries_t sym) const override;
    bool symmetryKnown(const voxel_c * pp) const override;
    bool isTransformationUnique(symmetries_t s, unsigned int trans) const override;

  public:

    // no copying and assigning
    symmetries_0_c(const symmetries_0_c&) = delete;
    symmetries_0_c& operator=(const symmetries_0_c&) = delete;
};

#endif
