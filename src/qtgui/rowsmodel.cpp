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
#include "rowsmodel.h"

#include <algorithm>
#include <utility>

RowsModel::RowsModel(QByteArrayList roles, QObject * parent) :
  QAbstractListModel(parent), m_roles(std::move(roles))
{}

int RowsModel::rowCount(const QModelIndex & parent) const {
  return parent.isValid() ? 0 : count();
}

QVariant RowsModel::data(const QModelIndex & index, int role) const {
  const int column = role - Qt::UserRole;
  if (!index.isValid() || index.row() >= count() || column < 0 || column >= m_roles.size())
    return {};
  return m_rows[size_t(index.row())].value(column);
}

QHash<int, QByteArray> RowsModel::roleNames(void) const {
  QHash<int, QByteArray> names;
  for (int i = 0; i < m_roles.size(); i++)
    names.insert(Qt::UserRole + i, m_roles[i]);
  return names;
}

QVariant RowsModel::value(int row, const QString & role) const {
  const qsizetype column = m_roles.indexOf(role.toUtf8());
  if (row < 0 || row >= count() || column < 0)
    return {};
  return m_rows[size_t(row)].value(column);
}

void RowsModel::setRows(std::vector<Row> rows) {
  const int old = count();
  const int n = int(rows.size());
  bool changed = n != old;

  for (int i = 0; i < std::min(old, n); i++)
    if (m_rows[size_t(i)] != rows[size_t(i)]) {
      m_rows[size_t(i)] = rows[size_t(i)];
      emit dataChanged(index(i), index(i));
      changed = true;
    }

  if (n > old) {
    beginInsertRows(QModelIndex(), old, n - 1);
    m_rows.insert(m_rows.end(), std::make_move_iterator(rows.begin() + old), std::make_move_iterator(rows.end()));
    endInsertRows();
    emit countChanged();
  } else if (n < old) {
    beginRemoveRows(QModelIndex(), n, old - 1);
    m_rows.resize(size_t(n));
    endRemoveRows();
    emit countChanged();
  }

  if (changed) {
    m_revision++;
    emit revisionChanged();
  }
}
