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
#ifndef __ASSEMBLER_1_H__
#define __ASSEMBLER_1_H__

#include "assembler.h"

#include <vector>
#include <set>
#include <stack>

class puzzle_c;
class gridType_c;
class mirrorInfo_c;

/* this class is only a base class containing the functionality for the matrix
 * the functions in here don't know anything about the meaning of the matrix
 * for them it mainly consists of a set of one and empty nodes
 *
 * for a real puzzle assembler this class must be derived from and some functions
 * need to be overloaded. These are:
 *
 * countNodes, prepare and solution
 *
 * when a new puzzle has to be solved the user calls createMatrix. This function
 * does some initialisation and then calls prepare. Here you have to fill in
 * the matrix with your node (including the header).
 *
 * In this preparation you are asked to try to make the algorithm avoid finding
 * rotated solutions (its for your own speed). But if you can not do so, you
 * need to call checkForTransformedAssemblies(symBreakerPiece). This tells
 * this base class to check each found solution, if it may be a rotation of
 * another one and drop it automatically.
 *
 * When assembling the function in this class calls solution, whenever a solution
 * is found. It's now your task to transform the gained information into a format
 * valuable for your interpretation format.
 *
 */

class assembler_1_c : public assembler_c {

protected:

  const puzzle_c * puzzle;
  unsigned int problem;

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

  std::vector<int> rows;
  std::vector<unsigned int> finished_a;
  std::vector<unsigned int> finished_b;

  unsigned int headerNodes;  // number of nodes within the header

  bool open_column_conditions_fulfillable(void);
  int find_best_unclosed_column(void);
  void cover_column_only(int col);
  void uncover_column_only(int col);
  void cover_column_rows(int col);
  void uncover_column_rows(int col);
  void hiderow(int r);
  void unhiderow(int r);
  bool column_condition_fulfilled(int col);
  void rec1(void);
  void rec2(int next_row);
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
  virtual int prepare(int res_filles, int res_vari);

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
  void GenerateFirstRow(int unsigned res_filled);

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

public:

  assembler_1_c(assemblerFrontend_c * fe);
  ~assembler_1_c(void);

  /* functions that are overloaded from assembler_c, for comments see there */
  virtual errState createMatrix(const puzzle_c * puz, unsigned int problemNum);
  void assemble(assembler_cb * callback);
  int getErrorsParam(void) { return 0; }
  virtual float getFinished(void);
  virtual void stop(void) { abbort = true; }
  virtual bool stopped(void) const { return !running; }
  virtual errState setPosition(const char * string, const char * version);
  virtual xml::node save(void) const;
  virtual void reduce(void);

  /* gets called when a solution is found. This function
   * then assembles the solution and returns an assembly
   */
  assembly_c * getAssembly(void);

  static bool canHandle(const puzzle_c * p, unsigned int problem);

  /* some more special information to find out possible piece placements */
  bool getPiecePlacementSupported(void) { return true; }
  unsigned int getPiecePlacement(unsigned int node, int delta, unsigned int piece, unsigned char *tran, int *x, int *y, int *z);
  unsigned int getPiecePlacementCount(unsigned int piece);
};

#endif
