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
    refresh();
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
  refresh();
  select(0);
}

int ShapesModel::rowCount(const QModelIndex & parent) const {
  if (parent.isValid())
    return 0;
  return int(m_doc->session().puzzle().getNumberOfShapes());
}

QColor ShapesModel::chipColor(int i) {
  btui::Rgb c = btui::pieceColor(i);
  return QColor::fromRgbF(c.r, c.g, c.b);
}

QVariant ShapesModel::data(const QModelIndex & index, int role) const {
  if (!index.isValid() || index.row() >= rowCount())
    return {};

  const int i = index.row();
  const voxel_c * s = m_doc->session().puzzle().getShape(unsigned(i));

  switch (role) {
    case IdTextRole:    return QStringLiteral("S%1").arg(i + 1);
    case LabelRole:     return QString::fromStdString(s->getName());
    case ColorRole:     return chipColor(i);
    case TextColorRole: return btui::prefersWhiteText(i) ? QColor(Qt::white) : QColor(Qt::black);
    case FixedRole:     return int(s->countState(voxel_c::VX_FILLED));
    case VariableRole:  return int(s->countState(voxel_c::VX_VARIABLE));
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

void ShapesModel::refresh(void) {
  beginResetModel();
  endResetModel();
  emit countChanged();
  const int n = rowCount();
  if (m_selected >= n) {
    m_selected = n - 1;
    emit selectedChanged();
  }
}
