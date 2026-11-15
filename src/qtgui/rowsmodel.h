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
#ifndef BTQT_ROWSMODEL_H
#define BTQT_ROWSMODEL_H

#include <QAbstractListModel>
#include <QByteArrayList>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

#include <vector>

/* A list of rows given whole, for what a controller works out from the
 * puzzle: the owner names the roles once and hands over the rows after each
 * change. Rows stay where they are -- only those that differ are updated,
 * and a change in number inserts or removes at the end -- so a view keeps
 * its delegates, and with them focus and hover, through an edit. Delegates
 * read the roles as `required property`s, typed.
 *
 * Keyed (setKeyed), the first role is a unique int naming the row's thing
 * (a shape): a row whose key is new is inserted where it stands, one whose
 * key is gone is removed, and the others keep their delegates, updated only
 * if they differ -- a piece added in the middle makes one delegate, not a
 * changed row for each one after it and one more at the end. Keys out of
 * order, or not unique, fall back to the rows by position.
 */
class RowsModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by a controller")

  Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
  // changes with any row: a binding that calls value() reads this too, to follow it
  Q_PROPERTY(int revision READ revision NOTIFY revisionChanged FINAL)

public:

  using Row = QVariantList;

  explicit RowsModel(QByteArrayList roles, QObject * parent = nullptr);

  int rowCount(const QModelIndex & parent = QModelIndex()) const override;
  QVariant data(const QModelIndex & index, int role) const override;
  QHash<int, QByteArray> roleNames(void) const override;

  int count(void) const { return int(m_rows.size()); }
  int revision(void) const { return m_revision; }

  /* a cell by its role's name, invalid for no such cell */
  Q_INVOKABLE QVariant value(int row, const QString & role) const;

  void setRows(std::vector<Row> rows);

  void setKeyed(bool keyed) { m_keyed = keyed; }

signals:

  void countChanged(void);
  void revisionChanged(void);

private:

  // the rows matched by key; false (nothing changed) when the keys do not allow it
  bool setRowsByKey(std::vector<Row> & rows, bool & changed);

  QByteArrayList m_roles;
  bool m_keyed = false;
  std::vector<Row> m_rows;
  int m_revision = 0;
};

#endif
