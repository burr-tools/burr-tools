/* Burr Solver
 * Copyright (C) 2003-2006  Andreas Röver
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
#ifndef __DISASSEMBLER_0_H__
#define __DISASSEMBLER_0_H__

#include "disassembler.h"
#include "voxel.h"

class puzzle_c;
class grouping_c;
class movementCache_c;
class assembly_c;
class separation_c;

class node0_c;

/* this class is a disassembler for the cube space.
 *
 * is is implemented using bill cuttlers algorithm, so please read there
 * in case you are interested how it works. The comments are written with
 * the thought that you know his algorithm
 */
class disassembler_0_c : public disassembler_c {

private:

  unsigned int piecenumber;

  /* these variables are used for the routine that looks
   * for the pieces to move find, checkmovement
   */
  int nextpiece, nextstep, nextdir, next_pn, nextstate, nextpiece2, state99piece, state99nextState;
  int * movement;
  bool * check;

  /* matrix should normally have 6 subarrays, for each of the 6 possible
   * directions (positive x negative x, positive y, ...) one, but because
   * the matrix for the negative direction in the same dimenstion is the
   * transposition (m[i][j] == m[j][i]) we save the calculation or copying
   * and rather do the transposition insde the checkmovement function
   */
  int * matrix[3];

  /* this is the cache with the already calculated movements */
  movementCache_c * cache;

  /* here we can group pieces together */
  grouping_c * groups;

  /* create matrix */
  void prepare(int pn, voxel_type * pieces, node0_c * searchnode);
  void prepare2(int pn);
  void init_find(node0_c * nd, int piecenumber, voxel_type * pieces);

  /* find all possible movements of starting from the state given to init_find
   * the functions returns the next possible state or 0 if no other state was found
   */
  node0_c * find(node0_c * searchnode, const int * weights);
  bool checkmovement(unsigned int maxPieces, int nextdir, int next_pn, int nextpiece, int nextstep);

  unsigned short subProbGroup(node0_c * st, voxel_type * pn, bool cond, int piecenumber);
  bool subProbGrouping(voxel_type * pn, int piecenumber);
  separation_c * checkSubproblem(int pieceCount, voxel_type * pieces, int piecenumber, node0_c * st, bool left, bool * ok, const int * weights);

  /* the real disassembly routine. It separates the puzzle into 2 parts
   * and get's called recursively with each subpart to disassemble
   *
   * the return is the disassembly tree for that part
   *
   * pieces contains the names of all the pieces that are still inside the
   * subpuzzle puzzle, start defines the starting position of these pieces
   */
  separation_c * disassemble_rec(int piecenumber, voxel_type * pieces, node0_c * start, const int * weights);

  const puzzle_c * puzzle;
  unsigned int problem;

  /* this array is used to convert piece number to the corresponding
   * shape number, as these are needed for the grouping functions
   */
  unsigned short * piece2shape;

  /* this array contains the weights of all the shapes involved in this problem */
  int * weights;

  /* construct the disassembler for this concrete problem, is can not be
   * changed, once you done that but you can analyse many assemblies for
   * disassembability
   */
  disassembler_0_c(const puzzle_c * puz, unsigned int prob);

  friend disassembler_c * gridType_c::getDisassembler(const puzzle_c * puz, unsigned int prob) const;

public:

  ~disassembler_0_c();

  /* because we can only have or don't have a disassembly sequence
   * we don't need the same complicated callback interface. The function
   * returns either the disassembly sequence or a null pointer.
   * you need to take care of freeing the disassembly sequence after
   * doing with it whatever you want
   */
  separation_c * disassemble(const assembly_c * assembly);
};

#endif
