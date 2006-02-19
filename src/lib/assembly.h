/* Burr Solver
 * Copyright (C) 2003-2006  Andreas Röver
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

#include "voxel.h"
#include "bt_assert.h"

#include <xmlwrapp/node.h>

#include <vector>

class puzzle_c;

/* this class contains the assembly for a puzzle
 * an assembly is a list of trnasformations and
 * positions for each piece in the final assembly
 *
 * the transformations are the same as defined in the voxel space class
 */
class placement_c {

public:

  // transformation 0xFF means that the piece is NOT inside the solution
  unsigned char transformation;

  int xpos, ypos, zpos;

  placement_c(unsigned char tran, int x, int y, int z) : transformation(tran), xpos(x), ypos(y), zpos(z) {}
  placement_c(const placement_c * orig) : transformation(orig->transformation), xpos(orig->xpos), ypos(orig->ypos), zpos(orig->zpos) {}

  int getX(void) const { return xpos; }
  int getY(void) const { return ypos; }
  int getZ(void) const { return zpos; }
  unsigned char getTransformation(void) const  { return transformation; }

  bool operator == (const placement_c & b) const {
    return ((transformation == b.transformation) &&
            (xpos == b.xpos) && (ypos == b.ypos) && (zpos == b.zpos));
  }

  placement_c & operator = (const placement_c & b) {
    transformation = b.transformation;
    xpos = b.xpos;
    ypos = b.ypos;
    zpos = b.zpos;
    return *this;
  }

  bool operator < (const placement_c & b) const {
    if (transformation < b.transformation) return true;
    if (transformation > b.transformation) return false;

    if (xpos < b.xpos) return true;
    if (xpos > b.xpos) return false;

    if (ypos < b.ypos) return true;
    if (ypos > b.ypos) return false;

    if (zpos < b.zpos) return true;
    if (zpos > b.zpos) return false;

    return false;
  }
};

class assembly_c {


  std::vector<placement_c> placements;

public:

  assembly_c(void) {}

  /**
   * copy constructor
   */
  assembly_c(const assembly_c * orig);
  assembly_c(const assembly_c * orig, unsigned char trans, const puzzle_c * puz, unsigned int prob);

  /**
   * load the assembly from xml file
   */
  assembly_c(const xml::node & node, unsigned int pieces);

  /* used to save to XML */
  xml::node save(void) const;

  void addPlacement(unsigned char tran, int x, int y, int z) {
    bt_assert(tran < NUM_TRANSFORMATIONS);
    placements.push_back(placement_c(tran, x, y, z));
  }

  void addNonPlacement(void) {
    placements.push_back(placement_c(0xff, 0, 0, 0));
  }

  unsigned int placementCount(void) const { return placements.size(); }

  unsigned char getTransformation(unsigned char num) const {
    bt_assert(num < placements.size());
    return placements[num].getTransformation();
  }

  int getX(unsigned char num) const {
    bt_assert(num < placements.size());
    return placements[num].getX();
  }

  int getY(unsigned char num) const {
    bt_assert(num < placements.size());
    return placements[num].getY();
  }

  int getZ(unsigned char num) const {
    bt_assert(num < placements.size());
    return placements[num].getZ();
  }

  /**
   * transform the assembly, the problem is that to rotate the
   * placements we need to know the sizes of the shapes because
   * the given position is always the corner with the lowest coordinates
   */
  void transform(unsigned char trans, const puzzle_c * puz, unsigned int prob);

  /* to be able to put assemblies into sets we need to have 2 operators
   * on assemblies, the == and the <
   */
  bool operator == (const assembly_c & b) const {
    /* two assemblies are equal if all placements and transformations
     * of all pieces are identical
     * Comparisons are only possible, when the two assemblies
     * have the same number of pieces
     */
    bt_assert(placements.size() == b.placements.size());

    for (unsigned int i = 0; i < placements.size(); i++)
      if (!(placements[i] == b.placements[i]))
        return false;

    return true;
  }

  bool compare(const assembly_c & b, unsigned int pivot) const;

  void sort(const puzzle_c * puz, unsigned int prob);

  bool smallerRotationExists(const puzzle_c * puz, unsigned int prob, unsigned int pivot) const;

  /* shifts a piece around by a certain amount */
  void shiftPiece(unsigned int pc, int dx, int dy, int dz);

  void exchangeShape(unsigned int s1, unsigned int s2);
};

#endif
