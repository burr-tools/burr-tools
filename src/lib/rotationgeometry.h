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

#include <cmath>
#include <cstdint>
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
 * centred in its grid cell, whose edges parallel to the rotation axis are
 * beveled at 45° by chamfer. play = 0.081, chamfer = 0 is the model the
 * Fortran SimpleRot code and rotationRules_c use implicitly.
 */
namespace rotationGeometry {

  struct cell_t {
    int x, y, z;
  };

  /** Rotate an integer vector by a quarter turn: axis 0=X 1=Y 2=Z,
   *  sense 0 = +90° (right-handed), 1 = -90°. Same convention as glRotatef. */
  void quarterTurn(int * x, int * y, int * z, unsigned int axis, unsigned int sense);

  /** Brick-grid symmetry id that rotates a shape like quarterTurn rotates
   *  vectors, to be combined with symmetries_c::transAdd(current, id). */
  unsigned char quarterTurnTrans(unsigned int axis, unsigned int sense);

  /** Doubled in-plane coordinates of a point for a turn about axis (cube
   *  centres at even values): X -> (y, z), Y -> (z, x), Z -> (x, y), so a
   *  positive angle is right-handed for every axis. */
  inline void inPlaneDoubled(int x, int y, int z, unsigned int axis, int * u, int * v) {
    if (axis == 0)      { *u = 2 * y; *v = 2 * z; }
    else if (axis == 1) { *u = 2 * z; *v = 2 * x; }
    else                { *u = 2 * x; *v = 2 * y; }
  }

  /** The coordinate of a point along axis. */
  inline int axialCoordinate(int x, int y, int z, unsigned int axis) {
    return (axis == 0) ? x : ((axis == 1) ? y : z);
  }

  /** The doubled 3D pivot for a doubled in-plane pivot (u, v); its axial
   *  coordinate, 0, does not matter for a turn about axis. */
  inline void pivotOf(unsigned int axis, int u, int v, int * hx, int * hy, int * hz) {
    if (axis == 0)      { *hx = 0; *hy = u; *hz = v; }
    else if (axis == 1) { *hx = v; *hy = 0; *hz = u; }
    else                { *hx = u; *hy = v; *hz = 0; }
  }

  /** Turn a lattice point a quarter turn about a doubled pivot at a cube
   *  centre or corner (both map the lattice onto itself). */
  void turnAbout(int * x, int * y, int * z, int hx, int hy, int hz, unsigned int axis, unsigned int sense);

  struct model_t {
    double play = 0;      ///< gap between neighbouring cubes, cube side is 1 - play, must be > 0
    /** 45° bevel of the cube edges parallel to the rotation axis: the length
     *  cut off each adjacent face, so the bevel face is chamfer·√2 wide. A
     *  puzzlecad $burr_bevel b on cubes of size s is b / (√2 s). */
    double chamfer = 0;
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

      /** clearance(p) if it is at least floor; otherwise some value below
       *  floor, found without looking at every pair */
      double clearanceAbove(const pose_t & p, double floor) const;

      /** Certify the piecewise linear path through the given poses. */
      verdict_e certifyPath(const std::vector<pose_t> & waypoints) const;

      /** Certify the pure quarter turn about the pivot, sense as in quarterTurn. */
      verdict_e certifyQuarterTurn(unsigned int sense) const;

      /** Largest distance of a moving cube corner from the pivot. */
      double radius(void) const { return rho; }

      /** A certified lower bound on the smallest clearance along the
       *  piecewise linear path, within eps of the true minimum. Since
       *  adding play Δ adds at least Δ to every clearance, the path is
       *  clear at every play of at least play - minClearance(path). */
      double minClearance(const std::vector<pose_t> & waypoints, double eps = 1e-4) const;

      /** True if the smallest clearance along the path is proven to be at
       *  least threshold; stops as soon as either is clear. */
      bool clearanceAtLeast(const std::vector<pose_t> & waypoints, double threshold) const;

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
      double chamfer;  // 45° cut of the square's corners
      double rho;
      double inRadius;   // of a cube: centres closer than twice this overlap
      double outRadius;  // of a cube: clearance >= centre distance - 2·outRadius

      bool certifySegment(const pose_t & a, const pose_t & b, verdict_e * v) const;
      /** Lipschitz branch and bound over one segment: a lower bound on its
       *  smallest clearance, within eps; stops early, returning a value
       *  below stopBelow, once a sample is below stopBelow. */
      double segmentMinimum(const pose_t & a, const pose_t & b, double eps, double stopBelow) const;
      /** The decision certifySegment needs at pose p, using the exact
       *  clearance only where the cubes' in- and circumcircles do not settle
       *  it: -1 if a pair overlaps, 1 if every pair is at least margin
       *  apart, 0 otherwise. The same decision as comparing clearance(p)
       *  with 0 and margin. For 0, *cl is clearance(p). */
      int classify(const pose_t & p, double margin, double * cl) const;
  };

  /** The pose that undoes p: the motion of the static pieces relative to
   *  the moving ones. */
  inline pose_t inversePose(const pose_t & p) {
    /* x -> R(x - P) + P + d inverts to y -> R'(y - P) + P - R' d, R' = R(-theta) */
    const double c = std::cos(p.theta), s = std::sin(p.theta);
    return { -p.theta, -(c * p.du + s * p.dv), -(-s * p.du + c * p.dv) };
  }

  /** A motion of the moving group: the piecewise linear curve through these
   *  poses, starting at the identity pose and ending at a quarter turn. */
  typedef std::vector<pose_t> path_t;

  /** Kinds of rotation path, see rotationTables_c and planTurn. */
  enum pathKind_e {
    PATH_PURE,    ///< plain quarter turn about the pivot
    PATH_WIGGLE,  ///< quarter turn with an offset amount·(cos θ + sin θ - 1)/(√2 - 1) along a direction
    PATH_SLIDE,   ///< shift by amount along a direction, quarter turn, shift back
    PATH_PLANNED  ///< any polyline, found by planTurn for one scene
  };

  /** Names one path of a family; the path follows from it, see makePath. */
  struct pathSpec_t {
    pathKind_e kind = PATH_PURE;
    unsigned int sense = 0;   ///< 0 = +90°, 1 = -90°, as in quarterTurn
    int dirU = 0, dirV = 0;   ///< one of the 8 compass directions, in-plane, unused for PATH_PURE
    double amount = 0;        ///< shift distance in cells, unused for PATH_PURE
    /** the waypoints of a PATH_PLANNED path, from the identity pose to the
     *  quarter turn of sense */
    std::vector<pose_t> waypoints = {};
  };

  /** The waypoints of a path. Wiggles are polylines of WIGGLE_SEGMENTS
   *  segments; the polyline, not the smooth profile, is the motion that is
   *  checked. */
  path_t makePath(const pathSpec_t & spec);

  constexpr int WIGGLE_SEGMENTS = 16;

  /** The pose at parameter t in [0, 1] along a path, its segments taking
   *  equal parameter time. Every pose lies on the polyline the checker
   *  certified, so animating with it shows exactly the checked motion. */
  pose_t poseAt(const path_t & path, double t);

  /** How planTurn searches: the configuration space (θ, du, dv) is cut
   *  into cells of shift × shift × 90°/steps, shifts up to maxShift. */
  struct planSettings_t {
    double maxShift = 0.5;
    double shift = 0.01;
    int steps = 180;
    /** cells with a centre clearance below this are never entered; a path
     *  needing less is not looked for (the solver passes 0) */
    double floor = -1;
    /** also compute the optimistic bound `upper` (a second search) */
    bool upper = false;
    /** find the path with the most clearance; false: any path with at
     *  least floor (faster, for deciding whether a turn is possible) */
    bool widest = true;
    /** without widest: give up after evaluating this many cells */
    size_t budget = 60000;
  };

  /** The outcome of planTurn. */
  struct turnPlan_t {
    bool found = false;
    path_t path;                ///< from the identity pose to the quarter turn
    /** certified lower bound on the path's smallest clearance */
    double clearance = -1e9;
    /** no motion with shifts up to maxShift has a smallest clearance above
     *  this (only with planSettings_t::upper; -inf: none reaches floor) */
    double upper = 1e9;
    /** the search ran out of its budget: the turn may be possible */
    bool gaveUp = false;
  };

  /**
   * The quarter turn of the scene's moving cubes, along the motion with the
   * most clearance: a widest path (largest smallest clearance) over the
   * grid of planSettings_t, shortcut, and its clearance certified on the
   * polyline itself (planarScene_c::minClearance). The play the turn needs
   * is at most model play - clearance; with `upper`, at least model play -
   * upper (for shifts up to maxShift). See design/2026-10-19-rotation-moves.md.
   *
   * The grid maps onto itself when the turn is seen from its end, so the
   * reverse turn meets the same cells.
   */
  turnPlan_t planTurn(const planarScene_c & scene, unsigned int sense, const planSettings_t & settings);

  /** The play a quarter turn needs, see requiredPlay. */
  struct playBound_t {
    bool found = false;
    /** the turn is proven possible at this play, along `path` */
    double needs = 0;
    path_t path;
    /** and proven impossible (for shifts up to maxShift) below this; 0 if
     *  nothing could be proven */
    double atLeast = 0;
  };

  /**
   * The smallest play at which the moving cubes can make the quarter turn
   * about the pivot among the static ones, by any motion with shifts up to
   * settings.maxShift: bisection over the play with planTurn, to within
   * tolerance. The chamfer is taken from model, its play is the
   * starting guess. With more play every clearance grows by at least the
   * extra play, so a path found at one play stays clear at every larger one.
   * Not found when the turn needs more than maxPlay.
   */
  playBound_t requiredPlay(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                           unsigned int axis, int pivotHx, int pivotHy, int pivotHz, unsigned int sense,
                           const model_t & model, const planSettings_t & settings,
                           double tolerance = 0.0005, double maxPlay = 0.4);

  /** The path of the reverse turn: the same motion run backwards, as seen
   *  by the moving cubes in their turned orientation. */
  path_t reversePath(const path_t & path);

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
      const std::vector<std::pair<int, int>> & blockers(int du, int dv) const;

      /** The path of `moving` about the pivot (doubled cell-index units)
       *  among `stat`, decided by table lookups. */
      bool blocked(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                   unsigned int axis, int pivotHx, int pivotHy, int pivotHz) const;

      /** The table of this path turned by +90° about the pivot, i.e. with its
       *  shift direction turned. Turning the whole scene commutes with the
       *  motion, so no checker calls are needed. */
      pathTable_c turned(void) const;

      /** The table of this path mirrored at the u axis: the other sense, with
       *  the shift direction mirrored. */
      pathTable_c mirrored(void) const;

    private:

      pathTable_c(int radius, const pathSpec_t & spec);

      int radius;
      int width;   // 4 * radius + 1 doubled offsets per in-plane axis
      pathSpec_t spec;

      /* [(du + 2 radius) * width + dv + 2 radius] */
      std::vector<std::vector<std::pair<int, int>>> table;

      unsigned int index(int du, int dv) const;

      friend class rotationTables_c;
  };

  /**
   * The family of fixed rotation paths a rotation move may use, one
   * pathTable_c each, ordered by how much the move disturbs: the pure turn
   * first, then per amount (ascending) the 8 wiggles and the 8 slides.
   *
   * Only the +90° tables for the directions (1,0) and (1,1) are computed
   * with the checker, all others are turned and mirrored copies.
   */
  /** The cells occupied by static cubes, as a dense bitmap over their
   *  bounding box: built once per group of turning pieces and then queried
   *  for every pivot, axis, sense and path. */
  class occupancy_c {

    public:

      explicit occupancy_c(const std::vector<cell_t> & cells);

      bool operator()(int x, int y, int z) const {
        if (x < x0 || y < y0 || z < z0 || x >= x0 + sx || y >= y0 + sy || z >= z0 + sz)
          return false;
        return bits[((size_t)(z - z0) * sy + (y - y0)) * sx + (x - x0)];
      }

    private:

      int x0 = 0, y0 = 0, z0 = 0, sx = 0, sy = 0, sz = 0;
      std::vector<bool> bits;
  };

  class rotationTables_c {

    public:

      rotationTables_c(const model_t & model, int radius, const std::vector<double> & amounts);

      int getRadius(void) const { return radius; }

      /** Number of paths per sense. */
      unsigned int size(void) const { return (unsigned int)tables[0].size(); }

      const pathTable_c & table(unsigned int sense, unsigned int idx) const;

      /** Index of the first clear path of this sense, -1 if all are blocked. */
      int firstClear(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                     unsigned int axis, int pivotHx, int pivotHy, int pivotHz, unsigned int sense) const;

      /** Static offsets that block the moving cube at doubled offset
       *  (du, dv) on every path of the family, which includes landing on
       *  its end cell. Blocking is pairwise, so this decides which pieces
       *  must turn together; see movementAnalysator_c::listRotations. */
      const std::vector<std::pair<int, int>> & blockedOnAllPaths(unsigned int sense, int du, int dv) const;

      /** The inverse of blockedOnAllPaths: for a static cube at doubled
       *  offset (du, dv) from a moving cube, every offset of that moving cube
       *  from the pivot at which the static cube blocks it on every path.
       *  Lets a caller find all such pivots of a cube pair at once. */
      const std::vector<std::pair<int, int>> & pivotsBlockedAt(unsigned int sense, int du, int dv) const;

      /** Same against a prepared occupancy, for callers that try many
       *  pivots, axes and senses against the same static cubes. */
      int firstClear(const std::vector<cell_t> & moving, const occupancy_c & stat,
                     unsigned int axis, int pivotHx, int pivotHy, int pivotHz, unsigned int sense) const;

    private:

      int radius;
      std::vector<pathTable_c> tables[2];

      /* Per sense and moving offset, the static offsets that block every
       * path of the family: one hit rejects the turn without walking all
       * tables, which is the common case in a dense puzzle. Indexed like
       * pathTable_c entries. */
      std::vector<std::vector<std::pair<int, int>>> blockedByAll[2];
      /* Per sense and moving offset (indexed like pathTable_c entries), the
       * static offsets that block some but not every path, each with the
       * paths it blocks as a bitset of `words` 64-bit words, the most
       * blocking first. firstClear ORs the bitsets of the occupied offsets
       * instead of walking the tables one by one, so its cost hardly depends
       * on the number of paths. Entry e owns the pairs from pairStart[e] to
       * pairStart[e + 1]. */
      unsigned int words = 0;
      std::vector<uint32_t> pairStart[2];
      std::vector<std::pair<int, int>> pairOffset[2];
      std::vector<uint64_t> pairBits[2];

      /* pivotsBlockedAt, indexed by (du + inverseReach) * (2 inverseReach + 1)
       * + dv + inverseReach; empty outside */
      int inverseReach = 0;
      std::vector<std::vector<std::pair<int, int>>> inverse[2];
      static const std::vector<std::pair<int, int>> none;
  };
}

#endif
