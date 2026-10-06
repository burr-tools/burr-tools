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
#include "scenecontroller.h"

#include "settingscontroller.h"
#include "theme.h"

#include <cmath>

SceneController::SceneController(SettingsController * settings, QObject * parent) :
  QObject(parent), m_settings(settings)
{
  m_timer.setInterval(16);
  connect(&m_timer, &QTimer::timeout, this, &SceneController::onTimer);

  applyCameraSettings();
  connect(m_settings, &SettingsController::changed, this, [this] {
    applyCameraSettings();
    emit frameChanged();
  });
  if (Theme * t = Theme::instance())
    connect(t, &Theme::changed, this, &SceneController::frameChanged);
}

void SceneController::applyCameraSettings(void) {
  m_camera.setRotationMethod(m_settings->rotationMethod() == QLatin1String("arcball")
                               ? btui::Camera::RotationMethod::Arcball : btui::Camera::RotationMethod::Drag);
  m_camera.setProjection(m_settings->stringValue(QStringLiteral("view.projection"), QStringLiteral("perspective"))
                             == QLatin1String("orthographic")
                           ? btui::Camera::Projection::Orthographic : btui::Camera::Projection::Perspective);
}

SceneFrame SceneController::baseFrame(float devicePixelRatio) const {
  SceneFrame f;
  const Theme * t = Theme::instance();
  f.clear = t ? t->canvas() : QColor(228, 232, 239);
  f.view = m_camera.viewMatrix();
  f.projection = m_camera.projectionMatrix();
  f.devicePixelRatio = devicePixelRatio;
  f.lighting = m_settings->lighting();
  f.meshRevision = ~quint64(0) - 1;
  return f;
}

SceneFrame SceneController::frame(float devicePixelRatio) const {
  return baseFrame(devicePixelRatio);
}

// --- animation -------------------------------------------------------------

void SceneController::startTicking(void) {
  if (!m_timer.isActive()) {
    m_clock.start();
    m_timer.start();
  }
}

void SceneController::onTimer(void) {
  const float dt = float(m_clock.restart());
  if (!tick(dt))
    m_timer.stop();
}

bool SceneController::tick(float dtMs) {
  const bool more = m_camera.tick(dtMs);
  emit frameChanged();
  return more;
}

void SceneController::home(void) {
  m_camera.home();
  startTicking();
  emit frameChanged();
}

void SceneController::fit(void) {
  m_camera.fit();
  startTicking();
  emit frameChanged();
}

// --- pointer ----------------------------------------------------------------

void SceneController::setViewportSize(float w, float h) {
  m_camera.setViewport(w, h);
  emit frameChanged();
}

void SceneController::pointerPress(Qt::MouseButton b, float x, float y, Qt::KeyboardModifiers mods) {
  if (m_gesture != Gesture::None)
    return;
  m_button = b;
  m_pressMods = mods;
  m_pressX = m_lastX = x;
  m_pressY = m_lastY = y;

  if (b == Qt::LeftButton)
    m_pendingKind = (m_pan != bool(mods & Qt::ShiftModifier)) ? Gesture::Pan : Gesture::Orbit;
  else if (b == Qt::MiddleButton)
    m_pendingKind = Gesture::Pan;
  else
    return;    // right button: reserved
  m_gesture = Gesture::Pending;
}

void SceneController::pointerMove(float x, float y) {
  if (m_gesture == Gesture::Pending) {
    if (std::hypot(x - m_pressX, y - m_pressY) <= kClickSlopDp)
      return;
    // past the slop: navigation starts from the press point, so nothing jumps
    if (m_pendingKind == Gesture::Orbit) {
      m_camera.beginRotate(m_pressX, m_pressY);
      m_gesture = Gesture::Orbit;
    } else {
      m_camera.cancelAnimation();
      m_gesture = Gesture::Pan;
    }
    m_lastX = m_pressX;
    m_lastY = m_pressY;
  }

  if (m_gesture == Gesture::Orbit) {
    m_camera.rotateTo(x, y);
  } else if (m_gesture == Gesture::Pan) {
    m_camera.panBy(x - m_lastX, y - m_lastY);
  } else {
    return;
  }
  m_lastX = x;
  m_lastY = y;
  emit frameChanged();
}

void SceneController::pointerRelease(Qt::MouseButton b, float x, float y) {
  if (b != m_button)
    return;
  if (m_gesture == Gesture::Pending && b == Qt::LeftButton)
    emit clicked(x, y, m_pressMods);
  if (m_gesture == Gesture::Orbit)
    m_camera.endRotate();
  m_gesture = Gesture::None;
  m_button = Qt::NoButton;
  emit frameChanged();
}

void SceneController::wheel(int angleDeltaY) {
  // the spec's delta follows the browser: about 100 per notch, positive
  // toward the user (zoom out); Qt reports 120 per notch, positive away
  float delta = -float(angleDeltaY) * (100.0f / 120.0f);
  if (m_settings->reverseScroll())
    delta = -delta;
  m_camera.wheel(delta);
  startTicking();
}

// --- view cube --------------------------------------------------------------

void SceneController::setCubeHover(const btui::ViewCube::Hit & h) {
  if (h == m_cubeHover)
    return;
  m_cubeHover = h;
  emit frameChanged();
}

void SceneController::cubeClick(const btui::ViewCube::Hit & h) {
  if (h.kind == btui::ViewCube::Kind::Home) {
    home();
    return;
  }
  if (auto q = btui::ViewCube::target(h, m_camera.orientation())) {
    m_camera.animateTo(*q, false);
    startTicking();
    emit frameChanged();
  }
}

void SceneController::cubeDoubleClick(const btui::ViewCube::Hit & h) {
  // only a face centre has a double-click meaning (C13)
  if (h.kind != btui::ViewCube::Kind::Region || h.dir.count() != 1)
    return;
  m_camera.animateTo(btui::ViewCube::uprightTarget(h.dir, m_camera.orientation()), false);
  startTicking();
  emit frameChanged();
}

/* The cube is dragged in its own widget coordinates; the drag is scaled to
 * the main view's size so the rotation method sees a proportionate move. */
void SceneController::cubeDragBegin(float x, float y) {
  m_cubeDragging = true;
  m_camera.beginRotate(x * m_camera.viewportWidth() / btui::ViewCube::kWidth,
                       y * m_camera.viewportHeight() / btui::ViewCube::kHeight);
}

void SceneController::cubeDragMove(float x, float y) {
  if (!m_cubeDragging)
    return;
  m_camera.rotateTo(x * m_camera.viewportWidth() / btui::ViewCube::kWidth,
                    y * m_camera.viewportHeight() / btui::ViewCube::kHeight);
  emit frameChanged();
}

void SceneController::cubeDragEnd(void) {
  if (!m_cubeDragging)
    return;
  m_cubeDragging = false;
  m_camera.endRotate();
  if (auto q = btui::ViewCube::snapNearest(m_camera.orientation())) {
    m_camera.animateTo(*q, false);
    startTicking();
  }
  emit frameChanged();
}
