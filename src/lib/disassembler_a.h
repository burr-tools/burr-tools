/* Burr Solver
 * Copyright (C) 2003-2007  Andreas Röver
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

#include <vector>

class grouping_c;
class puzzle_c;
class disassemblerNode_c;
class movementAnalysator_c;

/* this class is a disassembler for the cube space.
 *
 * is is implemented using Bill Cuttlers algorithm, so please read there
 * in case you are interested how it works. The comments are written with
 * the thought that you know his algorithm
 */
class disassembler_a_c : public disassembler_c {

  private:

    unsigned int piecenumber;

    /* here we can group pieces together */
    grouping_c * groups;

    /* this array is used to convert piece number to the corresponding
     * shape number, as these are needed for the grouping functions
     */
    unsigned short * piece2shape;

    const puzzle_c * puzzle;
    unsigned int problem;

  protected:

    unsigned short subProbGroup(disassemblerNode_c * st, const std::vector<unsigned int> & pn, bool cond);
    bool subProbGrouping(const std::vector<unsigned int> & pn);

    void groupReset(void);

    const int * weights;

    movementAnalysator_c *analyse;

  public:

    /* construct the disassembler for this concrete problem, is can not be
     * changed, once you done that but you can analyse many assemblies for
     * disassembability
     */
    disassembler_a_c(const puzzle_c *puz, unsigned int problem);
    ~disassembler_a_c(void);

    unsigned int getPiecenumber(void);
};


void create_new_params(disassemblerNode_c * st, disassemblerNode_c ** n, std::vector<unsigned int> & pn, int ** nw, const std::vector<unsigned int> & pieces, const int * weights, int part, bool cond);


#endif
