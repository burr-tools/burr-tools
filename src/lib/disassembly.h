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
#ifndef __DISASSEMBLY_H__
#define __DISASSEMBLY_H__

/* this module contains the datastructures to store the instruction how to assemble
 * and disassemble a puzzle
 */
#include "voxel.h"

#include "bt_assert.h"

#include <deque>
#include <vector>

/* forward declaration, the definition is below */
class state_c;

/* a list of states that lead to a separation of the
 * puzzle into 2 parts
 * the separation of a puzzle into lots of single pieces consists
 * of a tree of such separations. The root class starts with the assembled
 * puzzle and the lower parts divide the puzzle more and more until only
 * single pieces are left
 */
class separation_c {

  /* number of pieces that are in this subsection of the puzzle */
  unsigned int piecenumber;

  /* this array is here to identify the piecenumber for the given pieces
   * that are in this part of the tree
   */
  voxel_type * pieces;

  /* vector with all the states that finally lead to 2 separate
   * subpuzzles one state represents the  position of
   * all the pieces inside this subpuzzle relative to their
   * position in the completely assembled puzzle
   *
   * the last state will move all the pieces, that can be removed
   * a long way out of the puzzle (more than 10000 units)
   *
   * the first state is the beginning state, for this partition, e.g
   * for the root node the first state represents the assembles puzzle
   * with all values 0
   */
  std::deque <state_c *> states;

  /* the 2 parts the puzzle gets divided with the
   * last move. If one of this parts consists of only
   * one piece there will be a null pointer
   */
  separation_c * removed, *left;

  bool containsMultiMoves(void);

public:

  /* create a separation with sub separations r and l, pn pieces
   * and the pieces in the array pcs
   * the array is copied
   */
  separation_c(separation_c * r, separation_c * l, unsigned int pn, voxel_type * pcs);

  /* load a separation from an xml node */
  separation_c(const xml::node & node, unsigned int pieces);

  /* copy constructor */
  separation_c(const separation_c * cpy);

  /* save into an xml node */
  xml::node save(void) const;

  ~separation_c();

  /* return the number of moves that are required to separate the puzzle
   * this number is one smaller than the number of states
   */
  unsigned int getMoves(void) const { return states.size() - 1; }

  /* the number of moves to completely disassemble the puzzle, including
   * all sub separations
   */
  unsigned int sumMoves(void) const;

  /* fill a string with dot separated numbers containing the moves
   * required to disassemble the puzzle
   */
  int movesText(char * txt, int len);

  /* compares this and the given separation, for a higher level
   * one separation is bigger than the other if all levels
   * up to branch n are equal and on n+1 the level is larger then
   * the level of the other
   * if this assembly is bigger >0 is returned
   * if this assembler is smaller <0 is returned
   * if they are both equal =0 is returned
   */
  int compare(const separation_c * s2) const;

  /* get one state from the separation process */
  const state_c * getState(unsigned int num) const {
    bt_assert(num < states.size());
    return states[num];
  }

  /* get the separation for the pieces that were removed
   * and for the pieces that were left
   */
  const separation_c * getLeft(void) const { return left; }
  const separation_c * getRemoved(void) const { return removed; }

  /**
   * add a new state to the FRONT of the current state list
   * this is necessary because the list is generated when back
   * tracking from the graph search, so we visit the states in
   * the wrong order
   * keep in mind that the new state must have the same number
   * of pieces as all the other states
   */
  void addstate(state_c *st);

  /* return the number of pieces that are in this separation */
  unsigned int getPieceNumber(void) const { return piecenumber; }

  /* get the number for the num-th piece that is in this separation */
  voxel_type getPieceName(unsigned int num) const {
    bt_assert(num < piecenumber);
    return pieces[num];
  }

  /* shift one piece by the given amount in all movement states and all
   * supseparations
   */
  void shiftPiece(unsigned int pc, int dx, int dy, int dz);

  /* 2 pieces have exchanged their place in the problem list */
  void exchangeShape(unsigned int s1, unsigned int s2);

};

/* this class is used to store the complexity of a disassembly, it
 * is used as a memory efficient replacement for the basic information
 * in separation_c without the detailed disassembly instructions
 */
class separationInfo_c {

  private:

    /* this array contains the whole disassembly tree in prefix order, root, left, removed
     * when a certain subtree is empty a zero is included, otherwise the number is the number
     * of states within this tree node
     *
     * example, 3 2 1 0 0 0 0     tree root 3 --> 2 --> 1
     *
     * another example
     *
     *          3 1 1 0 0 0 1 1 0 0 0   tree root  3 --> 1 --> 1
     *                                              \--> 1 --> 1
     */
    std::vector<unsigned int> values;

    bool containsMultiMoves(unsigned int root);
    void recursiveConstruction(const separation_c * sep);

  public:

    /* separationInfo classes can only be created from normal disassemblies or
     * loaded from xml file
     */
    separationInfo_c(const xml::node & node);
    separationInfo_c(const separation_c * sep);

    /* save into an xml node */
    xml::node save(void) const;

    /* the following 3 functions return the exact same information
     * as the corresponding information in separation_c
     */
    unsigned int sumMoves(void) const;
    int movesText(char * txt, int len, unsigned int idx = 0);
    int compare(const separationInfo_c * s2) const;
};

/* defines one step in the separation process,
 * one state of relative piece positions on your way
 */
class state_c {

  /* arrays containing the positions of all the pieces
   * that are handled
   */
  int *dx, *dy, *dz;

#ifndef NDEBUG
  // we only keep the piecenumber for checking purposes
  unsigned int piecenumber;
#endif

public:

  /* create a new spate for pn pieces, you can not add more
   * pieces later on, so plan ahead
   */
  state_c(unsigned int pn);

  /* copy constructor */
  state_c(const state_c * cpy, unsigned int pn);

  ~state_c();

  /* set the position of a piece */
  void set(unsigned int piece, int x, int y, int z);

  /* the x, y or z position of a piece */
  int getX(unsigned int i) const {
    bt_assert(i < piecenumber);
    return dx[i];
  }
  int getY(unsigned int i) const {
    bt_assert(i < piecenumber);
    return dy[i];
  }
  int getZ(unsigned int i) const {
    bt_assert(i < piecenumber);
    return dz[i];
  }

  /* check, if the piece is removed in this state */
  bool pieceRemoved(unsigned int i) const;

  /* save into an xml node */
  xml::node save(unsigned int piecenumber) const;

  /* load from an xml node */
  state_c(const xml::node & node, unsigned int pn);

#ifndef NDEBUG
  /* on assert needs to check the piecenumber */
  friend void separation_c::addstate(state_c *st);
#endif
};

#endif
