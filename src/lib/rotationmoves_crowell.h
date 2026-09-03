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
 */
#ifndef __ROTATION_MOVES_CROWELL_H__
#define __ROTATION_MOVES_CROWELL_H__

#include "crowell_solver.h"
#include "rotationmoves.h"
#include "rotationrules.h"

#include <vector>

class problem_c;
class movementCache_c;
class symmetries_c;

/**
 * Fortran-style 90° move generator (SolveSlidingCube SimpleRot / PieceBuilder).
 *
 * Feature status, how each heuristic is implemented, and what is still missing:
 * see crowell_solver.h (read that before changing this class).
 *
 * Isolated from rotationMoves_0_c so Classic stays complete.
 */
class rotationMoves_crowell_c : public rotationMoves_c {

  private:

    static const unsigned int DCT_MAX = 3;

    const problem_c & problem;
    movementCache_c * cache;
    const symmetries_c * sym;
    rotationRules_c rules;

    disassemblerNode_c * searchnode;
    const std::vector<unsigned int> * pieces;
    unsigned int nextsubset;
    int nextpivot;
    unsigned int nextaxis;
    unsigned int nextsense;
    bool active;
    unsigned int largestIdx;
    unsigned int dirCounts[6];
    unsigned int skipRevAxis;
    unsigned int skipRevSense;
    bool skipReverse;

    std::vector<rotationRules_c::pivot_t> pivotCells;
    std::vector<rotationRules_c::cell_t> cachedStart;
    std::vector<rotationRules_c::cell_t> cachedOccupied;
    unsigned int cachedMask;
    bool cellsReady;

    void collectWorldCells(unsigned int pieceIdx, std::vector<rotationRules_c::cell_t> & out) const;
    unsigned int countFilled(unsigned int pieceIdx) const;
    void loadSubsetCells(void);
    void rebuildPivotCells(unsigned int axis);
    void startAxis(void);
    void advanceCandidate(void);
    bool skipCurrentDirection(void) const;
    disassemblerNode_c * tryCurrentCandidate(void);
    unsigned int nextSubsetMask(unsigned int mask, unsigned int n) const;

    static unsigned char rotationTransformId(unsigned int axis, unsigned int sense);
    static bool rotateDoubled(int * x, int * y, int * z, const rotationRules_c::pivot_t & pivot,
                              unsigned int axis, unsigned int sense);

  public:

    rotationMoves_crowell_c(const problem_c & puz, movementCache_c * cache_);
    virtual ~rotationMoves_crowell_c(void) {}

    virtual void init_find(disassemblerNode_c * nd, const std::vector<unsigned int> & pcs);
    virtual disassemblerNode_c * find(void);

  private:

    rotationMoves_crowell_c(const rotationMoves_crowell_c&);
    void operator=(const rotationMoves_crowell_c&);
};

#endif
