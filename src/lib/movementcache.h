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
#ifndef __MOVEMENTCACHE_H__
#define __MOVEMENTCACHE_H__

class voxel_c;
class problem_c;
class gridType_c;

/**
 * Calculates and stores the information required for movement analysis
 *
 * The movement cache calculates the following value:
 * - How far can one piece be moved relative to another piece, used by normal movement analysis
 *
 * Because calculating this information is expensive all the values
 * gained are stored in a table for later retrieval. That is why it
 * is called cache
 *
 * Internally this class holds a hash table for the stored values and
 * calls abstract functions for the calculation of the values, when they
 * are not inside the table.
 *
 * So only the derived classes do actually calculate something.
 */
class movementCache_c {

  private:

  /**
   * values are saved within a hash table, this is the entry for the table for the movement data
   */
  typedef struct moEntry {

    int dx; ///< relative x position of the 2nd piece
    int dy; ///< relative y position of the 2nd piece
    int dz; ///< relative z position of the 2nd piece

    unsigned int s1; ///< id of the first involved shape
    unsigned int s2; ///< id of the second involved shape

    /* the transformations of the 2 involved pieces
     * normally we would need only one transformation, that for piece 2
     * but the calculations involved to transform the 2 pieces so that
     * piece one has a fixed transformation are too expensive
     */
    unsigned short t1; ///< orientation of the first shape
    unsigned short t2; ///< orientation of the second shape

    /** the possible movement in positive directions */
    int * move;

    /** next in the linked list of the hash table */
    struct moEntry * next;

  } moEntry;

  /** the hash table */
  moEntry ** moHash;

  unsigned int moTableSize; ///< size of the hash table
  unsigned int moEntries;   ///< number of entries in the table

  /**
   * Saves the shapes in all orientations.
   * The voxel spaces are calculated on demand. The entry at the zero-th position are
   * pointers into the puzzle, so we must not free them
   */
  const voxel_c *** shapes;

  /** the mapping of piece numbers to shape ids */
  unsigned int * pieces;

  /** number of shapes */
  unsigned int num_shapes;

  /** number of possible transformations for each shape */
  unsigned int num_transformations;

  void moRehash(void); ///< this function resizes the hash table to roughly twice the size

  /** when the entry is not inside the table, this function calculates the values for the movement info */
  virtual int* moCalcValues(const voxel_c * sh1, const voxel_c * sh2, int dx, int dy, int dz) = 0;

  /// the gridtype used. We need this to make copies and transformations of the shapes
  const gridType_c * gt;

  /// get the transformed shape from the shapes array, calculating missing ones
  const voxel_c * getTransformedShape(unsigned int s, unsigned char t);

public:

  /** create the cache. The cache is then fixed to the puzzle and the problem, it can
   * and should be reused to analyse all assemblies found but can not be used for another puzzle
   */
  movementCache_c(const problem_c * puz);

  virtual ~movementCache_c(void);

  /**
   * return the values, that are:
   * how far can the 2nd piece be moved in positive x, y and z direction, when
   * the 2nd piece is offset by dx, dy and dz relative to the first,
   * the 2 pieces are the pieces p1 and p2 from the puzzle and problem defined in the constructor
   * and the 2 pieces are transformed by t1 and t2
   */
  void getMoValue(int dx, int dy, int dz, unsigned char t1, unsigned char t2, unsigned int p1, unsigned int p2, unsigned int directions, int * movements);

  /**
   * return the number of different directions of movement that are possible within
   * the space grid that that movement cache is for
   */
  virtual unsigned int numDirections(void) = 0;

  /** return the movement vector of the given direction */
  virtual void getDirection(unsigned int dir, int * x, int * y, int * z) = 0;
};

#endif

