/* Burr Solver
 * Copyright (C) 2003-2005  Andreas Röver
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


#ifndef __DISASSEMBLER_H__
#define __DISASSEMBLER_H__

#include "voxel.h"
#include "disassembly.h"

#include <functional>


/* node is used to build the search tree for the breadth first search of the
 * state tree each node contains the position of all the pieces relative
 * to their position in the assembled puzzle
 */
class node_c {

private:

  /* the nodes are used to save the shortest way from the start to each
   * node. So each node saves where the way to the start is
   */
  node_c * comefrom;

  /* number of pieces this node is handling */
  int piecenumber;

  /* displacement of each piece relative to
   * the position in the assembly
   */
  int *dx, *dy, *dz;

public:

  node_c(int pn) : comefrom(0), piecenumber(pn) {
    dx = new int[piecenumber];
    dy = new int[piecenumber];
    dz = new int[piecenumber];
  }

  ~node_c() {
    delete [] dx;
    delete [] dy;
    delete [] dz;
  }

  /* the comparison operations use "normalized" positions,
   * meaning all the pieces are shifted so, that the position
   * of piece 0 is (0; 0; 0). This prevents us from shifting
   * the whole set around without movement of the pieces
   * relativ to each other because all nodes with all pieces
   * shifted by the same mount do are equal
   */
  bool operator == (const node_c &b) const {
  
    for (int i = 1; i < piecenumber; i++) {
      if (dx[i] - dx[0] != b.dx[i] - b.dx[0]) return false;
      if (dy[i] - dy[0] != b.dy[i] - b.dy[0]) return false;
      if (dz[i] - dz[0] != b.dz[i] - b.dz[0]) return false;
    }
  
    return true;
  }

  /* this opeartion is required for the container. It brings
   * the nodes into an arbitraty but deterministic order
   */
  bool operator < (const node_c &b) const {
  
    for (int i = 1; i < piecenumber; i++) {
      if (dx[i] - dx[0] < b.dx[i] - b.dx[0]) return true;
      if (dx[i] - dx[0] > b.dx[i] - b.dx[0]) return false;
  
      if (dy[i] - dy[0] < b.dy[i] - b.dy[0]) return true;
      if (dy[i] - dy[0] > b.dy[i] - b.dy[0]) return false;
  
      if (dz[i] - dz[0] < b.dz[i] - b.dz[0]) return true;
      if (dz[i] - dz[0] > b.dz[i] - b.dz[0]) return false;
    }
  
    return false;
  }

  voxel_type getVoxel(assemblyVoxel_c * assm, int x, int y, int z, int piece) const {
    assert(piece < piecenumber);
    return assm->pieceNumber(x - dx[piece], y - dy[piece], z - dz[piece]);
  }

  int getX(int i) const {
    assert(i < piecenumber);
    return dx[i];
  }
  int getY(int i) const {
    assert(i < piecenumber);
    return dy[i];
  }
  int getZ(int i) const {
    assert(i < piecenumber);
    return dz[i];
  }
  void set(int i, int x, int y, int z) {
    assert(i < piecenumber);
    dx[i] = x;
    dy[i] = y;
    dz[i] = z;
  }

  /* check if the given piece is at a position outside
   * of the rest of the puzzle
   */
  bool is_piece_removed(int nr) const {
    assert(nr < piecenumber);
    return ((abs(dx[nr]) > 10000) || (abs(dy[nr]) > 10000) || (abs(dz[nr]) > 10000));
  }

  /* check if this node is for a state that separates
   * the puzzle into 2 pieces. this is the case if there
   * is one piece that is removed
   */
  bool is_separation() const {
    for (int i = 0; i < piecenumber; i++)
      if (is_piece_removed(i))
        return true;

    return false;
  }

  node_c * getComefrom(void) const {
    return comefrom;
  }

  void setComefrom(node_c *n) {
    comefrom = n;
  }

  int getPiecenumber(void) {
    return piecenumber;
  }

};

/* because we save pointers to nodes inside our nodes set, we need a special
 * operation for the comparison, the standard one would compare the pointers
 * and not the things the pointers point to
 */
class node_ptr_less : public std::binary_function<node_c *, node_c *, bool> {

public:

  bool operator()(const node_c * a, const node_c * b) const {
    return *a < *b;
  }
};


/* this class implements a burr disassembler. the interface is simple:
 * 1) construct the klass with the voxel space of the assembled puzzle,
 *    empty voxels should be 0xff, bieces should be enumerated continuously
 *    starting from 0
 * 2) call diassemble and evaluate the result
 */
class disassembler_c {

public:

  disassembler_c(void) {}
  virtual ~disassembler_c(void) {}

  /* because we can only have or don't have a disassembly sequence
   * we don't need the same complicated callback interface. The function
   * returns either the disassembly sequence or a null pointer.
   * you need to take care of freeing the disassembly sequence after
   * doing with it whatever you want
   */
  virtual disassembly_c * disassemble(void) { return 0; }

};

#endif
