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

  Q_PROPERTY(bool animating READ animating NOTIFY frameChanged)
  /* the size the camera projects for, in dp: the view item's, once it told */
  Q_PROPERTY(QSizeF viewportSize READ viewportSize NOTIFY frameChanged)

public:

  explicit SceneController(SettingsController * settings, QObject * parent = nullptr);

  bool animating(void) const { return m_camera.animating(); }
  QSizeF viewportSize(void) const { return QSizeF(m_camera.viewportWidth(), m_camera.viewportHeight()); }

  btui::Camera & camera(void) { return m_camera; }
  const btui::Camera & camera(void) const { return m_camera; }

  Q_INVOKABLE void home(void);
  Q_INVOKABLE void fit(void);

  // --- input from the view item, in dp ------------------------------------

  void setViewportSize(float w, float h);
  void pointerPress(Qt::MouseButton b, float x, float y, Qt::KeyboardModifiers mods);
  void pointerMove(float x, float y);
  void pointerRelease(Qt::MouseButton b, float x, float y);
  /* one wheel event; angleDeltaY as Qt reports it (120 per notch, positive
   * when the wheel turns away from the user) */
  void wheel(int angleDeltaY);

  static constexpr float kClickSlopDp = 4.0f;

  // --- the view cube ------------------------------------------------------

  btui::ViewCube::Hit cubeHover(void) const { return m_cubeHover; }
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

  /* re-read rotation method and projection from the settings */
  void applyCameraSettings(void);

  void startTicking(void);

  SettingsController * m_settings;
  btui::Camera m_camera;
  /* left drag pans instead of orbiting (the toolbar's Pan mode) */
  bool m_pan = false;

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
