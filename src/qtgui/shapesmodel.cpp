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

#include "../uicore/palette.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"

#include <algorithm>

ShapesModel::ShapesModel(DocumentController * doc, QObject * parent) :
  QAbstractListModel(parent), m_doc(doc)
{
  connect(doc, &DocumentController::documentReplaced, this, [this] {
    // a new document starts at its first shape
    m_selected = -1;
    refresh(true);
    select(0);
  });
  connect(doc, &DocumentController::historyApplied, this, [this](int, int shape) {
    refresh();
    if (shape >= 0)
      select(shape);
  });
  connect(doc, &DocumentController::puzzleEdited, this, [this](int shape) {
    refresh();
    if (shape >= 0)
      select(shape);
  });
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
    case IdTextRole:    return QStringLiteral("S%1").arg(i + 1);
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
  return {
    { IdTextRole, "idText" },
    { LabelRole, "label" },
    { ColorRole, "chipColor" },
    { TextColorRole, "chipTextColor" },
    { FixedRole, "fixedCount" },
    { VariableRole, "variableCount" },
    { WeightRole, "weight" },
  };
}

void ShapesModel::select(int i) {
  const int n = rowCount();
  int v = (n == 0 || i < 0) ? -1 : std::clamp(i, 0, n - 1);
  if (v == m_selected)
    return;
  m_selected = v;
  emit selectedChanged();
}

void ShapesModel::refresh(bool reset) {
  const int old = m_rows;
  const int n = int(m_doc->session().puzzle().getNumberOfShapes());
  if (reset) {
    beginResetModel();
    m_rows = n;
    m_counts.assign(size_t(n), Counts{});
    endResetModel();
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
  if (m_selected >= n) {
    m_selected = n - 1;
    emit selectedChanged();
  }
}
