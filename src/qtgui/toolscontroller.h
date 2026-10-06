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
#ifndef BTQT_TOOLSCONTROLLER_H
#define BTQT_TOOLSCONTROLLER_H

#include "../uicore/puzzletools.h"

#include <QAbstractListModel>
#include <QColor>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <vector>

class DocumentController;

/* The Status window's Shape Information table (legacy statusWindow_c).
 *
 * Rows are computed a few at a time on the GUI thread, as legacy does with
 * Fl::wait(0) between rows, so the dialog can show progress and Cancel.
 * Cancelling keeps the rows computed so far; the Select and Remove buttons
 * then act on those rows only, again as legacy does.
 */
class ShapeStatusModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by ToolsController")

  Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
  Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
  /* the Tools columns (Notch, Mill) exist for brick puzzles only */
  Q_PROPERTY(bool bricks READ bricks NOTIFY bricksChanged)
  Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)

public:

  enum Roles {
    IdTextRole = Qt::UserRole + 1,  ///< "S3" or "S3 - name"
    ColorRole,
    TextColorRole,
    FixedRole,
    VariableRole,
    TotalRole,
    MirrorRole,       ///< 1-based shape number this one equals, 0 for none
    ShapeRole,
    CompleteRole,
    FaceRole,
    EdgeRole,
    CornerRole,
    Holes2dRole,
    Holes3dRole,
    NotchRole,
    MillRole,
    SymmetryRole,     ///< the number, or "---" when unknown
    SelectedRole,
  };

  explicit ShapeStatusModel(DocumentController * doc, QObject * parent = nullptr);
  ~ShapeStatusModel() override;

  int rowCount(const QModelIndex & parent = QModelIndex()) const override;
  QVariant data(const QModelIndex & index, int role) const override;
  QHash<int, QByteArray> roleNames(void) const override;

  bool busy(void) const { return m_calc != nullptr; }
  double progress(void) const;
  bool bricks(void) const { return m_bricks; }
  int selectedCount(void) const;

  /* (Re)compute the table from the current puzzle. */
  Q_INVOKABLE void start(void);
  /* Stop computing; the rows so far stay. */
  Q_INVOKABLE void cancel(void);
  /* Forget the table (the dialog closed). */
  Q_INVOKABLE void clear(void);

  Q_INVOKABLE void setSelected(int row, bool selected);
  Q_INVOKABLE void selectHoles(void);
  /* kind: "shape", "complete" or "mirror" -- the Identical columns */
  Q_INVOKABLE void selectIdentical(const QString & kind);

  /* Remove the selected shapes, record the undo step and compute the table
   * again (legacy reopens the window). Returns how many were removed. */
  Q_INVOKABLE int removeSelected(void);

  /* chip colours of shape i (0-based), for the Identical columns */
  Q_INVOKABLE static QColor chipColor(int shape);
  Q_INVOKABLE static QColor chipTextColor(int shape);

  /* compute everything that is left now; for tests */
  void finish(void);

signals:

  void busyChanged(void);
  void bricksChanged(void);
  void progressChanged(void);
  void selectionChanged(void);

private:

  void step(void);
  void stop(void);

  DocumentController * m_doc;
  std::unique_ptr<btui::ShapeStatusCalculator> m_calc;
  std::vector<btui::ShapeStatus> m_rows;
  std::vector<bool> m_selected;
  unsigned int m_total = 0;
  bool m_bricks = false;
  QTimer m_timer;
};

/* The legacy menu tools whose dialogs QML draws: Convert, Import
 * assemblies and the Status window. The work itself is btui's
 * (uicore/puzzletools); this class turns its results into QML values and
 * records the undo steps.
 */
class ToolsController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  Q_PROPERTY(ShapeStatusModel * shapeStatus READ shapeStatus CONSTANT)

public:

  explicit ToolsController(DocumentController * doc, QObject * parent = nullptr);

  ShapeStatusModel * shapeStatus(void) const { return m_status; }

  /* [{ type, name }] the grids the current puzzle converts to; empty for none */
  Q_INVOKABLE QVariantList convertTargets(void) const;
  Q_INVOKABLE bool convert(int gridType);

  Q_INVOKABLE bool isBricks(void) const;

  /* [{ index, label ("P2 - name"), solutions, color, textColor }] */
  Q_INVOKABLE QVariantList problems(void) const;

  /* Import assemblies with options keyed like btui::ImportAssembliesOptions:
   * source, destination ("shapes" | "new" | "existing"), target, rangeMin,
   * rangeMax, dropDisconnected, dropMirror, dropSymmetric, dropNonMillable,
   * dropNonNotchable, dropIdentical, shapeMin, shapeMax. Missing keys keep
   * the legacy defaults. Returns the number of shapes added. */
  Q_INVOKABLE int importAssemblies(const QVariantMap & options);

  static btui::ImportAssembliesOptions importOptions(const QVariantMap & options);

private:

  DocumentController * m_doc;
  ShapeStatusModel * m_status;
};

#endif
