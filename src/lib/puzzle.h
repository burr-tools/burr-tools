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

#include <vector>
#include <iostream>

#include <assert.h>

/**
 * a structure containing the necessary information for
 * one puzzle piece
 */
typedef struct {
  pieceVoxel_c * piece;      //!< the shape
  unsigned int count;   //!< how often this piece appears
} shapeInfo;

/**
 * This class defines the puzzle
 * A puzzle is a collection of pieces and a shape that the
 * pieces should form once they are assembled
 */
class puzzle_c {

private:

  /**
   * The vector with the piece information.
   */
  std::vector<shapeInfo> shapes;

  /**
   * The definition of the assembled block.
   */
  pieceVoxel_c * result;

public:

  /**
   * Load the puzzle from a file
   */
  puzzle_c(std::istream * str);

  puzzle_c(const puzzle_c * orig);

  /**
   * Constructor for empty puzzle, with empty
   * result and no shapes
   */
  puzzle_c(void) : result(new pieceVoxel_c(0, 0, 0)) { }

  /**
   * Destructor.
   * Deletes all the shapes in the puzzle
   */
  ~puzzle_c(void);

  /**
   * Adds a piece to the puzzle.
   * The class then takes over ownership of the piece and
   * deletes it whenever apropriate
   */
  void addShape(pieceVoxel_c * p, int nr = 1);

  /**
   * Adds an empty piece to the puzzle.
   */
  void addShape(int sx, int sy, int sz, int nr = 1);

  /**
   * Removes the the piece with number \c nr
   */
  void removeShape(unsigned int nr);

  /**
   * Returns the result shape
   */
  pieceVoxel_c * getResult(void) { return result; }
  const pieceVoxel_c * getResult(void) const { return result; }

  /**
   * Returns the the piece with number \c nr
   */
  pieceVoxel_c * getShape(unsigned int nr) {
    assert(nr < shapes.size());
    return shapes[nr].piece;
  }

  const pieceVoxel_c * getShape(unsigned int nr) const {
    assert(nr < shapes.size());
    return shapes[nr].piece;
  }

  /**
   * Returns the number of different shapes
   */
  int getShapeNumber(void) const { return shapes.size(); }

  /**
   * returns the number of times this piece is used
   * most often this will be one
   */
  unsigned int getShapeCount(unsigned int nr) const {
    assert(nr < shapes.size());
    return shapes[nr].count;
  }

  void setShapeCount(unsigned int nr, unsigned int count) {
    assert(nr < shapes.size());
    assert(count > 0);
    shapes[nr].count = count;
  }

  /**
   * Returns the number of pieces sum of counts of all shapes
   */
  int getPieces(void) const;

  /**
   * Print the puzzle to the screen
   */
  void print(void);

  /**
   * save the puzzle inside the stream
   */
  void save(std::ostream * str) const;

  /**
   * save in the format of the PuzzleSolver3D
   */
  void PS3Dsave(std::ostream * str) const;

  /**
   * makes each shape appear only once and increase the piece counter for that.
   * this is necessary for the assembler, as it will find multiple
   * solutions if this is not the case
   */
  void orthogonalize(void);
};

#endif
