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
#ifndef __SYMMETRIES_0_H__
#define __SYMMETRIES_0_H__

#include "symmetries.h"

/* this is the symmetries class for cubes */
class symmetries_0_c : public symmetries_c {

  private:

    /* this variable contains the symmetries that are available because
     * of the subsetting of the cube because of skewing and scaling along the different axes
     * this is used to suppress certain symmetries for the placement and symmetry checks
     *
     * this value influences the rotx y z functions to only return those transformations
     * that are really available, also getNumTransformations and getNumTransformationsMirror are
     * influenced to only return the number of symmetries within this field, ...
     */
    const gridType_c * gt;


    symmetries_0_c(const gridType_c * gt);

    friend const symmetries_c * gridType_c::getSymmetries(void) const;

  public:

    unsigned int getNumTransformations(void) const;
    unsigned int getNumTransformationsMirror(void) const;
    bool symmetrieContainsTransformation(symmetries_t s, unsigned int t) const;
    unsigned char transAdd(unsigned char t1, unsigned char t2) const;
    unsigned char minimizeTransformation(symmetries_t s, unsigned char trans) const;
    unsigned int countSymmetryIntersection(symmetries_t resultSym, symmetries_t s2) const;
    bool symmetriesLeft(symmetries_t resultSym, symmetries_t s2) const;
    symmetries_t symmetryCalcuation(const voxel_c * pp) const;
    bool symmetryContainsMirror(symmetries_t sym) const;
    bool symmetryKnown(const voxel_c * pp) const;
};

#endif
