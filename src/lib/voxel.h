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

/**
 * this class get's thrown when there is an error on loading from a stream
 */
class load_error : public std::runtime_error {
public:
  load_error(const std::string & arg) : runtime_error(arg) {};
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
  int sx;
  /**
   * The y-size of the space.
   */
  int sy;
  /**
   * The z-size of the space.
   */
  int sz;

  /**
   * The number of voxel inside the space.
   * voxels is always equal to \f$sx*sy*sz\f$ it's just
   * here to ease things a bit
   */
  int voxels;

  /**
   * The space. It's dynamically allocated on construction
   * and deleted on destruction. the position of a voxel
   * inside this 1-dimensional structure is \f$ x + sx*(y + sy*z) \f$
   */
  voxel_type * space;

public:

  /**
   * Creates a new voxel space. Its of given size and
   * initializes all values to init.
   */
  voxel_c(int x, int y, int z, voxel_type init = 0);

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
   * load the voxel space from this stream
   */
  voxel_c(std::istream * str);

  /**
   * Destructor.
   * Free the space
   */
  ~voxel_c();

  /**
   * save this voxel space into the given file
   */
  void save(std::ostream * str) const;

  /**
   * make this voxelspace be identical to the one given
   */
  void copy(const voxel_c * orig);

  /**
   * Get the actual x-size of the space.
   */
  int getX() const { return sx; }
  /**
   * Get the actual y-size of the space.
   */
  int getY() const { return sy; }
  /**
   * Get the actual z-size of the space.
   */
  int getZ() const { return sz; }

  /**
   * Get the number of voxels
   */
  int getXYZ() const { return voxels; }

  /**
   * this function returns the index for a given triple of x, y and z
   */
  int getIndex(int x, int y, int z) const {
    assert((x>=0)&&(y>=0)&&(z>=0)&&(x<sx)&&(y<sy)&&(z<sz));
    return x + sx * (y + sy * z);
  }

  /**
   * Get the value of the voxel at position \f$(x; y; z)\f$
   */
  voxel_type get(int x, int y, int z) const {
    return space[getIndex(x, y, z)];
  }

  /**
   * same as get but returns VX_EMPTY for each voxel outside
   * the space
   */
  voxel_type get2(int x, int y, int z) const {
    if ((x>=0)&&(y>=0)&&(z>=0)&&(x<sx)&&(y<sy)&&(z<sz))
      return space[getIndex(x, y, z)];
    else
      return VX_EMPTY;
  }

  /**
   * Get voxel by index.
   * Sometimes the position of the voxel is not important but
   * just the value and we need to be sure to traverse the whole
   * space. Instead of using 3 nested loops for x, y and z we can
   * go over the 1-dimensional array using a loop up to getXYZ()
   * and this function for access
   */
  voxel_type get(int p) const {
    assert((p>=0)&&(p<voxels));
    return space[p];
  }

  /**
   * returns true, if a neighbor of the given
   * voxel has the given value
   */
  bool neighbour(int p, voxel_type val) const;

  /**
   * the x, y, z variant of the set function.
   */
  void set(int x, int y, int z, voxel_type val) {
    space[getIndex(x, y, z)] = val;
  }

  /**
   * The 1-dimensional variant of the set function.
   */
  void set(int p, voxel_type val) {
    assert((p>=0)&&(p<voxels));
    space[p] = val;
  }

  /**
   * Set all the voxels to the given value
   */
  void setAll(voxel_type val) {
    memset(space, val, voxels);
  }

  /**
   * print the space onto the screen.
   * The value in the space is added to base char
   */
  void print(char base = 'a') const;

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

  /**
   * Comparison of two voxel spaces.
   * two voxel spaces are equal if and only if:
   * their sizes are the same and
   * all their voxel values are identical
   */
  bool operator == (const voxel_c & op) const;

  /** resizes the voxelspace, spreserving the lover part
   * of the data, when the new one is smaller and
   * adding new voxels at the upper end, if the new space
   * is bigger
   */
  void resize(int nsx, int nsy, int nsz, voxel_type filler);

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

#endif
