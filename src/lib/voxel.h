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
#ifndef __VOXEL_H__
#define __VOXEL_H__

#include "bt_assert.h"
#include "symmetries.h"
#include "gridtype.h"
#include "types.h"

#include <stdio.h>
#include <string>

class xmlWriter_c;
class xmlParser_c;
class Polyhedron;

/** \file voxel.h
 * Contains the voxel base class and the class that gets thrown on load errors
 */

/**
 * This class handles one voxel space. A voxel space
 * is a 3 dimensional representation of a space using
 * some kind of regular units to fill the space. This is only an abstract
 * base class that provides functionality common to all other voxel spaces.
 * To have a real voxel space (e.g. made out of unit cubes) you need to inherit
 * from this class and provide functions for rotation and other things.
 *
 * Each voxel has 2 information: its state and its color. State is one of the
 * VoxelState enums values. And color can right now be one of 64 possible values
 */
class voxel_c {

protected:

  /**
   * each voxel needs to know the parameters for its space grid
   */
  const gridType_c *gt;

  unsigned int sx; ///< The x size of the space.
  unsigned int sy; ///< The y size of the space.
  unsigned int sz; ///< The z size of the space.

  /**
   * The number of voxel inside the space.
   * voxels is always equal to \f$sx*sy*sz\f$ it's just
   * here to ease things a bit
   */
  unsigned int voxels;

  /**
   * The space. It's dynamically allocated on construction
   * and deleted on destruction. The position of a voxel
   * inside this 1-dimensional structure is \f$ x + sx*(y + sy*z) \f$
   */
  voxel_type * space;

  /** \page BoundingBox Bounding Box
   *
   * A bounding box is a box that encloses the non-empty voxels within a voxel space.
   *
   * The bounding box is used in all different kind of situation to speed things up because
   * only non empty voxels need to be handled
   */
  unsigned int bx1; ///< lower x-coordinate of the \ref BoundingBox
  unsigned int bx2; ///< upper x-coordinate of the \ref BoundingBox
  unsigned int by1; ///< lower y-coordinate of the \ref BoundingBox
  unsigned int by2; ///< upper y-coordinate of the \ref BoundingBox
  unsigned int bz1; ///< lower z-coordinate of the \ref BoundingBox
  unsigned int bz2; ///< upper z-coordinate of the \ref BoundingBox
  bool doRecalc;    ///< bounding box recalculation is only done when this is true

  /**
   * the self symmetries of this voxel space.
   * The value may be invalid, meaning that the self symmetries
   * of the space are unknown and need to be calculated
   */
  mutable symmetries_t symmetries;

  /** \page Hotspot The Hot Spot
   *
   * When a piece is placed somewhere it is always done relative to this
   * point. This is necessary to be able to rotate assemblies.
   * just place the hotspot somewhere inside the voxelspace and it will
   * be possible to rotate a voxel space and place it at the same position without
   * knowing the size of the piece
   * The hotspot is also transformed, when the piece voxel space is transformed
   * The hotspot is in the centre of the given voxel for most of he defined
   * voxel spaces. This has the advantage that it is not necessary to calculate
   * the exact corner where is is, but it requires rotation independent centre definition
   *
   * a different interpretation of the hotspot:
   *
   * Observe, that a shape must always be in the first octant (x, y and z positive) regarding
   * the coordinate system because of the way the coordinates are handled. This means that rotations
   * can never be exact mathematical rotations around the source of the coordinate system
   * there is always a translation added that brind the shape back into the first octant.
   *
   * This added translation adds a major headache because it is dependent on the shape itself
   * its size and shape. It is easy to calculate for cubes, but impossible to easily find out
   * for the more obscure grids like the triangles.
   *
   * To make the roation appear to be around the center the hotspot contains the added translation
   * that shifted the whole shape into the first quadrant.
   *
   * Now it becomes simple to rotate voxel spaces in space without knowing their exact position.
   * All you do it rotate the position round the source
   *
   */

  int hx; ///< x-coordinate of \ref Hotspot
  int hy; ///< y-coordinate of \ref Hotspot
  int hz; ///< z-coordinate of \ref Hotspot

  /**
   * The name of the shape
   */
  std::string name;

  /**
   * Weight of the shape. This weight is used by the disassembler to
   * decide which piece of groups to move and which to keep still
   */
  int weight;

  /**
   * A cache for hotspot and bounding box coordinates of the rotated voxel
   *
   * calculating the hotspot and bounding box can be expensive
   * and as this information is required extremely often for the assembly
   * transformation we will cache this information
   *
   * the cache is simple, for each transformation it contains 3+6=9 values
   * if the first value is BBHSCACHE_UNINIT then the information for that
   * transformation is not yet available and needs to be calculated
   * the first 3 values are the hot spot position and the following 6 the
   * bounding box for the given transformation
   */
  int * BbHsCache;

protected:

  /**
   * updates the bounding box to fit the current shape inside the space.
   *
   * This function does nothing, when doRecalc is false see \ref skipRecalcBoundingBox.
   * This is useful when a lot of operations are done on the space and only at the end the
   * bounding box needs to be recalculated.
   *
   * As a side effect the function sets all color values of empty voxels to 0.
   *
   * When the space is empty the bounding box is set to all 0.
   *
   * The bounding box and hotspot cache is cleared by this function
   */
  void recalcBoundingBox(void);

public:

  /**
   * this enumeration defines some values that are used for some of the voxel spaces.
   *
   * generally there will be 2 types of usage for voxelspace
   * some single-piece and one multi-piece. The single piece will
   * use this enumeration to define a puzzle piece or a solution shape
   * the multi-piece will use the values of the voxels to
   * distinguish between different pieces
   */
  typedef enum {
    VX_EMPTY,   ///< This is used for empty voxels
    VX_FILLED,  ///< This value is used for filled foxels
    VX_VARIABLE ///< This value is used for voxels with variable content
  } VoxelState;

  /**
   * enable or diable the update of the bounding box after each operation.
   * Sometimes, when more complex operations are performed it is useful to not
   * update the bounding box every time but rather finish all operations first
   * and then update the box after all is finished. This can be done here
   * call this function with skipit = true at the start of such a block of
   * operations and at the end call it with skipit = false
   */
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
  voxel_c(unsigned int x, unsigned int y, unsigned int z, const gridType_c * gt, voxel_type init = 0);

  /**
   * Load a voxel space from xml node
   */
  voxel_c(xmlParser_c & pars, const gridType_c * gt);

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
   * make this voxelspace be identical to the one given. except for the name field
   * everything is copied
   */
  void copy(const voxel_c * orig);

  unsigned int getX(void) const { return sx; } ///< Return x-size of the voxel space
  unsigned int getY(void) const { return sy; } ///< Return y-size of the voxel space
  unsigned int getZ(void) const { return sz; } ///< Return z-size of the voxel space

  /** get the gridtype of this voxel space */
  const gridType_c * getGridType(void) const { return gt; }

  /**
   * Returns the squared diagonal of the space
   */
  unsigned int getDiagonal(void) const { return sx*sx + sy*sy + sz*sz; }

  /**
   * Returns the value of the biggest out of getX, getY or getZ.
   */
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
   * Get the number of voxels, which is getX()*getY()*getZ()
   */
  unsigned int getXYZ(void) const { return voxels; }

  /**
   * This function returns the index for a given triple of x, y and z
   */
  int getIndex(unsigned int x, unsigned int y, unsigned int z) const {
    bt_assert((x<sx)&&(y<sy)&&(z<sz));
    return x + sx * (y + sy * z);
  }

  /** The inverse of getIndex.
   *
   * For a given index the x, y and z coordinates inside this voxel space is returned
   */
  bool indexToXYZ(unsigned int index, unsigned int *x, unsigned int *y, unsigned int *z) const;

  /**
   * Get the value of the voxel at position \f$(x; y; z)\f$
   */
  voxel_type get(unsigned int x, unsigned int y, unsigned int z) const {
    return space[getIndex(x, y, z)];
  }

  /**
   * same as get but returns VX_EMPTY for each voxel outside
   * the space
   */
  voxel_type get2(int x, int y, int z) const {
    if ((x>=0)&&(y>=0)&&(z>=0)&&((long)x<(long)sx)&&((long)y<(long)sy)&&((long)z<(long)sz))
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
  voxel_type get(unsigned int p) const {
    bt_assert(p<voxels);
    return space[p];
  }

  /**
   * returns true, if a neighbour of the given
   * voxel has the given value
   */
  bool neighbour(unsigned int p, voxel_type val) const;

  /**
   * returns the coordinates for a neighbour to the given voxel
   *
   * idx is the index if the neighbour, if you want the next neighbour, give the next number
   * typ is what kind of neighbour you want, face (0), edge (1) or corner (2)
   * x, y, z coordinate for the source
   * xn, yn, zn, coordinate for the neighbour
   * return true, when a valid neighbour with that index exists
   *
   * This function is different for different grids. There is no default implementation, so each grid
   * has to define its own version of this function
   */
  virtual bool getNeighbor(unsigned int idx, unsigned int typ, int x, int y, int z, int * xn, int *yn, int *zn) const = 0;

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
    bt_assert(p<voxels);
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
   * this function transforms the given point by the given transformation
   * around the origin
   */
  virtual void transformPoint(int * x, int * y, int * z, unsigned int trans) const = 0;

  /**
   * shift the space around. Voxels that go over the
   * edge get lost. The size is not changed
   * the new empty space gets filled with the filler value
   */
  void translate(int dx, int dy, int dz, voxel_type filler);

  /**
   * changes the size of the voxel space to the smallest size
   * so that all voxels whose value is not 0 can be contained.
   *
   * as in some grids there are special conditions to consider
   * when doing this operation the function can be overloaded.
   * This default implementation will minimize to the bounding box.
   */
  virtual void minimizePiece(void);

  unsigned int boundX1(void) const { return bx1; } ///< Get the coordinate of the \ref BoundingBox
  unsigned int boundX2(void) const { return bx2; } ///< Get the coordinate of the \ref BoundingBox
  unsigned int boundY1(void) const { return by1; } ///< Get the coordinate of the \ref BoundingBox
  unsigned int boundY2(void) const { return by2; } ///< Get the coordinate of the \ref BoundingBox
  unsigned int boundZ1(void) const { return bz1; } ///< Get the coordinate of the \ref BoundingBox
  unsigned int boundZ2(void) const { return bz2; } ///< Get the coordinate of the \ref BoundingBox

  /**
   * get the bounding box of a rotated voxel space
   *
   * If the return pointers are 0 the value is not returned
   */
  bool getBoundingBox(unsigned char trans, int * x1, int * y1, int * z1, int * x2 = 0, int * y2 = 0, int * z2 = 0) const;

  /**
   * Comparison of two voxel spaces.
   * two voxel spaces are equal if and only if:
   * their sizes are the same and
   * all their voxel values are identical
   */
  bool operator == (const voxel_c & op) const;

  /**
   * Comparison of two voxel spaces.
   * 2 voxel spaces are identical, if their bounding
   * boxes have the same size and the voxels within
   * there boxes is identical
   *
   * if includeColors is true, the colours are included in the
   * comparison, meaning when the colours differ the
   * shapes are not equal, otherwise only the states are compared
   *
   * There are special conditions to take care in different grid (e.g
   * alignment along a bigger grid) so this function can be overloaded.
   * The default implementation simply treats all voxels equal
   */
  virtual bool identicalInBB(const voxel_c * op, bool includeColors = true) const;

  /**
   * comparison of 2 voxel spaces.
   * 2 spaces are identical, if one of the rotations
   * is identical to the other voxel space
   * you can specify, if you want to include the colours
   * in the comparison, or just want to compare the shape
   * naturally this function is relatively slow
   *
   * if includeMirror is true, the function checks against all transformations
   * including the mirrored shape, if it is false, mirrored transformations
   * are excluded
   *
   * if includeColors is true, the colours are included in the
   * comparison, meaning when the colours differ the
   * shapes are not equal
   */
  bool identicalWithRots(const voxel_c * op, bool includeMirror, bool includeColors) const;

  /**
   * this function returns the transformation, that transforms this voxel space into the
   * as parameter given one
   *
   * the returned transformation always contains a mirroring
   *
   * if no transformation can be found, return 0
   */
  unsigned char getMirrorTransform(const voxel_c * op) const;

  /**
   * resizes the voxelspace. preserving the lower part
   * of the data, when the new one is smaller and
   * adding new voxels at the upper end, if the new space
   * is bigger
   */
  void resize(unsigned int nsx, unsigned int nsy, unsigned int nsz, voxel_type filler);

  /**
   * resizes and translates the space so that the the given voxel can be included
   * the function returns the new coordinate of the point.
   *
   * Each grid has to implement it's own version because there are different
   * kinds of supergrids that are imposed on the normal grid
   */
  virtual void resizeInclude(int & px, int & py, int & pz) = 0;

  /**
   * scale the space, making x by x by x cubes out of single cubes
   *
   * This must be done differently by all grids. The default implementation
   * doesn't do anything. It's just there to allow grids without scaling as not
   * all grid have such a possibility (e.g. spheres can't be scaled)
   * if grid is true (not all functions need to support this)
   * the result it not filled, but rather a grid with only the outer edges filled
   */
  virtual void scale(unsigned int amount, bool grid = false);

  /**
   * Scale down voxel space by a certain amount.
   *
   * for the minimize scale function applied to all shapes
   * we need to first check, if all shapes can be scaled down
   * by a certain factor and then do it. If action is true, then
   * the shape is really scaled, otherwise you only get the fact
   * if it is scalable by the given amount
   *
   * This must be done differently by all grids. The default implementation
   * doesn't do anything. It's just there to allow grids without scaling as not
   * all grid have such a possibility (e.g. spheres can't be scaled)
   */
  virtual bool scaleDown(unsigned char by, bool action);

  private:

  /**
   * This function is used by conneced and fill holes.
   *
   * It finds out what groups of voxel spaces touch one another
   * Type: defines in which way the voxels touch (faces, edges, corners)
   * Inverse: defines whether to work on the voxlels defines by value or the other voxels
   * value: which voxels to work on
   * outsiudeZ: how to tread the Z direction outside: as empty for 3D pieces or as non existent for 2D pieces
   */
  void unionFind(int * tree, char type, bool inverse, voxel_type value, bool outsideZ) const;

  public:

  /**
   * checks the voxelspace for connectedness. It is checked
   * if there is no group of voxels, that is disconnected from
   * the rest of the voxels. There are several different types
   * of connectedness: face, edge, corner. Meaning the
   * voxels are at least connected via a common face, edge or corner
   *
   * there are also 2 modes: normal mode checks all the voxel
   * that are equal to value and inverse mode checks all the voxels
   * that are not equal to value this is useful for pieces
   * or a result shape that contain voxels of different value but
   * all these values belong to the same piece
   *
   * normally the outside is one node and voxels that are on the edge
   * of the shape may be merged with the outside, this can be suppressed
   * for the z-axis by setting outsideZ to false
   */
  bool connected(char type, bool inverse, voxel_type value, bool outsideZ = true) const;

  /**
   * fills in all empty voxels that are completely surrounded by non empty voxels,
   */
  void fillHoles(char type);

  /**
   * all possible orientations of one piece can be generated.
   * using this function by iterating nr from 0 to NUM_TRANSFORMATIONS (24 for cubes) excluding
   * because in some spacegrids there might be transformations that do
   * not exist with certain shapes, this function returns a bool showing
   * if the transformation action has succeeded
   *
   * This must be implmented by all grids separately
   */
  virtual bool transform(unsigned int nr) = 0;

  /**
   * This function returns the self symmetries of this voxel
   * space.
   *
   * Self symmetries are a list of transformations that transform the
   * voxel space into an identical shape.
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

  //@{
  /**
   * Functions to get the voxel state or color of a single voxel
   */
  int getState(unsigned int x, unsigned int y, unsigned int z) const { return get(x, y, z) & 0x3; }
  int getState2(int x, int y, int z) const { return get2(x, y, z) & 0x3; }
  int getState(unsigned int i) const { return get(i) & 0x3; }
  unsigned int getColor(unsigned int x, unsigned int y, unsigned int z) const { return get(x, y, z) >> 2; }
  unsigned int getColor2(int x, int y, int z) const { return get2(x, y, z) >> 2; }
  unsigned int getColor(unsigned int i) const { return get(i) >> 2; }
  //@}

  //@{
  /**
   * Functions to ask, if a voxel has a certain state
   */
  bool isEmpty(unsigned int x, unsigned int y, unsigned int z) const { return getState(x, y, z) == VX_EMPTY; }
  bool isEmpty2(int x, int y, int z) const { return getState2(x, y, z) == VX_EMPTY; }
  bool isEmpty(unsigned int i) const { return getState(i) == VX_EMPTY; }
  bool isFilled(unsigned int x, unsigned int y, unsigned int z) const { return getState(x, y, z) == VX_FILLED; }
  bool isFilled2(int x, int y, int z) const { return getState2(x, y, z) == VX_FILLED; }
  bool isFilled(unsigned int i) const { return getState(i) == VX_FILLED; }
  bool isVariable(unsigned int x, unsigned int y, unsigned int z) const { return getState(x, y, z) == VX_VARIABLE; }
  bool isVariable2(int x, int y, int z) const { return getState2(x, y, z) == VX_VARIABLE; }
  bool isVariable(unsigned int i) const { return getState(i) == VX_VARIABLE; }
  //@}

  //@{
  /**
   * Functions to set the state or color of a certain voxel.
   * The position is either given as a coordinate or as an index
   */
  void setState(unsigned int x, unsigned int y, unsigned int z, int state) { set(x, y, z, (get(x, y, z) & ~0x3) | state); }
  void setColor(unsigned int x, unsigned int y, unsigned int z, unsigned int color) { bt_assert(color < 64); set(x, y, z, (get(x, y, z) & 0x3) | color << 2); }
  void setState(unsigned int i, int state) { set(i, (get(i) & ~0x3) | state); }
  void setColor(unsigned int i, unsigned int color) { bt_assert(color < 64); set(i, (get(i) & 0x3) | color << 2); }
  //@}

  /**
   * Counts how many voxels there are of a certain state.
   * Color markings are ignored with this function, only the state
   * is considered
   */
  unsigned int countState(int state) const;

  //@{
  /**
   * Defined possible actions for the actionOnSpace function.
   */
  typedef enum {
    ACT_FIXED,    ///< Make voxel fixed
    ACT_VARIABLE, ///< Make voxels variable
    ACT_DECOLOR   ///< Set color to neutral
  } VoxelAction;

  /**
   * Change the state of some or all voxels. What is done is defined with the enumeration
   * fixed sets voxels to the fixed state, variable sets voxels to variable
   * and decolour removes colours from voxels
   * inside defines where to carry out the action, on inside cubes or on outside cubes
   * inside cubes do have 6 non-empty cubes as neighbours
   */
  void actionOnSpace(VoxelAction action, bool inside);
  //@}

  /**
   *  used to save to XML
   */
  void save(xmlWriter_c & xml) const;

  int getHx(void) const { return hx; } ///< Get the hotspot
  int getHy(void) const { return hy; } ///< Get the hotspot
  int getHz(void) const { return hz; } ///< Get the hotspot
  void setHotspot(int x, int y, int z); ///< Set the hotspot

  /**
   * Initialized the hotspot to a valid position.
   *
   * in some voxelspaces the hotspot needs to be in special
   * places to stay valid after all possible transformations
   * this function sets the hotspot so, that is has this
   * property. As many spaces do not have this requirement
   * there is a default implementation that puts the hotspot
   * at 0;0;0 if you need something special, overwrite this function
   */
  virtual void initHotspot(void);

  /**
   * this function returns the hotspot of the rotated space.
   */
  bool getHotspot(unsigned char trans, int * x, int * y, int * z) const;

  /** function to get the name */
  const std::string & getName(void) const { return name; }

  /**
   * Set the name for this voxel space.
   * if you give 0 or an empty string the name will be removed
   */
  void setName(const std::string & n) { name = n; }

  int getWeight(void) const { return weight; } ///< get the weight of this space
  void setWeight(int w) { weight = w; }        ///< set the weight of this space

  /**
   * Return if a given coordinate is valid to use.
   *
   * In some voxel spaces not all possible coordinates are valid. For example
   * in the sphere grid only the coordinates with (x+y+z) % 2 == 0 are valid
   * This function does that calculation
   */
  virtual bool validCoordinate(int x, int y, int z) const = 0;

  /* creates the intersection of the 2 given voxel spaces and
   * unifies the result with this voxel space
   *
   * it returns true, if there were voxels added
   */
  bool unionintersect(
      const voxel_c * va, int xa, int ya, int za,
      const voxel_c * vb, int xb, int yb, int zb
      );

  /**
   * this function is used to find out if the given
   * position is of the same voxel type as the voxel
   * at position 0.
   *
   * This is used to find out if a piece can be placed
   * at a certain position in a voxel grid
   */
  virtual bool onGrid(int x, int y, int z) const = 0;

  /**
   * this function returns a polyhedron mesh of this shape.
   * The mesh is then further used for STL export and
   * the displaying of this shape in the GUI
   * The Polyhedron is allocated using new, so you have to
   * delete it, when you no longer need it
   *
   */
  virtual Polyhedron * getMesh(double bevel, double offset) const;

  /* return true, when the given parameters will result in a usable
   * polyhedron, when offset or bevel gets too big return false
   */
  virtual bool meshParamsValid(double /*bevel*/, double /*offset*/) const { return true; }

  /**
   * returns the drawing mesh. ATTENTION for the sake of speed this mesh
   * will not be a proper halfedge mesh, most edges will be open, meaning
   * they don't have a pair, which is invalid and makes some
   * functions not work, but it is ok for drawing
   */
  virtual Polyhedron * getDrawingMesh(void) const;

  /**
   * this function must return a polygon that is the connecting face to the neighbor n for the
   * voxel at position x, y, z
   * The function is used by the default implementation of getMesh to generate the default basis mesh
   * it is also used by the 3D cursor drawing code
   */
  virtual void getConnectionFace(int /*x*/, int /*y*/, int /*z*/, int /*n*/, double /*bevel*/, double /*offset*/, std::vector<float> & /*faceCorners*/) const {};

  /**
   * calculate the size, that the returned mesh has
   */
  virtual void calculateSize(float * x, float * y, float * z) const { *x = 0; *y = 0; *z = 0; }


  virtual void recalcSpaceCoordinates(float * /*x*/, float * /*y*/, float * /*z*/) const {}

private:

  virtual Polyhedron * getMeshInternal(double bevel, double offset, bool fast) const;

  // no copying and assigning
  void operator=(const voxel_c&);

};

/* some defines used for the flags of the faces of the generated meshes
 */

#define FF_COLOR_LIGHT    0x01 // this helps with the colorisation
#define FF_VARIABLE_MARK  0x02 // when set, the face is supposed to be with a variable marker
#define FF_VARIABLE_FACE  0x04 // when set, the face itself is the variable marker only one or none of these 2 should be set
#define FF_WIREFRAME      0x08 // when set, the face stays when in wire frame more
#define FF_BEVEL_FACE     0x10 // the face got added because the polyhedron has a bevel
#define FF_OFFSET_FACE    0x20 // the face is added because the polyhedron has an offset
#define FF_PROCESSED_FACE 0x40 // tags faces already processed by the polygon fill routine
#define FF_INSIDE_FACE    0x80 // tags faces already processed by the polygon fill routine

#endif
