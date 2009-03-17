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

#include <stdlib.h>
#include <stdint.h>

class assembly_c;

/**
 * The node structure used by the disassembler.
 *
 * This node contains the current position of all involved pieces. It also
 * contains a pointer to the node that we came from to reach this node
 *
 * Finally we have some additional redundant information for easier
 * manipulation: the direction and the amount the pieces were moved to
 * get to this node.
 *
 * The node is used by the disassembler to construct its search tree. As
 * the tree can grow pretty large with a lot of nodes, it is important to
 * keep the node small. There is some possible optimisation available
 * in this node. For example you could save just the direction, amount and
 * pieces involved the transition from the come-from node to this node.
 *
 * The redundant information could be calculated from the come-from node.
 */
class disassemblerNode_c {

private:

  /**
   * Maximum possible distance from the original position.
   * we have a movement limitation that pieces must not overstep, otherwise
   * the used int datatype will overrun, this is the value
   */
  static const int16_t maxMove = 32767;

  /**
   * The node that we came from to reach this node.
   * The nodes are used to save the shortest way from the start to each
   * node. So each node saves where the way to the start is. The root
   * node obviously has a NULL pointer here.
   */
  disassemblerNode_c * comefrom;

  /**
   * Number of pieces this node is handling
   */
  unsigned int piecenumber;

  /**
   * The position and orientation of all involved pieces.
   *
   * For more memory performance I will try to limit the
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
  int16_t * dat;

  /**
   * A reference counter for automatic deletion of the node.
   * Contains the number of pointers that point to this node
   * most of there pointers are comefrom pointers from other nodes
   * but one increment results from the pointer inside the node lists
   */
  unsigned int refcount;

  /**
   * The direction pieces are moved.
   *
   * The direction the pieces were moved to get from the come-from
   * node to this node.
   *
   * When several pieces were moved in different directions (coordinated
   * motion) this field will contain an invalid value. The interpretation
   * is up to the user
   */
  unsigned int dir;

  /**
   * Amount of steps that pieces are moved.
   *
   * There was a movement involved to get from the come-from node to this
   * node. The amount is saved in this field
   */
  int amount;

  /**
   * The has value of this node.
   *
   * Because calculating the hash value is expensive and we need it pretty
   * ofen we cache the value in here. A value of 0 means we don't know
   * the hash value and need to calculate it.
   */
  unsigned int hashValue;

  /**
   * Waylenth to get from the startnode to this node.
   *
   * The waylength is not identical to the number of nodes to get here.
   * It might be. But if you want to favour certain moves you can given them a shorter
   * step size.
   */
  unsigned int waylength;

public:

  /**
   * Create a new node.
   *
   * Create a new node with the given number of pieces the given come-from pointer
   * and the defined values for direction, amount.
   * Thepsize is added to the waylength of the come-from pointer and the result will be
   * saved in our waylength value
   */
  disassemblerNode_c(unsigned int pn, disassemblerNode_c * comf, int _dir, int _amount, int step = 1);

  /** creates a root node from an assembly */
  disassemblerNode_c(const assembly_c * assm);

  /** create a new root node with pn pieces */
  disassemblerNode_c(unsigned int pn);

  ~disassemblerNode_c();

  /**
   * Replace this node with the information from another node
   *
   * This will only work, if the other node actually stands the the
   * same node but reached that position via an other way
   *
   * In that case the come-from pointer is replaced and the reference
   * counting is updated
   *
   * In other cases this will do strange things or throw an assert
   * when compiled with debug
   */
  void replaceNode(const disassemblerNode_c *n);

  /**
   * this function is used by outsiders to free
   * their own pointers to this node.
   *
   * if the function returs true, delete the node
   */
  bool decRefCount(void) {
    bt_assert(refcount > 0);
    refcount--;
    return refcount == 0;
  }

  /**
   * Tell this node that you now have a pointer to it
   */
  void incRefCount(void) {
    refcount++;
  }

  /**
   * Return the hash value for this node.
   *
   * The has value uses relative positions just like the comparison
   * below. So if 2 nodes are equal according to the comparison
   * below they will return the same hash value
   */
  unsigned int hash(void) const;

  /**
   * The comparison operations using "normalized" positions.
   * This means all the pieces are shifted so, that the position
   * of piece 0 is (0; 0; 0). This prevents us from shifting
   * the whole set around without movement of the pieces
   * relative to each other because all nodes with all pieces
   * shifted by the same mount do are equal
   */
  bool operator == (const disassemblerNode_c &b) const;

  /** return x-position of piece i */
  int getX(unsigned int i) const {
    bt_assert(i < piecenumber);
    return dat[4*i+0];
  }

  /** return y-position of piece i */
  int getY(unsigned int i) const {
    bt_assert(i < piecenumber);
    return dat[4*i+1];
  }

  /** return z-position of piece i */
  int getZ(unsigned int i) const {
    bt_assert(i < piecenumber);
    return dat[4*i+2];
  }

  /** return orientation of piece i */
  unsigned int getTrans(unsigned int i) const {
    bt_assert(i < piecenumber);
    return (unsigned char)dat[4*i+3];
  }

  /** return the number of pieces that are handled in this node */
  unsigned int getPiecenumber(void) const {
    return piecenumber;
  }

  /**
   * Setup piece i to be removed in this node.
   *
   * The x, y and z value define the direction in which the
   * piece is removed
   */
  void setRemove(unsigned int i, int x, int y, int z) {
    bt_assert(i < piecenumber);
    bt_assert(abs(x) < maxMove && abs(y) < maxMove && abs(z) < maxMove);

    dat[4*i+0] = x;
    dat[4*i+1] = y;
    dat[4*i+2] = z;
    dat[4*i+3] = (int16_t)0xFFFF;
    hashValue = 0;
  }

  /**
   * set position of piece i in this node to the given values
   */
  void set(unsigned int i, int x, int y, int z, unsigned int tr) {
    bt_assert(i < piecenumber);
    bt_assert(abs(x) < maxMove && abs(y) < maxMove && abs(z) < maxMove);

    dat[4*i+0] = x;
    dat[4*i+1] = y;
    dat[4*i+2] = z;
    dat[4*i+3] = (int16_t)tr;
    hashValue = 0;
  }

  /**
   * set position of piece i in this node relative to the position in the come-from node
   */
  void set(unsigned int i, int tx, int ty, int tz)
  {
    bt_assert(i < piecenumber);
    bt_assert(comefrom);
    bt_assert(abs(comefrom->dat[4*i+0]+tx) < maxMove &&
              abs(comefrom->dat[4*i+1]+ty) < maxMove &&
              abs(comefrom->dat[4*i+2]+tz) < maxMove);

    dat[4*i+0] = comefrom->dat[4*i+0]+tx;
    dat[4*i+1] = comefrom->dat[4*i+1]+ty;
    dat[4*i+2] = comefrom->dat[4*i+2]+tz;
    dat[4*i+3] = comefrom->dat[4*i+3];
    hashValue = 0;
  }

  /**
   * check if piece number nr is removed in this node
   */
  bool is_piece_removed(unsigned int nr) const {
    bt_assert(nr < piecenumber);
    return (dat[4*nr+3] == (int16_t)0xFFFF);
  }

  /**
   * check, if there is any piece that is removed in this node
   */
  bool is_separation() const;

  /** get the come-from node */
  const disassemblerNode_c * getComefrom(void) const {
    return comefrom;
  }

  /** get the amount of movement for this node */
  int getAmount(void) const {
    return amount;
  }

  /** get the movement direction for this node */
  unsigned int getDirection(void) const {
    return dir;
  }

  /** get the waylength from root node to this node */
  unsigned int getWaylength(void) const {
    return waylength;
  }

  /**
   * Next-pointer for the hash-table lists.
   *
   * These node will be saved in disassembler-hash-tables. For
   * those tables we need a next-pointer to save lists of nodes
   * that go into the same hash-bucket
   */
  disassemblerNode_c * next;
};

#endif

