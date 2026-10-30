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
#ifndef BTQT_SCENECONTROLLER_H
#define BTQT_SCENECONTROLLER_H

#include "scenerenderer.h"

#include "../uicore/camera.h"
#include "../uicore/viewcube.h"

#include <QElapsedTimer>
#include <QObject>
#include <QSizeF>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class SettingsController;

/* What a VoxelViewport shows and how it is navigated: a camera driven by
 * pointer drags, the wheel and the view cube, with eased animations, and
 * the frame to draw.
 *
 * The main 3D view (ViewportController) and the STL export preview
 * (StlExportController) are both one; they differ only in their scene,
 * which a subclass provides by overriding frame(). The navigation follows
 * the user's settings -- rotation method, reverse scroll, projection -- in
 * both, so the preview handles exactly like the main view.
 */
class SceneController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("a base class")

  Q_PROPERTY(bool animating READ animating NOTIFY frameChanged FINAL)
  /* the size the camera projects for, in dp: the view item's, once it told */
  Q_PROPERTY(QSizeF viewportSize READ viewportSize NOTIFY frameChanged FINAL)

public:

  explicit SceneController(SettingsController * settings, QObject * parent = nullptr);

  bool animating(void) const { return m_camera.animating(); }
  QSizeF viewportSize(void) const { return QSizeF(m_camera.viewportWidth(), m_camera.viewportHeight()); }

  btui::Camera & camera(void) { return m_camera; }
  const btui::Camera & camera(void) const { return m_camera; }

  /* the multisampling Settings ▸ 3D view ▸ Anti-aliasing asks for (1, 2, 4 or 8) */
  int wantedSamples(void) const { return m_samples; }

  Q_INVOKABLE void home(void);
  Q_INVOKABLE void fit(void);

  // --- input from the view item, in dp ------------------------------------

  void setViewportSize(float w, float h);
  void pointerPress(Qt::MouseButton b, float x, float y, Qt::KeyboardModifiers mods);
  void pointerMove(float x, float y);
  void pointerRelease(Qt::MouseButton b, float x, float y);
  /* the press was taken away before its release (a modal opened, the
   * window manager took the pointer): the gesture ends where it got to,
   * without a click */
  void pointerCancel(void);
  /* one wheel event; angleDeltaY as Qt reports it (120 per notch, positive
   * when the wheel turns away from the user) */
  void wheel(int angleDeltaY);

  static constexpr float kClickSlopDp = 4.0f;

  /* the pointer over the view without a button, and the modifiers held
   * (also when only those change); it left. The main view edits from it
   * (C06): the ghost and the cursor follow. */
  virtual void pointerHover(float x, float y, Qt::KeyboardModifiers mods);
  virtual void pointerLeave(void);
  /* orbiting or panning with the pointer: the view shows the grab cursor */
  bool navigating(void) const { return m_gesture == Gesture::Orbit || m_gesture == Gesture::Pan; }
  bool panning(void) const { return m_gesture == Gesture::Pan; }
  /* the edit tool whose cursor the view shows, -1 for none, and its colour
   * (invalid: Default) -- the main view's, when a click would edit */
  virtual int editCursorTool(void) const { return -1; }
  virtual QColor editCursorColour(void) const { return {}; }

  // --- the keyboard, while the view has focus (WCAG 2.1.1) ----------------

  /* orbit by degrees about the view's own axes, at once */
  void orbitStep(float rightDeg, float upDeg);
  /* pan by dp, at once */
  void panStep(float dxDp, float dyDp);
  /* zoom by wheel notches, eased as the wheel's (+ in) */
  void zoomStep(int notches);
  /* the view cube's 90 degree turns and rolls, and its faces, animated */
  void turn(btui::ViewCube::Arrow a);
  void roll(int direction);
  void viewFrom(btui::CubeDir face);

  // --- the view cube ------------------------------------------------------

  const btui::ViewCube::Hit & cubeHover(void) const { return m_cubeHover; }
  void setCubeHover(const btui::ViewCube::Hit & h);
  void cubeClick(const btui::ViewCube::Hit & h);
  void cubeDoubleClick(const btui::ViewCube::Hit & h);
  /* dragging the cube orbits the view; on release a nearby view snaps */
  void cubeDragBegin(float x, float y);
  void cubeDragMove(float x, float y);
  void cubeDragEnd(void);

  // --- rendering -----------------------------------------------------------

  /* the frame to draw now: the camera on an empty canvas here; subclasses
   * add their scene */
  virtual SceneFrame frame(float devicePixelRatio) const;

  /* advance animations by dt (the timer calls it; tests call it directly) */
  bool tick(float dtMs);

signals:

  /* anything that changes the picture: camera, options, scene, theme */
  void frameChanged(void);
  /* a press and release within the click slop -- the editing phase uses it */
  void clicked(float x, float y, Qt::KeyboardModifiers mods);

protected:

  /* the frame's camera and canvas, for subclasses to add to */
  SceneFrame baseFrame(float devicePixelRatio) const;

  /* how a voxel style is drawn -- the one rule for the 3D view, the export
   * previews and the exported pictures. Flat and STL: back faces culled
   * (closed solids), translucent layers blended back to front (Flat's
   * variable voxels; STL has none). Classic: legacy's one translucent layer
   * and both sides of every face (its bevelled mesh's seams). */
  static void applyStyle(SceneFrame & f, btui::VoxelStyle style);

  /* Re-read from the settings what the view draws with: rotation method,
   * projection, lighting, voxel style and anti-aliasing. Read once here and
   * on every change, not per frame: a frame is built while the render
   * thread waits for it (QQuickRhiItemRenderer::synchronize). */
  void readSettings(void);

  void startTicking(void);

  /* a drag began to orbit or pan: the pointer navigates, not edits */
  virtual void navigationStarted(void) {}

  SettingsController * m_settings;
  btui::Camera m_camera;
  /* left drag pans instead of orbiting (the toolbar's Pan mode) */
  bool m_pan = false;
  // readSettings()
  bool m_lighting = true;
  btui::VoxelStyle m_voxelStyle = btui::VoxelStyle::Flat;
  int m_samples = 4;

private:

  void onTimer(void);

  // pointer gesture
  enum class Gesture { None, Pending, Orbit, Pan };
  Gesture m_gesture = Gesture::None;
  Gesture m_pendingKind = Gesture::None;
  Qt::MouseButton m_button = Qt::NoButton;
  Qt::KeyboardModifiers m_pressMods;
  float m_pressX = 0, m_pressY = 0, m_lastX = 0, m_lastY = 0;

  // view cube
  btui::ViewCube::Hit m_cubeHover;
  bool m_cubeDragging = false;

  QTimer m_timer;
  QElapsedTimer m_clock;
};

#endif
