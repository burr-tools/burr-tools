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
#include "viewportcontroller.h"

#include "documentcontroller.h"
#include "guarded.h"
#include "layoutcontroller.h"
#include "settingscontroller.h"
#include "shapesmodel.h"
#include "theme.h"

#include "../uicore/palette.h"
#include "../uicore/vectorexport.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <QFile>
#include <QFileInfo>
#include <QThread>

#include <algorithm>

namespace {

  // persisted Display menu options (C06: "all persisted")
  const char * const kAxes = "view.display.axes";
  const char * const kBounds = "view.display.bounds";
  const char * const kSlab = "view.display.layerSlab";
  const char * const kDim = "view.display.dimOtherLayers";
  const char * const kProjection = "view.projection";
  const char * const kColourView = "view.colourView";

  btui::Rgba8 rgba(const QColor & c, float alpha = 1.0f) {
    return { std::uint8_t(c.red()), std::uint8_t(c.green()), std::uint8_t(c.blue()),
             std::uint8_t(std::clamp(c.alphaF() * alpha, 0.0f, 1.0f) * 255.0f + 0.5f) };
  }

  btui::Plane planeOf(int p) {
    return p == 1 ? btui::Plane::XZ : (p == 2 ? btui::Plane::YZ : btui::Plane::XY);
  }
}

ViewportController::ViewportController(SettingsController * settings, DocumentController * doc, ShapesModel * shapes,
                                       LayoutController * layout, QObject * parent) :
  SceneController(settings, parent), m_doc(doc), m_shapes(shapes), m_layout(layout)
{
  // the camera follows the settings in SceneController
  connect(m_settings, &SettingsController::changed, this, &ViewportController::optionsChanged);
  // Settings ▸ Voxel style changes the geometry itself
  connect(m_settings, &SettingsController::changed, this, [this] {
    if (m_settings->voxelStyle() != m_meshStyle)
      scheduleMesh();
  });
  connect(m_layout, &LayoutController::changed, this, [this] {
    emit optionsChanged();
    emit frameChanged();
  });

  /* One change to the puzzle reaches here several times over (an edit: the
   * document's signal, the shapes list's count and maybe its selection). The
   * shape itself is looked up at once, which is cheap and keeps the
   * properties QML reads current; its mesh is built once, after the burst. */
  connect(m_shapes, &ShapesModel::selectedChanged, this, &ViewportController::refreshShape);
  connect(m_shapes, &ShapesModel::countChanged, this, &ViewportController::refreshShape);
  connect(m_doc, &DocumentController::documentReplaced, this, &ViewportController::refreshShape);
  connect(m_doc, &DocumentController::historyApplied, this, &ViewportController::refreshShape);
  connect(m_doc, &DocumentController::puzzleEdited, this, &ViewportController::refreshShape);

  refreshShape();
  buildMesh();
}

bool ViewportController::boolSetting(const char * key, bool def) const {
  return m_settings->boolValue(QString::fromLatin1(key), def);
}

void ViewportController::setBoolSetting(const char * key, bool v) {
  if (boolSetting(key, !v) == v && m_settings->store().contains(key))
    return;
  m_settings->setBoolValue(QString::fromLatin1(key), v);
  emit optionsChanged();
  emit frameChanged();
}

void ViewportController::setNavMode(const QString & m) {
  const bool pan = (m == QLatin1String("pan"));
  if (pan == m_pan)
    return;
  m_pan = pan;
  emit optionsChanged();
}

bool ViewportController::displayAxes(void) const { return boolSetting(kAxes, true); }
void ViewportController::setDisplayAxes(bool v) { setBoolSetting(kAxes, v); }
bool ViewportController::displayBounds(void) const { return boolSetting(kBounds, true); }
void ViewportController::setDisplayBounds(bool v) { setBoolSetting(kBounds, v); }
bool ViewportController::displayLayerSlab(void) const { return boolSetting(kSlab, true); }
void ViewportController::setDisplayLayerSlab(bool v) { setBoolSetting(kSlab, v); }
bool ViewportController::displayDimOtherLayers(void) const { return boolSetting(kDim, false); }
void ViewportController::setDisplayDimOtherLayers(bool v) { setBoolSetting(kDim, v); }

QString ViewportController::projection(void) const {
  return m_settings->stringValue(QString::fromLatin1(kProjection), QStringLiteral("perspective"));
}

void ViewportController::setProjection(const QString & p) {
  const QString v = (p == QLatin1String("orthographic")) ? p : QStringLiteral("perspective");
  if (v == projection())
    return;
  m_settings->setStringValue(QString::fromLatin1(kProjection), v);
  m_camera.setProjection(v == QLatin1String("orthographic") ? btui::Camera::Projection::Orthographic
                                                            : btui::Camera::Projection::Perspective);
  emit optionsChanged();
  emit frameChanged();
}

QString ViewportController::colourView(void) const {
  return m_settings->stringValue(QString::fromLatin1(kColourView), QStringLiteral("piece"));
}

void ViewportController::setColourView(const QString & v) {
  const QString c = (v == QLatin1String("voxel")) ? v : QStringLiteral("piece");
  if (c == colourView())
    return;
  m_settings->setStringValue(QString::fromLatin1(kColourView), c);
  scheduleMesh();
  emit optionsChanged();
}

int ViewportController::layerCount(void) const {
  return m_shape ? btui::layerCount(*m_shape, planeOf(m_plane)) : 0;
}

void ViewportController::clampLayer(void) {
  const int n = layerCount();
  m_layer = n > 0 ? std::clamp(m_layer, 0, n - 1) : 0;
}

void ViewportController::setPlane(int p) {
  p = std::clamp(p, 0, 2);
  if (p == m_plane)
    return;
  m_plane = p;
  clampLayer();     // C08: keep the layer, clamped to the new plane's count
  emit layerChanged();
  emit frameChanged();
}

void ViewportController::setLayer(int l) {
  const int n = layerCount();
  l = n > 0 ? std::clamp(l, 0, n - 1) : 0;
  if (l == m_layer)
    return;
  m_layer = l;
  emit layerChanged();
  emit frameChanged();
}

void ViewportController::layerStep(int delta) {
  setLayer(m_layer + delta);
}

bool ViewportController::editorVisible(void) const { return m_layout->editorVisible(); }
bool ViewportController::showViewCube(void) const { return m_settings->showViewCube(); }

QString ViewportController::shapeId(void) const {
  return m_shapeIndex >= 0 ? QStringLiteral("S%1").arg(m_shapeIndex + 1) : QString();
}

QString ViewportController::shapeLabel(void) const {
  return m_shape ? QString::fromStdString(m_shape->getName()) : QString();
}

QColor ViewportController::shapeColor(void) const {
  return m_shapeIndex >= 0 ? ShapesModel::chipColor(m_shapeIndex) : QColor();
}

/* The selected shape, looked up again after any change to the puzzle or the
 * selection; its mesh follows once the current burst of changes is over. */
void ViewportController::refreshShape(void) {
  const puzzle_c & p = m_doc->session().puzzle();
  const int sel = m_shapes->selected();
  m_shape = (sel >= 0 && unsigned(sel) < p.getNumberOfShapes()) ? p.getShape(unsigned(sel)) : nullptr;
  m_shapeIndex = m_shape ? sel : -1;
  clampLayer();
  emit sceneChanged();
  emit layerChanged();
  scheduleMesh();
}

void ViewportController::scheduleMesh(void) {
  m_meshDirty = true;
  if (m_meshQueued)
    return;
  m_meshQueued = true;
  QMetaObject::invokeMethod(this, [this] {
    m_meshQueued = false;
    buildMesh();
  }, Qt::QueuedConnection);
}

/* Build the selected shape's mesh, if it is out of date. The camera keeps
 * its orientation and zoom, as legacy showSingleShape() keeps them, and
 * re-frames on the new shape's bounds -- zoom is relative to the fitted view.
 */
void ViewportController::buildMesh(void) {
  if (!m_meshDirty)
    return;
  m_meshDirty = false;
  const puzzle_c & p = m_doc->session().puzzle();

  if (m_shape) {
    btui::MeshOptions opt;
    opt.style = m_settings->voxelStyle() == QLatin1String("legacy") ? btui::VoxelStyle::Legacy : btui::VoxelStyle::Flat;
    opt.piece = btui::pieceColor(m_shapeIndex);
    opt.colors = colourView() == QLatin1String("voxel") ? btui::ColorMode::Voxel : btui::ColorMode::Piece;
    for (unsigned i = 0; i < p.colorNumber(); i++) {
      unsigned char r, g, b;
      p.getColor(i, &r, &g, &b);
      opt.palette.push_back({ r / 255.0f, g / 255.0f, b / 255.0f });
    }
    // once per burst of selection, history and edit changes (scheduleMesh)
    m_mesh.reset();
    guarded([&] {
      auto mesh = std::make_shared<btui::ShapeMesh>(btui::buildShapeMesh(*m_shape, opt));
      const btui::Vec3 c = (mesh->boundsMin + mesh->boundsMax) * 0.5f;
      m_camera.setScene(c, std::max(0.5f, btui::length(mesh->boundsMax - mesh->boundsMin) * 0.5f));
      m_mesh = std::move(mesh);
    });
  } else {
    m_mesh.reset();
  }
  m_meshStyle = m_settings->voxelStyle();
  m_meshRevision++;

  emit sceneChanged();      // emptyShape follows the mesh
  emit frameChanged();
}

SceneFrame ViewportController::frame(float devicePixelRatio) const {
  /* a mesh still queued is built now when asked for on this object's own
   * thread (an export, a test); the render thread draws the last one, and
   * the queued build's frameChanged brings the next frame */
  if (m_meshDirty && QThread::currentThread() == thread())
    const_cast<ViewportController *>(this)->buildMesh();
  SceneFrame f = baseFrame(devicePixelRatio);
  const Theme * t = Theme::instance();
  // the flat style's variable voxels: the inner ones show through
  f.translucentLayers = m_meshStyle != QLatin1String("legacy");

  // the Entities scene; Puzzle and Solver bring their own in later phases
  if (m_layout->workspace() != LayoutController::Entities || !m_shape || !t)
    return f;

  f.mesh = m_mesh;
  f.meshRevision = m_meshRevision;

  if (displayAxes())
    btui::addAxes(f.lines, *m_shape, rgba(t->axisX()), rgba(t->axisY()), rgba(t->axisZ()), 2.5f);
  if (displayBounds())
    btui::addBoxEdges(f.lines, btui::gridBounds(*m_shape), rgba(t->line2()), 1.0f, 4.0f, 4.0f);

  // the slab and dimming only while the voxel editor shows the layer (C06)
  if (editorVisible() && layerCount() > 0) {
    if (displayLayerSlab()) {
      const btui::Box slab = btui::layerSlab(*m_shape, planeOf(m_plane), m_layer);
      btui::addBoxFaces(f.overlayFaces, slab, rgba(t->accent(), 0.11f));
      btui::addBoxEdges(f.lines, slab, rgba(t->accent()), 1.8f);
    }
    if (displayDimOtherLayers()) {
      f.dimAxis = btui::layerAxis(planeOf(m_plane));
      f.dimLayer = float(m_layer);
    }
  }
  return f;
}

// --- vector export -----------------------------------------------------------

QString ViewportController::vectorExtension(int format) {
  const auto f = btui::VectorFormat(std::clamp(format, 0, int(btui::VectorFormat::PGF)));
  const std::string_view e = btui::vectorExtension(f);
  return QString::fromLatin1(e.data(), qsizetype(e.size()));
}

bool ViewportController::exportVector(const QUrl & file, int format) const {
  if (format < 0 || format > int(btui::VectorFormat::PGF))
    return false;
  const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
  if (path.isEmpty())
    return false;

  const SceneFrame f = frame(1);
  btui::VectorInput in;
  in.mesh = f.mesh.get();
  in.overlayFaces = &f.overlayFaces;
  in.lines = &f.lines;
  in.view = f.view;
  in.projection = f.projection;
  in.width = m_camera.viewportWidth();
  in.height = m_camera.viewportHeight();
  in.lighting = f.lighting;
  in.dimAxis = f.dimAxis;
  in.dimLayer = f.dimLayer;
  in.dimAlpha = f.dimAlpha;

  const std::string out = guarded([&] {
    return btui::writeVector(btui::projectScene(in), btui::VectorFormat(format),
                             QFileInfo(path).completeBaseName().toStdString());
  });
  if (out.empty())
    return false;
  QFile o(path);
  if (!o.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  return o.write(out.data(), qint64(out.size())) == qint64(out.size()) && o.flush();
}
