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
#ifndef BTQT_VOXELVIEWPORT_H
#define BTQT_VOXELVIEWPORT_H

#include <QPointer>
#include <QtQml/qqmlregistration.h>
#include <QtQuick/QQuickRhiItem>

#include "scenecontroller.h"   // a Q_PROPERTY type: moc needs it complete

/* The 3D view's surface (spec C06 "canvas"): a QQuickRhiItem that draws the
 * controller's frame with SceneRenderer and passes pointer input on in dp.
 *
 * It never takes keyboard focus -- pressing in the 3D view must not pull
 * focus out of the Solver's lists (C20 sticky focus).
 */
class VoxelViewport : public QQuickRhiItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(SceneController * controller READ controller WRITE setController NOTIFY controllerChanged)

public:

  explicit VoxelViewport(QQuickItem * parent = nullptr);

  SceneController * controller(void) const { return m_controller.data(); }
  void setController(SceneController * c);

  /* the multisampling to draw with, of what the graphics device offers:
   * 8x where it can, else 4x or 2x -- smooth voxel edges (C06) */
  static int bestSampleCount(const QList<int> & supported);

  /* the multisampling the scene is drawn with: 4 until the device is known */
  int msaaSamples(void) const { return m_samples; }

signals:

  void controllerChanged(void);

protected:

  QQuickRhiItemRenderer * createRenderer(void) override;
  void itemChange(ItemChange change, const ItemChangeData & data) override;

  void geometryChange(const QRectF & newGeometry, const QRectF & oldGeometry) override;
  void mousePressEvent(QMouseEvent * e) override;
  void mouseMoveEvent(QMouseEvent * e) override;
  void mouseReleaseEvent(QMouseEvent * e) override;
  void wheelEvent(QWheelEvent * e) override;

private:

  /* the window's device decides the sample count; its QRhi exists once
   * the scene graph is initialised */
  void adoptBestSampleCount(void);
  void watchWindow(QQuickWindow * w);

  QPointer<SceneController> m_controller;
  QMetaObject::Connection m_frameConnection;
  QMetaObject::Connection m_windowConnection;
  int m_samples = 4;
};

#endif
