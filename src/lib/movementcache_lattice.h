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
#ifndef __MOVEMENTCACHE_LATTICE_H__
#define __MOVEMENTCACHE_LATTICE_H__

#include "movementcache.h"

class latticeGridInfo_c;

/**
 * Movement cache of a described grid (step G4 of
 * design/2026-11-19-ng-merge-plan.md): the directions are NG's movement
 * vectors, and a voxel may advance one step when the cells NG derived for
 * its kind and the direction (from the voxel polyhedra) are empty, as
 * movementCache_1_c does with its hand-made table for prisms.
 */
class movementCache_lattice_c : public movementCache_c {

  public:

    movementCache_lattice_c(std::shared_ptr<const solverSpec_t> puz, const latticeGridInfo_c & info);

  private:

    std::vector<unsigned int> moCalcValues(const voxel_c * sh1, const voxel_c * sh2, int dx, int dy, int dz) override;
    unsigned int numDirections(void) override;
    void getDirection(unsigned int dir, int * x, int * y, int * z) override;

    const latticeGridInfo_c & inf;

    movementCache_lattice_c(const movementCache_lattice_c &) = delete;
    movementCache_lattice_c & operator=(const movementCache_lattice_c &) = delete;
};

#endif
