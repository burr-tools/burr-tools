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

#ifndef __DL_ASSEMBLER_H__
#define __DL_ASSEMBLER_H__

#include "assembler.h"

#include "voxel.h"
#include "puzzle.h"

/* the order of pieces is mostly unimportant, except for the
 * very first piece. this is added with removed symmetry of the
 * target shape. for this the first piece must not have any symmetry
 * at all, or the program will also calc some or all rotatet solutions  FIXME
 *
 * currently we also find all solutions for puzzles with multiple identical
 * pieces serveral time. e.g. if your puzzle has 2 pieces of type a and 3 of
 * type b and all others once you get each solution 2! times 3! = 12 times.
 * this is a real problem with e.g. loveley18. Here we have 11 identical piece
 * wich leads to each solution 11! = 39,916,800 times. This is not good.
 *
 * the problem is how to avoid these solutions with the dancing link algorithm
 * the current idea is to sort the identical pieces. Piece n+1 must have a position
 * that is later in the position list than piece n. How to achieve this. If one of
 * the pieces with serveral instances get's places, all others need to be placed, too
 *
 * the problem is the dancing link alg. may select a piece to place next and also
 * a unit of the puzzle to be filled next. In cas a piece is selected it's simply
 * the upper mentioned step to take. Place all the instances of the piece at once
 * using the ordered theorem. and then go on dancing :-)
 *
 * but if we selct a voxel to be filled then the program goes through all pieces
 * and all possible placements of these pieces that fill the selected voxel we have
 * to check each possibility if a piece is selected that
 *

 */
class assembler_0_c : public assembler_c {

private:

  /* this are the members of the node. One array for each member. This
   * accellerates access.
   *
   * colCount is a shared member. it's column for normal nodes and count for
   * column header nodes
   */
  int * left;
  int * right;
  int * up;
  int * down;
  int * colCount;
  
  /* used to collect the data neccessary to construct and for the iterative algorithm
   * the assembly, it contains the indexes to the selected rows
   * the columns array contains the indices of the coveres columns
   * the pos value contains the number of pieces placed
   */
  int * rows;
  int * columns;
  int pos;

  /* voxelindex is the invers of the function column. it returns
   * the index (not x, y, z) of a given column in the matrix
   */
  int * voxelindex;

  /* the message object that gets called with the solutions as param */
  assembler_cb * asm_bc;

  /* now this isn't hard to guess, is it? */
  int piecenumber;

  /* used to abbort the searching */
  bool abbort;

  /* used to save if the search is running */
  bool running;

  /* an array for each piece saying how often this one appears */
  int *multiPieceCount;
  int *multiPieceIndex;

  /* this array contains the index of the first node of the nodes that
   * belong to the piece of the array index given
   */
  int *pieceStart;

  /* cover one column:
   * - remove the column from the column header node list,
   * - remove all rows where the given column is 1
   */
  void cover(register int col);

  /* does the same ar cover but returns
   * false if one of the columns does now contain a 0
   */
  bool try_cover(register int col, int * columns);

  /* uncover the given column
   * this is the exact inverse operation of cover. It requires that the
   * maxtrix has the same state as after the corresponding cover.
   * so
   *
   * cover(i); uncover(i);
   *
   * will result in the same matrix as bevore
   */
  void uncover(register int col);

  /* 2 helper functions that cover and uncover one
   * selected row
   */
  void cover_row(register int r);
  void uncover_row(register int r);

  /* same as cover row, but using try_cover and abborting
   * as soon as one of the columns does contain a zero
   * and then uncovering all that was already done
   */
  bool try_cover_row(register int r);

  /* assm is used to contain the assembled puzzle
   * once an assembly has been found
   */
  voxel_c * assm;

  /* get's called when a solution is found. this function
   * then assembles the solution inside assm and calles the
   * callback function with assm as parameter
   */
  bool solution(void);

  int *nodeF;
  int *numF;
  int *pieceF;
  int *nodeB;
  int *numB;
  int *piece;
  int *searchState;
  void iterativeMultiSearch(void);

  /* this function creates the matrix for the search function
   * because we need to know how many nodes we need to allocate the
   * arrays with the right size, we add a parameter. if this is true
   * the function will not access the array but only count the number
   * of nodes used. this number is returned
   */
  void prepare(puzzle_c * puz, int res_filles, int res_vari);

  /* used by reduce to find out if the given position is a dead end
   * and will always lead to non solvable positions
   *
   * you have 2 parameters to control how thorough the process is carried out
   * here we have the following idea. if after placing one piece there is a
   * forced move somewhere (e.g. one piece can now be placed only in one
   * place, we could continue placing this piece and check if after that
   * we went into a cul-de-sac. rec gives tha number of pieces that are placed
   * maximally until we assume that it's not a dead end.
   *
   * the second value specifies the branc level, meaning: when we placed on piece
   * and now one other has only very few placements left, we could try all those
   * and check if all of them lead to dead ends. branch specifies how many placements
   * are maximally checked.
   *
   * be careful with these values as they may lead to extraordinary long calculation
   * times if set to high.
   *
   * good values are 3-5 for rec and 1 or 2 for branch level.
   *
   * it may a complete waste of time for some puzzles to call this function so you
   * have to decide
   *
   * the function returns the accumulated number of placements that were removed
   */
  bool checkmatrix(int rec, int branch);

  /* internal error state */
  int errorsState;
  int errorsParam;

  /* number of iterations the asseble routine run */
  unsigned long iterations;

  /* first and one after last column for the variable voxels */
  int varivoxelStart;
  int varivoxelEnd;

  /* the number of holes the assembles piece will have. Holes are
   * voxels in the variable voxel set that are not filled. The other
   * voxels are all filled
   */
  int holes;


public:

  assembler_0_c(const puzzle_c * puz);
  ~assembler_0_c();

  void reduce(void);

  unsigned long getIterations(void) { return iterations; }

  void assemble(assembler_cb * callback);

  /* the functions for the other process requesting information about the searcher */
  const char * errors(void);
  virtual float getFinished(void);
  virtual void stop(void) { abbort = true; }
  virtual bool stopped(void) { return !running; }
  virtual bool getPosition(char * string, int len);
  virtual void setPosition(char * string);
};

#endif
