/* Burr Solver
 * Copyright (C) 2003-2009  Andreas Röver
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

/* this class is a baseclass for disassemblers. it provides commong functionality
 * for both the disassembler_0 and disassembler_1
 *
 * is is implemented using Bill Cuttlers algorithm, so please read there
 * in case you are interested how it works. The comments are written with
 * the thought that you know his algorithm
 *
 * the space grid dependend information is collected from movementCache classes.
 * those are like the assemblerFrontends for the assembler (a bit more complicated
 * though)
 */
class disassembler_a_c : public disassembler_c {

  private:

    /* here we can group pieces together */
    grouping_c * groups;

    const problem_c * puzzle;

    /* this array is used to convert piece number to the corresponding
     * shape number, as these are needed for the grouping functions
     */
    unsigned short * piece2shape;

    movementAnalysator_c *analyse;

    unsigned short subProbGroup(const disassemblerNode_c * st, const std::vector<unsigned int> & pn, bool cond);
    bool subProbGrouping(const std::vector<unsigned int> & pn);

    separation_c * checkSubproblem(int pieceCount, const std::vector<unsigned int> & pieces, const disassemblerNode_c * st, bool left, bool * ok);

  protected:

    void init_find(disassemblerNode_c * nd, const std::vector<unsigned int> & pieces) {
      analyse->init_find(nd, pieces);
    }

    disassemblerNode_c * find(void) { return analyse->find(); }

    /* one a separating node has been found by the disassemble_rec function, it should call this function
     * to analyze the sub-problems
     */
    separation_c * checkSubproblems(const disassemblerNode_c * st, const std::vector<unsigned int> &pieces);

    /* this function must be implemented by the real disassemblers */
    virtual separation_c * disassemble_rec(const std::vector<unsigned int> & pieces, disassemblerNode_c * start) = 0;

  public:

    /* construct the disassembler for this concrete problem, is can not be
     * changed, once you done that but you can analyse many assemblies for
     * disassembability
     */
    disassembler_a_c(const problem_c *puz);
    ~disassembler_a_c(void);

    /* because we can only have or don't have a disassembly sequence
     * we don't need the same complicated callback interface. The function
     * returns either the disassembly sequence or a null pointer.
     * you need to take care of freeing the disassembly sequence after
     * doing with it whatever you want
     */
    separation_c * disassemble(const assembly_c * assembly);
};


void create_new_params(const disassemblerNode_c * st, disassemblerNode_c ** n, std::vector<unsigned int> & pn, const std::vector<unsigned int> & pieces, int part, bool cond);


#endif
