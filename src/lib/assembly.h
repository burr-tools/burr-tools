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
#ifndef __ASSEMBLY_H__
#define __ASSEMBLY_H__

#include "bt_assert.h"
#include "symmetries.h"
#include "gridtype.h"

#include <vector>

// this value is used for transformation to specify unplaced pieces
#define UNPLACED_TRANS 0xff

class problem_c;
class voxel_c;
class xmlWriter_c;
class xmlParser_c;

/**
 * this class contains the information for the placement of
 * one piece within the assembly.
 *
 * That is the position and the orientation of that piece
 * Only internally used in assembly
 */
class placement_c {

public:

  /** the transformation of the piece.
   * if transformation is equal to UNPLACED_TRANS then the piece in NOT placed inside the assembly
   */
  unsigned char transformation;

  /** position of the hotspot of the piece */
  int xpos, ypos, zpos;

  /** Initialise placement with given values */
  placement_c(unsigned char tran, int x, int y, int z) : transformation(tran), xpos(x), ypos(y), zpos(z) {}
  /** copy constructor */
  placement_c(const placement_c * orig) : transformation(orig->transformation), xpos(orig->xpos), ypos(orig->ypos), zpos(orig->zpos) {}
  /** copy constructor */
  placement_c(const placement_c & orig) : transformation(orig.transformation), xpos(orig.xpos), ypos(orig.ypos), zpos(orig.zpos) {}

  /** get x position of piece */
  int getX(void) const { return xpos; }
  /** get y position of piece */
  int getY(void) const { return ypos; }
  /** get z position of piece */
  int getZ(void) const { return zpos; }
  /** get transformation of piece */
  unsigned char getTransformation(void) const  { return transformation; }

  /** check if 2 placements are identical */
  bool operator == (const placement_c & b) const {
    return ((transformation == b.transformation) &&
            (xpos == b.xpos) && (ypos == b.ypos) && (zpos == b.zpos));
  }

  /** assignment operation */
  placement_c & operator = (const placement_c & b) {
    transformation = b.transformation;
    xpos = b.xpos;
    ypos = b.ypos;
    zpos = b.zpos;
    return *this;
  }

  /** comparison operation, this is an arbitrary order for placements */
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

/**
 * this class contains mirror information for a given problem.
 * this means it contains pairs of pieces that are mirror shapes
 * of one another and how one piece is transformed into the other
 */
class mirrorInfo_c {

  /** structure containing mirror information for one piece pair */
  typedef struct {
    unsigned int pc1;     //< first piece involved
    unsigned int pc2;     //< second piece which is the mirror piece of pc1

    unsigned char trans;  //< how to transform pc1 onto pc2
  } entry;

  /** the mirror pairs */
  std::vector<entry> entries;

  public:

    mirrorInfo_c(void) {};

    /**
     * adds a pair of pieces that are mirrors or one another.
     * trans transforms the first piece into the 2nd
     */
    void addPieces(unsigned int p1, unsigned int p2, unsigned char trans);

    /**
     * returns the piece information for piece p.
     * p_out and trans contains the attached info
     * returns only true, when valid entry was found
     */
    bool getPieceInfo(unsigned int p, unsigned int * p_out, unsigned char * trans) const;

private:

  // no copying and assigning
  mirrorInfo_c(const mirrorInfo_c&);
  void operator=(const mirrorInfo_c&);
};

/** this class contains the assembly for a puzzle.
 * an assembly is a list of transformations and
 * positions for each piece in the final assembly
 *
 * the transformations are the same as defined in the voxel space class
 */
class assembly_c {

private:

  /** the placements of all pieces of the puzzle.
   * the order here is the same as the order of problem definition
   */
  std::vector<placement_c> placements;

  /** pointer to the symmetry class for this puzzle */
  const symmetries_c * sym;

  /** the equality operation.
   * 2 assemblies are equal, if all placements are equal
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

  /**
   * returns true, if assembly b is smaller than this assembly.
   * This function is used in the smallerRotationExists function
   * to compare different orientations of one and the same
   * assembly. For normalisation purposes.
   */
  bool compare(const assembly_c & b, unsigned int pivot) const;

  /**
   * returns true, if one of the pieces within this assembly is
   * a mirror orientation.
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
  assembly_c(xmlParser_c & pars, unsigned int pieces, const gridType_c * gt);

  /** used to save to XML */
  void save(xmlWriter_c & xml) const;

  /** add placement for the next piece.
   * Pieces are added one after another with this function
   */
  void addPlacement(unsigned char tran, int x, int y, int z) {
    bt_assert(tran < sym->getNumTransformations());
    bt_assert(tran != UNPLACED_TRANS);
    placements.push_back(placement_c(tran, x, y, z));
  }

  /** add a non placed piece.
   * as all pieces of a puzzle must be inside the assembly this
   * function needs to be called when a piece is not used in the
   * assembly.
   */
  void addNonPlacement(void) {
    placements.push_back(placement_c(UNPLACED_TRANS, 0, 0, 0));
  }

  /** how many pieces are in this assembly */
  unsigned int placementCount(void) const { return placements.size(); }

  /** check if a piece is placed in this assembly */
  bool isPlaced(unsigned int num) const {
    return placements[num].getTransformation() != UNPLACED_TRANS;
  }

  /** get the transformation of a piece */
  unsigned char getTransformation(unsigned int num) const {
    bt_assert(num < placements.size());
    bt_assert(placements[num].getTransformation() != UNPLACED_TRANS);
    return placements[num].getTransformation();
  }

  /** get the x-position of a piece */
  int getX(unsigned int num) const {
    bt_assert(num < placements.size());
    bt_assert(placements[num].getTransformation() != UNPLACED_TRANS);
    return placements[num].getX();
  }

  /** get the y-position of a piece */
  int getY(unsigned int num) const {
    bt_assert(num < placements.size());
    bt_assert(placements[num].getTransformation() != UNPLACED_TRANS);
    return placements[num].getY();
  }

  /** get the z-position of a piece */
  int getZ(unsigned int num) const {
    bt_assert(num < placements.size());
    bt_assert(placements[num].getTransformation() != UNPLACED_TRANS);
    return placements[num].getZ();
  }

  /**
   * transform the assembly. the problem is that to rotate the
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

  /**
   * return true, if this is a non-normal assembly.
   * This is used to drop rotated assemblies
   */
  bool smallerRotationExists(const problem_c * puz, unsigned int pivot, const mirrorInfo_c * mir, bool complete) const;

  /**
   * exchange 2 shapes.
   * this is used when editing a problem and exchanging the order of 2 pieces in a problem
   */
  void exchangeShape(unsigned int s1, unsigned int s2);

  /**
   * compare 2 assemblies according to their used pieces.
   * used to sort assemblies by the pieces they use.
   * Assemblies which use pieces with smaller indices are smaller than assemblies
   * that use pieces with bigger indices.
   */
  int comparePieces(const assembly_c * b) const;

  /**
   * sort the pieces within the assembly so that multipieces are ordered by
   * ascending placement order.
   */
  void sort(const problem_c * puz);

  /** calculate a voxelspace that is identical to the assembly with
   * all pieces put into the space
   */
  voxel_c * createSpace(const problem_c * puz) const;

  void removePieces(unsigned int from, unsigned int cnt);

  void addNonPlacedPieces(unsigned int from, unsigned int cnt);

private:

  // no copying and assigning
  assembly_c(const assembly_c&);
  void operator=(const assembly_c&);

};

#endif
