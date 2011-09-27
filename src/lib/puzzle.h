/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
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

#include <stdint.h>
#include <vector>
#include <string>

#include <stdint.h>

class voxel_c;
class gridType_c;
class problem_c;
class xmlWriter_c;
class xmlParser_c;

/**
 * This class defines the puzzle.
 * A puzzle is a collection of shapes and a set of problems associated
 * with these shapes and finally the color used for color constraint colors
 */
class puzzle_c {

private:

  /** The gridtype of the puzzle.
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

  /** The constraint colours.
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
   * bool to signify if the comment should open on load.
   * so when the gui loads a puzzle where this is true, it
   * will always display the comment.
   */
  bool commentPopup;

public:

  /**
   * copy constructor this will NOT copy the labels and solutions of
   * the problems.
   */
  puzzle_c(const puzzle_c * orig);

  /**
   * Constructor for empty puzzle. no shape, no problem and no colours
   * ownership of the given gridtype is taken over, the memory
   * is freed on destruction of this class
   */
  puzzle_c(gridType_c * g) : gt(g), commentPopup(false) { }

  /**
   * load the puzzle from the XML file
   */
  puzzle_c(xmlParser_c & pars);

  /**
   * save the puzzle using the given XML-writer
   */
  void save(xmlWriter_c & xml) const;

  /**
   * Destructor.
   * Deletes all the shapes and problems in the puzzle
   */
  ~puzzle_c(void);

  /** \name some functions to get the current set grid type for this puzzle */
  //@{
  const gridType_c * getGridType(void) const { return gt; }
  gridType_c * getGridType(void) { return gt; }
  //@}


  /** \name shape handling */
  //@{
  /** Add the given shape to the puzzle.
   * The space is taken over, and freed when the puzzle is destroyed
   * Returns the index of the new shape
   */
  unsigned int addShape(voxel_c * p);
  /** add an empty shape of the given size return the index of the new shape */
  unsigned int addShape(unsigned int sx, unsigned int sy, unsigned int sz);
  /** return how many shapes there are in the puzzle */
  unsigned int getNumberOfShapes(void) const { return shapes.size(); }
  /** get a shape */
  const voxel_c * getShape(unsigned int idx) const { bt_assert(idx < shapes.size()); return shapes[idx]; }
  /** get a shape */
  voxel_c * getShape(unsigned int idx) { bt_assert(idx < shapes.size()); return shapes[idx]; }
  /**
   * remove the num-th shape.
   * be careful this changes all ids and so all problems must be updated
   * changing them, removing solutions, result shapes or pieces in problems...
   */
  void removeShape(unsigned int idx);
  /**
   *  exchange 2 shapes in the list of shapes.
   *  this function takes care to update all the problems and solutions
   *  because they only index into the shape list and exchangin shapes requires
   *  updating tose indices
   */
  void exchangeShapes(unsigned int s1, unsigned int s2);
  //@}


  /** \name  handle puzzle colours */
  //@{
  /** add a color, return the index of the new color */
  unsigned int addColor(unsigned char r, unsigned char g, unsigned char b);
  /**
   * remove a color with given index.
   * All shapes are updated to not use that
   * color any more, color constraints are updated for all problems to no
   * longer use that color, its your task to make sure the now invalid solutions
   * are removed
   */
  void removeColor(unsigned int idx);
  /** change the RGB value of one color */
  void changeColor(unsigned int idx, unsigned char r, unsigned char g, unsigned char b);
  /** get the RGB value of one color */
  void getColor(unsigned int idx, unsigned char * r, unsigned char * g, unsigned char * b) const;
  /** return the number of defined colors */
  unsigned int colorNumber(void) const { return colors.size(); }
  //@}


  /** \name handle problems */
  //@{
  /** add a new empty problem return its index */
  unsigned int addProblem(void);
  /** add a new problem as copy from another problem (from another puzzle).
   * A copy of the provided problem is created
   */
  unsigned int addProblem(const problem_c * prob);
  /** return the number of problems within this puzzle */
  unsigned int getNumberOfProblems(void) const { return problems.size(); }
  /** remove problem with the given index freeing all its ressources */
  void removeProblem(unsigned int p);
  /** exchange problem at indes p1 with problem at index p2 */
  void exchangeProblems(unsigned int p1, unsigned int p2);
  /** get the problem at index p */
  const problem_c * getProblem(unsigned int p) const { bt_assert(p < problems.size()); return problems[p]; }
  /** get the problem at index p */
  problem_c * getProblem(unsigned int p) { bt_assert(p < problems.size()); return problems[p]; }
  //@}


  /** \name the puzzle comment functions */
  //@{
  /** set comment there is no limitation in size or characters.  */
  void setComment(const std::string & com) { comment = com; }
  /** get comment */
  const std::string & getComment(void) const { return comment; }
  /** find out if the comment popup flas is set */
  bool getCommentPopup(void) const { return commentPopup; }
  /** set or reset comment popup flag */
  void setCommentPopup(bool val) { commentPopup = val; }
  //@}

private:

  // no copying and assigning
  puzzle_c(const puzzle_c&);
  void operator=(const puzzle_c&);

};

#endif
