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


#ifndef __ASSEMBLY_H__
#define __ASSEMBLY_H__

#include <xmlwrapp/node.h>

/* this class contains the assembly for a puzzle
 * an assembly is a list of trnasformations and
 * positions for each piece in the final assembly
 *
 * the transformations are the same as defined in the voxel space class
 */
class assembly_c {

  class placement_c {

    unsigned char transformation;

    int xpos, ypos, zpos;

    placement_c(unsigned char tran, int x, int y, int z) : transformation(tran), xpos(x), ypos(y), zpos(z) {}
  };

  vector<placement_c> placements;

public:

  assembly_c(void) {}

  void addPlacement(unsigned char tran, int x, int y, int z) {
    assert(tran < 24);
    placements.push_back(placement_c(tran, x, y, z));
  }

  unsigned int placements(void) { return placements.size(); }

  unsigned char getTransformation(unsigned char num) {
    assert(num < placements.size());
    return placements[num].transformation;
  }

  int getX(unsigned char num) {
    assert(num < placements.size());
    return placements[num].xpos;
  }

  int char getY(unsigned char num) {
    assert(num < placements.size());
    return placements[num].ypos;
  }

  int char getZ(unsigned char num) {
    assert(num < placements.size());
    return placements[num].zpos;
  }

  /* this creates a voxel space containing the piece numbers in that place
   * where voxels of the pieces are placed. The function does some assume
   * that the placement places all pieces within the result shape.
   * all color information of the pieces is lost
   */
  assemblyVoxel_c * getVoxelSpace(const puzzle_c * puz, unsigned int prob);
};


#endif
