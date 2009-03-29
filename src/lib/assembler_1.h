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
#ifndef __ASSEMBLER_1_H__
#define __ASSEMBLER_1_H__

#include "assembler.h"

#include <vector>
#include <set>
#include <stack>

class problem_c;
class gridType_c;
class mirrorInfo_c;

/**
 * This class is an assembler class.
 *
 * This assembler is written with ideas from Wei-Hwa Huang. It can handle ranges for the piece
 * numbers and thus also multiple instances of one piece.
 *
 * But for simple cases it is not really optimal.
 *
 * It also has a problem with guessing how much of the analysis it done. This number is growing exponentially
 * meaning in the beginning it is growing very slowly resulting in huge time-left numbers while at the
 * end it is getting very fast and the time-left value dropping really fast.
 */

class assembler_1_c : public assembler_c {

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
  std::vector<unsigned int> up;
  std::vector<unsigned int> down;
  std::vector<unsigned int> colCount;
  std::vector<unsigned int> weight;
  std::vector<unsigned int> min;
  std::vector<unsigned int> max;

  /* this vector contains all columns that are used for the hole
   * optimisation: up to "holes" instances of these columns might
   * be zero
   */
  std::vector<unsigned int> holeColumns;
  unsigned int holes;

  /* this function gets called whenever an assembly was found
   * when a callback is available it will call getAssembly to
   * obtain the assembly for the found solution when the
   * field avoidTransformedAssemblies is true then the assembly
   * is checked, if it has been found before. The assembly
   * is normalized in inserted into a set of assemblies for
   * later reference
   */
  void solution(void);

  /* used to abort the searching */
  bool abbort;

  /* used to save if the search is running */
  bool running;

  std::vector<unsigned int> rows;
  std::vector<unsigned int> finished_a;
  std::vector<unsigned int> finished_b;
  std::vector<unsigned int> hidden_rows;  // rows that nodes to rows that are currently hidden
  // because there are several batched of rows that need hiding these batches are separated
  // by a zero because the header row will never get hidden...
  std::vector<unsigned int>task_stack;
  std::vector<unsigned int>next_row_stack;
  std::vector<unsigned int>column_stack;

  unsigned int headerNodes;  // number of nodes within the header

  bool open_column_conditions_fulfillable(void);
  int find_best_unclosed_column(void);
  void cover_column_only(int col);
  void uncover_column_only(int col);
  void cover_column_rows(int col);
  void uncover_column_rows(int col);
  void hiderow(int r);
  void unhiderow(int r);
  void hiderows(unsigned int r);
  void unhiderows(void);
  bool column_condition_fulfilled(int col);
  bool column_condition_fulfillable(int col);
//  void rec(unsigned int next_row);
  void iterative(void);
  void remove_row(register unsigned int r);
  void remove_column(register unsigned int c);
  unsigned int clumpify(void);


  /**
   * this function is called by the default implementation of prepare
   * to check, if the piece fits at the given position
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
  int prepare(bool hasRange, unsigned int rangeMin, unsigned int rangeMax);

  /* internal error state */
  errState errorsState;
  int errorsParam;

  /* now this isn't hard to guess, is it? */
  unsigned int piecenumber;

  /* the message object that gets called with the solutions as param */
  assembler_cb * asm_bc;

  /* this vector contains the placement (transformation and position) for
   * a piece in a row
   */
  class piecePosition {

  public:

    int x, y, z;
    unsigned char transformation;
    unsigned int row;            // first node in this row
    unsigned int piece;

    piecePosition(unsigned int pc_, int x_, int y_, int z_, unsigned char transformation_, unsigned int row_) : x(x_), y(y_), z(z_),
      transformation(transformation_), row(row_), piece(pc_) {}
  };
  std::vector<piecePosition> piecePositions;

  /* the members for rotations rejection
   */
  bool avoidTransformedAssemblies;
  unsigned int avoidTransformedPivot;
  mirrorInfo_c * avoidTransformedMirror;

  /// set to true, when complete analysis is requested
  bool complete;

  /* the variables for debugging assembling processes
   */
  bool debug;         // debugging enabled
  int debug_loops;    // how many loops to run ?

  unsigned long iterations;

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
  void GenerateFirstRow(unsigned int res_filled);

  /* this function adds a node to the matrix that belongs to the first columns that represent
   * the pieces. This is normally the first thing you do, when you start a new line in the matrix
   * The information you provide is required to restore the exact piece in placement that this
   * line stands for
   * the return value is a number that has to be given to the voxel node creation routine
   * it contains the number of the node that is created with this function
   */
  int AddPieceNode(unsigned int piece, unsigned int rot, unsigned int x, unsigned int y, unsigned int z);

  /* adds a node with a vertain weight, for piece range node counting
   */
  void AddRangeNode(unsigned int col, unsigned int piecenode, unsigned int weight);


  /* this is in a way the inverse of the function above. You give a node number and get
   * the exact piece and placement the line this node belongs to stands for
   * this function is used in the solution function to restore the placement of the piece
   */
  void getPieceInformation(unsigned int node, unsigned int * piece, unsigned char *tran, int *x, int *y, int *z);

  /* this adds a normal node that represents a used voxel within the solution
   * piecenode is the number that you get from AddPieceNode, col is a number
   * that can be calculated from the x, y and z position of the voxel
   */
  void AddVoxelNode(unsigned int col, unsigned int piecenode);

  /* these functions provide access to the cover information for you */
  unsigned int getRight(int pos) { return right[pos]; }
  unsigned int getColCount(int pos) { return colCount[pos]; }

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

  unsigned int reducePiece;

public:

  assembler_1_c(void);
  ~assembler_1_c(void);

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
  void debug_step(unsigned long num = 1);
  assembly_c * getAssembly(void);

  static bool canHandle(const problem_c * p);

  /* some more special information to find out possible piece placements */
  bool getPiecePlacementSupported(void) { return true; }
  unsigned int getPiecePlacement(unsigned int node, int delta, unsigned int piece, unsigned char *tran, int *x, int *y, int *z);
  unsigned int getPiecePlacementCount(unsigned int piece);
  unsigned long getIterations(void) { return iterations; }
};

#endif
