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

#include "bt_assert.h"

#include <atomic>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <numbers>
#include <optional>
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
 * Fortran SimpleRot code and PR #50's rotationRules_c use implicitly. The
 * solver's default is the STL export's pieces, play 0.002 and bevel 0.03
 * (rotationSettings_t::model).
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

  /** The one doubled in-plane offset (ou, ov) of a moving cube from the
   *  pivot at which a quarter turn in sense puts it onto the cell of a
   *  static cube in its layer at doubled offset (du, dv) from it: the
   *  pivots at which the static cube blocks the turn's end pose, whatever
   *  the motion (the relation ROTATION_SLOW's groups are closed under).
   *  The turn takes (u, v) to (-v, u) in sense 0 and to (v, -u) in sense 1.
   *  False for an offset between no two cube centres (odd du or dv). */
  inline bool endPosePivot(unsigned int sense, int du, int dv, int * ou, int * ov) {
    if ((du | dv) & 1)
      return false;
    if (sense == 0) { *ou = (dv - du) / 2; *ov = -(du + dv) / 2; }
    else            { *ou = -(du + dv) / 2; *ov = (du - dv) / 2; }
    return true;
  }

  struct model_t {
    double play = 0;     ///< gap between neighbouring cubes, cube side is 1 - play, must be > 0
    /** 45° bevel of the cube edges parallel to the rotation axis: the length
     *  cut off each adjacent face, so the bevel face is chamfer·√2 wide. A
     *  puzzlecad $burr_bevel b on cubes of size s is b / (√2 s). Printed
     *  cubes are beveled on all 12 edges; every cross-section of such a
     *  cube lies inside the octagon checked here, so for separate cubes the
     *  check is conservative. A printed piece is one body, though: the
     *  gaps of width play between its own cubes are solid and its concave
     *  edges filled, which the separate cubes checked here leave open. A
     *  static corner passing such a joint can reach up to about play / 2
     *  into the printed piece on a motion certified clear. */
    double chamfer = 0;
  };

  /** play > 0, chamfer >= 0 and a cube larger than its chamfer: the
   *  models planarScene_c accepts. */
  bool validModel(const model_t & model);

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

      /** A moving and a static cube of one layer: their centres relative
       *  to the pivot. */
      struct pair_t {
        double mu, mv, su, sv;
      };

      /** Every pair of a moving and a static cube that share a layer. */
      std::vector<pair_t> pairs(void) const;

      /** Half the side of a cube, and its bevel. */
      double halfSide(void) const { return half; }
      double bevel(void) const { return chamfer; }

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
        int axial;     // the layer's coordinate along the axis
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

  /** Kinds of rotation path, see rotationTables_c and decideTurn. */
  enum pathKind_e {
    PATH_PURE,    ///< plain quarter turn about the pivot
    PATH_WIGGLE,  ///< quarter turn with an offset amount·(cos θ + sin θ - 1)/(√2 - 1) along a direction
    PATH_SLIDE,   ///< shift by amount along a direction, quarter turn, shift back
    PATH_PLANNED, ///< any polyline, found by decideTurn for one scene
    PATH_RAMP     ///< shift out linearly until holdFrom degrees, hold until holdTo, back by 90
  };

  /** In-plane shift directions, in sixteenths of a full turn counter-
   *  clockwise from +u: even ones are the 8 compass directions. */
  constexpr int DIRECTIONS = 16;

  /** Names one path of a family; the path follows from it, see makePath. */
  struct pathSpec_t {
    pathKind_e kind = PATH_PURE;
    unsigned int sense = 0;   ///< 0 = +90°, 1 = -90°, as in quarterTurn
    int dir = 0;              ///< shift direction, 0 to DIRECTIONS - 1, unused for PATH_PURE
    double amount = 0;        ///< shift distance in cells, unused for PATH_PURE
    /** the waypoints of a PATH_PLANNED path, from the identity pose to the
     *  quarter turn of sense */
    std::vector<pose_t> waypoints = {};
    /** PATH_RAMP: the shift is reached at holdFrom and held until holdTo
     *  (degrees turned, 0 < holdFrom <= holdTo < 90) */
    int holdFrom = 0, holdTo = 0;
  };

  /** The unit vector of a shift direction (exact for the compass ones). */
  void directionVector(int dir, double * u, double * v);

  /** The compass family of fixed paths, ordered by how much a move
   *  disturbs: the pure turn, then per amount (ascending) the 8 wiggles and
   *  the 8 slides; +90° only, rotationTables_c adds the mirrored -90°.
   *  The amounts are sorted and duplicates dropped. */
  std::vector<pathSpec_t> compassFamily(const std::vector<double> & amounts);

  /** The paths the 16-direction family adds to compassFamily(compass) for
   *  the amounts `wide`: wiggles and slides in every direction the compass
   *  family lacks, and ramps out by 15° and held to 45°, out by 45° and held
   *  to 75° (its reverse), and the tent (out by 45° and straight back), per
   *  amount (ascending, duplicates dropped) in all 16 directions. See
   *  design/2026-10-19-rotation-moves.md,
   *  "The motions" and "Corpus results". */
  std::vector<pathSpec_t> wideExtension(const std::vector<double> & compass, const std::vector<double> & wide);

  /** The waypoints of a path. Wiggles are polylines of WIGGLE_SEGMENTS
   *  segments; the polyline, not the smooth profile, is the motion that is
   *  checked. */
  path_t makePath(const pathSpec_t & spec);

  constexpr int WIGGLE_SEGMENTS = 16;

  /** The pose at parameter t in [0, 1] along a path, its segments taking
   *  equal parameter time. Every pose lies on the polyline the checker
   *  certified, so animating with it shows exactly the checked motion. */
  pose_t poseAt(const path_t & path, double t);

  /** The path of the reverse turn: the same motion run backwards, as seen
   *  by the moving cubes in their turned orientation. */
  path_t reversePath(const path_t & path);

  /** A path with fewer waypoints: from each kept waypoint of `full` the
   *  straight segment to the farthest one that is proven to have at least
   *  `keep` clearance, else to the next one (so with `keep` at most the
   *  smallest clearance of `full` the result has at least `keep` too). */
  path_t shortcutPath(const planarScene_c & scene, const path_t & full, double keep);

  /** How decideTurn searches. */
  struct decideSettings_t {
    /** the motion may shift by up to this in each in-plane direction */
    double maxShift = 0.75;
    /** boxes are cut no finer than this, in cells (an angle counts by the
     *  arc of the farthest moving corner): a turn whose passage is thinner
     *  than a few of these is left undecided */
    double resolution = 1.0 / 8192;
    /** the clearance a motion must keep to count as possible: the path
     *  found is certified again by the caller, after shortcutting */
    double margin = 1e-4;
    /** give up (undecided) beyond this many boxes: a few hundred bytes
     *  each. Turns at a printed play take up to some ten thousand, only
     *  hairline passages more. Runs at the same time (a solve's workers)
     *  also share a budget of a few of these: a run that would exceed it
     *  waits for room, so a run's answer does not depend on the others
     *  (setPlannerBoxBudget). */
    size_t maxBoxes = 500000;
    /** set from another thread to give up at once; nullptr: never */
    const std::atomic<bool> * stop = nullptr;
  };

  /** What decideTurn can say. */
  enum turnAnswer_e {
    TURN_POSSIBLE,    ///< proven: `path` makes the turn
    TURN_IMPOSSIBLE,  ///< proven: no motion within the shift bound makes it
    TURN_UNDECIDED    ///< neither, at this resolution or within these limits
  };

  /** The outcome of decideTurn. */
  struct turnDecision_t {
    turnAnswer_e answer = TURN_UNDECIDED;
    /** TURN_POSSIBLE: from the identity pose to the quarter turn, every
     *  pose with at least `margin` clearance; not shortcut */
    path_t path;
    /** boxes made and searches run, for statistics */
    size_t boxes = 0;
    unsigned int rounds = 0;
    /** undecided because of maxBoxes or stop, not the resolution */
    bool gaveUp = false;
  };

  /**
   * Decides whether the scene's moving cubes can make the quarter turn of
   * sense among the static ones, by any motion in the plane with angles
   * between the start and the quarter turn and shifts up to maxShift.
   *
   * The space of poses (angle, shift) is cut into boxes, lazily. A box is
   * free when one edge normal separates every pose in it from each
   * obstacle, blocked when every pose in it lies inside one obstacle, else
   * it is split. A chain of free boxes from the start to the end pose is a
   * motion; when the blocked boxes leave no chain, there is none. Both
   * answers are proofs. See design/2026-10-19-rotation-moves.md, "The
   * planner".
   *
   * With more play the free poses only grow: possible stays possible,
   * impossible at a play is impossible at every smaller one.
   */
  turnDecision_t decideTurn(const planarScene_c & scene, unsigned int sense, const decideSettings_t & settings);

  /** The boxes all planner runs at the same time may hold together
   *  (decideSettings_t::maxBoxes), 2 000 000 unless set; returns the one
   *  before. For tests. */
  size_t setPlannerBoxBudget(size_t boxes);

  /** The answer to "can the moving cubes make this turn", see answerTurn. */
  struct turnVerdict_t {
    turnAnswer_e answer = TURN_UNDECIDED;
    /** TURN_POSSIBLE: a path that makes the turn, certified clear on the
     *  scene (planarScene_c::certifyPath); empty when an inverse known path
     *  made it (knownPath_t) */
    path_t path;
    /** TURN_POSSIBLE because of the known path with this index; -1: by
     *  decideTurn */
    int known = -1;
  };

  /**
   * The one definition of a turn: can the scene's moving cubes make the
   * quarter turn of sense by any motion in the plane with shifts up to
   * settings.maxShift? A known path that certifies clear answers yes with
   * that path (the first of them); else decideTurn decides, and its path is
   * shortcut (keeping half the margin) and certified. A possible answer
   * whose path does not certify is undecided: the turn may be possible.
   * Every caller that asks whether a turn works asks this.
   */
  turnVerdict_t answerTurn(const planarScene_c & scene, unsigned int sense, const decideSettings_t & settings,
                           const std::vector<path_t> & known = {});

  /** A path known to make a turn, for answerTurn. inverse: the motion of
   *  the static cubes relative to the moving ones (how the search records a
   *  turn in which the other pieces followed it), certified on the scene
   *  with the two roles swapped. */
  struct knownPath_t {
    path_t path;
    bool inverse = false;
  };

  /** answerTurn on the scene of these cubes and model; known paths may be
   *  inverse ones. */
  turnVerdict_t answerTurn(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                           unsigned int axis, int pivotHx, int pivotHy, int pivotHz, unsigned int sense,
                           const model_t & model, const decideSettings_t & settings,
                           const std::vector<knownPath_t> & known);

  /** Where decideRemoval's group leaves the others. */
  struct removalGoal_t {
    /** true: a slide along the turn axis, towards larger axial coordinates
     *  for sign +1, smaller ones for -1; false: a slide in the plane along
     *  the unit vector (wu, wv) (in-plane coordinates, see planarScene_c) */
    bool axial = true;
    int sign = 1;
    double wu = 1, wv = 0;
    /** the motion turns by at most this either way (radians) */
    double maxAngle = 0.5 * std::numbers::pi;
  };

  /**
   * Decides whether `moving` can leave `stat` by a motion in the plane of
   * axis followed by a straight slide: decideTurn's motions (turning about
   * the pivot, shifts up to maxShift in each in-plane direction), with
   * angles between -goal.maxAngle and goal.maxAngle, ending at any pose
   * from which the slide of `goal` is clear. For a slide along the axis that
   * pose must be clear of the static cubes of every layer the group passes
   * (each moving cube's footprint against every static cube of its own
   * layer and the layers ahead of it); for one in the plane, of every static
   * cube of its layer along the way. The motion up to that pose is judged
   * against the same layer only, like a turn.
   *
   * POSSIBLE: `path` runs from the identity pose to such a pose, with the
   * margin; certifyRemoval checks it. IMPOSSIBLE: a proof that no motion
   * within the bounds reaches one. UNDECIDED: neither, at the resolution or
   * within maxBoxes.
   */
  turnDecision_t decideRemoval(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                               unsigned int axis, int pivotHx, int pivotHy, int pivotHz, const model_t & model,
                               const removalGoal_t & goal, const decideSettings_t & settings);

  /** Certifies a removal: the path among the same layer's static cubes as
   *  planarScene_c::certifyPath does, then the slide of goal from its last
   *  pose. */
  verdict_e certifyRemoval(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                           unsigned int axis, int pivotHx, int pivotHy, int pivotHz, const model_t & model,
                           const removalGoal_t & goal, const path_t & path);

  /** The outcome of widestTurn. */
  struct turnPlan_t {
    bool found = false;
    path_t path;                ///< from the identity pose to the quarter turn, shortcut
    /** certified lower bound on the path's smallest clearance */
    double clearance = -1e9;
    /** nothing found because decideTurn was undecided: the turn may be possible */
    bool undecided = false;
  };

  /**
   * The quarter turn of the scene's moving cubes along a motion with about
   * the most room, the one to animate: decideTurn with the largest margin
   * (to within tolerance) that still leaves a motion, shortcut keeping most
   * of that margin, its clearance certified on the polyline itself
   * (planarScene_c::minClearance). No motion keeps more than the start and
   * the end pose have, which is where the search for the margin starts.
   */
  turnPlan_t widestTurn(const planarScene_c & scene, unsigned int sense, const decideSettings_t & settings,
                        double tolerance = 0.001);

  /** The play (or bevel) a quarter turn needs, see requiredPlay and
   *  requiredBevel; for requiredBevel read bevel for play below. */
  struct playBound_t {
    bool found = false;
    /** upper bound: the turn is proven possible at this play, along `path`
     *  or the known path `known` */
    double upper = 0;
    path_t path;
    /** the known path that makes the turn at `upper` (then `path` is it,
     *  or empty for an inverse one); -1: decideTurn's path */
    int known = -1;
    /** lower bound: proven impossible at this play and below, for every
     *  motion with shifts up to maxShift (a wilder one could still get
     *  through); 0 if nothing could be proven. Also set when the turn was
     *  not found up to maxPlay. */
    double lower = 0;
  };

  /**
   * The smallest play at which the moving cubes can make the quarter turn
   * about the pivot among the static ones, by any motion with shifts up to
   * settings.maxShift: bisection over the play with answerTurn, to within
   * tolerance, once for the smallest play proven possible and once for the
   * largest proven impossible. Between the two decideTurn is undecided at
   * its resolution; they are mostly less than 0.001 apart. The chamfer is
   * taken from model, its play is the starting guess. Not found when the
   * turn needs more than maxPlay.
   *
   * known: paths the caller knows (a recorded one), which decideTurn may
   * miss (it wants its margin, and stays within maxShift); each play is
   * asked with them, so a turn whose known path is clear at the model's
   * play needs at most that play.
   */
  playBound_t requiredPlay(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                           unsigned int axis, int pivotHx, int pivotHy, int pivotHz, unsigned int sense,
                           const model_t & model, const decideSettings_t & settings,
                           double tolerance = 0.0005, double maxPlay = 0.4,
                           const std::vector<knownPath_t> & known = {});

  /**
   * The smallest bevel at which the quarter turn is possible at the
   * model's play, like requiredPlay with the bevel in place of the play:
   * more bevel only takes material away, so it only adds motions. upper
   * is the bevel proven possible (0 when the turn works without one),
   * lower the largest proven impossible; not found when even the largest
   * bevel the play allows (validModel) does not do. The bevel is what a
   * printer leaves to choose once its play is given.
   */
  playBound_t requiredBevel(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                            unsigned int axis, int pivotHx, int pivotHy, int pivotHz, unsigned int sense,
                            const model_t & model, const decideSettings_t & settings,
                            double tolerance = 0.0005, const std::vector<knownPath_t> & known = {});

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

      /** The same path's table at a model with at least tighter's play
       *  (and the same bevel): with more play a pair only gets clearer, so
       *  only tighter's blockers are certified again. Equal to the table
       *  built from scratch. */
      pathTable_c(const model_t & model, const pathTable_c & tighter);

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

  /**
   * The family of fixed rotation paths a rotation move may use, one
   * pathTable_c each, in the family's order (firstClear returns the first
   * clear one, which a move records).
   *
   * Only the +90° tables for directions 0 to 3 (sixteenths) are computed
   * with the checker, all others are turned and mirrored copies.
   */
  class rotationTables_c {

    public:

      /** The tables of a family of +90° paths, in its order; the -90°
       *  paths are their mirror images. */
      rotationTables_c(const model_t & model, int radius, const std::vector<pathSpec_t> & family);

      /** The tables of tighter's family and radius at a model with at least
       *  its play and the same bevel, from tighter's (see pathTable_c): for
       *  the planner's gate, which needs a second set at more play. */
      rotationTables_c(const model_t & model, const rotationTables_c & tighter);

      int getRadius(void) const { return radius; }

      /** Number of paths per sense. */
      unsigned int size(void) const { return (unsigned int)paths[0].size(); }

      pathTableView_c table(unsigned int sense, unsigned int idx) const;

      /** Index of the first clear path of this sense, -1 if all are
       *  blocked. Throws like pathTable_c::blocked if a moving cube lies
       *  outside the radius. */
      int firstClear(const std::vector<cell_t> & moving, const std::vector<cell_t> & stat,
                     unsigned int axis, int pivotHx, int pivotHy, int pivotHz, unsigned int sense) const;

      /** Static offsets that block the moving cube at doubled offset
       *  (du, dv) on every path of the family, which includes landing on
       *  its end cell. Blocking is pairwise, so this decides which pieces
       *  must turn together; see movementAnalysator_c::listRotations. */
      blockerRange_c blockedOnAllPaths(unsigned int sense, int du, int dv) const;

      /** The inverse of blockedOnAllPaths: for a static cube at doubled
       *  offset (du, dv) from a moving cube, every offset of that moving cube
       *  from the pivot at which the static cube blocks it on every path.
       *  Lets a caller find all such pivots of a cube pair at once. */
      blockerRange_c pivotsBlockedAt(unsigned int sense, int du, int dv) const;

      /** Same against a prepared occupancy or any isStatic(x, y, z) -> bool
       *  predicate, avoiding building a separate occupancy_c per candidate. */
      template<typename OccupiedFn>
      requires std::invocable<const OccupiedFn &, int, int, int>
      int firstClear(const std::vector<cell_t> & moving, const OccupiedFn & isStatic,
                     unsigned int axis, int pivotHx, int pivotHy, int pivotHz, unsigned int sense) const {
        if (sense > 1 || axis > 2)
          bt_te("rotationTables_c::firstClear: sense must be 0 or 1, axis 0 to 2");

        const pathTable_c & first = baseTables[0];

        /* in-plane doubled pivot */
        int pu, pv;
        if (axis == 0)      { pu = pivotHy; pv = pivotHz; }
        else if (axis == 1) { pu = pivotHz; pv = pivotHx; }
        else                { pu = pivotHx; pv = pivotHy; }

        struct movingCube_t { int layer, u, v; };
        movingCube_t stackCubes[64];
        std::vector<movingCube_t> heapCubes;
        movingCube_t * cubes = stackCubes;
        const size_t n = moving.size();
        if (n > 64) {
          heapCubes.resize(n);
          cubes = heapCubes.data();
        }

        for (size_t k = 0; k < n; k++) {
          const cell_t & c = moving[k];
          int u, v;
          if (axis == 0)      { u = 2 * c.y - pivotHy; v = 2 * c.z - pivotHz; }
          else if (axis == 1) { u = 2 * c.z - pivotHz; v = 2 * c.x - pivotHx; }
          else                { u = 2 * c.x - pivotHx; v = 2 * c.y - pivotHy; }
          if (!first.covers(u, v) || ((u ^ v) & 1))
            bt_te("rotationTables_c::firstClear: cube outside the tables or edge pivot");
          cubes[k] = { axialCoordinate(c.x, c.y, c.z, axis), u, v };
        }

        auto occupied = [&](int layer, int su, int sv) {
          const int u = (pu + su) / 2, v = (pv + sv) / 2;
          if (axis == 0)      return static_cast<bool>(isStatic(layer, u, v));
          else if (axis == 1) return static_cast<bool>(isStatic(v, layer, u));
          else                return static_cast<bool>(isStatic(u, v, layer));
        };

        const unsigned char mirrorXf = sense ? 4 : 0;
        const auto * commonData = blockedByAllEntries.data();
        for (size_t k = 0; k < n; k++) {
          const unsigned int s = first.slot(cubes[k].u, sense ? -cubes[k].v : cubes[k].v);
          for (const auto & [su, sv] : blockerRange_c(commonData + blockedByAllOffsets[s],
                                                      commonData + blockedByAllOffsets[s + 1], mirrorXf))
            if (occupied(cubes[k].layer, su, sv))
              return -1;
        }

        /* OR the paths every occupied blocker blocks; the first path left over
         * is the first clear one. Sense 1 mirrors (u, v) -> (u, -v) into sense 0. */
        const size_t numPaths = paths[sense].size();
        const uint64_t lastWord = (numPaths % 64) ? (uint64_t(1) << (numPaths % 64)) - 1 : ~uint64_t(0);
        uint64_t local[16];
        std::vector<uint64_t> heap;
        uint64_t * blocked = local;
        if (words > 16) {
          heap.assign(words, 0);
          blocked = heap.data();
        } else
          std::fill(local, local + words, 0);
        auto allBlocked = [&]() {
          for (unsigned int w = 0; w + 1 < words; w++)
            if (blocked[w] != ~uint64_t(0))
              return false;
          return (blocked[words - 1] & lastWord) == lastWord;
        };

        for (size_t k = 0; k < n; k++) {
          const unsigned int s = first.slot(cubes[k].u, sense ? -cubes[k].v : cubes[k].v);
          for (uint32_t j = pairStart[s]; j < pairStart[s + 1]; j++) {
            const auto & off = pairOffset[j];
            const int su = off.first, sv = sense ? -off.second : off.second;
            if (!occupied(cubes[k].layer, su, sv))
              continue;
            const uint64_t * b = &pairBits[(size_t)j * words];
            for (unsigned int w = 0; w < words; w++)
              blocked[w] |= b[w];
            if (allBlocked())
              return -1;
          }
        }

        for (size_t i = 0; i < numPaths; i++)
          if (!((blocked[i / 64] >> (i % 64)) & 1))
            return (int)i;
        return -1;
      }

    private:

      struct pathEntry_t {
        uint16_t baseIdx;
        unsigned char xform;
        pathSpec_t spec;
      };

      /** the rest of a constructor, once the distinct +90° base tables are
       *  built: family[i] is a view of baseTables[baseIndex[i]] */
      void assemble(const std::vector<pathSpec_t> & family, const std::vector<size_t> & baseIndex,
                    std::vector<std::optional<pathTable_c>> & built);

      int radius;
      std::vector<pathTable_c> baseTables;
      std::vector<pathEntry_t> paths[2];

      /* Sense-0 blockers common to every path (same-parity CSR like pathTable_c);
       * sense 1 is served by mirroring across the u axis (xform 4). */
      std::vector<uint32_t> blockedByAllOffsets;
      std::vector<std::pair<int16_t, int16_t>> blockedByAllEntries;

      /* Sense-0 partial blockers (blocking some but not all paths), same-parity
       * CSR indexed like pathTable_c, most-blocking first; sense 1 mirrors v. */
      unsigned int words = 0;
      std::vector<uint32_t> pairStart;
      std::vector<std::pair<int16_t, int16_t>> pairOffset;
      std::vector<uint64_t> pairBits;

      /* Sense-0 inverse of blockedByAll (even-only relative offsets (su-du, sv-dv)
       * in [-inverseReach, inverseReach], CSR); sense 1 mirrors across u. */
      int inverseReach = 0;
      std::vector<uint32_t> inverseOffsets;
      std::vector<std::pair<int16_t, int16_t>> inverseEntries;
  };
}

#endif
