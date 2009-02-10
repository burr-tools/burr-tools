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
#ifndef __ASSEMBLY_H__
#define __ASSEMBLY_H__

#include "bt_assert.h"
#include "symmetries.h"
#include "gridtype.h"

#include <xmlwrapp/node.h>

#include <vector>

// this vailue is used for transformation to specify unplaced pieces
#define UNPLACED_TRANS 0xff

class problem_c;
class voxel_c;

/**
 * this class contains the information for the placement of
 * one piece within the assembly.
 *
 * That is the position and the orientation of that piece
 */
class placement_c {

public:

  // transformation UNPLACED_TRANS means that the piece is NOT inside the solution
  unsigned char transformation;

  int xpos, ypos, zpos;

  placement_c(unsigned char tran, int x, int y, int z) : transformation(tran), xpos(x), ypos(y), zpos(z) {}
  placement_c(const placement_c * orig) : transformation(orig->transformation), xpos(orig->xpos), ypos(orig->ypos), zpos(orig->zpos) {}
  placement_c(const placement_c & orig) : transformation(orig.transformation), xpos(orig.xpos), ypos(orig.ypos), zpos(orig.zpos) {}

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

/* this class contains mirror information for a given puzzle
 * this means it contains pairs of pieces that are mirror shapes
 * of one another and how one piece is transformed into the other
 */
class mirrorInfo_c {

  typedef struct {
    unsigned int pc1;
    unsigned int pc2;

    unsigned char trans;
  } entry;

  std::vector<entry> entries;

  public:

    mirrorInfo_c(void) {};

    /* adds a pair of pieces that are mirrors or one another,
     * trans transforms the first piece into the 2nd
     */
    void addPieces(unsigned int p1, unsigned int p2, unsigned char trans);

    /* returns the piece information for piece p
     * p_out and trans contains the attached info
     * returns only true, when valid entry was found
     */
    bool getPieceInfo(unsigned int p, unsigned int * p_out, unsigned char * trans) const;
};

/* this class contains the assembly for a puzzle
 * an assembly is a list of transformations and
 * positions for each piece in the final assembly
 *
 * the transformations are the same as defined in the voxel space class
 */
class assembly_c {

private:

  std::vector<placement_c> placements;
  const symmetries_c * sym;


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

  /**
   * returns true, if one of the pieces within this assembly is
   * a mirror orientation
   *
   * this is used by the smaller rotation exists check,
   * to see, if the mirrored orientation can be achieved
   * with the given pieces
   */
  bool containsMirroredPieces(void) const;

  /**
   * returns true, if all shapes are at least the minimum required times
   * inside the assembly
   */
  bool validSolution(const problem_c * puz) const;

public:

  assembly_c(const gridType_c * gt) : sym(gt->getSymmetries()) {}

  /**
   * copy constructor
   */
  assembly_c(const assembly_c * orig);

  /**
   * load the assembly from xml file
   */
  assembly_c(const xml::node & node, unsigned int pieces, const gridType_c * gt);

  /* used to save to XML */
  xml::node save(void) const;

  void addPlacement(unsigned char tran, int x, int y, int z) {
    bt_assert(tran < sym->getNumTransformations());
    bt_assert(tran != UNPLACED_TRANS);
    placements.push_back(placement_c(tran, x, y, z));
  }

  void addNonPlacement(void) {
    placements.push_back(placement_c(UNPLACED_TRANS, 0, 0, 0));
  }

  unsigned int placementCount(void) const { return placements.size(); }

  bool isPlaced(unsigned int num) const {
    return placements[num].getTransformation() != UNPLACED_TRANS;
  }

  unsigned char getTransformation(unsigned int num) const {
    bt_assert(num < placements.size());
    bt_assert(placements[num].getTransformation() != UNPLACED_TRANS);
    return placements[num].getTransformation();
  }

  int getX(unsigned int num) const {
    bt_assert(num < placements.size());
    bt_assert(placements[num].getTransformation() != UNPLACED_TRANS);
    return placements[num].getX();
  }

  int getY(unsigned int num) const {
    bt_assert(num < placements.size());
    bt_assert(placements[num].getTransformation() != UNPLACED_TRANS);
    return placements[num].getY();
  }

  int getZ(unsigned int num) const {
    bt_assert(num < placements.size());
    bt_assert(placements[num].getTransformation() != UNPLACED_TRANS);
    return placements[num].getZ();
  }

  /**
   * transform the assembly, the problem is that to rotate the
   * placements we need to know the sizes of the shapes because
   * the given position is always the corner with the lowest coordinates
   *
   * The function returns true, when the transformation has been done
   * successfully and false if not.
   *
   * when complete is true, then the symmetry of the solution shape
   * is NOT used to find out which symmetries to check for
   * instead all possible orientations are checked, also all possible
   * translations are checked
   *
   * When the function returns false it will leave the assembly in an undefined state
   * maybe some pieces have already been replaced while other are still in their
   * initial position, so you have to throw away the assembly when that happens
   */
  bool transform(unsigned char trans, const problem_c * puz, const mirrorInfo_c * mir);


  bool smallerRotationExists(const problem_c * puz, unsigned int pivot, const mirrorInfo_c * mir, bool complete) const;

  void exchangeShape(unsigned int s1, unsigned int s2);

  int comparePieces(const assembly_c * b) const;

  /* sort the pieces within the assembly so that they do have the intended order, first
   * piece at a "smaller" position
   */
  void sort(const problem_c * puz);

  /* calculate a voxelspace that is identical to the assembly with
   * all pieces put into the space
   */
  voxel_c * createSpace(const problem_c * puz) const;

};

#endif
