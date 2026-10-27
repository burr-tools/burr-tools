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
#include "editorcontroller.h"
#include "guarded.h"
#include "layoutcontroller.h"
#include "localpath.h"
#include "settingscontroller.h"
#include "shapesmodel.h"
#include "statuscontroller.h"
#include "theme.h"

#include "../uicore/palette.h"
#include "../uicore/vectorexport.h"
#include "../uicore/vocabulary.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
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

  QString word(std::string_view w) { return QString::fromLatin1(w.data(), qsizetype(w.size())); }
}

ViewportController::ViewportController(SettingsController * settings, DocumentController * doc, ShapesModel * shapes,
                                       LayoutController * layout, EditorController * editor, QObject * parent) :
  SceneController(settings, parent), m_doc(doc), m_shapes(shapes), m_layout(layout), m_editor(editor)
{
  /* the camera, lighting and voxel style follow the settings in
   * SceneController, connected first; Settings ▸ Voxel style changes the
   * geometry itself */
  readDisplay();
  connect(m_settings, &SettingsController::changed, this, [this] {
    readDisplay();
    if (m_voxelStyle != m_meshStyle)
      scheduleMesh();
    emit optionsChanged();
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
  connect(m_doc, &DocumentController::puzzleEdited, this,
          [this](int, DocumentController::Change c) { edited(c); });
  // the slab follows the editor's plane and layer, the ghosts its hover preview
  connect(m_editor, &EditorController::layerChanged, this, [this] {
    updateSlab();
    emit frameChanged();
  });
  connect(m_editor, &EditorController::previewChanged, this, &ViewportController::frameChanged);
  connect(this, &SceneController::clicked, this, &ViewportController::click);
  // Edit mode, the tool or Mirror / Span changed under a still pointer: the target follows
  connect(m_editor, &EditorController::editModeChanged, this, [this] {
    if (m_editor->over3d())
      pointerHover(m_hoverX, m_hoverY, QGuiApplication::queryKeyboardModifiers());
  });

  refreshShape();
  buildMesh();
}

void ViewportController::readDisplay(void) {
  m_display.axes = m_settings->boolValue(QString::fromLatin1(kAxes), true);
  m_display.bounds = m_settings->boolValue(QString::fromLatin1(kBounds), true);
  m_display.slab = m_settings->boolValue(QString::fromLatin1(kSlab), true);
  m_display.dim = m_settings->boolValue(QString::fromLatin1(kDim), false);
}

void ViewportController::setDisplay(const char * key, bool & option, bool v) {
  if (option == v && m_settings->store().contains(key))
    return;
  option = v;
  m_settings->setBoolValue(QString::fromLatin1(key), v);
  emit optionsChanged();
  emit frameChanged();
}

QString ViewportController::navMode(void) const {
  return word(btui::kNavModes.word(m_pan ? btui::NavMode::Pan : btui::NavMode::Orbit));
}

void ViewportController::setNavMode(const QString & m) {
  const bool pan = btui::kNavModes.parse(m.toStdString()) == btui::NavMode::Pan;
  if (pan == m_pan)
    return;
  m_pan = pan;
  emit optionsChanged();
}

void ViewportController::setDisplayAxes(bool v) { setDisplay(kAxes, m_display.axes, v); }
void ViewportController::setDisplayBounds(bool v) { setDisplay(kBounds, m_display.bounds, v); }
void ViewportController::setDisplayLayerSlab(bool v) { setDisplay(kSlab, m_display.slab, v); }
void ViewportController::setDisplayDimOtherLayers(bool v) { setDisplay(kDim, m_display.dim, v); }

QString ViewportController::projection(void) const {
  return word(btui::kProjections.word(
      btui::kProjections.parse(m_settings->stringValue(QString::fromLatin1(kProjection), QString()).toStdString())));
}

void ViewportController::setProjection(const QString & p) {
  const btui::Camera::Projection v = btui::kProjections.parse(p.toStdString());
  const QString w = word(btui::kProjections.word(v));
  if (w == projection())
    return;
  m_settings->setStringValue(QString::fromLatin1(kProjection), w);
  m_camera.setProjection(v);
  emit optionsChanged();
  emit frameChanged();
}

btui::ColorMode ViewportController::colourViewValue(void) const {
  return btui::kColourViews.parse(m_settings->stringValue(QString::fromLatin1(kColourView), QString()).toStdString());
}

QString ViewportController::colourView(void) const {
  return word(btui::kColourViews.word(colourViewValue()));
}

void ViewportController::setColourView(const QString & v) {
  const btui::ColorMode c = btui::kColourViews.parse(v.toStdString());
  if (c == colourViewValue())
    return;
  m_settings->setStringValue(QString::fromLatin1(kColourView), word(btui::kColourViews.word(c)));
  // the STL style draws in the shape's colour alone: nothing to rebuild (a
  // later switch of style rebuilds, and reads the colours then)
  if (m_voxelStyle != btui::VoxelStyle::Stl)
    scheduleMesh();
  emit optionsChanged();
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

QColor ViewportController::shapeTextColor(void) const {
  return m_shapeIndex >= 0 && !btui::prefersWhiteText(m_shapeIndex) ? QColor(Qt::black) : QColor(Qt::white);
}

/* The selected shape, looked up again after any change to the puzzle or the
 * selection; its mesh follows once the current burst of changes is over. */
void ViewportController::refreshShape(void) {
  const puzzle_c & p = m_doc->session().puzzle();
  const int sel = m_shapes->selected();
  m_shape = (sel >= 0 && unsigned(sel) < p.getNumberOfShapes()) ? p.getShape(unsigned(sel)) : nullptr;
  m_shapeIndex = m_shape ? sel : -1;
  emit sceneChanged();
  scheduleMesh();
}

/* An edit rebuilds the mesh only when it changed what the mesh shows: a
 * rename or a weight does not, a colour only in the voxel colour view. */
void ViewportController::edited(DocumentController::Change change) {
  switch (change) {
    case DocumentController::Change::Structure:
    case DocumentController::Change::Voxels:
      refreshShape();
      break;
    case DocumentController::Change::Label:
      emit sceneChanged();       // the view tag's label
      break;
    case DocumentController::Change::Palette:
      if (colourViewValue() == btui::ColorMode::Voxel && m_voxelStyle != btui::VoxelStyle::Stl)
        scheduleMesh();
      break;
  }
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
    opt.style = m_voxelStyle;
    opt.piece = btui::pieceColor(m_shapeIndex);
    opt.colors = colourViewValue();
    for (unsigned i = 0; i < p.colorNumber(); i++) {
      unsigned char r, g, b;
      p.getColor(i, &r, &g, &b);
      opt.palette.push_back({ r / 255.0f, g / 255.0f, b / 255.0f });
    }
    // once per burst of selection, history and edit changes (scheduleMesh)
    m_mesh.reset();
    guarded([&] {
      auto mesh = std::make_shared<btui::ShapeMesh>(btui::buildShapeMesh(*m_shape, opt));
      m_pick = btui::buildPickMesh(*m_shape);
      const btui::Vec3 c = (mesh->boundsMin + mesh->boundsMax) * 0.5f;
      m_camera.setScene(c, std::max(0.5f, btui::length(mesh->boundsMax - mesh->boundsMin) * 0.5f));
      m_mesh = std::move(mesh);
    });
  } else {
    m_mesh.reset();
    m_pick.clear();
  }
  m_meshStyle = m_voxelStyle;
  updateSlab();
  m_meshRevision++;

  emit sceneChanged();      // emptyShape follows the mesh
  emit frameChanged();
}

/* From the editor's shape, which it reads from the puzzle as it is: on the
 * editor's layerChanged after an edit, m_shape may not be refreshed yet */
void ViewportController::updateSlab(void) {
  const btui::Plane plane = m_editor->planeValue();
  const voxel_c * s = m_editor->shape();
  m_hasSlab = s && btui::layerCount(*s, plane) > 0;
  m_layerPick.clear();
  m_layerPickDirty = true;
  if (m_hasSlab)
    m_hasSlab = guarded([&] {
      m_slab = btui::layerSlab(*s, plane, m_editor->layer());
      return true;
    });
}

bool ViewportController::slabShown(void) const {
  return m_display.slab && m_hasSlab && editorVisible();
}

// --- 3D editing (C06) ------------------------------------------------------------

btui::Target3d ViewportController::targetAt(float x, float y, Qt::KeyboardModifiers mods) {
  if (m_meshDirty)
    buildMesh();           // pick against the shape as it is
  const voxel_c * s = m_editor->shape();
  const std::optional<btui::Tool> tool = m_editor->viewportAction(mods);
  if (!s || !tool || m_layout->workspace() != LayoutController::Entities)
    return {};
  const btui::Camera::Ray r = m_camera.rayAt(x, y);
  const btui::PickHit voxel = btui::pick(m_pick, r.origin, r.dir);
  const bool shown = slabShown();
  if (shown && m_layerPickDirty) {
    m_layerPickDirty = false;
    guarded([&] {
      m_layerPick = btui::buildPickMesh(*btui::layerCells(*s, m_editor->planeValue(), m_editor->layer()));
      return true;
    });
  }
  const btui::PickHit slab = shown ? btui::pick(m_layerPick, r.origin, r.dir) : btui::PickHit{};
  return guarded([&] { return btui::target3d(*s, *tool, voxel, slab, shown); });
}

void ViewportController::pointerHover(float x, float y, Qt::KeyboardModifiers mods) {
  m_hoverX = x;
  m_hoverY = y;
  if (navigating() || x < 0)
    return;
  const btui::Target3d t = targetAt(x, y, mods);
  m_editor->hover3d(t, mods);

  // the status bar names the target (C06; product decision 2026-10-27)
  const btui::PlaneAxes a = btui::planeAxes(m_editor->planeValue());
  auto cell = [&](const btui::Cell3 & c) {
    return tr("X%1 Y%2 Z%3").arg(c.x).arg(c.y).arg(c.z);
  };
  QString text;
  switch (t.source) {
    case btui::TargetSource::Face:
      text = tr("Cursor %1").arg(cell(t.cell));
      break;
    case btui::TargetSource::Slab:
      text = tr("Cursor %1 · in the active layer (%2%3)").arg(cell(t.cell))
                 .arg(QChar::fromLatin1(btui::axisLetter(a.layer))).arg(m_editor->layer());
      break;
    case btui::TargetSource::Origin:
      text = tr("Cursor %1 · the first voxel goes to the origin").arg(cell(t.cell));
      break;
    case btui::TargetSource::Rejected:
      text = tr("Outside the grid: nothing to place here");
      break;
    case btui::TargetSource::None:
      break;
  }
  if (StatusController * st = m_editor->status())
    st->setCursorText(text);
}

void ViewportController::pointerLeave(void) {
  m_hoverX = m_hoverY = -1;
  if (m_editor->over3d()) {
    m_editor->leave3d();
    if (StatusController * st = m_editor->status())
      st->setCursorText(QString());
  }
}

void ViewportController::navigationStarted(void) {
  // a drag navigates: no ghost while it lasts (C06 hover_preview)
  m_editor->leave3d();
}

void ViewportController::click(float x, float y, Qt::KeyboardModifiers mods) {
  const btui::Target3d t = targetAt(x, y, mods);
  m_editor->click3d(t, mods);
  // the voxels changed under the pointer: what it now points at
  pointerHover(x, y, mods);
}

int ViewportController::editCursorTool(void) const {
  return m_editor->viewportActive() ? int(m_editor->toolInEffect()) : -1;
}

QColor ViewportController::editCursorColour(void) const {
  return m_editor->cursorColour();
}

SceneFrame ViewportController::frame(float devicePixelRatio) const {
  /* a mesh still queued is built now when asked for on this object's own
   * thread (an export, a test); the render thread draws the last one, and
   * the queued build's frameChanged brings the next frame */
  if (m_meshDirty && QThread::currentThread() == thread())
    const_cast<ViewportController *>(this)->buildMesh();
  SceneFrame f = baseFrame(devicePixelRatio);
  const Theme * t = Theme::instance();
  applyStyle(f, m_meshStyle);

  // the Entities scene; Puzzle and Solver bring their own in later phases
  if (m_layout->workspace() != LayoutController::Entities || !m_shape || !t)
    return f;

  f.mesh = m_mesh;
  f.meshRevision = m_meshRevision;

  if (m_display.axes)
    btui::addAxes(f.lines, *m_shape, rgba(t->axisX()), rgba(t->axisY()), rgba(t->axisZ()), 2.5f);
  if (m_display.bounds)
    btui::addBoxEdges(f.lines, btui::gridBounds(*m_shape), rgba(t->line2()), 1.0f, 4.0f, 4.0f);

  // the slab and dimming only while the voxel editor shows the layer (C06)
  const btui::Plane plane = m_editor->planeValue();
  const int layer = m_editor->layer();
  if (editorVisible() && btui::layerCount(*m_shape, plane) > 0) {
    if (m_display.slab && m_hasSlab) {
      btui::addBoxFaces(f.overlayFaces, m_slab, rgba(t->accent(), 0.11f));
      btui::addBoxEdges(f.lines, m_slab, rgba(t->accent()), 1.8f);
    }
    if (m_display.dim) {
      f.dimAxis = btui::layerAxis(plane);
      f.dimLayer = float(layer);
    }
  }
  addGhosts(f);
  return f;
}

/* The editor's hover preview as ghosts (C06 "Ghost preview"): Fixed and
 * Variable place cubes in the drawing colour at 55 % outlined in accent,
 * dashed for Variable; a voxel they would change gets a faint dashed
 * "replace" overlay; Erase shows danger over the voxels, Paint the drawing
 * colour at 50 %. */
void ViewportController::addGhosts(SceneFrame & f) const {
  const Theme * t = Theme::instance();
  if (!m_shape || !t)
    return;
  // a Fixed or Variable press on a face whose far side is outside the grid:
  // the face outlined, dashed danger, and no ghost (C06)
  const btui::Target3d & target = m_editor->target3d();
  if (m_editor->over3d() && m_editor->viewportActive() && target.source == btui::TargetSource::Rejected)
    btui::addFaceOutline(f.ghostLines, m_pick, target.voxel, target.face,
                         { rgba(t->danger()), rgba(t->danger()), 2.5f, 5.0f, 4.0f });
  const std::vector<btui::GhostCell> ghosts = m_editor->preview();
  if (ghosts.empty())
    return;

  // the drawing colour: the voxel colour, or Default's -- the shape's own
  QColor draw;
  const int c = m_editor->drawColour();
  const puzzle_c & p = m_doc->session().puzzle();
  if (c > 0 && unsigned(c) <= p.colorNumber()) {
    unsigned char r, g, b;
    p.getColor(unsigned(c) - 1, &r, &g, &b);
    draw = QColor(r, g, b);
  } else {
    const btui::Rgb pc = btui::pieceColor(m_shapeIndex);
    draw = QColor::fromRgbF(pc.r, pc.g, pc.b);
  }
  const bool variable = m_editor->toolInEffect() == EditorController::Variable;

  for (const btui::GhostKind kind : { btui::GhostKind::Place, btui::GhostKind::Replace, btui::GhostKind::Erase,
                                      btui::GhostKind::Paint }) {
    std::vector<std::array<int, 3>> cells;
    for (const btui::GhostCell & g : ghosts)
      if (g.kind == kind)
        cells.push_back({ g.cell.x, g.cell.y, g.cell.z });
    if (cells.empty())
      continue;
    btui::GhostStyle s;
    switch (kind) {
      case btui::GhostKind::Place:
        s = { rgba(draw, 0.55f), rgba(t->accent()), 2.0f, variable ? 5.0f : 0.0f, variable ? 4.0f : 0.0f };
        break;
      case btui::GhostKind::Replace:
        s = { rgba(draw, 0.22f), rgba(t->accent()), 1.5f, 5.0f, 4.0f };
        break;
      case btui::GhostKind::Erase:
        s = { rgba(t->danger(), 0.5f), rgba(t->danger()), 2.0f };
        break;
      case btui::GhostKind::Paint:
        s = { rgba(draw, 0.5f), rgba(t->accent()), 2.0f };
        break;
    }
    guarded([&] { btui::addGhost(f.ghostFaces, f.ghostLines, *m_shape, cells, s); });
  }
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
  const QString path = localPath(file);
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
  in.cullBackFaces = f.cullBackFaces;

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
