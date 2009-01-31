/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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
#ifndef __MOVEMENT_ANALYSATOR_H__
#define __MOVEMENT_ANALYSATOR_H__

#include <vector>

class puzzle_c;
class disassemblerNode_c;
class movementCache_c;
class assembly_c;

/* this class is can do analysation of movements within a puzzle
 *
 * is is implemented using Bill Cuttlers algorithm, so please read there
 * in case you are interested how it works. The comments are written with
 * the thought that you know his algorithm
 */
class movementAnalysator_c {

  private:

    /* matrix should normally have 6 subarrays, for each of the 6 possible
     * directions (positive x negative x, positive y, ...) one, but because
     * the matrix for the negative direction in the same dimension is the
     * transposition (m[i][j] == m[j][i]) we save the calculation or copying
     * and rather do the transposition inside the checkmovement function
     */
    int ** matrix;
    int * movement;
    int * weights;
    unsigned int piecenumber;

    const puzzle_c * puzzle;
    unsigned int problem;

    movementCache_c * cache;

    /* these variables are used for the routine that looks
     * for the pieces to move find, checkmovement
     */
    int nextpiece, nextstep, next_pn, nextstate, nextpiece2, state99nextState;
    unsigned int nextdir;
    unsigned int maxstep;
    disassemblerNode_c * state99node;

    void prepare(const std::vector<unsigned int> & pieces, disassemblerNode_c * searchnode);
    bool checkmovement(unsigned int maxPieces, int nextdir, int next_pn, int nextpiece, int nextstep);

    disassemblerNode_c * newNode(int nextdir, disassemblerNode_c * searchnode, int amount, const std::vector<unsigned int> & pieces);

    /* creates a new node that contains the merged movements of the given 2 nodes
     * merged movement means that a piece is moved the maximum amount specified in
     * both nodes. But only one direction is allowed, so if one piece moves this
     * way and another piece that way 0 i sreturned
     * the function also returns zero, if the new node would be identical to n1 or n0
     * also the amount must be identical in both nodes, so if piece a moves 1 unit
     * in node n0 and andother piece move 2 units in node n1 0 is returned
     */
    disassemblerNode_c * newNodeMerge(const disassemblerNode_c *n0, const disassemblerNode_c *n1, disassemblerNode_c * searchnode, int nextdir, const std::vector<unsigned int> & pieces);

  public:

    /* construct the disassembler for this concrete problem, is can not be
     * changed, once you done that but you can analyse many assemblies for
     * disassembability
     */
    movementAnalysator_c(const puzzle_c *puz, unsigned int problem);
    ~movementAnalysator_c(void);

    /* 2 sets of functions, one including coordinated motion, and one that doesn't */
    void init_find0(disassemblerNode_c * nd, const std::vector<unsigned int> & pieces);
    disassemblerNode_c * find0(disassemblerNode_c * searchnode, const std::vector<unsigned int> & pieces);
    void completeFind0(disassemblerNode_c * searchnode, const std::vector<unsigned int> & pieces, std::vector<disassemblerNode_c*> * result);
};

#endif
