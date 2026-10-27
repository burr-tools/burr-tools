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
#ifndef BTQT_COLOURSMODEL_H
#define BTQT_COLOURSMODEL_H

#include <QAbstractListModel>

#include <QColor>
#include <QtQml/qqmlregistration.h>

#include <vector>

class DocumentController;
class ShapesModel;

/* The Drawing colour footer (C09): the puzzle's colours -- row 0 Default,
 * then C1, C2, ... --, which one new and painted voxels get (drawColour),
 * and Add, Edit and Remove. What those do to the puzzle is
 * btui::drawcolours'; here each becomes one undo step (AK_COLOR_PALETTE).
 *
 * Add appends a preset at once (the mock's quick add). Edit needs a colour
 * from the user: the model asks QML for one (chooserRequested) and carries
 * on when QML answers (chooseColour / cancelChoose), as DocumentController
 * does with its file dialogs.
 */
class ColoursModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
  /* the colour id new and painted voxels get; 0 is Default */
  Q_PROPERTY(int drawColour READ drawColour WRITE select NOTIFY drawColourChanged FINAL)
  Q_PROPERTY(QString drawName READ drawName NOTIFY drawColourChanged FINAL)
  /* the drawing colour's RGB; invalid for Default, which is the shape's */
  Q_PROPERTY(QColor drawRgb READ drawRgb NOTIFY drawColourChanged FINAL)
  /* the colour to show for the drawing colour: Default shows the selected
   * shape's; invalid when there is no shape */
  Q_PROPERTY(QColor drawShown READ drawShown NOTIFY drawColourChanged FINAL)
  Q_PROPERTY(bool canAdd READ canAdd NOTIFY countChanged FINAL)
  /* the most a puzzle can have (btui::maxColours) */
  Q_PROPERTY(int maxColours READ maxColours CONSTANT FINAL)

public:

  enum Roles {
    NameRole = Qt::UserRole + 1,
    ColorRole,          ///< invalid for Default: QML shows the shape's colour
    IsDefaultRole,
  };
  Q_ENUM(Roles)

  ColoursModel(DocumentController * doc, ShapesModel * shapes, QObject * parent = nullptr);

  int rowCount(const QModelIndex & parent = QModelIndex()) const override;
  QVariant data(const QModelIndex & index, int role) const override;
  QHash<int, QByteArray> roleNames(void) const override;

  int count(void) const { return rowCount(); }
  int drawColour(void) const { return m_draw; }
  QString drawName(void) const { return nameOf(m_draw); }
  QColor drawRgb(void) const { return colourOf(m_draw); }
  QColor drawShown(void) const;
  bool canAdd(void) const;
  static int maxColours(void);

  /* "Default" (translated) for 0, "C1", "C2", ... */
  static QString nameOf(int id);

  /* clamped to the list */
  Q_INVOKABLE void select(int id);

  /* the colour of id as the chooser shows it; invalid for Default */
  Q_INVOKABLE QColor colourOf(int id) const;

  /* Add: the next preset colour (btui::presetColour), drawn with at once */
  Q_INVOKABLE void addColour(void);
  /* Edit colour id; Default cannot be edited. */
  Q_INVOKABLE void requestEdit(int id);
  /* QML's answer to chooserRequested */
  Q_INVOKABLE void chooseColour(const QColor & c);
  Q_INVOKABLE void cancelChoose(void);

  /* Remove, or first removeConfirmRequested() when it would lose solutions;
   * the answer comes back as removeColour(). The drawing colour becomes
   * Default (C09). */
  Q_INVOKABLE void requestRemove(int id);
  Q_INVOKABLE void removeColour(int id);

  /* Re-read the colours after the puzzle changed; reset for a new document,
   * which starts at Default. */
  void refresh(bool reset = false);

signals:

  void countChanged(void);
  void drawColourChanged(void);
  /* QML opens the colour chooser with this title, starting at initial */
  void chooserRequested(const QString & title, const QColor & initial);
  /* the colours changed under an open chooser: QML closes it, its answer
   * would go to another colour */
  void chooserCancelled(void);
  void removeConfirmRequested(int id, const QString & name);
  /* a message for the status bar (App flashes it) */
  void flashRequested(const QString & text);

private:

  template <class F> bool edit(F && f);
  bool isColour(int id) const { return id >= 0 && id < rowCount(); }
  /* emit drawColourChanged if what it shows changed */
  void publishDraw(void);

  DocumentController * m_doc;
  ShapesModel * m_shapes;
  int m_rows = 0;
  int m_draw = 0;
  /* the chooser is up for an Edit of this id; -1 none */
  int m_choosing = -1;
  /* what views were last told, to emit only on a change */
  std::vector<QRgb> m_rgb;               ///< per id >= 1
  int m_shownDraw = -1;
  QColor m_shownColour;
};

#endif
