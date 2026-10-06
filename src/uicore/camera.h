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
#ifndef BTUI_CAMERA_H
#define BTUI_CAMERA_H

#include "vecmath.h"

/* The 3D view's camera (spec C06 navigation, C13 view cube).
 *
 * The camera orbits a target -- the centre of what is shown -- at a distance
 * derived from the zoom: zoom 1 frames the scene's bounding sphere, and the
 * user may zoom between 0.3x and 4x of that. The orientation is a quaternion
 * rather than azimuth/elevation, because both legacy rotation methods rotate
 * freely (no fixed "up") and the view cube can roll the view; a manual orbit
 * after a roll therefore keeps the roll, as C13 requires.
 *
 * Time only advances through tick(), which makes every animation -- the
 * 360 ms snaps and the eased wheel zoom -- deterministic under test.
 *
 * Screen positions are in dp with the origin at the top left of the view.
 */
namespace btui {

  class Camera {

    public:

      enum class Projection { Perspective, Orthographic };

      /* Drag is legacy "new rotation method" (method2_c, the default);
       * Arcball is legacy arcBall_c. Both are ported unchanged. */
      enum class RotationMethod { Drag, Arcball };

      static constexpr float kFovYDeg = 15.0f;       // legacy gluPerspective(15, ...)
      static constexpr float kAnimMs = 360.0f;       // C13 snap duration
      static constexpr float kZoomTauMs = 90.0f;     // C06 zoom easing time constant
      static constexpr float kMinZoom = 0.3f;
      static constexpr float kMaxZoom = 4.0f;
      static constexpr float kHomeYawDeg = -32.0f;   // C13 default view
      static constexpr float kHomePitchDeg = 26.0f;

      Camera(void);

      // --- what is viewed ----------------------------------------------------

      void setViewport(float widthDp, float heightDp);
      float viewportWidth(void) const { return vw; }
      float viewportHeight(void) const { return vh; }

      /* The bounding sphere to frame. Orientation, zoom and pan are kept, so
       * switching shapes does not throw the view around (legacy behaviour).
       */
      void setScene(Vec3 centre, float radius);
      Vec3 sceneCentre(void) const { return centre; }
      float sceneRadius(void) const { return radius; }

      Projection projection(void) const { return proj; }
      void setProjection(Projection p) { proj = p; }

      RotationMethod rotationMethod(void) const { return method; }
      void setRotationMethod(RotationMethod m) { method = m; }

      // --- state -------------------------------------------------------------

      Quat orientation(void) const { return orient; }
      void setOrientation(Quat q) { orient = normalize(q); }

      float zoom(void) const { return z; }
      float zoomTarget(void) const { return zTarget; }
      void setZoom(float v);

      /* pan of the target point in view space, in world units */
      float panX(void) const { return px; }
      float panY(void) const { return py; }

      /* camera distance at zoom 1, framing the scene sphere in both axes */
      float fitDistance(void) const;
      float distance(void) const { return fitDistance() / z; }

      Mat4 viewMatrix(void) const;
      Mat4 projectionMatrix(void) const;
      void clipPlanes(float * zNear, float * zFar) const;

      /* world units per dp at the target's depth */
      float worldPerDp(void) const;

      // --- navigation -------------------------------------------------------

      /* Orbit with the active rotation method. Starting an orbit cancels any
       * running animation where it stands. */
      void beginRotate(float x, float y);
      void rotateTo(float x, float y);
      void endRotate(void);
      bool rotating(void) const { return dragging; }

      /* move the content by a screen distance; cancels a running animation */
      void panBy(float dxDp, float dyDp);

      /* One wheel event. delta follows the browser convention the spec uses:
       * positive scrolls "down" and zooms out. The target zoom is multiplied
       * by exp(-delta * 0.0016); tick() eases toward it. Cancels a snap. */
      void wheel(float delta);

      /* Advance time. Returns true while anything is still moving. */
      bool tick(float dtMs);

      // --- animations (C13) -------------------------------------------------

      /* Animate to an orientation over kAnimMs, easing in and out; with
       * frame=true zoom and pan go back to the framed view as well (Home).
       * Starting from the target already, nothing animates. */
      void animateTo(Quat target, bool frame);

      /* Home: the default isometric view, framed. */
      void home(void);

      /* Fit: zoom 1 and no pan, orientation kept. */
      void fit(void);

      bool animating(void) const { return anim; }

      /* stop a running animation at its current frame */
      void cancelAnimation(void);

      // --- picking ----------------------------------------------------------

      struct Ray { Vec3 origin, dir; };

      /* the world-space ray through a point of the view */
      Ray rayAt(float x, float y) const;

      /* where a world point lands on the view, in dp (z: view-space depth) */
      Vec3 project(Vec3 world) const;

      // --- helpers ----------------------------------------------------------

      /* R = Rz(roll) Rx(pitch) Ry(yaw), the mock's convention: yaw turns
       * about the world Y axis, positive pitch looks down from above. */
      static Quat fromYawPitchRoll(float yawDeg, float pitchDeg, float rollDeg);

      static Quat homeOrientation(void) { return fromYawPitchRoll(kHomeYawDeg, kHomePitchDeg, 0); }

      /* The orientation that looks along -dir (dir points from the target to
       * the viewer) with no roll. Looking straight down or up the Y axis,
       * yawHintDeg (rounded to a multiple of 90) keeps the view axis-aligned
       * (C13: the top and bottom views come out square). */
      static Quat lookFrom(Vec3 dir, float yawHintDeg);

      /* yaw of the current view direction, degrees */
      float yawDeg(void) const;

    private:

      Vec3 arcballVector(float x, float y) const;
      Vec3 dragVector(float x, float y) const;

      float vw = 800, vh = 600;
      Vec3 centre;
      float radius = 1;
      Projection proj = Projection::Perspective;
      RotationMethod method = RotationMethod::Drag;

      Quat orient;
      float z = 1, zTarget = 1;
      float px = 0, py = 0;

      // rotation drag state
      bool dragging = false;
      Quat dragStartOrient;
      Vec3 dragStartVec;
      float lastX = 0, lastY = 0;

      // animation state
      bool anim = false;
      bool animFrame = false;
      float animT = 0;
      Quat animFrom, animTo;
      float animZoomFrom = 1, animPanXFrom = 0, animPanYFrom = 0;
  };
}

#endif
