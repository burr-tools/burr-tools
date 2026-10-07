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
#ifndef BTQT_VIEWCUBEITEM_H
#define BTQT_VIEWCUBEITEM_H

#include "../uicore/viewcube.h"

#include <QPointer>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

class QPainter;
#include "scenecontroller.h"   // a Q_PROPERTY type: moc needs it complete

/* The view cube (spec C13): drawn with QPainter in its 156 x 170 dp
 * widget, its geometry and hit-testing from btui::ViewCube, its camera the
 * main view's.
 *
 * QPainter rather than the 3D renderer: the cube is a small overlay whose
 * labels must skew with their faces, which QTransform::quadToQuad does
 * exactly, and painting into a QImage makes it testable without a GPU.
 */
class ViewCubeItem : public QQuickPaintedItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(SceneController * controller READ controller WRITE setController NOTIFY controllerChanged)

public:

  explicit ViewCubeItem(QQuickItem * parent = nullptr);

  SceneController * controller(void) const { return m_controller.data(); }
  void setController(SceneController * c);

  void paint(QPainter * p) override;

  /* the cube for an orientation, projection, hover and theme; widget
   * coordinates (dp). Exposed so tests draw it into an image. */
  static void paintCube(QPainter * p, btui::Quat orient, bool perspective, const btui::ViewCube::Hit & hover, bool dark);

  /* what is under a point in item coordinates -- the item is the design's
   * 156 x 170 plus the axis indicator's room at the left and bottom
   * (ViewCube::kPadLeft / kPadBottom). Public for the tests. */
  btui::ViewCube::Hit hitAt(QPointF p) const;

signals:

  void controllerChanged(void);

protected:

  void hoverMoveEvent(QHoverEvent * e) override;
  void hoverLeaveEvent(QHoverEvent * e) override;
  void mousePressEvent(QMouseEvent * e) override;
  void mouseMoveEvent(QMouseEvent * e) override;
  void mouseReleaseEvent(QMouseEvent * e) override;
  void mouseDoubleClickEvent(QMouseEvent * e) override;

private:

  void updateCursor(const btui::ViewCube::Hit & h);

  QPointer<SceneController> m_controller;
  QMetaObject::Connection m_frameConnection;
  QPointF m_press;
  btui::ViewCube::Hit m_pressHit;
  bool m_dragging = false;
  bool m_afterDoubleClick = false;   ///< the release that ends a double-click is not a click
};

#endif
