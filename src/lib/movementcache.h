/* Burr Solver
 * Copyright (C) 2003-2008  Andreas R�ver
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

/* when this is defined the cache will collect some information while
 * running and print some statistics when destructed
 */
#undef MV_CACHE_DEBUG

/* this class calculates the possible movement between 2 pieces
 * because that calculation is relatively expensive it caches
 * that value. That's the reason for the name
 */
class movementCache_c {

  protected:

  /* values are saved within a hash table, this is the
   * entry for the table
   */
  typedef struct entry {

    /* position of piece two relative to piece one */
    int dx, dy, dz;

    /* the 2 shapes */
    unsigned int s1, s2;

    /* the transformations of the 2 involved pieces
     * normally we would need only one transformation, that for piece 2
     * but the calculations involved to transform the 2 pieces so that
     * piece one has a fixed transformation are too expensive
     */
    unsigned short t1, t2;

    /* the possible movement in positive directions */
    int * move;

    /* for the linked list in the hash table */
    struct entry * next;

  } entry;

  private:

  /* the hash table */
  entry ** hash;

  /* the size of the table and the number of entries within the table */
  unsigned int tableSize;
  unsigned int entries;

  /* the shapes in all possible transformations. The voxel spaces are
   * calculated on demand. The entry at the zero-th position are
   * pointers into the puzzle, so we must not free them
   */
  const voxel_c *** shapes;

  /* the mapping of pieces to shape numbers */
  unsigned int * pieces;

  /* number of shapes */
  unsigned int num_shapes;

  /* number of possible transformations for each shape */
  unsigned int num_transformations;

  /* this function resizes the hash table to roughly twice the size */
  void rehash(void);

  /* when the entry is not inside the table, this function calculates the values */
  virtual void calcValues(entry * e, const voxel_c * sh1, const voxel_c * sh2, int dx, int dy, int dz) = 0;

  const gridType_c * gt;

  const unsigned int directions;

#ifdef MV_CACHE_DEBUG

  unsigned long maxListLen;
  unsigned long cacheHits;
  unsigned long cacheRequests;
  unsigned long cachCollisions;

#endif

public:

  /* create the cache, the cache is then fixed to the puzzle and the problem, it can
   * and should be reused to analyse all assemblies found but can not be used for another puzzle
   */
  movementCache_c(const problem_c * puz, unsigned int directions);

  virtual ~movementCache_c(void);

  /* return the values, that are:
   * how far can the 2nd piece be moved in positive x, y and z direction, when
   * the 2nd piece is offset by dx, dy and dz relative to the first,
   * the 2 pieces are the pieces p1 and p2 from the puzzle and problem defined in the constructor
   * and the 2 pieces are transformed by t1 and t2
   */
  void getValue(int dx, int dy, int dz, unsigned char t1, unsigned char t2, unsigned int p1, unsigned int p2,
      unsigned int directions, int * movements);

  /* remove all information that involves one shape from the cache */
  void removePieceInfo(unsigned int s);

  /* return the number of different directions of movement that are possible within
   * the space grid that that movement cache is for
   */
  virtual unsigned int numDirections(void) = 0;

  /* return the movement vector of the given direction */
  virtual void getDirection(unsigned int dir, int * x, int * y, int * z) = 0;

};

#endif
