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
#include "editorcontroller.h"

#include "coloursmodel.h"
#include "guarded.h"
#include "shapesmodel.h"
#include "statuscontroller.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <algorithm>

namespace {
  btui::ModifierKeys keysOf(Qt::KeyboardModifiers m) {
    return { m.testFlag(Qt::ShiftModifier), m.testFlag(Qt::AltModifier), m.testFlag(Qt::ControlModifier),
             m.testFlag(Qt::MetaModifier) };
  }

  bool sameKeys(const btui::ModifierKeys & a, const btui::ModifierKeys & b) {
    return a.shift == b.shift && a.alt == b.alt && a.control == b.control && a.meta == b.meta;
  }
}

EditorController::EditorController(DocumentController * doc, ShapesModel * shapes, ColoursModel * colours,
                                   StatusController * status, QObject * parent) :
  QObject(parent), m_doc(doc), m_shapes(shapes), m_colours(colours), m_status(status)
{
  connect(m_shapes, &ShapesModel::selectedChanged, this, &EditorController::refreshShape);
  connect(m_shapes, &ShapesModel::countChanged, this, &EditorController::refreshShape);
  connect(m_doc, &DocumentController::documentReplaced, this, [this] {
    // the session dropped any stroke with the old puzzle
    m_stroking = m_rightDown = false;
    refreshShape();
    emit shapeChanged();         // another voxel type: the planes
  });
  connect(m_doc, &DocumentController::historyApplied, this, [this] {
    // an undo mid-drag ended the stroke (DocumentSession): the drag paints no more
    if (m_stroking) {
      m_stroking = m_rightDown = false;
      emit toolChanged();
    }
    contentEdited();
  });
  connect(m_doc, &DocumentController::puzzleEdited, this, [this](int, DocumentController::Change c) {
    switch (c) {
      case DocumentController::Change::Voxels:
      case DocumentController::Change::Structure:
        contentEdited();
        break;
      case DocumentController::Change::Palette:
        bump();                  // the cells' colours
        break;
      case DocumentController::Change::Label:
        break;
    }
  });
  connect(m_colours, &ColoursModel::drawColourChanged, this, &EditorController::drawColourChanged);
  // the hover preview follows the tool, Mirror and Span, the voxels and the colour
  for (auto s : { &EditorController::toolChanged, &EditorController::modifiersChanged,
                  &EditorController::revisionChanged, &EditorController::drawColourChanged })
    connect(this, s, this, &EditorController::previewChanged);
  refreshShape();
}

int EditorController::drawColour(void) const {
  return std::max(0, m_colours->drawColour());
}

QColor EditorController::cursorColour(void) const {
  const int c = drawColour();
  return c > 0 ? m_colours->colourOf(c) : QColor();
}

// --- tools -------------------------------------------------------------------

void EditorController::setTool(Tool t) {
  if (t == m_tool || int(t) < Fixed || int(t) > Paint)
    return;
  m_tool = t;
  emit toolChanged();
}

EditorController::Tool EditorController::toolInEffect(void) const {
  if (m_stroking)
    return Tool(int(m_strokeTool));
  // the keys held over the grid, or over the 3D view (its quick tools)
  const btui::ModifierKeys keys = m_hoverCol >= 0 ? m_keys : m_over3d ? keysOf(m_keys3d) : btui::ModifierKeys{};
  return Tool(int(btui::toolInEffect(btui::Tool(int(m_tool)), btui::currentPlatform(), keys, false)));
}

void EditorController::setEditMode(bool on) {
  if (on == m_editMode)
    return;
  m_editMode = on;
  emit editModeChanged();
  emit toolChanged();          // viewportActive
}

std::optional<btui::Tool> EditorController::viewportAction(Qt::KeyboardModifiers m) const {
  if (auto q = btui::quickTool(btui::currentPlatform(), keysOf(m)))
    return q;
  if (m_editMode)
    return btui::Tool(int(m_tool));
  return std::nullopt;
}

bool EditorController::quickHeld3d(void) const {
  return m_over3d && btui::quickTool(btui::currentPlatform(), keysOf(m_keys3d)).has_value();
}

void EditorController::hover3d(const btui::Target3d & t, Qt::KeyboardModifiers m) {
  const Tool before = toolInEffect();
  const bool wasActive = viewportActive();
  const bool wasHeld = quickHeld3d();
  const bool changed = !m_over3d || m_keys3d != m || t.source != m_target3d.source || t.cell != m_target3d.cell ||
                       t.voxel != m_target3d.voxel || t.face != m_target3d.face;
  m_over3d = true;
  m_keys3d = m;
  m_target3d = t;
  if (toolInEffect() != before || viewportActive() != wasActive || quickHeld3d() != wasHeld)
    emit toolChanged();
  if (changed)
    emit previewChanged();
}

void EditorController::leave3d(void) {
  if (!m_over3d)
    return;
  m_over3d = false;
  m_keys3d = Qt::NoModifier;
  m_target3d = {};
  emit toolChanged();          // the tool in effect, viewportActive, quickHeld3d
  emit previewChanged();
}

bool EditorController::click3d(const btui::Target3d & t, Qt::KeyboardModifiers m) {
  const voxel_c * s = shape();
  const std::optional<btui::Tool> tool = viewportAction(m);
  if (!s || !tool || (t.source != btui::TargetSource::Face && t.source != btui::TargetSource::Slab &&
                      t.source != btui::TargetSource::Origin))
    return false;
  const std::vector<btui::Cell3> cells =
      btui::expand(t.cell, { int(s->getX()), int(s->getY()), int(s->getZ()) }, modifiers());
  const unsigned colour = unsigned(drawColour());
  const int idx = m_shapeIndex;
  bool changed = false;
  guarded([&] {
    m_doc->edit(puzzleHistory_c::AK_ENTITIES_CLICK_3D, DocumentController::Change::Voxels, idx, idx,
                [&](puzzle_c & p) { return changed = btui::applyTool(*p.getShape(unsigned(idx)), cells, *tool, colour); });
  });
  return changed;
}

void EditorController::setMirror(int bits) {
  bits &= 7;
  if (bits == m_mirror)
    return;
  m_mirror = bits;
  emit modifiersChanged();
}

void EditorController::setSpan(int bits) {
  bits &= 7;
  if (bits == m_span)
    return;
  m_span = bits;
  emit modifiersChanged();
}

void EditorController::toggleMirror(int axis) {
  if (axis >= 0 && axis < 3)
    setMirror(m_mirror ^ (1 << axis));
}

void EditorController::toggleSpan(int axis) {
  if (axis >= 0 && axis < 3)
    setSpan(m_span ^ (1 << axis));
}

btui::Modifiers EditorController::modifiers(void) const {
  btui::Modifiers m;
  for (int a = 0; a < 3; a++) {
    m.mirror[size_t(a)] = m_mirror & (1 << a);
    m.span[size_t(a)] = m_span & (1 << a);
  }
  return m;
}

// --- plane and layer (C08) -----------------------------------------------------

const voxel_c * EditorController::shape(void) const {
  const puzzle_c & p = m_doc->session().puzzle();
  return m_shapeIndex >= 0 && unsigned(m_shapeIndex) < p.getNumberOfShapes() ? p.getShape(unsigned(m_shapeIndex))
                                                                             : nullptr;
}

bool EditorController::planeAvailable(int plane) const {
  if (plane < XY || plane > YZ)
    return false;
  return btui::planeAvailable(m_doc->session().puzzle().getGridType()->getType(), btui::Plane(plane));
}

void EditorController::setPlane(Plane p) {
  if (p == m_plane || !planeAvailable(p))
    return;
  m_plane = p;
  clampLayer();     // C08: keep the layer, clamped to the new plane's count
  m_hoverCol = m_hoverRow = -1;
  emit layerChanged();
  emit layerDotsChanged();
  bump();
}

int EditorController::layerCount(void) const {
  const voxel_c * s = shape();
  return s ? btui::layerCount(*s, planeValue()) : 0;
}

int EditorController::layerAxis(void) const {
  return btui::planeAxes(planeValue()).layer;
}

QString EditorController::layerAxisName(void) const {
  return QString(QChar::fromLatin1(btui::axisLetter(layerAxis())));
}

void EditorController::clampLayer(void) {
  const int n = layerCount();
  m_layer = n > 0 ? std::clamp(m_layer, 0, n - 1) : 0;
}

void EditorController::setLayer(int l) {
  const int n = layerCount();
  l = n > 0 ? std::clamp(l, 0, n - 1) : 0;
  if (l == m_layer)
    return;
  m_layer = l;
  emit layerChanged();
  bump();
}

void EditorController::layerStep(int delta) {
  setLayer(m_layer + delta);
}

QList<bool> EditorController::layerDots(void) const {
  QList<bool> dots;
  const voxel_c * s = shape();
  if (!s)
    return dots;
  const int n = layerCount();
  dots.reserve(n);
  for (int l = 0; l < n; l++)
    dots.append(btui::layerHasVoxels(*s, planeValue(), l));
  return dots;
}

// --- the shape and its changes -------------------------------------------------

void EditorController::bump(void) {
  m_revision++;
  emit revisionChanged();
}

void EditorController::refreshShape(void) {
  const int sel = m_shapes->selected();
  const puzzle_c & p = m_doc->session().puzzle();
  const int idx = sel >= 0 && unsigned(sel) < p.getNumberOfShapes() ? sel : -1;
  if (idx != m_shapeIndex) {
    if (m_stroking)
      release();                 // a stroke belongs to its shape
    m_shapeIndex = idx;
    emit shapeChanged();
  }
  // a voxel type without the plane (another document) falls back to XY
  if (!planeAvailable(m_plane))
    m_plane = XY;
  contentEdited();
}

void EditorController::contentEdited(void) {
  clampLayer();
  emit layerChanged();
  emit layerDotsChanged();
  emit sizeChanged();
  bump();
}

// --- grid size (C02) -------------------------------------------------------------

int EditorController::size(int axis) const {
  const voxel_c * s = shape();
  return s && axis >= 0 && axis < 3 ? btui::gridSteps(*s, axis) : 0;
}

void EditorController::setApplyToAll(bool v) {
  if (v == m_applyToAll)
    return;
  m_applyToAll = v;
  emit applyToAllChanged();
}

void EditorController::setSize(int axis, int steps) {
  const voxel_c * s = shape();
  if (!s || axis < 0 || axis > 2)
    return;
  steps = std::clamp(steps, 1, btui::maxGridSteps);
  const int delta = steps - btui::gridSteps(*s, axis);
  if (delta == 0) {
    emit sizeChanged();          // a typed value that clamped back: show the real one
    return;
  }
  const int idx = m_shapeIndex;
  int blocked = 0;
  guarded([&] {
    m_doc->edit(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, DocumentController::Change::Voxels,
                m_applyToAll ? -1 : idx, idx, [&](puzzle_c & p) {
      if (m_applyToAll) {
        const btui::ResizeAllResult r = btui::resizeAllShapes(p, axis, delta);
        blocked = r.blocked;
        return r.changed > 0;
      }
      switch (btui::resizeShape(*p.getShape(unsigned(idx)), axis, steps)) {
        case btui::ResizeResult::Changed: return true;
        case btui::ResizeResult::Blocked: blocked = 1; return false;
        case btui::ResizeResult::Unchanged: return false;
      }
      return false;
    });
  });
  if (blocked > 0) {
    emit sizeChanged();          // the field shows the size kept
    emit flashRequested(tr("Blocked: voxels exist beyond this size — prune or move the piece first"));
  }
}

// --- strokes (C08 Drawing interactions) ---------------------------------------------

QString EditorController::cursorText(int col, int row) const {
  const btui::PlaneAxes a = btui::planeAxes(planeValue());
  return tr("Cursor %1%2 %3%4 · %5%6")
      .arg(QChar::fromLatin1(btui::axisLetter(a.col))).arg(col)
      .arg(QChar::fromLatin1(btui::axisLetter(a.row))).arg(row)
      .arg(QChar::fromLatin1(btui::axisLetter(a.layer))).arg(m_layer);
}

void EditorController::hover(int col, int row) {
  if (col < 0 || row < 0)
    col = row = -1;
  if (col == m_hoverCol && row == m_hoverRow)
    return;
  const Tool before = toolInEffect();
  m_hoverCol = col;
  m_hoverRow = row;
  m_status->setCursorText(col >= 0 ? cursorText(col, row) : QString());
  if (toolInEffect() != before)
    emit toolChanged();
  emit previewChanged();
}

/* One hover preview, whichever pointer is active (C06 hover_preview): the
 * 2D grid's cell, or the 3D view's target with the viewport action. */
std::vector<btui::GhostCell> EditorController::preview(void) const {
  const voxel_c * s = shape();
  if (!s || m_stroking)
    return {};
  btui::Cell3 t;
  btui::Tool tool;
  if (m_hoverCol >= 0 && m_hoverRow >= 0) {
    t = btui::voxelAt(planeValue(), m_hoverCol, m_hoverRow, m_layer);
    tool = btui::Tool(int(toolInEffect()));
  } else if (m_over3d) {
    const std::optional<btui::Tool> a = viewportAction(m_keys3d);
    const btui::TargetSource src = m_target3d.source;
    if (!a || (src != btui::TargetSource::Face && src != btui::TargetSource::Slab && src != btui::TargetSource::Origin))
      return {};
    t = m_target3d.cell;
    tool = *a;
  } else {
    return {};
  }
  const std::vector<btui::Cell3> cells =
      btui::expand(t, { int(s->getX()), int(s->getY()), int(s->getZ()) }, modifiers());
  return guarded([&] { return btui::previewTool(*s, cells, tool, unsigned(drawColour())); });
}

void EditorController::setModifiers(Qt::KeyboardModifiers m) {
  const btui::ModifierKeys k = keysOf(m);
  if (sameKeys(k, m_keys))
    return;
  const Tool before = toolInEffect();
  m_keys = k;
  if (toolInEffect() != before)
    emit toolChanged();
}

bool EditorController::press(int col, int row, bool rightButton, Qt::KeyboardModifiers m) {
  if (!shape())
    return false;
  if (m_stroking)
    release();
  m_keys = keysOf(m);
  m_strokeTool = btui::toolInEffect(btui::Tool(int(m_tool)), btui::currentPlatform(), m_keys, rightButton);
  m_rightDown = rightButton;
  m_stroking = true;
  m_lastCol = m_lastRow = -1;
  m_doc->beginStroke(puzzleHistory_c::AK_ENTITIES_GRID_PAINT, m_shapeIndex);
  emit toolChanged();
  apply(col, row);
  return true;
}

void EditorController::dragTo(int col, int row) {
  if (m_stroking)
    apply(col, row);
}

void EditorController::apply(int col, int row) {
  if (col == m_lastCol && row == m_lastRow)
    return;
  m_lastCol = col;
  m_lastRow = row;
  const voxel_c * s = shape();
  if (!s || col < 0 || row < 0)
    return;
  const btui::Cell3 t = btui::voxelAt(planeValue(), col, row, m_layer);
  const std::vector<btui::Cell3> cells =
      btui::expand(t, { int(s->getX()), int(s->getY()), int(s->getZ()) }, modifiers());
  const btui::Tool tool = m_strokeTool;
  const unsigned colour = unsigned(drawColour());
  const int idx = m_shapeIndex;
  guarded([&] {
    m_doc->strokeEdit(idx, [&](puzzle_c & p) { return btui::applyTool(*p.getShape(unsigned(idx)), cells, tool, colour); });
  });
}

void EditorController::release(void) {
  if (!m_stroking)
    return;
  m_stroking = false;
  m_rightDown = false;
  guarded([&] { m_doc->endStroke(); });
  emit toolChanged();
}
