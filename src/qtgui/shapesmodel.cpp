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
#include "shapesmodel.h"
#include "documentcontroller.h"
#include "guarded.h"

#include "../uicore/palette.h"
#include "../uicore/shapelist.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <algorithm>

ShapesModel::ShapesModel(DocumentController * doc, QObject * parent) :
  QAbstractListModel(parent), m_doc(doc)
{
  connect(doc, &DocumentController::documentReplaced, this, [this] {
    // a new document starts at its first shape. The selection is reset
    // behind select()'s change check, so an empty document (File > New)
    // still tells the views it has none -- once, and only if it changed.
    const int before = m_selected;
    m_selected = -1;
    refresh(true);
    select(0);
    if (m_selected == -1 && before != -1)
      emit selectedChanged();
  });
  // the change names the shape to select: one selectedChanged, for the end state
  connect(doc, &DocumentController::historyApplied, this, [this](int, int shape) { refresh(false, shape); });
  connect(doc, &DocumentController::puzzleEdited, this, [this](int shape) { refresh(false, shape); });
  refresh(true);
  select(0);
}

int ShapesModel::rowCount(const QModelIndex & parent) const {
  return parent.isValid() ? 0 : m_rows;
}

const ShapesModel::Counts & ShapesModel::countsOf(int row) const {
  Counts & c = m_counts[size_t(row)];
  if (c.fixed < 0) {
    const voxel_c * s = m_doc->session().puzzle().getShape(unsigned(row));
    c.fixed = int(s->countState(voxel_c::VX_FILLED));
    c.variable = int(s->countState(voxel_c::VX_VARIABLE));
  }
  return c;
}

QColor ShapesModel::chipColor(int i) {
  btui::Rgb c = btui::pieceColor(i);
  return QColor::fromRgbF(c.r, c.g, c.b);
}

QVariant ShapesModel::data(const QModelIndex & index, int role) const {
  // while rows are being removed the views may still ask for them: the
  // puzzle and the counts already have the new number
  const puzzle_c & p = m_doc->session().puzzle();
  if (!index.isValid() || index.row() >= rowCount() || unsigned(index.row()) >= p.getNumberOfShapes() ||
      size_t(index.row()) >= m_counts.size())
    return {};

  const int i = index.row();
  const voxel_c * s = p.getShape(unsigned(i));

  switch (role) {
    case IdTextRole:    return idTextOf(i);
    case LabelRole:     return QString::fromStdString(s->getName());
    case ColorRole:     return chipColor(i);
    case TextColorRole: return btui::prefersWhiteText(i) ? QColor(Qt::white) : QColor(Qt::black);
    case FixedRole:     return countsOf(i).fixed;
    case VariableRole:  return countsOf(i).variable;
    case WeightRole:    return s->getWeight();
    default:            return {};
  }
}

QHash<int, QByteArray> ShapesModel::roleNames(void) const {
  static const QHash<int, QByteArray> names{
    { IdTextRole, "idText" },
    { LabelRole, "label" },
    { ColorRole, "chipColor" },
    { TextColorRole, "chipTextColor" },
    { FixedRole, "fixedCount" },
    { VariableRole, "variableCount" },
    { WeightRole, "weight" },
  };
  return names;
}

bool ShapesModel::canDelete(void) const {
  return btui::canDeleteShape(m_doc->session().puzzle(), 0);
}

QString ShapesModel::idTextOf(int row) const {
  return QString::fromStdString(btui::shapeId(row));
}

int ShapesModel::weightOf(int row) const {
  if (!isRow(row))
    return 1;
  return guarded([&] { return m_doc->session().puzzle().getShape(unsigned(row))->getWeight(); });
}

template <class F> bool ShapesModel::edit(int select, F && f) {
  return guarded([&] { return m_doc->editStructure(select, std::forward<F>(f)); });
}

void ShapesModel::addShape(void) {
  const int from = m_selected;
  const int added = count();
  if (edit(added, [&](puzzle_c & p) { btui::newShape(p, from); return true; }))
    emit flashRequested(tr("Shape %1 added").arg(idTextOf(added)));
}

void ShapesModel::duplicateShape(int row) {
  if (!isRow(row))
    return;
  const int added = count();
  if (edit(added, [&](puzzle_c & p) { return btui::duplicateShape(p, row) >= 0; }))
    emit flashRequested(tr("Shape %1 duplicated as %2").arg(idTextOf(row), idTextOf(added)));
}

void ShapesModel::requestDelete(int row) {
  if (!isRow(row))
    return;
  guarded([&] {
    const puzzle_c & p = m_doc->session().puzzle();
    if (!btui::canDeleteShape(p, row))
      return;
    if (btui::deleteLosesSolutions(p, row))
      emit deleteConfirmRequested(row, idTextOf(row));
    else
      deleteShape(row);
  });
}

void ShapesModel::deleteShape(int row) {
  if (!isRow(row) || !btui::canDeleteShape(m_doc->session().puzzle(), row))
    return;
  const int n = count();
  /* The selected shape: the neighbour when it was the one deleted (the
   * spec's "nearest remaining"); otherwise the same shape, a row up when it
   * came after the deleted one (a row's own trash button deletes rows that
   * are not selected). */
  const int keep = m_selected == row || m_selected < 0 ? std::min(row, n - 2)
                 : m_selected > row ? m_selected - 1 : m_selected;
  if (!edit(keep, [&](puzzle_c & p) { return btui::deleteShape(p, row) >= 0; }))
    return;
  // say what went, and that the shapes after it are renumbered (C01)
  if (row < n - 1)
    emit flashRequested(tr("Shape S%1 deleted — later shapes renumbered S%1…S%2").arg(row + 1).arg(n - 1));
  else
    emit flashRequested(tr("Shape S%1 deleted").arg(row + 1));
}

void ShapesModel::renameShape(int row, const QString & label) {
  if (!isRow(row))
    return;
  m_onlyRow = row;                       // one row's label: no recount, no other row
  m_onlyRole = LabelRole;
  // the selection stays: a rename can be committed by clicking another row
  edit(m_selected, [&](puzzle_c & p) { return btui::renameShape(p, row, label.toStdString()); });
  m_onlyRow = -1;
}

void ShapesModel::moveShape(int from, int to) {
  if (!isRow(from))
    return;
  to = std::clamp(to, 0, count() - 1);
  if (edit(to, [&](puzzle_c & p) { return btui::moveShape(p, from, to); }))
    emit flashRequested(tr("Order changed — shapes renumbered S1…S%1").arg(count()));
}

void ShapesModel::changeWeight(int row, int delta) {
  if (!isRow(row))
    return;
  m_onlyRow = row;
  m_onlyRole = WeightRole;
  // the selection stays: a hovered row's W+ / W- is not a selection
  edit(m_selected, [&](puzzle_c & p) { return btui::changeWeight(p, row, delta); });
  m_onlyRow = -1;
}

void ShapesModel::requestRename(int row) {
  if (!isRow(row))
    return;
  select(row);
  emit renameRequested(row);
}

void ShapesModel::selectBy(int delta) {
  if (count() > 0)
    select(std::max(0, m_selected + delta));
}

void ShapesModel::select(int i) {
  const int n = rowCount();
  int v = (n == 0 || i < 0) ? -1 : std::clamp(i, 0, n - 1);
  if (v == m_selected)
    return;
  m_selected = v;
  emit selectedChanged();
}

void ShapesModel::refresh(bool reset, int want) {
  const int old = m_rows;
  const int n = int(m_doc->session().puzzle().getNumberOfShapes());
  if (reset) {
    beginResetModel();
    m_rows = n;
    m_counts.assign(size_t(n), Counts{});
    endResetModel();
  } else if (m_onlyRow >= 0 && n == old && m_onlyRow < n) {
    // one of the list's own one-row changes (rename, weight): that row and role
    emit dataChanged(index(m_onlyRow), index(m_onlyRow), { m_onlyRole });
  } else {
    m_counts.assign(size_t(n), Counts{});    // any shape may have changed
    if (n > old) {
      beginInsertRows(QModelIndex(), old, n - 1);
      m_rows = n;
      endInsertRows();
    } else if (n < old) {
      beginRemoveRows(QModelIndex(), n, old - 1);
      m_rows = n;
      endRemoveRows();
    }
    if (std::min(n, old) > 0)
      emit dataChanged(index(0), index(std::min(n, old) - 1));
  }
  if (n != old)
    emit countChanged();
  // the selection: the one the change asks for, else kept within the list
  const int sel = n == 0 ? -1 : std::clamp(want >= 0 ? want : m_selected, -1, n - 1);
  if (sel != m_selected) {
    m_selected = sel;
    emit selectedChanged();
  }
}
