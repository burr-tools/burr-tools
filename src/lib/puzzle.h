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


#ifndef __PUZZLE_H__
#define __PUZZLE_H__

/** @ file
 * Contains the definitions for the puzzle class
 */

#include "voxel.h"
#include "disassembly.h"
#include "assembly.h"

#include <vector>
#include <iostream>

#include <assert.h>

#include <xmlwrapp/node.h>


class problem_c;
class assembler_c;

/**
 * This class defines the puzzle
 * A puzzle is a collection of shapes and a set of problems associated
 * with these shapes
 * each problem defines a solution shape and a set of pieces (multiple
 * occurances are possible) that need to be assembles into the given solution
 * shape
 * the class also handles the solutions that belong to the problem
 */
class puzzle_c {

private:

  /**
   * The vector with the shapes
   */
  std::vector<pieceVoxel_c*> shapes;

  /**
   * the vector with the problems
   */
  std::vector<problem_c*> problems;

  /**
   * there can be many colors to contrain the placement of pieces
   * these are the actual colors used to display them
   */
  typedef struct colorDef {
    unsigned char r, g, b;
  } colorDef;

  std::vector<colorDef> colors;

  /* some information about the puzzle */
  std::string designer;
  std::string comment;

public:

  /**
   * copy constructor
   */
  puzzle_c(const puzzle_c * orig);

  /**
   * Constructor for empty puzzle, no shape, no problem and no colors
   */
  puzzle_c(void);

  /**
   * load the puzzle from the XML file
   */
  puzzle_c(const xml::node & node);

  /* used to save to XML */
  xml::node save(void) const;


  /**
   * Destructor.
   * Deletes all the shapes in the puzzle
   */
  ~puzzle_c(void);

  /**
   * add a shape to the puzzle
   */
  unsigned int addShape(pieceVoxel_c * p);

  /* add empty shape of given size */
  unsigned int addShape(int sx, int sy, int sz);

  /* return the pointer to voxel space with the id */
  const pieceVoxel_c * getShape(unsigned int) const;
  pieceVoxel_c * getShape(unsigned int);

  /* remove the num-th shape
   * be careful this changes all ids and so all problems must be updated
   */
  void removeShape(unsigned int);

  /* return how many shapes there are */
  unsigned int shapeNumber(void) const;

  /**
   * handle puzzle colors
   */
  void addColor(unsigned char r, unsigned char g, unsigned char b);
  void removeColor(unsigned int idx);
  void changeColor(unsigned int idx, unsigned char r, unsigned char g, unsigned char b);
  void getColor(unsigned int idx, unsigned char * r, unsigned char * g, unsigned char * b) const;
  unsigned int colorNumber(void) const;


  /**
   * similar functions for problems
   */
  unsigned int addProblem(void);

  /* return number of problems */
  unsigned int problemNumber(void) const;

  /* remove one problem */
  void removeProblem(unsigned int);

  /* create copy of the given problem and add the new problem at the end */
  unsigned int copyProblem(unsigned int);

  /* name of a problem */
  const std::string & probGetName(unsigned int prob) const;
  void probSetName(unsigned int prob, std::string name);

  /* set the shape-id for the result shape this the problem */
  void probSetResult(unsigned int prob, unsigned int shape);

  /* get the id for the result shape */
  unsigned int probGetResult(unsigned prob) const;

  /* get the result shape voxel space */
  const pieceVoxel_c * probGetResultShape(unsigned int prob) const;
  pieceVoxel_c * probGetResultShape(unsigned int prob);

  /* add a shape to the pieces of the problem */
  void probAddShape(unsigned int prob, unsigned int shape, unsigned int count);

  /* change the instance count for one shape of the problem */
  void probSetShapeCount(unsigned int prob, unsigned int shapeID, unsigned int count);

  /* remove the shape from the problem */
  void probRemoveShape(unsigned int prob, unsigned int shapeID);

  /* return the number of shapes in the problem */
  unsigned int probShapeNumber(unsigned int prob) const;

  /* return the number of pieces in the problem (sum of all counts of all shapes */
  unsigned int probPieceNumber(unsigned int prob) const;

  /* return the shape id of the given shape (index into the shape array of the puzzle */
  unsigned int probGetShape(unsigned int prob, unsigned int shapeID) const;

  /* return the voxel of the piece shape */
  const pieceVoxel_c * probGetShapeShape(unsigned int prob, unsigned int shapeID) const;
  pieceVoxel_c * probGetShapeShape(unsigned int prob, unsigned int shapeID);

  /* return the instance count for one shape of the problem */
  unsigned int probGetShapeCount(unsigned int prob, unsigned int shapeID) const;

  /* functions to handle the solver and the solutions */

  void probSetAssembler(unsigned int prob, assembler_c * assm);
  assembler_c * probGetAssembler(unsigned int prob);
  const assembler_c * probGetAssembler(unsigned int prob) const;

  void probAddSolution(unsigned int prob, assembly_c * voxel);
  void probAddSolution(unsigned int prob, assembly_c * voxel, separation_c * tree);
  void probRemoveAllSolutions(unsigned int prob);
  unsigned int probSolutionNumber(unsigned int prob) const;
  assembly_c * probGetAssembly(unsigned int prob, unsigned int sol);
  separation_c * probGetDisassembly(unsigned int prob, unsigned int sol);

  const assembly_c * probGetAssembly(unsigned int prob, unsigned int sol) const;
  const separation_c * probGetDisassembly(unsigned int prob, unsigned int sol) const;

  /* edit the color matrix */
  void probAllowPlacement(unsigned int prob, unsigned int pc, unsigned int res);
  void probDisallowPlacement(unsigned int prob, unsigned int pc, unsigned int res);
  bool probPlacementAllowed(unsigned int prob, unsigned int pc, unsigned int res) const;

  /* some additional information about the puzzle */
  void setDesigner(const std::string & name);
  void setComment(const std::string & comment);
  const std::string & getDesigner(void) const;
  const std::string & getComment(void) const;

  /**
   * makes each shape appear only once and increase the piece counter for that.
   * this is necessary for the assembler, as it will find multiple
   * solutions if this is not the case
   */
  void orthogonalize(void);
};

#endif
