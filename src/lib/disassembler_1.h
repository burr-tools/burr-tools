/* Burr Solver
 * Copyright (C) 2003  Andreas Röver
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

#ifndef __DISASSEMBLER_1_H__
#define __DISASSEMBLER_1_H__

#include "voxel.h"
#include "disassembler.h"

/* this class implements a burr disassembler. the interface is simple:
 * 1) construct the klass with the voxel space of the assembled puzzle,
 *    empty voxels should be 0xff, bieces should be enumerated continuously
 *    starting from 0
 * 2) call diassemble and evaluate the result
 */
class disassembler_1_c : public disassembler_c {

private:

  voxel_c * assm;
  int piecenumber;

  /* bounding box */
  int *bx1, *bx2, *by1, *by2, *bz1, *bz2;

  /* these variables are used for the routine that looks
   * for the pieces to move find, checkmovement
   */
  int nextpiece, nextstep, nextdir, next_pn;
  int * movement;
  bool * check;

  /* matrix should normally have 6 subarrays, for each of the 6 possible
   * directions (positive x negative x, positive y, ...) one, but because
   * the matrix for the negative direction in the same dimenstion is the
   * transposition (m[i][j] == m[j][i]) we save the calculation or copying
   * and rather do the transposition insde the checkmovement function
   */
  int * matrix[3];

  /* calculate the bounding boxes for the given problem
   * this function gets called from the constructor
   */
  void calcbounds(void);

  /* create matrix */
  void prepare(int pn, voxel_type * pieces, node_c * searchnode);
  void prepare_rec(int pn, int d, int a, int b);
  void init_find(node_c * nd, int piecenumber, voxel_type * pieces);

  /*
   * find all possible movements of starting from the state given to init_find
   * the functions returns the next possible state or 0 if no other state was found
   */
  node_c * find(node_c * searchnode);
  bool checkmovement(void);

  /* the real disassembly routine. It separates the puzzle into 2 parts
   * and get's called recursively with each subpart to disassemble
   *
   * the return is the disassembly tree for that part
   *
   * pieces contains the names of all the pieces that are still inside the
   * subpuzzle puzzle, start defines the starting position of these pieces
   */
  separation_c * disassemble_rec(int piecenumber, voxel_type * pieces, node_c * start);

public:

  disassembler_1_c(voxel_c * prob, int piecenum);
  ~disassembler_1_c();

  /* because we can only have or don't have a disassembly sequence
   * we don't need the same complicated callback interface. The function
   * returns either the disassembly sequence or a null pointer.
   * you need to take care of freeing the disassembly sequence after
   * doing with it whatever you want
   */
  disassembly_c * disassemble(void);

};

#endif
