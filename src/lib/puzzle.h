/* Burr Solver
 * Copyright (C) 2003-2008  Andreas Röver
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

#include "bt_assert.h"

#include <vector>

#include <xmlwrapp/node.h>

class voxel_c;
class gridType_c;
class problem_c;

/**
 * This class defines the puzzle
 * A puzzle is a collection of shapes and a set of problems associated
 * with these shapes and finally the color used for color constraint colors
 */
class puzzle_c {

private:

  /**
   * each puzzle has exactly one grid type, this is it
   * normally it doesn't change, except if you convert
   * a puzzle of one grid type into an other with a converter
   */
  gridType_c *gt;

  /**
   * The vector with the shapes
   */
  std::vector<voxel_c*> shapes;

  /**
   * the vector with the problems
   */
  std::vector<problem_c*> problems;

  /**
   * there can be many colours to constrain the placement of pieces
   * these are the actual colours used to display them
   * the red part is in the lowest 8 bit, followed by green and blue in the
   * highest part
   */
  std::vector<uint32_t> colors;

  /**
   * some information about the puzzle
   * no format is forced its free for the user
   */
  std::string comment;

  /**
   * bool to signify if the comment should open on load
   * so when the gui loads a puzzle where this is true, it
   * will always display the comment.
   */
  bool commentPopup;

public:

  /**
   * copy constructor this will NOT copy the labels and solutions of
   * the problems...
   */
  puzzle_c(const puzzle_c * orig);

  /**
   * Constructor for empty puzzle, no shape, no problem and no colours
   * ownership of the given gridtype is taken over, the memory
   * is freed on destruction of this class
   */
  puzzle_c(gridType_c * g) : gt(g) { }

  /**
   * load the puzzle from the XML file
   */
  puzzle_c(const xml::node & node);

  /**
   * save the puzzle into a XML node that is returned
   */
  xml::node save(void) const;

  /**
   * Destructor.
   * Deletes all the shapes in the puzzle
   */
  ~puzzle_c(void);

  /**
   * fome functions to set and return the current set grid type for this puzzle
   * if you set a new gridType, you should really know what you do
   * it may break a lot
   */
  const gridType_c * getGridType(void) const { return gt; }
  gridType_c * getGridType(void) { return gt; }
  void setGridType(gridType_c * newGt);

  /**
   * functions to add shapes to the puzzle
   * - add a shape, the voxel space is taken over, be sure the space has the right
   *   grid type there can be no check for that. The space is taken over, and freed
   *   when the puzzle is destroyed
   * - the 2nd function adds empty shape of given size
   * all functions return the index of the added shape
   */
  unsigned int addShape(voxel_c * p);
  unsigned int addShape(unsigned int sx, unsigned int sy, unsigned int sz);

  /**
   *  return how many shapes there are in the puzzle
   */
  unsigned int shapeNumber(void) const { return shapes.size(); }

  /**
   *  return the pointer to voxel space with the given index
   */
  const voxel_c * getShape(unsigned int idx) const { bt_assert(idx < shapes.size()); return shapes[idx]; }
  voxel_c * getShape(unsigned int idx) { bt_assert(idx < shapes.size()); return shapes[idx]; }

  /* remove the num-th shape
   * be careful this changes all ids and so all problems must be updated
   */
  void removeShape(unsigned int);

  /**
   *  exchange 2 shapes in the list of shapes
   *  this function takes care to update all the problems and solutions
   *  because they only index into the shape list and exchangin shapes requires
   *  updating tose indices
   */
  void exchangeShape(unsigned int s1, unsigned int s2);

  /**
   * handle puzzle colours
   * - add a color, return the index of the new color
   * - remove a color with given index, all shapes are updated to not use that
   *   color any more, color constraints are updated for all problems to no
   *   longer use that color, its your task to make sure the now invalid solutions
   *   are removed
   * - change the RGB value of one color
   * - get the RGB value of one color
   * - return the number of defined colors
   */
  unsigned int addColor(unsigned char r, unsigned char g, unsigned char b);
  void removeColor(unsigned int idx);
  void changeColor(unsigned int idx, unsigned char r, unsigned char g, unsigned char b);
  void getColor(unsigned int idx, unsigned char * r, unsigned char * g, unsigned char * b) const;
  unsigned int colorNumber(void) const { return colors.size(); }

  /**
   * handle problems:
   * - add a new empty problem return its index
   * - return the number of problems within this puzzle
   * - remove problem with the given index freeing all its ressources
   * - copy a problem, doubling all except the label and the solutions
   * - exchange problem at indes p1 with problem at index p2
   * - get the problem at index p
   */
  unsigned int addProblem(void);
  unsigned int problemNumber(void) const { return problems.size(); }
  void removeProblem(unsigned int p);
  unsigned int copyProblem(unsigned int);
  void exchangeProblem(unsigned int p1, unsigned int p2);
  const problem_c * getProblem(unsigned int p) const { bt_assert(p < problems.size()); return problems[p]; }
  problem_c * getProblem(unsigned int p) { bt_assert(p < problems.size()); return problems[p]; }

  /**
   * the puzzle comment functions:
   * - set comment there is no limitation in size or characters...
   * - get comment
   */
  void setComment(const std::string & com) { comment = com; }
  const std::string & getComment(void) const { return comment; }

  /**
   * a flag for the comment, if set to true it is supposed that
   * the comment will open when the file is loaded within the gui
   */
  bool getCommentPopup(void) const { return commentPopup; }
  void setComemntPopup(bool val) { commentPopup = val; }
};

#endif
