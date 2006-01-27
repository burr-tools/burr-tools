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


#ifndef __DL_ASSEMBLER_H__
#define __DL_ASSEMBLER_H__

#include "assembler.h"

#include "voxel.h"
#include "puzzle.h"

#include <vector>
#include <set>

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
 */


/* this class is only a base class containing the functionality for the matrix
 * the functions in here don't know anything about the meaning of the matrix
 * for them it mainly consists of a set of one and empty nodes
 *
 * for a real puzzle assemlber this class must be dereved from and some functions
 * need to be overloaded. These are:
 *
 * countNodes, prepare and solution
 *
 * when a new puzzle has to be solved the user calls createMatrix. This function
 * does some initialisation and then calls countNodes. You have to return the number
 * of nodes that you require for your part of the matrix. The createMatrix function
 * then allocaed enough memory for this and calls prepare. Here you have to fill in
 * the matrix with your node (including the header.
 *
 * When assembling the function in this class calls solution, whenever a solution
 * is found. It's now your task to transform the gained information into a format
 * valuable for your interpretation format.
 *
 * there is one assumption in the class: empty voxels don't exist. So you have to find
 * an interpretation for your space that doesn't require empty voxels within the 3d
 * space of the voxels
 */

class assembler_0_c : public assembler_c {

private:

  const puzzle_c * puzzle;
  unsigned int problem;

  /* this are the members of the node. One array for each member. This
   * accellerates access.
   *
   * colCount is a shared member. it's column for normal nodes and count for
   * column header nodes
   */
  std::vector<unsigned int> left;
  std::vector<unsigned int> right;
  std::vector<unsigned int> up;
  std::vector<unsigned int> down;
  std::vector<unsigned int> colCount;

  /* used to abbort the searching */
  bool abbort;

  /* used to save if the search is running */
  bool running;

  /* an array for each piece saying how often this one appears */
  unsigned int *multiPieceCount;
  unsigned int *multiPieceIndex;

  /* this array contains the index of the first node of the nodes that
   * belong to the piece of the array index given
   */
  unsigned int *pieceStart;

  /* cover one column:
   * - remove the column from the column header node list,
   * - remove all rows where the given column is 1
   */
  void cover(register unsigned int col);

  /* does the same ar cover but returns
   * false if one of the columns does now contain a 0
   */
  bool try_cover(register unsigned int col, unsigned int * columns);

  /* uncover the given column
   * this is the exact inverse operation of cover. It requires that the
   * maxtrix has the same state as after the corresponding cover.
   * so
   *
   * cover(i); uncover(i);
   *
   * will result in the same matrix as bevore
   */
  void uncover(register unsigned int col);

  /* 2 helper functions that cover and uncover one
   * selected row
   */
  void cover_row(register unsigned int r);
  void uncover_row(register unsigned int r);

  /* same as cover row, but using try_cover and abborting
   * as soon as one of the columns does contain a zero
   * and then uncovering all that was already done
   */
  bool try_cover_row(register unsigned int r);

  /* get's called when a solution is found. this function
   * then assembles the solution and returns an assembly
   */
  assembly_c * getAssembly(void);

  /* this function gets called whenever an assembly was found
   * when a callback is avaliable it will call getAssembly to
   * obtain the assembly for the found solution when the
   * field avoidTransformedAssemblies is true then the assembly
   * is checked, if it has been found bevore. The assembly
   * is normalized in inserted into a set of assemblies for
   * later reference
   */
  void solution(void);

  /* used to collect the data neccessary to construct and for the iterative algorithm
   * the assembly, it contains the indexes to the selected rows
   * the columns array contains the indices of the coveres columns
   * the pos value contains the number of pieces placed
   */
  unsigned int pos;
  unsigned int *rows;
  unsigned int *columns;
  unsigned int *nodeF;
  unsigned int *numF;
  unsigned int *pieceF;
  unsigned int *nodeB;
  unsigned int *numB;
  unsigned int *piece;
  unsigned int *searchState;
  void iterativeMultiSearch(void);

  /* this function creates the matrix for the search function
   * because we need to know how many nodes we need to allocate the
   * arrays with the right size, we add a parameter. if this is true
   * the function will not access the array but only count the number
   * of nodes used. this number is returned
   *
   * return error codes
   */
  virtual int prepare(const puzzle_c * puz, int res_filles, int res_vari, unsigned int problemNum) = 0;

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
  bool checkmatrix(unsigned int rec, unsigned int branch);

  /* internal error state */
  errState errorsState;
  int errorsParam;

  /* number of iterations the asseble routine run */
  unsigned long iterations;

  /* the number of holes the assembles piece will have. Holes are
   * voxels in the variable voxel set that are not filled. The other
   * voxels are all filled
   */
  int holes;

  /* first and one after last column for the variable voxels */
  unsigned int varivoxelStart;
  unsigned int varivoxelEnd;

  /* now this isn't hard to guess, is it? */
  unsigned int piecenumber;

  /* the message object that gets called with the solutions as param */
  assembler_cb * asm_bc;

  /* this value contains the piecenumber that the reduce procedure is currently working on
   * the value is only valid, when reduce is running
   */
  unsigned int reducePiece;

  /* this vector contains the placement (transformation and position) for
   * a piece in a row
   */
  class piecePosition {

  public:

    int x, y, z;
    unsigned char transformation;
    unsigned int row;            // first node in this row

    piecePosition(int x_, int y_, int z_, unsigned char transformation_, unsigned int row_) : x(x_), y(y_), z(z_),
      transformation(transformation_), row(row_) {}
  };
  std::vector<piecePosition> piecePositions;

  /* the members for rotations avoidment
   */
  bool avoidTransformedAssemblies;
  unsigned int avoidTransformedPivot;


protected:

  /* as this is only a backend doing the processing on the matrix, there needs to
   * be a frontend creating the matrix and evaluating the results. These functions
   * are helpers for the frontend
   */

  /* this function creates the first row of the matrix. As the createMatrix function
   * has already set up some variables you only need to specify the value res_filled that is
   * given to you as a parameter to the function prepare. You normally call this function
   * in prepare
   */
  void GenerateFirstRow(int unsigned res_filled);

  /* call this whenever you start to add information for a new piece */
  void nextPiece(unsigned int piece, unsigned int count, unsigned int number);

  /* this function adds a node to the matrix that belongs to the first columns that represent
   * the pieces. This is normally the first thing you do, when you start a new line in the matrix
   * The information you provide is required to restore the exact piece in placement that this
   * line stands for
   * the return value is a number that has to be given to the voxel node creation routine
   * it contains the number of the node that is created with this function
   */
  int AddPieceNode(unsigned int piece, unsigned int rot, unsigned int x, unsigned int y, unsigned int z);

  /* this is in a way the inverse of the function above. You give a node number and get
   * the exact piece and placement the line this node belonges to stands for
   * this function is used in the solution function to restore the placement of the piece
   */
  void getPieceInformation(unsigned int node, unsigned char *tran, int *x, int *y, int *z);

  /* this adds a normal node that represents a used voxel within the solution
   * piecenode is the number that you get from AddPieceNode, col is a number
   * that can be calculated from the x, y and z position of the voxel
   */
  void AddVoxelNode(unsigned int col, unsigned int piecenode);

  /* these functions provde access to the cover information for you */
  unsigned int getRows(int pos) { return rows[pos]; }
  unsigned int getRight(int pos) { return right[pos]; }
  unsigned int getPiece(int pos) { return piece[pos]; }
  unsigned int getColCount(int pos) { return colCount[pos]; }
  unsigned int getVarivoxelStart(void) { return varivoxelStart; }
  unsigned int getPos(void) { return pos; }

  /* finally after assembling a puzzle and creating something meaningful from the cover
   * information you need to call the callback of the user, use this function to get the
   * callback class
   */
  assembler_cb * getCallback(void) { return asm_bc; }

  unsigned int getPiecenumber(void) { return piecenumber; }

  /* call this function if you think that there might be
   * rotated assemblies found. Here a description of how the whole aspekt of
   * rotation avoiding is suppoed to work
   * the frontend is supposed to initialize the assembler so that as few as
   * possible double assemblies are found by selecting one piece and not placing
   * this piece in all possible positions. But this will not always work, if
   * the frontend is are not absolutely certain that it has avoided all possible
   * rotations it should call this funtion. This will then add an additional check
   * for each found assembly
   */
  void checkForTransformedAssemblies(unsigned int pivot);

public:

  assembler_0_c(void);
  ~assembler_0_c(void);

  /* functions that are overloaded from assembler_c, for comments see there */
  virtual errState createMatrix(const puzzle_c * puz, unsigned int problemNum);
  void assemble(assembler_cb * callback);
  int getErrorsParam(void) { return errorsParam; }
  virtual float getFinished(void);
  virtual void stop(void) { abbort = true; }
  virtual bool stopped(void) const { return !running; }
  virtual errState setPosition(const char * string, const char * version);
  virtual xml::node save(void) const;
  virtual void reduce(void);
  virtual unsigned int getReducePiece(void) { return reducePiece; }
  virtual unsigned long getIterations(void) { return iterations; }

  /* some more special information to find out possible piece placements */
  unsigned int getPiecePlacement(unsigned int node, int delta, unsigned int piece, unsigned char *tran, int *x, int *y, int *z);
  unsigned int getPiecePlacementCount(unsigned int piece);
};

#endif
