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
 * rounded with radius rounding. play = 0.081, rounding = 0 is the model the
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

  struct model_t {
    double play;      ///< gap between neighbouring cubes, cube side is 1 - play
    double rounding;  ///< radius of the cube edges parallel to the rotation axis
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

      /** Certify the piecewise linear path through the given poses. */
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
      double half;     // half side of the core square (cube minus rounding)
      double rounding;
      double rho;

      bool certifySegment(const pose_t & a, const pose_t & b, verdict_e * v) const;
  };
}

#endif
