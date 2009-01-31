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
   */
  int *dx, *dy, *dz;
  unsigned int *trans;

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
    return dx[i];
  }

  int getY(int i) const {
    bt_assert(i < piecenumber);
    return dy[i];
  }

  int getZ(int i) const {
    bt_assert(i < piecenumber);
    return dz[i];
  }

  unsigned int getTrans(int i) const {
    bt_assert(i < piecenumber);
    return trans[i];
  }

  int getPiecenumber(void) const {
    return piecenumber;
  }

  void set(int i, int x, int y, int z, unsigned int tr) {
    bt_assert(i < piecenumber);
    dx[i] = x;
    dy[i] = y;
    dz[i] = z;
    trans[i] = tr;
    hashValue = 0;
  }

  void set(int i, const disassemblerNode_c * n, int tx, int ty, int tz) {
    bt_assert(i < piecenumber);
    dx[i] = n->dx[i]+tx;
    dy[i] = n->dy[i]+ty;
    dz[i] = n->dz[i]+tz;
    trans[i] = n->trans[i];
    hashValue = 0;
  }

  /* check if the given piece is at a position outside
   * of the rest of the puzzle
   */
  bool is_piece_removed(int nr) const {
    bt_assert(nr < piecenumber);
    return ((abs(dx[nr]) > 10000) || (abs(dy[nr]) > 10000) || (abs(dz[nr]) > 10000));
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
