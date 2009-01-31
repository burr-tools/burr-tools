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
#ifndef __DISASSEMBLER_NODE_H__
#define __DISASSEMBLER_NODE_H__

#include "bt_assert.h"

class assembly_c;

/* node is used to build the search tree for the breadth first search of the
 * state tree each node contains the position of all the pieces relative
 * to their position in the assembled puzzle
 */
class disassemblerNode_c {

private:

  /* the nodes are used to save the shortest way from the start to each
   * node. So each node saves where the way to the start is
   */
  disassemblerNode_c * comefrom;

  /* number of pieces this node is handling */
  int piecenumber;

  /* the position of the piece inside the cube
   * and the transformation of that piece
   *
   * for more memory performance I will try to limit the
   * amount of memory required by allocating only one big
   * chunk of memory with interleaved data
   * at position x%4 == 0 is x, ==1 is y ==2 is z ==3 is trans
   *
   * a piece NOT inside the rest is signified by
   * trans == 0xFF, the direction the pieces were move out
   * should be obtained from dir below, when trans is 0xFF
   * then the data fields also contain the direction, not the
   * position of the piece
   */
  signed char * dat;

  /* contains the number of pointers that point to this node
   * most of there pointers are comefrom pointers from other nodes
   * but one increment results from the pointer inside the node lists
   */
  unsigned int refcount;

  /* with regard to the parent node this node defines a movement
   * of a set of pieces along a certain axis by a certain amount,
   * because this information is required often, we save it in here
   */
  unsigned int dir;    // the interpretation of dir is up to the user, but is the same as nextdir
  int amount;

  unsigned int hashValue;

  /* waylenth to get from the startnode to this one, this has nothing to do
   * with the number of moves used for assembling, but to favor other moves
   */
  unsigned int waylength;

public:

  disassemblerNode_c(int pn, disassemblerNode_c * comf, int _dir, int _amount, int step = 1);

  /* creates a root node from an assembly */
  disassemblerNode_c(const assembly_c * assm);

  ~disassemblerNode_c();

  void replaceNode(const disassemblerNode_c *n);

  /* this function is used by outsiders to free
   * their own pointers to this node, if the function
   * returs true, delete the node
   */
  bool decRefCount(void) {
    bt_assert(refcount > 0);
    refcount--;
    return refcount == 0;
  }

  void incRefCount(void) {
    refcount++;
  }

  unsigned int hash(void) const;

  /* the comparison operations use "normalized" positions,
   * meaning all the pieces are shifted so, that the position
   * of piece 0 is (0; 0; 0). This prevents us from shifting
   * the whole set around without movement of the pieces
   * relative to each other because all nodes with all pieces
   * shifted by the same mount do are equal
   */
  bool operator == (const disassemblerNode_c &b) const;

  int getX(int i) const {
    bt_assert(i < piecenumber);
    return dat[4*i+0];
  }

  int getY(int i) const {
    bt_assert(i < piecenumber);
    return dat[4*i+1];
  }

  int getZ(int i) const {
    bt_assert(i < piecenumber);
    return dat[4*i+2];
  }

  unsigned int getTrans(int i) const {
    bt_assert(i < piecenumber);
    return (unsigned char)dat[4*i+3];
  }

  int getPiecenumber(void) const {
    return piecenumber;
  }

  void set(int i, int x, int y, int z, unsigned int tr) {
    bt_assert(i < piecenumber);
    bt_assert(abs(x) < 128 && abs(y) < 128 && abs(z) < 128);

    dat[4*i+0] = x;
    dat[4*i+1] = y;
    dat[4*i+2] = z;
    dat[4*i+3] = tr;
    hashValue = 0;
  }

  void set(int i, const disassemblerNode_c * n, int tx, int ty, int tz) {
    bt_assert(i < piecenumber);

    bt_assert(abs(n->dat[4*i+0]+tx) < 128 &&
              abs(n->dat[4*i+1]+ty) < 128 &&
              abs(n->dat[4*i+2]+tz) < 128);

    dat[4*i+0] = n->dat[4*i+0]+tx;
    dat[4*i+1] = n->dat[4*i+1]+ty;
    dat[4*i+2] = n->dat[4*i+2]+tz;
    dat[4*i+3] = n->dat[4*i+3];
    hashValue = 0;
  }

  /* check if the given piece is at a position outside
   * of the rest of the puzzle
   */
  bool is_piece_removed(int nr) const {
    bt_assert(nr < piecenumber);
    return ((unsigned char)dat[4*nr+3] == 0xFF);
  }

  /* check if this node is for a state that separates
   * the puzzle into 2 pieces. This is the case if there
   * is one piece that is removed
   */
  bool is_separation() const;

  const disassemblerNode_c * getComefrom(void) const {
    return comefrom;
  }

  int getAmount(void) const {
    return amount;
  }

  unsigned int getDirection(void) const {
    return dir;
  }

  unsigned int getWaylength(void) const {
    return waylength;
  }

  /* for the links in the hashtable we use this
   * pointer
   */
  disassemblerNode_c * next;
};

#endif
