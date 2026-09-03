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
#ifndef __ROTATION_MOVES_0_H__
#define __ROTATION_MOVES_0_H__

#include "rotationmoves.h"
#include "rotationrules.h"
#include "bt_classic_solver.h"

#include <vector>

class problem_c;
class disassemblerNode_c;
class movementCache_c;
class symmetries_c;

/**
 * Generate valid 90° rotation moves for brick grids.
 *
 * Single-piece and compound (multi-piece rigid) rotations share the same pivot,
 * axis, and sense. Compound moves treat the selected pieces as one body for
 * clearance checks. Single-voxel pieces are not rotated (spinning a unit cube
 * cannot free anything).
 *
 * Pivots include voxel centres, empty cells in the moving bounding box, and
 * in-plane faces/edges/corners (Fortran SimpleRot), stored in doubled
 * cell-index units.
 */
class rotationMoves_0_c : public rotationMoves_c {

  private:

    const problem_c & problem;
    movementCache_c * cache;
    const symmetries_c * sym;
    rotationRules_c rules;

    /* iterator state for find-style enumeration */
    disassemblerNode_c * searchnode;
    const std::vector<unsigned int> * pieces;
    unsigned int nextsubset;
    int nextpivot;
    unsigned int nextaxis;
    unsigned int nextsense;
    bool active;

    std::vector<rotationRules_c::pivot_t> pivotCells;

    void collectWorldCells(unsigned int pieceIdx, std::vector<rotationRules_c::cell_t> & out) const;
    void rebuildPivotCells(unsigned int subsetMask, unsigned int axis);
    disassemblerNode_c * tryCurrentCandidate(void);

    static void rotateVector(int * x, int * y, int * z, unsigned int axis, unsigned int sense);
    static bool rotateDoubled(int * x, int * y, int * z, const rotationRules_c::pivot_t & pivot,
                              unsigned int axis, unsigned int sense);
    static unsigned char rotationTransformId(unsigned int axis, unsigned int sense);
    static unsigned int nextSubsetMask(unsigned int mask, unsigned int n);

  public:

    rotationMoves_0_c(const problem_c & puz, movementCache_c * cache_);
    virtual ~rotationMoves_0_c(void) {}

    virtual void init_find(disassemblerNode_c * nd, const std::vector<unsigned int> & pcs);
    virtual disassemblerNode_c * find(void);

  private:

    // no copying and assigning
    rotationMoves_0_c(const rotationMoves_0_c&);
    void operator=(const rotationMoves_0_c&);
};

#endif
