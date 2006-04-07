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
#ifndef __VOXEL_H__
#define __VOXEL_H__

#include "bt_assert.h"
#include "symmetries.h"
#include "gridtype.h"
#include "types.h"

#include <xmlwrapp/node.h>

/**
 * this class get's thrown when there is an error on loading from a stream
 */
class load_error {

  const xml::node node;
  const std::string text;

public:
  load_error(const std::string & arg, const xml::node & nd) : node(nd), text(arg) {};

  load_error(const std::string & arg) : text(arg) {};

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

protected:

  /* each voxel needs to know the parameters for its gridspace
   */
  const gridType_c *gt;

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

  /**
   * the self symmetries of this voxel space
   * this value is only valid when the lowest bit 1 is set
   * if the bit is not set the symmetries need to be calculated
   */
  symmetries_t symmetries;

  /**
   * this is the hot spot of the voxel
   * when a piece is places somewhere it is always done relative to this
   * point. This is necessary to be able to rotate assemblies.
   * just place the hotspot somewhere inside the voxelspace and it will
   * be possible to rotate a voxel space and place it at the same position without
   * knowing the size of the piece
   * The hotspot is also transformed, when the piece voxel space is transformed
   */
  int hx, hy, hz;

  /* shapes can be named */
  std::string name;

protected:

  void recalcBoundingBox(void);

public:

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

  void skipRecalcBoundingBox(bool skipit) {
    if (skipit)
      doRecalc = false;
    else {
      doRecalc = true;
      recalcBoundingBox();
    }
  }

  // a few of the constructor are private so that voxel spaces can only be constructed via
  // the factory in gridType_c
public:

  /**
   * Creates a new voxel space. Its of given size and
   * initializes all values to init.
   */
  voxel_c(unsigned int x, unsigned int y, unsigned int z, const gridType_c * gt, voxel_type init = 0, voxel_type outs = VX_EMPTY);
  /**
   * load from xml node
   */
  voxel_c(const xml::node & node, const gridType_c * gt);

  /**
   * Copy constructor using reference. Transformation allows to
   * have a rotated version of this voxel space
   */
  voxel_c(const voxel_c & orig);

  /**
   * Copy constructor using pointer. Transformation allows to
   * have a rotated version of this voxel space
   */
  voxel_c(const voxel_c * orig);

  /**
   * Destructor.
   * Free the space
   */
  virtual ~voxel_c();

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
    bt_assert((x<sx)&&(y<sy)&&(z<sz));
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
    symmetries = symmetryInvalid();
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
    bt_assert((p>=0)&&(p<voxels));
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
    symmetries = symmetryInvalid();
  }

  /**
   * The 1-dimensional variant of the set function.
   */
  void set(unsigned int p, voxel_type val) {
    bt_assert((p>=0)&&(p<voxels));
    space[p] = val;
    recalcBoundingBox();
    symmetries = symmetryInvalid();
  }

  /**
   * Set all the voxels to the given value
   */
  void setAll(voxel_type val) {
    memset(space, val, voxels);
    recalcBoundingBox();
    symmetries = symmetryInvalid();
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
  virtual void rotatex(int by = 1) = 0;
  virtual void rotatey(int by = 1) = 0;
  virtual void rotatez(int by = 1) = 0;

  /**
   * this function transformes the given point by the given transformation
   * around the the origin
   */
  virtual void transformPoint(int * x, int * y, int * z, unsigned int trans) const = 0;

  /**
   * shift the space around. Voxels that go over the
   * edge get lost. the size is not changed
   * the new empty space gets filled with the filler value
   */
  void translate(int dx, int dy, int dz, voxel_type filler);

  /**
   * mirrors the space along the given axis
   */
  virtual void mirrorX(void);
  virtual void mirrorY(void);
  virtual void mirrorZ(void);

  /**
   * changes the size of the voxel space to the smallest size
   * so that all voxels whose value is not 0 can be contained.
   */
  virtual void minimizePiece(void);

  unsigned int boundX1(void) const { return bx1; }
  unsigned int boundX2(void) const { return bx2; }
  unsigned int boundY1(void) const { return by1; }
  unsigned int boundY2(void) const { return by2; }
  unsigned int boundZ1(void) const { return bz1; }
  unsigned int boundZ2(void) const { return bz2; }

  /**
   * get the bounding box of a rotated voxel space
   */
  virtual void getBoundingBox(unsigned char trans, int * x1, int * y1, int * z1, int * x2 = 0, int * y2 = 0, int * z2 = 0) const;

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
   *
   * if includeColors is true, the colors are included in the
   * comparison, meaning when the colors differ the
   * shapes are not equal
   */
  virtual bool identicalInBB(const voxel_c * op, bool includeColors = true) const;

  /**
   * comparison of 2 voxel spaces.
   * 2 spaces are identical, if one of the rotations
   * is identical to the other voxel space
   * you can specify, if you want to include the colors
   * in the comparison, or just want to compare the shape
   * naturally this function is relatively slow
   *
   * if includeMirror is true, the function checks agains all transformations
   * including the mirrored shape, if it is false, mirrored transformations
   * are excluded
   *
   * if includeColors is true, the colors are included in the
   * comparison, meaning when the colors differ the
   * shapes are not equal
   */
  bool identicalWithRots(const voxel_c * op, bool includeMirror, bool includeColors) const;

  /** resizes the voxelspace, spreserving the lover part
   * of the data, when the new one is smaller and
   * adding new voxels at the upper end, if the new space
   * is bigger
   */
  void resize(unsigned int nsx, unsigned int nsy, unsigned int nsz, voxel_type filler);

  /**
   * scale the space, making x by x by x cubes out of single cubes
   */
  void scale(unsigned int amount);

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
   * using this function by iterating nr from 0 to NUM_TRANSFORMATIONS (24 for cubes) excluding
   */
  void transform(unsigned int nr);

  /**
   * this function returns the self symmetries of this voxel
   * space. The returned value is a bitfiled containing a one
   * for each transformations that maps the voxel space
   * into itself
   */
  symmetries_t selfSymmetries(void) const;

  /**
   * this function returns the smallest transformation number
   * that results in an identical shape for this voxel space
   */
  unsigned char normalizeTransformation(unsigned char trans) const {
    return gt->getSymmetries()->minimizeTransformation(selfSymmetries(), trans);
  }


public:

  int getState(unsigned int x, unsigned int y, unsigned int z) const { return get(x, y, z) & 0x3; }
  int getState2(int x, int y, int z) const { return get2(x, y, z) & 0x3; }
  int getState(unsigned int i) const { return get(i) & 0x3; }
  unsigned int getColor(unsigned int x, unsigned int y, unsigned int z) const { return get(x, y, z) >> 2; }
  unsigned int getColor2(int x, int y, int z) const { return get2(x, y, z) >> 2; }
  unsigned int getColor(unsigned int i) const { return get(i) >> 2; }

  bool isEmpty(unsigned int x, unsigned int y, unsigned int z) const { return getState(x, y, z) == VX_EMPTY; }
  bool isEmpty2(int x, int y, int z) const { return getState2(x, y, z) == VX_EMPTY; }
  bool isFilled(unsigned int x, unsigned int y, unsigned int z) const { return getState(x, y, z) == VX_FILLED; }
  bool isFilled2(int x, int y, int z) const { return getState2(x, y, z) == VX_FILLED; }
  bool isVariable(unsigned int x, unsigned int y, unsigned int z) const { return getState(x, y, z) == VX_VARIABLE; }
  bool isVariable2(int x, int y, int z) const { return getState2(x, y, z) == VX_VARIABLE; }

  void setState(unsigned int x, unsigned int y, unsigned int z, int state) { set(x, y, z, (get(x, y, z) & ~0x3) | state); }
  void setColor(unsigned int x, unsigned int y, unsigned int z, unsigned int color) { bt_assert(color < 64); set(x, y, z, (get(x, y, z) & 0x3) | color << 2); }
  void setState(unsigned int i, int state) { set(i, (get(i) & ~0x3) | state); }
  void setColor(unsigned int i, unsigned int color) { bt_assert(color < 64); set(i, (get(i) & 0x3) | color << 2); }


  unsigned int countState(int state) const;

  /* do something on the voxel space, what is done is defined with the enum
   * fixed sets voxels to the fixed state, variable sets voxels to variable
   * and decolor removes colors from voxels
   * inside defines where to carry out the action, on inside cubes or on outside cubes
   * inside cubes do have 6 nonempty cubes as neighbours
   */
  enum {
    ACT_FIXED,
    ACT_VARIABLE,
    ACT_DECOLOR
  };
  void actionOnSpace(int action, bool inside);

  /* used to save to XML */
  xml::node save(void) const;


  /* functions for hotspot management */
  int getHx(void) const { return hx; }
  int getHy(void) const { return hy; }
  int getHz(void) const { return hz; }
  void setHotspot(int x, int y, int z) { hx = x; hy = y; hz = z; }
  /* this function returns the hotspot, if the voxel space would be rotated
   * by the given transformation
   */
  virtual void getHotspot(unsigned char trans, int * x, int * y, int * z) const;

  /* functions to set the name */
  const std::string & getName(void) const { return name; }

  // if you give 0 or an empty string the name will be removed
  void setName(const std::string & n) { name = n; }

  /* for the minimize scale function applied to all shapes
   * we need to first check, if all shapes can be scaled down
   * by a certain factor and then do it. if action is true, then
   * the shape is really scaled, otherwise you only get the fact
   * if it is scalable by the given amount
   */
  bool scaleDown(unsigned char by, bool action);
};

#endif
