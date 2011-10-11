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
#ifndef __MOVEMENT_ANALYSATOR_H__
#define __MOVEMENT_ANALYSATOR_H__

#include <vector>

class problem_c;
class disassemblerNode_c;
class movementCache_c;
class assembly_c;
class countingNodeHash;

/**
 * this class is can do analysis of movements within a puzzle.
 *
 * It takes a position of pieces and then tries to find all other
 * positions that can be reached directly from that starting point.
 *
 * It is implemented using Bill Cutlers algorithm, so please read there
 * in case you are interested how it works. The comments are written with
 * the thought that you know his algorithm
 */
class movementAnalysator_c {

  private:

    /* matrix should normally have one subarray for each direction
     * (positive x negative x, positive y, ...), but because
     * the matrix for the negative direction in the same direction is the
     * transposition (m[i][j] == m[j][i]) we save the calculation or copying
     * and rather do the transposition inside the checkmovement function
     */
    unsigned int * matrix;
    unsigned int * movement;
    int * weights;
    unsigned int piecenumber;

    movementCache_c * cache;

    countingNodeHash * nodes;

    /* these variables are used for the routine that looks
     * for the pieces to move find, checkmovement
     */
    int nextpiece, next_pn, nextstate, nextpiece2, state99nextState;
    unsigned int nextdir;
    unsigned int maxstep, nextstep;
    disassemblerNode_c * state99node;
    disassemblerNode_c * searchnode;
    const std::vector<unsigned int> * pieces;

    void prepare(void);
    bool checkmovement(unsigned int maxPieces, unsigned int nextstep);
    disassemblerNode_c * newNode(unsigned int amount);
    disassemblerNode_c * newNodeMerge(const disassemblerNode_c *n0, const disassemblerNode_c *n1);

  public:

    /**
     * construct the analyser for this concrete problem.
     * This can not be changed, once you done that but you can analyse
     * many positions
     */
    movementAnalysator_c(const problem_c & puz);
    ~movementAnalysator_c(void);

    /* you use either the 2 functions below, or completeFind
     * the below functions return one possible movement after another and you can stop as soon
     * as you want, while completeFind will always find all possible movements
     *
     * to use the functions below first call init_find and then repeatedly find until
     * either you don't want no more, or find returns 0
     */
    void init_find(disassemblerNode_c * nd, const std::vector<unsigned int> & pieces);
    disassemblerNode_c * find(void);

    void completeFind(disassemblerNode_c * searchnode, const std::vector<unsigned int> & pieces, std::vector<disassemblerNode_c*> * result);

    /**
     * or use findMatching to find a very specific move
     * this involves the pieces in the given vector and the piece must be moved in the given direction
     * as few pieces as possible will be moved to achieve this
     * just as completeFind this is complete with init_find, so nothing else needs to be called
     */
    disassemblerNode_c * findMatching(disassemblerNode_c * nd, const std::vector<unsigned int> & pcs, unsigned int piece, int dx, int dy, int dz);

  private:

    // no copying and assigning
    movementAnalysator_c(const movementAnalysator_c&);
    void operator=(const movementAnalysator_c&);

};

#endif
