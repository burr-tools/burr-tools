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

#include <xmlwrapp/xmlwrapp.h>


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
  std::vector<pieceVoxel_c*> results;

  /**
   * there can be many colors for contrain the placement of pieces
   * this are the actual colors used to display them
   */
  typedef struct colorDef {
    unsigned char r, g, b;
  } colorDef;

  std::vector<colorDef> colors;

  /* the 2d bitmap
   * the bitmap is always square and alows only for the here necessary modifications
   */
  
  class bitmap_c {
  
  private:
  
    unsigned char *map;
    unsigned int colors;
  
  public:
  
    /* create new bitmap with size rows and columns, all bit are cleared */
    bitmap_c(unsigned int col);
  
    ~bitmap_c(void) { if (map) delete [] map; }
  
    /* add a new color at the end */
    void add(void);
  
    /* remove the given color */
    void remove(unsigned int col);
  
    void set(unsigned int pcCol, unsigned int resCol, bool value) {
  
      assert(pcCol < colors);
      assert(resCol < colors);
  
      int idx = resCol * colors + pcCol;
  
      if (value)
        map[idx >> 3] |= (1 << (idx & 7));
      else
        map[idx >> 3] &= ~(1 << (idx & 7));
    }
  
    bool get(unsigned int pcCol, unsigned int resCol) const {
      assert(pcCol < colors);
      assert(resCol < colors);
  
      int idx = resCol * colors + pcCol;
  
      return (map[idx >> 3] & (1 << (idx & 7)) != 0);
    }
  };



  /**
   * this array contains the constrains for the colors for each pair of
   * colors is defines, if it is allowed to place a voxel inside a piece shape
   * at the result where the corresponding voxel has a certain color
   */
  bitmap_c colorConstraints;

public:

  /**
   * Load the puzzle from a file
   */
  puzzle_c(std::istream * str);

  puzzle_c(const puzzle_c * orig);

  /**
   * Constructor for empty puzzle, with no result and and no shapes
   */
  puzzle_c(void) : colorConstraints(0) {

    results.push_back(new pieceVoxel_c(0, 0, 0));

  }

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
   * Returns the num-th result shape
   */
  pieceVoxel_c * getResult(int num) {
    assert(num < results.size());
    return results[num];
  }
  const pieceVoxel_c * getResult(int num) const {
    assert(num < results.size());
    return results[num];
  }

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

  /* used to save to XML */
  xml::node save(void) const;


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

  /* color constrain handling functions */

  void addColor(void);
  void removeColor(unsigned int col);
  void setColor(unsigned int col, unsigned char r, unsigned char g, unsigned char b);
  void allowPlacement(unsigned int pc, unsigned int res);
  void disallowPlacement(unsigned int pc, unsigned int res);
  bool placementAllowed(unsigned int pc, unsigned int res) const;
};

#endif
