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
#ifndef __DL_ASSEMBLER_H__
#define __DL_ASSEMBLER_H__

#include "assembler.h"

#include <vector>
#include <set>
#include <stack>

class gridType_c;
class mirrorInfo_c;

/* this class is an assembler class. It provides the dancing link implementation
 * for a real assembler an assembler frontend must be given
 *
 * when a new puzzle has to be solved the user calls createMatrix. This function
 * creates the internal data structures for the solver.
 *
 * whenever a solution is found the callback is called
 *
 * this class is a basic dancing link assembler, besides variable cells it is very
 * simple, no ranges or even multi pieces, assembler_1_c is responsible for those
 */

class assembler_0_c : public assembler_c {

protected:

  const problem_c * puzzle;

private:

  /* this are the members of the node. One array for each member. This
   * accelerates access.
   *
   * colCount is a shared member. It is column for normal nodes and count for
   * column header nodes
   */
  std::vector<unsigned int> left;
  std::vector<unsigned int> right;
  std::vector<unsigned int> upDown;   // this is special, is contains in reality 2 arrays interleaved
                                      // the even entries (0, 2, 4, ...) are up and the odd (1, 3, 5, ...) are down
                                      // I've done this to save a register for the assembly versions of the cover
                                      // and uncover functions and to speed them up by %
  std::vector<unsigned int> colCount;

  //to make access to the up and down vectors easier, the following 2 macros are provided
#define up(x) upDown[2*(x)]
#define down(x) upDown[2*(x)+1]

  /* used to abort the searching */
  bool abbort;

  /* used to save if the search is running */
  bool running;

  /* cover one column:
   * - remove the column from the column header node list,
   * - remove all rows where the given column is 1
   */
  void cover(unsigned int col);

  /* uncover the given column
   * this is the exact inverse operation of cover. It requires that the
   * matrix has the same state as after the corresponding cover.
   * so
   *
   * cover(i); uncover(i);
   *
   * will result in the same matrix as before
   */
  void uncover(unsigned int col);

  /* 2 helper functions that cover and uncover one
   * selected row
   */
  void cover_row(register unsigned int r);
  void uncover_row(register unsigned int r);

  /* same as cover row, but aborting
   * as soon as one of the columns does contain a zero
   * and then uncovering all that was already done
   */
  bool try_cover_row(register unsigned int r, unsigned int * columns);

  /* these 2 functions remove and reinsert rows from the matrix
   * they only remove the given row
   */
  void remove_row(register unsigned int r);
  void reinsert_row(register unsigned int r);

  void remove_column(register unsigned int c);

  /* this function gets called whenever an assembly was found
   * when a callback is available it will call getAssembly to
   * obtain the assembly for the found solution when the
   * field avoidTransformedAssemblies is true then the assembly
   * is checked, if it has been found before. The assembly
   * is normalized in inserted into a set of assemblies for
   * later reference
   */
  void solution(void);

  /* used to collect the data necessary to construct and for the iterative algorithm
   * the assembly, it contains the indexes to the selected rows
   * the columns array contains the indices of the covered columns
   * the pos value contains the number of pieces placed
   */
  unsigned int pos;
  unsigned int *rows;
  unsigned int *columns;

  void iterativeMultiSearch(void);

  /* this function checks, if the given piece can be placed
   * at the given position inside the result
   */
  bool canPlace(const voxel_c * piece, int x, int y, int z) const;

  /* this function creates the matrix for the search function
   * because we need to know how many nodes we need to allocate the
   * arrays with the right size, we add a parameter. If this is true
   * the function will not access the array but only count the number
   * of nodes used. This number is returned
   *
   * return error codes
   */
  int prepare(void);

  /* used by reduce to find out if the given position is a dead end
   * and will always lead to non solvable positions
   */
  bool checkmatrix(void);

  /* internal error state */
  errState errorsState;
  int errorsParam;

  /* number of iterations the assemble routine run */
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
    unsigned int piece;

    piecePosition(int x_, int y_, int z_, unsigned char transformation_, unsigned int row_, unsigned int pc) : x(x_), y(y_), z(z_),
      transformation(transformation_), row(row_), piece(pc) {}
  };
  std::vector<piecePosition> piecePositions;

  /* the members for rotations rejection
   */
  bool avoidTransformedAssemblies;
  unsigned int avoidTransformedPivot;
  mirrorInfo_c * avoidTransformedMirror;

  /// set to true, when complete rotation analysis is requested
  bool complete;

  /* the variables for debugging assembling processes
   */
  bool debug;         // debugging enabled
  int debug_loops;    // how many loops to run ?

  unsigned int clumpify(void);

protected:

  /* as this is only a back end doing the processing on the matrix, there needs to
   * be a front end creating the matrix and evaluating the results. These functions
   * are helpers for the front end
   */

  /* this function creates the first row of the matrix. As the createMatrix function
   * has already set up some variables you only need to specify the value res_filled that is
   * given to you as a parameter to the function prepare. You normally call this function
   * in prepare
   */
  void GenerateFirstRow(void);

  /* this function adds a node to the matrix that belongs to the first columns that represent
   * the pieces. This is normally the first thing you do, when you start a new line in the matrix
   * The information you provide is required to restore the exact piece in placement that this
   * line stands for
   * the return value is a number that has to be given to the voxel node creation routine
   * it contains the number of the node that is created with this function
   */
  int AddPieceNode(unsigned int piece, unsigned int rot, unsigned int x, unsigned int y, unsigned int z);

  /* this is in a way the inverse of the function above. You give a node number and get
   * the exact piece and placement the line this node belongs to stands for
   * this function is used in the solution function to restore the placement of the piece
   */
  void getPieceInformation(unsigned int node, unsigned char *tran, int *x, int *y, int *z, unsigned int *pc);

  /* this adds a normal node that represents a used voxel within the solution
   * piecenode is the number that you get from AddPieceNode, col is a number
   * that can be calculated from the x, y and z position of the voxel
   */
  void AddVoxelNode(unsigned int col, unsigned int piecenode);

  /* these functions provide access to the cover information for you */
  unsigned int getRows(int pos) { return rows[pos]; }
  unsigned int getRight(int pos) { return right[pos]; }
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
   * rotated assemblies found. Here a description of how the whole aspect of
   * rotation avoiding is supposed to work
   * the front end is supposed to initialize the assembler so that as few as
   * possible double assemblies are found by selecting one piece and not placing
   * this piece in all possible positions. But this will not always work, if
   * the front end is are not absolutely certain that it has avoided all possible
   * rotations it should call this function. This will then add an additional check
   * for each found assembly
   */
  void checkForTransformedAssemblies(unsigned int pivot, mirrorInfo_c * mir);

public:

  assembler_0_c(void);
  ~assembler_0_c(void);

  /* functions that are overloaded from assembler_c, for comments see there */
  errState createMatrix(const problem_c * puz, bool keepMirror, bool keepRotations, bool complete);
  void assemble(assembler_cb * callback);
  int getErrorsParam(void) { return errorsParam; }
  virtual float getFinished(void);
  virtual void stop(void) { abbort = true; }
  virtual bool stopped(void) const { return !running; }
  virtual errState setPosition(const char * string, const char * version);
  virtual void save(xmlWriter_c & xml) const;
  virtual void reduce(void);
  virtual unsigned int getReducePiece(void) { return reducePiece; }
  virtual unsigned long getIterations(void) { return iterations; }

  /* some more special information to find out possible piece placements */
  bool getPiecePlacementSupported(void) { return true; }
  unsigned int getPiecePlacement(unsigned int node, int delta, unsigned int piece, unsigned char *tran, int *x, int *y, int *z);
  unsigned int getPiecePlacementCount(unsigned int piece);

  void debug_step(unsigned long num = 1);
  assembly_c * getAssembly(void);

  static bool canHandle(const problem_c * p);
};

#endif
