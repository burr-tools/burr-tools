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
#include <unordered_map>
#include <unordered_set>
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

bool RowsModel::setRowsByKey(std::vector<Row> & rows, bool & changed) {
  auto key = [](const Row & r) { return r.value(0).toInt(); };
  std::unordered_map<int, int> wanted;          // key -> row in `rows`
  for (int i = 0; i < int(rows.size()); i++)
    if (!wanted.emplace(key(rows[size_t(i)]), i).second)
      return false;
  std::unordered_set<int> had;
  for (const Row & r : m_rows)
    if (!had.insert(key(r)).second)
      return false;
  // the rows kept must come in the same order as before
  int last = -1;
  for (const Row & r : m_rows) {
    const auto it = wanted.find(key(r));
    if (it == wanted.end())
      continue;
    if (it->second < last)
      return false;
    last = it->second;
  }

  // the rows gone, a run at a time from the end
  for (int i = count() - 1; i >= 0;) {
    if (wanted.count(key(m_rows[size_t(i)]))) {
      i--;
      continue;
    }
    int first = i;
    while (first > 0 && !wanted.count(key(m_rows[size_t(first - 1)])))
      first--;
    beginRemoveRows(QModelIndex(), first, i);
    m_rows.erase(m_rows.begin() + first, m_rows.begin() + i + 1);
    endRemoveRows();
    changed = true;
    i = first - 1;
  }
  // the new ones where they stand, a run at a time; the kept ones updated
  size_t at = 0;
  for (size_t j = 0; j < rows.size();) {
    if (at < m_rows.size() && key(m_rows[at]) == key(rows[j])) {
      if (m_rows[at] != rows[j]) {
        m_rows[at] = std::move(rows[j]);
        emit dataChanged(index(int(at)), index(int(at)));
        changed = true;
      }
      at++;
      j++;
      continue;
    }
    size_t end = j;
    while (end < rows.size() && !(at < m_rows.size() && key(m_rows[at]) == key(rows[end])))
      end++;
    beginInsertRows(QModelIndex(), int(at), int(at + (end - j)) - 1);
    m_rows.insert(m_rows.begin() + std::ptrdiff_t(at), std::make_move_iterator(rows.begin() + std::ptrdiff_t(j)),
                  std::make_move_iterator(rows.begin() + std::ptrdiff_t(end)));
    endInsertRows();
    changed = true;
    at += end - j;
    j = end;
  }
  return true;
}

namespace {
  int runsIn(const std::vector<int> & rows) {
    int runs = rows.empty() ? 0 : 1;
    for (size_t i = 1; i < rows.size(); i++)
      runs += rows[i] != rows[i - 1] + 1 ? 1 : 0;
    return runs;
  }
}

void RowsModel::removeRowsAt(const std::vector<int> & rows) {
  if (rows.empty())
    return;
  const int old = count();
  const int n = old - int(rows.size());
  if (runsIn(rows) <= kRuns) {
    // a run at a time from the last: the ones before keep their numbers
    for (size_t e = rows.size(); e > 0;) {
      size_t b = e - 1;
      while (b > 0 && rows[b - 1] == rows[b] - 1)
        b--;
      beginRemoveRows(QModelIndex(), rows[b], rows[e - 1]);
      m_rows.erase(m_rows.begin() + rows[b], m_rows.begin() + rows[e - 1] + 1);
      endRemoveRows();
      e = b;
    }
  } else {
    // one pass: the rows kept close up from the first one gone
    beginRemoveRows(QModelIndex(), n, old - 1);
    size_t w = size_t(rows[0]), j = 0;
    for (size_t r = size_t(rows[0]); r < size_t(old); r++) {
      if (j < rows.size() && size_t(rows[j]) == r) {
        j++;
        continue;
      }
      m_rows[w++] = std::move(m_rows[r]);
    }
    m_rows.resize(size_t(n));
    endRemoveRows();
    if (rows[0] < n)
      emit dataChanged(index(rows[0]), index(n - 1));
  }
  emit countChanged();
  m_revision++;
  emit revisionChanged();
}

void RowsModel::insertRowsAt(const std::vector<int> & rows, std::vector<Row> values) {
  if (rows.empty())
    return;
  const int old = count();
  const int k = int(rows.size());
  const int n = old + k;
  if (runsIn(rows) <= kRuns) {
    // a run at a time from the last: before it stand old rows only, so it
    // goes in at its place less the new rows before it
    for (int e = k; e > 0;) {
      int b = e - 1;
      while (b > 0 && rows[size_t(b - 1)] == rows[size_t(b)] - 1)
        b--;
      const int at = rows[size_t(b)] - b;
      beginInsertRows(QModelIndex(), at, at + (e - b) - 1);
      m_rows.insert(m_rows.begin() + at, std::make_move_iterator(values.begin() + b),
                    std::make_move_iterator(values.begin() + e));
      endInsertRows();
      e = b;
    }
  } else {
    // one pass from the end: each place takes the next new row if it goes
    // there, else the last old row not moved yet
    beginInsertRows(QModelIndex(), old, n - 1);
    m_rows.resize(size_t(n));
    int i = old, j = k;
    for (int w = n - 1; j > 0; w--)
      m_rows[size_t(w)] = w == rows[size_t(j - 1)] ? std::move(values[size_t(--j)]) : std::move(m_rows[size_t(--i)]);
    endInsertRows();
    if (rows[0] < old)
      emit dataChanged(index(rows[0]), index(old - 1));
  }
  emit countChanged();
  m_revision++;
  emit revisionChanged();
}

void RowsModel::setRows(std::vector<Row> rows) {
  const int old = count();
  if (m_keyed) {
    bool changed = false;
    if (setRowsByKey(rows, changed)) {
      if (count() != old)
        emit countChanged();
      if (changed) {
        m_revision++;
        emit revisionChanged();
      }
      return;
    }
  }
  const int n = int(rows.size());
  bool changed = n != old;

  // one signal for the span of changed rows: thousands of rows that shift by
  // one (a sorted insert) are one update, not thousands
  int first = -1, last = -1;
  for (int i = 0; i < std::min(old, n); i++)
    if (m_rows[size_t(i)] != rows[size_t(i)]) {
      m_rows[size_t(i)] = std::move(rows[size_t(i)]);
      if (first < 0)
        first = i;
      last = i;
    }
  if (first >= 0) {
    emit dataChanged(index(first), index(last));
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
