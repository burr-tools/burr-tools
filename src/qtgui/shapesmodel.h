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

#include "documentcontroller.h"

#include <QAbstractListModel>

#include <vector>
#include <QColor>
#include <QtQml/qqmlregistration.h>


/* The puzzle's shapes for the Shapes list (C01, contract section 2), which
 * one is selected, and the list's actions: new, duplicate, delete, rename,
 * weight, reorder. What each action does to the puzzle is
 * btui::shapelist's; here it becomes one undo step (editStructure) and a
 * message in the status bar.
 *
 * Ids and chip colours come from the list position (S1 ... SN), as the
 * spec's "position is identity" rule demands, so a reorder or a delete
 * renumbers the shapes after it.
 */
class ShapesModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  Q_PROPERTY(int selected READ selected WRITE select NOTIFY selectedChanged FINAL)
  Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
  /* Delete is possible: a second shape remains (AC-C01-05) */
  Q_PROPERTY(bool canDelete READ canDelete NOTIFY countChanged FINAL)

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
  Q_ENUM(Roles)

  explicit ShapesModel(DocumentController * doc, QObject * parent = nullptr);

  int rowCount(const QModelIndex & parent = QModelIndex()) const override;
  QVariant data(const QModelIndex & index, int role) const override;
  QHash<int, QByteArray> roleNames(void) const override;

  int selected(void) const { return m_selected; }
  /* -1 selects nothing; anything else is clamped to the list */
  Q_INVOKABLE void select(int i);
  /* the shape delta rows away, stopping at the ends (↑ / ↓ in the lists) */
  Q_INVOKABLE void selectBy(int delta);
  int count(void) const { return rowCount(); }
  bool canDelete(void) const;
  /* a row's weight, for menus built when they open; 1 for no shape */
  Q_INVOKABLE int weightOf(int row) const;
  /* a row's id, "S3" (btui::shapeId): the one place it is spelt */
  Q_INVOKABLE QString idTextOf(int row) const;

  /* The list's actions, each one undo step and selecting the shape it
   * leaves for the user; a row that names no shape does nothing. */

  /* an empty shape as big as the selected one, appended */
  Q_INVOKABLE void addShape(void);
  /* a copy of the row, appended (AC-C01-04) */
  Q_INVOKABLE void duplicateShape(int row);
  /* Delete, or first deleteConfirmRequested() when the delete would lose
   * solutions undo cannot restore; the answer comes back as deleteShape(). */
  Q_INVOKABLE void requestDelete(int row);
  Q_INVOKABLE void deleteShape(int row);
  /* empty clears the label */
  Q_INVOKABLE void renameShape(int row, const QString & label);
  /* to the given position, the shapes between shifting (drag, Move earlier
   * / later); the selection follows the shape */
  Q_INVOKABLE void moveShape(int from, int to);
  Q_INVOKABLE void changeWeight(int row, int delta);
  /* open the row's inline rename (the command and the context menu do;
   * the row's own pencil and double-click open it directly) */
  Q_INVOKABLE void requestRename(int row);

  /* the chip colour of shape i, by position */
  static QColor chipColor(int i);

  /* Re-read the shapes after the puzzle changed, and select `want` (when it
   * is a row) or keep the selection within the list -- one selectedChanged,
   * for the end state. The rows stay: a change in number inserts or removes
   * rows at the end, every other row is updated in place, so a view keeps
   * its delegates. A new document
   * (`reset`) replaces them all.
   */
  void refresh(bool reset = false, int want = -1);

signals:

  void selectedChanged(void);
  void countChanged(void);

  void renameRequested(int row);
  /* the delete of row would lose solutions: QML asks, then deleteShape() */
  void deleteConfirmRequested(int row, const QString & idText);
  /* a message for the status bar (App flashes it) */
  void flashRequested(const QString & text);

private:

  /* one undo step changing the list; select is the shape selected after
   * it. Returns whether the puzzle changed. */
  template <class F> bool edit(int select, F && f);
  /* one undo step changing a row's name or weight, the selection kept */
  template <class F> bool editLabel(int row, F && f);
  void edited(int selectedShape, DocumentController::Change change, int shape);
  bool isRow(int row) const { return row >= 0 && row < rowCount(); }

  /* a shape's voxel counts, read once per change of the puzzle */
  struct Counts {
    int fixed = -1;            ///< -1: not counted since the last change
    int variable = -1;
  };
  const Counts & countsOf(int row) const;

  DocumentController * m_doc;
  int m_selected = -1;
  int m_rows = 0;                          ///< the rows the views have been told about
  mutable std::vector<Counts> m_counts;    ///< per row, filled as the views ask
};

#endif
