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
#ifndef BTQT_SHAPESMODEL_H
#define BTQT_SHAPESMODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <QtQml/qqmlregistration.h>

class DocumentController;

/* The puzzle's shapes for the Shapes list (contract section 2), and which
 * one is selected.
 *
 * Read-only in this first cut: selecting is all it does, which is enough to
 * choose the shape the 3D view shows. Ids and chip colours come from the
 * list position (S1 ... SN), as the spec's "position is identity" rule
 * demands.
 */
class ShapesModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  Q_PROPERTY(int selected READ selected WRITE select NOTIFY selectedChanged)
  Q_PROPERTY(int count READ count NOTIFY countChanged)

public:

  enum Roles {
    IdTextRole = Qt::UserRole + 1,
    LabelRole,
    ColorRole,
    TextColorRole,
    FixedRole,
    VariableRole,
    WeightRole,
  };

  explicit ShapesModel(DocumentController * doc, QObject * parent = nullptr);

  int rowCount(const QModelIndex & parent = QModelIndex()) const override;
  QVariant data(const QModelIndex & index, int role) const override;
  QHash<int, QByteArray> roleNames(void) const override;

  int selected(void) const { return m_selected; }
  /* -1 selects nothing; anything else is clamped to the list */
  Q_INVOKABLE void select(int i);
  int count(void) const { return rowCount(); }

  /* the chip colour of shape i, by position */
  static QColor chipColor(int i);

  /* Re-read the shapes after the puzzle changed; keeps the selection when
   * it still exists, else clamps it (the spec's "nearest remaining").
   */
  void refresh(void);

signals:

  void selectedChanged(void);
  void countChanged(void);

private:

  DocumentController * m_doc;
  int m_selected = -1;
};

#endif
