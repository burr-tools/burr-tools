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

class grouping_c;
class puzzle_c;
class disassemblerNode_c;

/* this class is a disassembler for the cube space.
 *
 * is is implemented using Bill Cuttlers algorithm, so please read there
 * in case you are interested how it works. The comments are written with
 * the thought that you know his algorithm
 */
class disassembler_a_c : public disassembler_c {

  private:

    /* matrix should normally have 6 subarrays, for each of the 6 possible
     * directions (positive x negative x, positive y, ...) one, but because
     * the matrix for the negative direction in the same dimension is the
     * transposition (m[i][j] == m[j][i]) we save the calculation or copying
     * and rather do the transposition inside the checkmovement function
     */
    int ** matrix;
    int * movement;
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
    void prepare(int pn, unsigned int * pieces, disassemblerNode_c * searchnode);
    bool checkmovement(unsigned int maxPieces, int nextdir, int next_pn, int nextpiece, int nextstep);



    disassemblerNode_c * newNode(int next_pn, int nextdir, disassemblerNode_c * searchnode, const int * weights, int amount);

    /* creates a new node that contains the merged movements of the given 2 nodes
     * merged movement means that a piece is moved the maximum amount specified in
     * both nodes. But only one direction is allowed, so if one piece moves this
     * way and another piece that way 0 i sreturned
     * the function also returns zero, if the new node would be identical to n1 or n0
     * also the amount must be identical in both nodes, so if piece a moves 1 unit
     * in node n0 and andother piece move 2 units in node n1 0 is returned
     */
    disassemblerNode_c * newNodeMerge(const disassemblerNode_c *n0, const disassemblerNode_c *n1, disassemblerNode_c * searchnode, int next_pn, int nextdir, const int * weights);

    unsigned short subProbGroup(disassemblerNode_c * st, unsigned int * pn, bool cond, int piecenumber);
    bool subProbGrouping(unsigned int * pn, int piecenumber);

    void groupReset(void);

    const int * weights;

    unsigned int getPiecenumber(void);

  public:

    /* construct the disassembler for this concrete problem, is can not be
     * changed, once you done that but you can analyse many assemblies for
     * disassembability
     */
    disassembler_a_c(movementCache_c * cache, const puzzle_c *puz, unsigned int problem);
    ~disassembler_a_c(void);
};


void create_new_params(disassemblerNode_c * st, disassemblerNode_c ** n, unsigned int ** pn, int ** nw, int piecenumber, unsigned int * pieces, const int * weights, int part, bool cond);


#endif
