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


#ifndef __MOVEMENTCACHE_H__
#define __MOVEMENTCACHE_H__


#include "voxel.h"
#include "puzzle.h"

class movementCache {

  typedef struct entry {
    int dx, dy, dz;
    unsigned int mx, my, mz;
    unsigned int s1, s2;
    struct entry * next;
    unsigned short t1, t2;
  } entry;


  entry ** hash;

  unsigned int tableSize;
  unsigned int entries;

  const pieceVoxel_c *** shapes;
  unsigned int * pieces;

  unsigned int num_shapes;


  void rehash(void);

  entry * calcValues(unsigned char s1, unsigned char t1, unsigned int s2, unsigned int t2,
                     int dx, int dy, int dz);

public:

  movementCache(const puzzle_c * puz, unsigned int problem);

  ~movementCache(void);

  void getValue(int dx, int dy, int dz, unsigned char t1, unsigned char t2, unsigned int p1, unsigned int p2,
                int * mx, int * my, int * mz);
};

#endif
