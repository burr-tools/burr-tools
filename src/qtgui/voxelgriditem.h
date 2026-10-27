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
#ifndef BTQT_VOXELGRIDITEM_H
#define BTQT_VOXELGRIDITEM_H

#include "editorcontroller.h"   // a Q_PROPERTY type: moc needs it complete

#include "../uicore/gridcells.h"

#include <QPointer>
#include <QQuickPaintedItem>
#include <QStaticText>
#include <QtQml/qqmlregistration.h>

class QPainter;

/* The voxel editor's 2D grid (C08): one layer of the selected shape in the
 * editor's plane, as the cells of its voxel type (btui::gridLayout), with
 * rulers and axis lines, the hovered cell's outline and the tool cursor;
 * the pointer paints through the EditorController.
 *
 * QPainter, as the view cube: a few hundred flat cells, hatching and text,
 * which QPainter draws directly and a test can draw into an image.
 */
class VoxelGridItem : public QQuickPaintedItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(EditorController * editor READ editor WRITE setEditor NOTIFY editorChanged FINAL)
  /* a cell's size in dp (C08: fits the area, 16 ... 120) */
  Q_PROPERTY(int cellSize READ cellSize NOTIFY cellSizeChanged FINAL)

public:

  explicit VoxelGridItem(QQuickItem * parent = nullptr);

  EditorController * editor(void) const { return m_editor.data(); }
  void setEditor(EditorController * e);
  int cellSize(void) const { return m_cell; }

  void paint(QPainter * p) override;

  /* the cell (column, row) at an item point, (-1, -1) for none */
  Q_INVOKABLE QPoint cellAt(QPointF p) const;
  /* the centre of a cell in item coordinates; tests click there */
  Q_INVOKABLE QPointF cellCentre(int col, int row) const;

signals:

  void editorChanged(void);
  void cellSizeChanged(void);

protected:

  void geometryChange(const QRectF & newGeometry, const QRectF & oldGeometry) override;
  void hoverEnterEvent(QHoverEvent * e) override;
  void hoverMoveEvent(QHoverEvent * e) override;
  void hoverLeaveEvent(QHoverEvent * e) override;
  void mousePressEvent(QMouseEvent * e) override;
  void mouseMoveEvent(QMouseEvent * e) override;
  void mouseReleaseEvent(QMouseEvent * e) override;
  void mouseUngrabEvent(void) override;
  /* Shift, Alt and Ctrl pressed or released over the grid change the
   * quick tool at once, without a pointer move */
  bool eventFilter(QObject * watched, QEvent * e) override;

private:

  void relayout(void);
  void updateCursor(void);
  void hoverAt(QPointF p);
  /* the stroke from the last point to p, every cell on the way */
  void strokeTo(QPointF p);
  QPointF toItem(btui::Pt u) const;
  btui::Pt toUnits(QPointF p) const;
  QColor colourOf(unsigned colour) const;

  QPointer<EditorController> m_editor;
  QList<QMetaObject::Connection> m_connections;
  btui::GridLayout m_layout;
  quint64 m_layoutRevision = ~quint64(0);
  int m_cell = 16;
  QPointF m_origin;                   ///< the grid's top left, item coordinates
  bool m_hovered = false;
  bool m_filtering = false;
  bool m_pressed = false;
  QPointF m_last;                     ///< the stroke's last point
  std::vector<QStaticText> m_numbers;   ///< the rulers' numbers, laid out once
};

#endif
