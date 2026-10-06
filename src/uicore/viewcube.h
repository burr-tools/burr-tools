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
#ifndef BTUI_VIEWCUBE_H
#define BTUI_VIEWCUBE_H

#include "vecmath.h"

#include <optional>
#include <vector>

/* The view cube's geometry and behaviour (spec C13), without drawing.
 *
 * Everything is in the coordinates of the spec's 156 x 170 dp widget. The
 * cube is the world-axis cube [-1, 1]^3 seen with the camera's orientation,
 * projected like the main view: in perspective from 5.5 half-sizes away, or
 * orthographically.
 *
 * The 26 cube regions -- 6 face centres, 12 edges, 8 corners -- are named by
 * the direction they snap to: a vector whose components are -1, 0 or 1,
 * with one non-zero component for a face, two for an edge and three for a
 * corner. The snap target is that vector normalised, and the highlight
 * covers the region's patch on every face it touches.
 */
namespace btui {

  struct Vec2 { float x = 0, y = 0; };

  struct CubeDir {
    int x = 0, y = 0, z = 0;
    int count(void) const { return (x != 0) + (y != 0) + (z != 0); }
    Vec3 vec(void) const { return { float(x), float(y), float(z) }; }
    bool operator==(const CubeDir &) const = default;
  };

  class ViewCube {

    public:

      // widget layout (C13 anatomy, in dp)
      static constexpr float kWidth = 156, kHeight = 170;
      static constexpr float kCentreX = 78, kCentreY = 100;
      static constexpr float kHalf = 30;               // cube half-size
      static constexpr float kEyeDistance = 5.5f;      // perspective camera, in half-sizes
      static constexpr float kInner = 0.56f;           // face-centre region: |u|, |v| <= kInner
      static constexpr float kHomeX = 26, kHomeY = 40, kHomeHit = 15;
      static constexpr float kArrowOffset = 50;        // arrow centres from the cube centre
      static constexpr float kArrowHit = 11;
      static constexpr float kRollCcwX = 103, kRollCcwY = 44;   // the arch over the top
      static constexpr float kRollCwX = 132, kRollCwY = 72;     // the ")" down the right
      static constexpr float kRollRadius = 9;
      static constexpr float kRollHit = 16;
      static constexpr float kAxisX = 18, kAxisY = 152, kAxisLength = 24;
      /* Room outside the design's 156 x 170 for the axis indicator, whose
       * labels sit 33 dp from its origin -- past the left and bottom edges
       * when an axis points that way. The item adds it at the left and the
       * bottom, so the cube itself keeps its place (C13). */
      static constexpr float kPadLeft = 22, kPadBottom = 22;
      static constexpr float kItemWidth = kWidth + kPadLeft, kItemHeight = kHeight + kPadBottom;

      enum class Kind { None, Region, Home, Arrow, Roll };
      enum class Arrow { Up, Down, Left, Right };

      struct Hit {
        Kind kind = Kind::None;
        CubeDir dir;                 // Kind::Region
        Arrow arrow = Arrow::Up;     // Kind::Arrow
        int roll = 0;                // Kind::Roll: +1 counter-clockwise, -1 clockwise
        bool operator==(const Hit &) const = default;
      };

      /* A face of the cube and the frame its label is drawn in: normal n,
       * the label's baseline direction b and its up direction t (b x t = n).
       * Faces: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z.
       */
      struct Face {
        Vec3 n, b, t;
        const char * label = nullptr;
      };
      static const Face & face(int i);

      /* A rectangle on one face in its (b, t) coordinates, each in [-1, 1]. */
      struct Patch {
        int face;
        float u0, u1, v0, v1;
      };

      /* Project a point of the cube to the widget; z is the view-space
       * depth (larger is nearer the viewer). */
      static Vec3 project(Quat orient, bool perspective, Vec3 p);

      /* is the face turned toward the eye */
      static bool faceVisible(Quat orient, bool perspective, int f);

      /* view direction equals a face normal and the up axis is a world axis:
       * the cube shows the 90-degree and roll arrows */
      static bool faceAligned(Quat orient);

      /* What is under a widget point. Arrows and roll controls only exist
       * while the view is face-aligned. */
      static Hit hitTest(Quat orient, bool perspective, float x, float y);

      /* The patches a region covers on the faces it touches (all of them,
       * visible or not -- the caller skips hidden faces). */
      static std::vector<Patch> regionPatches(CubeDir dir);

      /* Where clicking a hit takes the view. Regions look along their
       * direction with no roll (the top and bottom stay axis-aligned to the
       * current yaw); arrows and roll controls step 90 degrees from the
       * current orientation, keeping everything else. Home is the camera's
       * own business and gives nothing here. */
      static std::optional<Quat> target(const Hit & h, Quat current);

      /* Double-click on a face centre: that face view, rolled so the face's
       * label reads upright. */
      static Quat uprightTarget(CubeDir faceDir, Quat current);

      /* After the cube itself was dragged: the region view the orientation is
       * within about 14 degrees of, if any (legacy snapNearest). */
      static std::optional<Quat> snapNearest(Quat current);

      static float yawOf(Quat q);
  };
}

#endif
