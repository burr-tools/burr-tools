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


#ifndef __VOXEL_H__
#define __VOXEL_H__

/** @file
 * Contains definitions for voxel space.
 */

#include <sys/types.h>

#include <iostream>
#include <stdexcept>
#include <assert.h>

#include <xmlwrapp/node.h>

#include "symmetries.h"

/**
 * the type used for one voxel, \c u_int8_t
 * allows up to 255 differen pieces this should
 * be enough for almost all puzzles, but just for
 * the case it isn't we can easily change the type
 */
#ifdef WIN32
typedef unsigned char voxel_type;
#else
typedef u_int8_t voxel_type;
#endif


/**
 * this class get's thrown when there is an error on loading from a stream
 */
class load_error {

  const xml::node node;
  const std::string text;

public:
  load_error(const std::string & arg, const xml::node & nd) : node(nd), text(arg) {};

  const char * getText(void) const { return text.c_str(); }
  const xml::node getNode(void) const { return node; }
};

/**
 * This class handles one voxel space. A voxel space
 * is a 3 dimensional representation of a space using
 * cubes. Each cube can have certain values of type
 * voxel_type
 */
class voxel_c {

private:

  /**
   * The x-size of the space.
   */
  unsigned int sx;
  /**
   * The y-size of the space.
   */
  unsigned int sy;
  /**
   * The z-size of the space.
   */
  unsigned int sz;

  /**
   * The number of voxel inside the space.
   * voxels is always equal to \f$sx*sy*sz\f$ it's just
   * here to ease things a bit
   */
  unsigned int voxels;

  /**
   * The space. It's dynamically allocated on construction
   * and deleted on destruction. the position of a voxel
   * inside this 1-dimensional structure is \f$ x + sx*(y + sy*z) \f$
   */
  voxel_type * space;

  /**
   * the value get2 should return for values outside of the space
   */
  voxel_type outside;

  /**
   * the voxel space has a bounding box, that encloses a region inside
   */
  unsigned int bx1, bx2;
  unsigned int by1, by2;
  unsigned int bz1, bz2;
  bool doRecalc;

protected:

  void recalcBoundingBox(void);

  void skipRecalcBoundingBox(bool skipit) {
    if (skipit)
      doRecalc = false;
    else {
      doRecalc = true;
      recalcBoundingBox();
    }
  }



public:

  /**
   * Creates a new voxel space. Its of given size and
   * initializes all values to init.
   */
  voxel_c(unsigned int x, unsigned int y, unsigned int z, voxel_type init = 0, voxel_type outs = 0);

  /**
   * Copy constructor using reference. Transformation allows to
   * have a rotated version of this voxel space
   */
  voxel_c(const voxel_c & orig, unsigned int transformation = 0);

  /**
   * Copy constructor using pointer. Transformation allows to
   * have a rotated version of this voxel space
   */
  voxel_c(const voxel_c * orig, unsigned int transformation = 0);

  /**
   * Destructor.
   * Free the space
   */
  ~voxel_c();

  /**
   * make this voxelspace be identical to the one given
   */
  void copy(const voxel_c * orig);

  /**
   * Get the actual x-size of the space.
   */
  unsigned int getX(void) const { return sx; }
  /**
   * Get the actual y-size of the space.
   */
  unsigned int getY(void) const { return sy; }
  /**
   * Get the actual z-size of the space.
   */
  unsigned int getZ(void) const { return sz; }

  /**
   * returns the squared diagonal of the space
   */
  unsigned int getDiagonal(void) const { return sx*sx + sy*sy + sz*sz; }

  unsigned int getBiggestDimension(void) const {
    if (sx > sy)
      if (sz > sx)
        return sz;
      else
        return sx;
    else
      if (sz > sy)
        return sz;
      else
        return sy;
  }

  /**
   * Get the number of voxels
   */
  unsigned int getXYZ(void) const { return voxels; }

  /**
   * this function returns the index for a given triple of x, y and z
   */
  int getIndex(unsigned int x, unsigned int y, unsigned int z) const {
    assert((x<sx)&&(y<sy)&&(z<sz));
    return x + sx * (y + sy * z);
  }

  /**
   * Get the value of the voxel at position \f$(x; y; z)\f$
   */
  voxel_type get(unsigned int x, unsigned int y, unsigned int z) const {
    return space[getIndex(x, y, z)];
  }

  /**
   * sets the value of the outside
   */
  void setOutside(voxel_type val) {
    outside = val;
    recalcBoundingBox();
  }

  /**
   * same as get but returns 0 for each voxel outside
   * the space
   */
  voxel_type get2(int x, int y, int z) const {
    if ((x>=0)&&(y>=0)&&(z>=0)&&((long)x<(long)sx)&&((long)y<(long)sy)&&((long)z<(long)sz))
      return space[getIndex(x, y, z)];
    else
      return outside;
  }

  /**
   * Get voxel by index.
   * Sometimes the position of the voxel is not important but
   * just the value and we need to be sure to traverse the whole
   * space. Instead of using 3 nested loops for x, y and z we can
   * go over the 1-dimensional array using a loop up to getXYZ()
   * and this function for access
   */
  voxel_type get(unsigned int p) const {
    assert((p>=0)&&(p<voxels));
    return space[p];
  }

  /**
   * returns true, if a neighbor of the given
   * voxel has the given value
   */
  bool neighbour(unsigned int p, voxel_type val) const;

  /**
   * the x, y, z variant of the set function.
   */
  void set(unsigned int x, unsigned int y, unsigned int z, voxel_type val) {
    space[getIndex(x, y, z)] = val;
    recalcBoundingBox();
  }

  /**
   * The 1-dimensional variant of the set function.
   */
  void set(unsigned int p, voxel_type val) {
    assert((p>=0)&&(p<voxels));
    space[p] = val;
    recalcBoundingBox();
  }

  /**
   * Set all the voxels to the given value
   */
  void setAll(voxel_type val) {
    memset(space, val, voxels);
    recalcBoundingBox();
  }

  /**
   * counts the number of voxels that have the given
   * value
   */
  unsigned int count(voxel_type val) const;

  /**
   * rotate the space 90 degree around the given axis.
   * The voxel space is resized to that it's contents
   * fits into the rotated position
   */
  void rotatex(void);
  void rotatey(void);
  void rotatez(void);

  /**
   * shift the space around. Voxels that go over the
   * edge get lost. the size is not changed
   * the new empty space gets filled with the filler value
   */
  void translate(int dx, int dy, int dz, voxel_type filler);

  /**
   * mirrors the space along the given axis
   */
  void mirrorX(void);
  void mirrorY(void);
  void mirrorZ(void);

  /**
   * changes the size of the voxel space to the smallest size
   * so that all voxels whose value is not val can be contained.
   */
  void minimize(voxel_type val);

  unsigned int boundX1(void) const { return bx1; }
  unsigned int boundX2(void) const { return bx2; }
  unsigned int boundY1(void) const { return by1; }
  unsigned int boundY2(void) const { return by2; }
  unsigned int boundZ1(void) const { return bz1; }
  unsigned int boundZ2(void) const { return bz2; }

  /**
   * Comparison of two voxel spaces.
   * two voxel spaces are equal if and only if:
   * their sizes are the same and
   * all their voxel values are identical
   */
  bool operator == (const voxel_c & op) const;

  /**
   * comparison of two voxel spaces.
   * 2 voxel spaces are identical, if their bounding
   * boxes have the same size and the voxels within
   * there boxes is identical
   */
  bool identicalInBB(const voxel_c * op) const;

  /** resizes the voxelspace, spreserving the lover part
   * of the data, when the new one is smaller and
   * adding new voxels at the upper end, if the new space
   * is bigger
   */
  void resize(unsigned int nsx, unsigned int nsy, unsigned int nsz, voxel_type filler);

  /** checks the voxelspace for connectedness. It is checked
   * if there is no group of voxels, that is disconnected from
   * the rest of the voxels. There are serveral different types
   * of connectedness: face, edge, corner. Meaning the
   * voxels are at least connected via a common face, edge or corner
   *
   * there are also 2 modes: normal mode checks all the voxel
   * that are equal to value and inverse mode checks all the voxels
   * that are not equal to value this is useful for pieces
   * or a result shape that contain voxels of different value but
   * all these values belong to the same piece
   */
  bool connected(char type, bool inverse, voxel_type value) const;

  /** all possible rotations of one piece can be generated
   * using this function by iterating nr from 0 to 24 excluding
   */
  void transform(unsigned int nr);

  /**
   * this function returns the self symmetries of this voxel
   * space. The returned value is a bitfiled containing a one
   * for each transformations that maps the voxel space
   * into itself
   */
  symmetries_t selfSymmetries(void) const;
};

/* now 2 more specialised voxel spaces */

/* this voxel space saves a piece. The voxels store 2 informations
 * the state of the voxel: empty, variable and filled and
 * also a value called color. This value is used to add additional
 * constraints on the placement of pieces
 */

class pieceVoxel_c : public voxel_c {

public:

  pieceVoxel_c(unsigned int x, unsigned int y, unsigned int z, voxel_type init = 0) : voxel_c(x, y, z, init, VX_EMPTY) { }
  pieceVoxel_c(const voxel_c & orig, unsigned int transformation = 0) : voxel_c(orig, transformation) { }
  pieceVoxel_c(const voxel_c * orig, unsigned int transformation = 0) : voxel_c(orig, transformation) { }
  pieceVoxel_c(const xml::node & node);

  /**
   * this enum defines some values that are used for some of
   * the voxel spaces
   *
   * generally there will be 2 types of usage for voxelspace
   * sone single-piece and one multi-piece. The single piece will
   * use this enum to define a puzzle piece or a solution shape
   * the multi-piece will use the values of the voxels to
   * distinguish between different pieces
   */
  enum {
    VX_EMPTY,
    VX_FILLED,
    VX_VARIABLE
  };

  int getState(unsigned int x, unsigned int y, unsigned int z) const { return get(x, y, z) & 0x3; }
  int getState2(int x, int y, int z) const { return get2(x, y, z) & 0x3; }
  int getState(unsigned int i) const { return get(i) & 0x3; }
  unsigned int getColor(unsigned int x, unsigned int y, unsigned int z) const { return get(x, y, z) >> 2; }
  unsigned int getColor2(int x, int y, int z) const { return get2(x, y, z) >> 2; }
  unsigned int getColor(unsigned int i) const { return get(i) >> 2; }

  void setState(unsigned int x, unsigned int y, unsigned int z, int state) { set(x, y, z, (get(x, y, z) & ~0x3) | state); }
  void setColor(unsigned int x, unsigned int y, unsigned int z, unsigned int color) { assert(color < 64); set(x, y, z, (get(x, y, z) & 0x3) | color << 2); }
  void setState(unsigned int i, int state) { set(i, (get(i) & ~0x3) | state); }
  void setColor(unsigned int i, unsigned int color) { assert(color < 64); set(i, (get(i) & 0x3) | color << 2); }

  void minimizePiece(void);

  unsigned int countState(int state) const;

  /**
   * this function makes all voxels that are completely surrounded (6 neibor pieces)
   * variable. This is useful, if you want to make invisible pieces contain holes
   * it's not flawless, as it leaves pieces along inside holes static on
   */
  void makeInsideHoly(void);

  /* used to save to XML */
  xml::node save(void) const;
};

/* this voxel space if available to store solutions
 * this is different from the normal space by having a special
 * state for empty and still the piece numbers start with 0
 */

class assemblyVoxel_c : public voxel_c {

public:

  enum {
    VX_EMPTY = 0xff
  };

  assemblyVoxel_c(unsigned int x, unsigned int y, unsigned int z, voxel_type init = VX_EMPTY) : voxel_c(x, y, z, init, VX_EMPTY) { }
  assemblyVoxel_c(const voxel_c & orig, unsigned int transformation = 0) : voxel_c(orig, transformation) { }
  assemblyVoxel_c(const voxel_c * orig, unsigned int transformation = 0) : voxel_c(orig, transformation) { }
  assemblyVoxel_c(const xml::node & node);

  bool isEmpty(unsigned int x, unsigned int y, unsigned int z) const { return get(x, y, z) == VX_EMPTY; }
  bool isEmpty2(int x, int y, int z) const { return get2(x, y, z) == VX_EMPTY; }
  bool isEmpty(unsigned int i) const { return get(i) == VX_EMPTY; }
  unsigned int pieceNumber(unsigned int x, unsigned int y, unsigned int z) const { return get(x, y, z); }
  unsigned int pieceNumber2(int x, int y, int z) const { return get2(x, y, z); }
  unsigned int pieceNumber(unsigned int i) const { return get(i); }

  void clear(unsigned int x, unsigned int y, unsigned int z) { set(x, y, z, VX_EMPTY); }
  void clear(unsigned int i) { set(i, VX_EMPTY); }
  void setPiece(unsigned int x, unsigned int y, unsigned int z, int num) { assert(num < VX_EMPTY); set(x, y, z, num); }
  void setPiece(unsigned int i, int num) { assert(num < VX_EMPTY); set(i, num); }

  /* used to save to XML */
  xml::node save(void) const;
};

#endif
