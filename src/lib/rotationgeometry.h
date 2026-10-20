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
#ifndef __ROTATION_GEOMETRY_H__
#define __ROTATION_GEOMETRY_H__

#include <compare>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>

/**
 * Exact geometry of planar rigid motions of cube pieces, the single source of
 * truth for rotation moves (see design/2026-10-19-rotation-moves.md).
 *
 * A motion that rotates about an axis-aligned line and translates
 * perpendicular to it keeps every cube in its axial layer, and cubes of
 * different layers can only touch on a face plane. So the 3D question reduces
 * exactly to 2D: per axial layer, moving squares against static squares.
 *
 * Physical model: every cube is an axis-aligned cube of side 1 - play,
 * centred in its grid cell, with sharp edges. play = 0.081 is the model the
 * Fortran SimpleRot code and PR #50's rotationRules_c use implicitly.
 */
namespace rotationGeometry {

  struct cell_t {
    int x, y, z;
  };

  /** Rotate an integer vector by a quarter turn: axis 0=X 1=Y 2=Z,
   *  sense 0 = +90° (right-handed), 1 = -90°. Same convention as glRotatef.
   *  Throws on any other axis or sense, like every function here. */
  void quarterTurn(int * x, int * y, int * z, unsigned int axis, unsigned int sense);

  /** Brick-grid symmetry id that rotates a shape like quarterTurn rotates
   *  vectors, to be combined with symmetries_c::transAdd(current, id). */
  unsigned char quarterTurnTrans(unsigned int axis, unsigned int sense);

  struct model_t {
    double play;      ///< gap between neighbouring cubes, cube side is 1 - play, must be > 0
  };

  /** Pose of the moving group: rotate by theta (radians, right-handed about
   *  the axis) around the scene pivot, then translate by (du, dv). */
  struct pose_t {
    double theta, du, dv;
  };

  /** The outcome of certifying a path. */
  enum verdict_e {
    PATH_CLEAR,      ///< proven collision-free
    PATH_BLOCKED,    ///< a sampled pose overlaps a static cube
    PATH_UNDECIDED   ///< too close to call at the minimum step, treat as blocked
  };

  /**
   * Moving and static cubes projected onto the plane perpendicular to one
   * axis, grouped by axial layer. In-plane coordinates: X axis -> (y, z),
   * Y axis -> (z, x), Z axis -> (x, y), so a positive theta is a
   * right-handed rotation for every axis.
   */
  class planarScene_c {

    public:

      /** pivot is in doubled cell-index units (voxel (x,y,z) centre is
       *  (2x,2y,2z)), only its two in-plane components are used. */
      planarScene_c(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                    unsigned int axis, int pivotHx, int pivotHy, int pivotHz, const model_t & model);

      /** Signed clearance of the closest same-layer (moving, static) pair at
       *  pose p: distance between the cubes when apart, minus the penetration
       *  depth when overlapping. Huge when nothing can collide. */
      double clearance(const pose_t & p) const;

      /** Certify the piecewise linear path through the given poses. Stops
       *  at the first segment that is blocked or undecided. */
      verdict_e certifyPath(const std::vector<pose_t> & waypoints) const;

      /** Certify the pure quarter turn about the pivot, sense as in quarterTurn. */
      verdict_e certifyQuarterTurn(unsigned int sense) const;

      /** Largest distance of a moving cube corner from the pivot. */
      double radius(void) const { return rho; }

    private:

      struct square_t {
        double u, v;   // centre relative to the pivot
      };

      struct layer_t {
        std::vector<square_t> moving;
        std::vector<square_t> stat;
      };

      std::vector<layer_t> layers;
      double half;     // half side of the cube
      double rho;

      bool certifySegment(const pose_t & a, const pose_t & b, verdict_e * v) const;
  };

  /** A motion of the moving group: the piecewise linear curve through these
   *  poses, starting at the identity pose and ending at a quarter turn. */
  typedef std::vector<pose_t> path_t;

  /** Kinds of rotation path, see rotationTables_c. */
  enum pathKind_e {
    PATH_PURE,    ///< plain quarter turn about the pivot
    PATH_WIGGLE,  ///< quarter turn with an offset amount·sin(πt) along a direction
    PATH_SLIDE    ///< shift by amount along a direction, quarter turn, shift back
  };

  /** Names one path of a family; the path follows from it, see makePath. */
  struct pathSpec_t {
    pathKind_e kind;
    unsigned int sense;   ///< 0 = +90°, 1 = -90°, as in quarterTurn
    int dirU, dirV;       ///< one of the 8 compass directions, in-plane, unused for PATH_PURE
    double amount;        ///< shift distance in cells, unused for PATH_PURE
  };

  /** The waypoints of a path. Wiggles are polylines of WIGGLE_SEGMENTS
   *  segments; the polyline, not the sine, is the motion that is checked. */
  path_t makePath(const pathSpec_t & spec);

  constexpr int WIGGLE_SEGMENTS = 16;

  /** The pose at parameter t in [0, 1] along a path, its segments taking
   *  equal parameter time. Every pose lies on the polyline the checker
   *  certified, so animating with it shows exactly the checked motion. */
  pose_t poseAt(const path_t & path, double t);

  /** Apply one of the 8 dihedral symmetries of the square (low 2 bits: +90°
   *  quarter turns; bit 2: mirror across the u axis after turning). */
  inline std::pair<int, int> applyOffsetTransform(unsigned char xform, int u, int v) {
    switch (xform) {
      case 0: return {  u,  v };
      case 1: return { -v,  u };
      case 2: return { -u, -v };
      case 3: return {  v, -u };
      case 4: return {  u, -v };
      case 5: return { -v, -u };
      case 6: return { -u,  v };
      case 7: return {  v,  u };
      default: return { u, v };
    }
  }

  inline unsigned char inverseOffsetTransform(unsigned char xform) {
    static constexpr unsigned char inv[8] = { 0, 3, 2, 1, 4, 5, 6, 7 };
    return inv[xform & 7];
  }

  /** Read-only range of static blocker offsets, transforming flat base-table
   *  entries on the fly without copying or allocating. */
  class blockerRange_c {

    public:

      class iterator {
        public:
          using iterator_category = std::random_access_iterator_tag;
          using value_type = std::pair<int, int>;
          using difference_type = std::ptrdiff_t;
          using pointer = void;
          using reference = std::pair<int, int>;

          iterator() = default;
          iterator(const std::pair<int16_t, int16_t> * p, unsigned char xf) : ptr(p), xform(xf) {}

          std::pair<int, int> operator*() const { return applyOffsetTransform(xform, ptr->first, ptr->second); }
          std::pair<int, int> operator[](difference_type n) const { return *(*this + n); }

          iterator & operator++() { ++ptr; return *this; }
          iterator operator++(int) { iterator tmp = *this; ++ptr; return tmp; }
          iterator & operator--() { --ptr; return *this; }
          iterator operator--(int) { iterator tmp = *this; --ptr; return tmp; }
          iterator & operator+=(difference_type n) { ptr += n; return *this; }
          iterator & operator-=(difference_type n) { ptr -= n; return *this; }
          iterator operator+(difference_type n) const { return { ptr + n, xform }; }
          iterator operator-(difference_type n) const { return { ptr - n, xform }; }
          difference_type operator-(const iterator & o) const { return ptr - o.ptr; }

          bool operator==(const iterator & o) const { return ptr == o.ptr; }
          auto operator<=>(const iterator & o) const { return ptr <=> o.ptr; }

        private:
          const std::pair<int16_t, int16_t> * ptr = nullptr;
          unsigned char xform = 0;
      };

      blockerRange_c() = default;
      blockerRange_c(const std::pair<int16_t, int16_t> * b, const std::pair<int16_t, int16_t> * e,
                     unsigned char xf) :
        first(b), last(e), xform(xf) {}

      iterator begin(void) const { return { first, xform }; }
      iterator end(void) const { return { last, xform }; }
      size_t size(void) const { return (size_t)(last - first); }
      bool empty(void) const { return first == last; }

    private:
      const std::pair<int16_t, int16_t> * first = nullptr;
      const std::pair<int16_t, int16_t> * last = nullptr;
      unsigned char xform = 0;
  };

  class pathTableView_c;

  /**
   * Lookup table for one fixed path, filled by the certified checker.
   *
   * Along a fixed motion every (moving, static) cube pair collides or not
   * independently of all other cubes, so the path is blocked iff some moving
   * cube has a static cube of its layer in its table entry. The table is
   * therefore exactly as precise as planarScene_c, at the cost of a lookup
   * per cube. Motions that are searched for, rather than fixed in advance,
   * cannot be tabulated: they depend on all cubes at once.
   *
   * Offsets are in-plane, in doubled cell units, relative to the pivot, in
   * the frames of planarScene_c: both even for a pivot at a cube centre,
   * both odd for one at a cube corner. Mixed parity (pivot on an edge
   * midpoint) cannot map the lattice onto itself by a quarter turn.
   *
   * Only same-parity slots are indexed, and their blocker lists are stored
   * flat (CSR-style) to avoid per-slot vector overhead.
   */
  class pathTable_c {

    public:

      /** Covers moving cubes up to radius cells from the pivot along u and v. */
      pathTable_c(const model_t & model, int radius, const pathSpec_t & spec);

      int getRadius(void) const { return radius; }
      const pathSpec_t & getSpec(void) const { return spec; }

      /** True if the table has an entry for this moving cube offset. */
      bool covers(int du, int dv) const;

      /** Offsets of the static cubes that block the moving cube at (du, dv). */
      blockerRange_c blockers(int du, int dv) const;

      /** The path of `moving` about the pivot (doubled cell-index units)
       *  among `stat`, decided by table lookups. Throws if a moving cube is
       *  not covered, even on a layer without static cubes: the radius must
       *  bound the whole moving group. */
      bool blocked(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                   unsigned int axis, int pivotHx, int pivotHy, int pivotHz) const;

    private:

      int radius;
      pathSpec_t spec;

      /* Same-parity CSR layout:
       * even slots (2r+1)^2, then odd slots (2r)^2; offsets has numSlots + 1 entries. */
      std::vector<uint32_t> offsets;
      std::vector<std::pair<int16_t, int16_t>> entries;

      unsigned int slot(int du, int dv) const;
      blockerRange_c blockersTransformed(int du, int dv, unsigned char xform) const;

      friend class pathTableView_c;
      friend class rotationTables_c;
  };

  /**
   * Lightweight view of a pathTable_c under a dihedral symmetry (quarter turns
   * and optional mirror). Turning or mirroring the whole scene commutes with
   * the motion, so rotationTables_c stores only the base tables it computes
   * with the checker and serves every turned/mirrored path through a view.
   */
  class pathTableView_c {

    public:

      pathTableView_c(const pathTable_c & t) : base(&t), spec(&t.getSpec()), xform(0) {}
      pathTableView_c(const pathTable_c * b, const pathSpec_t * s, unsigned char xf) :
        base(b), spec(s), xform(xf) {}

      int getRadius(void) const { return base->getRadius(); }
      const pathSpec_t & getSpec(void) const { return *spec; }
      bool covers(int du, int dv) const { return base->covers(du, dv); }

      blockerRange_c blockers(int du, int dv) const;

      bool blocked(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                   unsigned int axis, int pivotHx, int pivotHy, int pivotHz) const;

    private:

      const pathTable_c * base;
      const pathSpec_t * spec;
      unsigned char xform;
  };

  /**
   * The family of fixed rotation paths a rotation move may use, ordered by
   * how much the move disturbs: the pure turn first, then per amount
   * (ascending) the 8 wiggles and the 8 slides.
   *
   * Only the +90° tables for the pure turn and directions (1,0) and (1,1)
   * are computed and stored; all other directions and the -90° sense are
   * served as turned and mirrored views of those base tables.
   */
  class rotationTables_c {

    public:

      /** amounts are sorted and duplicates dropped. */
      rotationTables_c(const model_t & model, int radius, const std::vector<double> & amounts);

      int getRadius(void) const { return radius; }

      /** Number of paths per sense. */
      unsigned int size(void) const { return (unsigned int)paths[0].size(); }

      pathTableView_c table(unsigned int sense, unsigned int idx) const;

      /** Index of the first clear path of this sense, -1 if all are
       *  blocked. Throws like pathTable_c::blocked if a moving cube lies
       *  outside the radius. */
      int firstClear(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                     unsigned int axis, int pivotHx, int pivotHy, int pivotHz, unsigned int sense) const;

    private:

      struct pathEntry_t {
        uint16_t baseIdx;
        unsigned char xform;
        pathSpec_t spec;
      };

      int radius;
      std::vector<pathTable_c> baseTables;
      std::vector<pathEntry_t> paths[2];
  };
}

#endif
