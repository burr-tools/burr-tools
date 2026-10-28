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
#include "inspectorcontroller.h"

#include "documentcontroller.h"
#include "editorcontroller.h"
#include "guarded.h"
#include "settingscontroller.h"
#include "shapesmodel.h"
#include "viewportcontroller.h"

#include "../uicore/shapeops.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <algorithm>

namespace {
  const char * const kOpenKeys[3] = { "inspector.open.transform", "inspector.open.fit", "inspector.open.repair" };
  constexpr bool kOpenDefaults[3] = { true, false, false };

  /* the operations whose state the buttons show, worked out once per change */
  constexpr int kChecked[] = { btui::task::Prune, btui::task::Origin, btui::task::Center, btui::task::Minimize,
                               btui::task::Scale2, btui::task::Scale3, btui::task::FillHoles,
                               btui::task::FixedInner, btui::task::FixedOuter, btui::task::VariableInner,
                               btui::task::VariableOuter, btui::task::ClearInner, btui::task::ClearOuter };

  /* Fit & scale's: on every shape while Apply to all shapes is on (legacy) */
  bool followsApplyToAll(int task) {
    switch (task) {
      case btui::task::Prune: case btui::task::Origin: case btui::task::Center: case btui::task::Minimize:
      case btui::task::Scale2: case btui::task::Scale3:
        return true;
      default:
        return false;
    }
  }

  const char * kindName(btui::OpKind k) {
    switch (k) {
      case btui::OpKind::Flip: return "flip";
      case btui::OpKind::Rotate: return "rotate";
      case btui::OpKind::Nudge: return "nudge";
    }
    return "";
  }

  QString str(std::string_view s) { return QString::fromUtf8(s.data(), qsizetype(s.size())); }
}

InspectorController::InspectorController(SettingsController * settings, DocumentController * doc,
                                         ShapesModel * shapes, EditorController * editor,
                                         ViewportController * viewport, QObject * parent) :
  QObject(parent), m_settings(settings), m_doc(doc), m_shapes(shapes), m_editor(editor), m_viewport(viewport)
{
  for (int i = 0; i < 3; i++)
    m_open[i] = m_settings->boolValue(QString::fromLatin1(kOpenKeys[i]), kOpenDefaults[i]);

  /* typing in the editor changes the shape at every stroke: the states (a
   * Minimize over every shape tries each prime on a copy of each) wait for
   * a pause; a new selection or document shows its own at once */
  m_settle.setSingleShot(true);
  m_settle.setInterval(120);
  connect(&m_settle, &QTimer::timeout, this, &InspectorController::recompute);
  connect(m_doc, &DocumentController::puzzleEdited, this, [this](int, DocumentController::Change c) {
    if (c == DocumentController::Change::Voxels || c == DocumentController::Change::Structure)
      shapeEdited();
  });
  connect(m_doc, &DocumentController::historyApplied, this, &InspectorController::shapeEdited);
  connect(m_doc, &DocumentController::documentReplaced, this, [this] {
    setPreviewTask(-1);
    m_viewport->clearTransformPreview();
    emit typeChanged();
    recompute();
  });
  connect(m_shapes, &ShapesModel::selectedChanged, this, [this] {
    refreshPreview();
    recompute();
  });
  connect(m_editor, &EditorController::applyToAllChanged, this, &InspectorController::recompute);
  recompute();
}

void InspectorController::setOpen(int i, bool v) {
  if (m_open[i] == v)
    return;
  m_open[i] = v;
  m_settings->setBoolValue(QString::fromLatin1(kOpenKeys[i]), v);
  emit sectionsChanged();
}

// --- the voxel type ----------------------------------------------------------------

const btui::OpsDescriptor & InspectorController::descriptor(void) const {
  return btui::opsDescriptor(m_doc->session().puzzle().getGridType()->getType());
}

bool InspectorController::isTransform(int task) const {
  const auto & b = descriptor().buttons;
  return std::any_of(b.begin(), b.end(), [task](const btui::TransformButton & t) { return t.task == task; });
}

QVariantMap InspectorController::tasks(void) const {
  namespace k = btui::task;
  return {
    { QStringLiteral("prune"), k::Prune }, { QStringLiteral("center"), k::Center },
    { QStringLiteral("origin"), k::Origin }, { QStringLiteral("minimize"), k::Minimize },
    { QStringLiteral("scale2"), k::Scale2 }, { QStringLiteral("scale3"), k::Scale3 },
    { QStringLiteral("fillHoles"), k::FillHoles },
    { QStringLiteral("fixedInner"), k::FixedInner }, { QStringLiteral("fixedOuter"), k::FixedOuter },
    { QStringLiteral("variableInner"), k::VariableInner }, { QStringLiteral("variableOuter"), k::VariableOuter },
    { QStringLiteral("clearInner"), k::ClearInner }, { QStringLiteral("clearOuter"), k::ClearOuter },
  };
}

QVariantList InspectorController::transformButtons(void) const {
  QVariantList out;
  for (const btui::TransformButton & b : descriptor().buttons)
    out.append(QVariantMap{
      { QStringLiteral("task"), b.task },
      { QStringLiteral("kind"), QString::fromLatin1(kindName(b.kind)) },
      { QStringLiteral("axis"), b.axis },
      { QStringLiteral("sign"), b.sign },
      { QStringLiteral("glyph"), str(b.glyph) },
      { QStringLiteral("arrowDeg"), b.arrowDeg },
      { QStringLiteral("layer"), b.layer },
      { QStringLiteral("caption"), str(b.caption) },
      { QStringLiteral("tip"), str(b.tooltip) },
      { QStringLiteral("id"), str(b.id) },
    });
  return out;
}

bool InspectorController::pads(void) const {
  return descriptor().pads;
}

bool InspectorController::scaleShown(void) const {
  return descriptor().scale;
}

// --- what may run ------------------------------------------------------------------

int InspectorController::shape(void) const {
  const int s = m_shapes->selected();
  return s >= 0 && unsigned(s) < m_doc->session().puzzle().getNumberOfShapes() ? s : -1;
}

std::vector<unsigned int> InspectorController::targets(int task) const {
  const int s = shape();
  if (s < 0)
    return {};
  if (followsApplyToAll(task) && m_editor->applyToAll()) {
    std::vector<unsigned int> all(m_doc->session().puzzle().getNumberOfShapes());
    for (unsigned int i = 0; i < all.size(); i++)
      all[i] = i;
    return all;
  }
  return { unsigned(s) };
}

void InspectorController::shapeEdited(void) {
  refreshPreview();
  m_settle.start();
}

void InspectorController::recompute(void) {
  m_settle.stop();
  m_checks.clear();
  m_minimizeFactor = 1;
  guarded([&] {
    const puzzle_c & p = m_doc->session().puzzle();
    for (const int task : kChecked) {
      const btui::OpCheck c = btui::checkTask(p, targets(task), task);
      m_checks.emplace(task, Check{ c.allowed, QString::fromStdString(c.reason) });
    }
    if (descriptor().minimize && m_checks[btui::task::Minimize].allowed)
      m_minimizeFactor = btui::minimizeFactor(p, targets(btui::task::Minimize));
  });
  m_revision++;
  emit stateChanged();
}

bool InspectorController::allowed(int task) const {
  const auto it = m_checks.find(task);
  return it == m_checks.end() || it->second.allowed;
}

QString InspectorController::reason(int task) const {
  const auto it = m_checks.find(task);
  return it == m_checks.end() ? QString() : it->second.reason;
}

// --- running -----------------------------------------------------------------------

void InspectorController::run(int task) {
  const int s = shape();
  if (s < 0)
    return;
  guarded([&] {
    const std::vector<unsigned int> shapes = targets(task);
    // checked again: the states may be a pause behind the shape
    const btui::OpCheck c = btui::checkTask(m_doc->session().puzzle(), shapes, task);
    if (!c.allowed) {
      emit flashRequested(QString::fromStdString(c.reason));
      return;
    }
    btui::OpResult r;
    m_doc->edit(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, DocumentController::Change::Voxels,
                shapes.size() > 1 ? -1 : s, s, [&](puzzle_c & p) {
      r = btui::runTask(p, shapes, task);
      return r.changed;
    });
    if (!r.message.empty())
      emit flashRequested(QString::fromStdString(r.message));
  });
}

// --- the preview -------------------------------------------------------------------

void InspectorController::setPreviewTask(int task) {
  if (task == m_previewTask)
    return;
  m_previewTask = task;
  emit previewChanged();
}

void InspectorController::preview(int task) {
  setPreviewTask(isTransform(task) ? task : -1);
  refreshPreview();
}

void InspectorController::clearPreview(void) {
  setPreviewTask(-1);
  m_viewport->clearTransformPreview();
}

void InspectorController::refreshPreview(void) {
  const int s = shape();
  if (m_previewTask < 0 || s < 0) {
    m_viewport->clearTransformPreview();
    return;
  }
  guarded([&] {
    btui::TaskPreview t = btui::previewTask(*m_doc->session().puzzle().getShape(unsigned(s)), m_previewTask);
    ViewportController::TransformPreview p;
    p.result = std::shared_ptr<const voxel_c>(std::move(t.result));
    p.blocked = t.blocked;
    p.axis = t.axis;
    p.angleDeg = t.angleDeg;
    p.hint = t.hint != btui::TaskPreview::Hint::None;
    switch (t.hint) {
      case btui::TaskPreview::Hint::None: break;
      case btui::TaskPreview::Hint::Rotate: p.kind = btui::TransformHint::Rotate; break;
      case btui::TaskPreview::Hint::Mirror: p.kind = btui::TransformHint::Mirror; break;
      case btui::TaskPreview::Hint::Nudge: p.kind = btui::TransformHint::Nudge; break;
    }
    m_viewport->setTransformPreview(std::move(p));
  });
}
