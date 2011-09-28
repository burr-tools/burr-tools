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
#ifndef __DISASSEMBLER_A_H__
#define __DISASSEMBLER_A_H__

#include "disassembler.h"
#include "movementanalysator.h"

#include <vector>

class grouping_c;
class problem_c;
class disassemblerNode_c;
class assembly_c;

/**
 * this class is a base-class for disassemblers.
 *
 * It provides common functionality for all disassemblers.
 * This is mainly bookkeeping of disassemblerNode_c objects
 *
 * the space grid dependent information is collected from movementCache classes.
 * those are like the assemblerFrontends for the assembler (a bit more complicated
 * though)
 *
 * All that the real disassemblers need to to is implement the disassemble_rec function
 * which analyses one piece of the puzzle until it falls apart
 */
class disassembler_a_c : public disassembler_c {

  private:

    /**
     * For grouping pieces
     */
    grouping_c * groups;

    /**
     * the problem we solve
     */
    const problem_c * puzzle;

    /**
     * Converts piece number to the corresponding shape number.
     *
     * These are needed for the grouping functions
     */
    unsigned short * piece2shape;

    /**
     * the movement analysator we use.
     *
     * The movement analysator will return the possible moves from a given position
     */
    movementAnalysator_c *analyse;

    unsigned short subProbGroup(const disassemblerNode_c * st, const std::vector<unsigned int> & pn, bool cond);
    bool subProbGrouping(const std::vector<unsigned int> & pn);

    separation_c * checkSubproblem(int pieceCount, const std::vector<unsigned int> & pieces, const disassemblerNode_c * st, bool left, bool * ok);

  protected:

    /** start analysing the position given in the disassemblerNode */
    void init_find(disassemblerNode_c * nd, const std::vector<unsigned int> & pieces) {
      analyse->init_find(nd, pieces);
    }

    /** get one possible next position for the currently running analysis */
    disassemblerNode_c * find(void) { return analyse->find(); }

    /**
     * Analyse a sub-problem.
     *
     * once a separating node has been found by the disassemble_rec function,
     * it should call this function to analyse the sub-problems
     */
    separation_c * checkSubproblems(const disassemblerNode_c * st, const std::vector<unsigned int> &pieces);

    /** this function must be implemented by the real disassemblers */
    virtual separation_c * disassemble_rec(const std::vector<unsigned int> & pieces, disassemblerNode_c * start) = 0;

  public:

    /**
     * construct the disassembler for this concrete problem.
     * The problem can not be changed, once you done that but
     * you can analyse many assemblies for disassembability
     */
    disassembler_a_c(const problem_c *puz);
    ~disassembler_a_c(void);

    /**
     * Disassemble an assembly of the puzzle.
     *
     * Because we can only have or don't have a disassembly sequence
     * we don't need the same complicated call-back interface. The function
     * returns either the disassembly sequence or a null pointer.
     * you need to take care of deleting the disassembly sequence after
     * doing with it whatever you want.
     */
    separation_c * disassemble(const assembly_c * assembly);

  private:

    // no copying and assigning
    disassembler_a_c(const disassembler_a_c&);
    void operator=(const disassembler_a_c&);
};

#endif
