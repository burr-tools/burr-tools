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
 * with these shapes
 * each problem defines a solution shape and a set of pieces (multiple
 * occurrences are possible) that need to be assembles into the given solution
 * shape
 * the class also handles the solutions that belong to the problem
 */
class puzzle_c {

private:

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
   */
  typedef struct colorDef {
    unsigned char r, g, b;
  } colorDef;

  std::vector<colorDef> colors;

  /* some information about the puzzle */
  std::string comment;
  /* flat to signify if the comment should open on load */
  bool commentPopup;

public:

  /**
   * copy constructor
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

  /* used to save to XML */
  xml::node save(void) const;

  /**
   * Destructor.
   * Deletes all the shapes in the puzzle
   */
  ~puzzle_c(void);

  /**
   * return the current set grid type for this puzzle
   */
  const gridType_c * getGridType(void) const { return gt; }
  gridType_c * getGridType(void) { return gt; }

  /**
   * set a new gridType, you should really know what you do, when
   * you call this function, as it may change a lot
   */
  void setGridType(gridType_c * newGt);

  /**
   * add a shape to the puzzle
   */
  unsigned int addShape(voxel_c * p);

  /* add empty shape of given size */
  unsigned int addShape(int sx, int sy, int sz);

  /* return the pointer to voxel space with the id */
  const voxel_c * getShape(unsigned int idx) const { bt_assert(idx < shapes.size()); return shapes[idx]; }
  voxel_c * getShape(unsigned int idx) { bt_assert(idx < shapes.size()); return shapes[idx]; }

  /* remove the num-th shape
   * be careful this changes all ids and so all problems must be updated
   */
  void removeShape(unsigned int);

  /* return how many shapes there are */
  unsigned int shapeNumber(void) const { return shapes.size(); }

  /* exchange 2 shapes in the list of shapes */
  void exchangeShape(unsigned int s1, unsigned int s2);

  /**
   * handle puzzle colours
   */
  void addColor(unsigned char r, unsigned char g, unsigned char b);
  void removeColor(unsigned int idx);
  void changeColor(unsigned int idx, unsigned char r, unsigned char g, unsigned char b);
  void getColor(unsigned int idx, unsigned char * r, unsigned char * g, unsigned char * b) const;
  unsigned int colorNumber(void) const { return colors.size(); }

  /* add a new empty problem */
  unsigned int addProblem(void);

  /* return number of problems */
  unsigned int problemNumber(void) const { return problems.size(); }

  /* remove one problem */
  void removeProblem(unsigned int p);

  /* create copy of the given problem and add the new problem at the end */
  unsigned int copyProblem(unsigned int);

  /* exchange problem at indes p1 with problem at index p2 */
  void exchangeProblem(unsigned int p1, unsigned int p2);

  /* get the problem, don't keep the pointer as the problem
   * might get deleted from the puzzle
   */
  const problem_c * getProblem(unsigned int p) const { bt_assert(p < problems.size()); return problems[p]; }
  problem_c * getProblem(unsigned int p) { bt_assert(p < problems.size()); return problems[p]; }

  /* some additional information about the puzzle */
  void setComment(const std::string & com) { comment = com; }
  const std::string & getComment(void) const { return comment; }

  /* a flag for the comment, if set it is supposed that
   * the comment will open when the file is loaded within the gui
   */
  bool getCommentPopup(void) const { return commentPopup; }
  void setComemntPopup(bool val) { commentPopup = val; }
};

#endif
