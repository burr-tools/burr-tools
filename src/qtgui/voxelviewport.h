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
 * A press never takes keyboard focus -- it must not pull focus out of the
 * Solver's lists (C20 sticky focus); Tab reaches it where the QML lets it
 * (the main view: its keys, WCAG 2.1.1).
 *
 * Over it without a button the pointer and the modifiers held go to the
 * controller (pointerHover), which the main view edits from (C06): the
 * modifiers also when only they change, so a quick tool's cursor and ghost
 * appear the moment its key goes down (AC-C06-08) -- at most once a frame:
 * the latest pointer and modifiers are kept and picked in updatePolish()
 * (the Qt Quick guide's section 7). The cursor is the edit tool's while a
 * click would edit, the grab hand while dragging.
 */
class VoxelViewport : public QQuickRhiItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(SceneController * controller READ controller WRITE setController NOTIFY controllerChanged FINAL)
  /* QQuickItem's activeFocusOnTab, which Qt 6.8 does not let QML set on a
   * type of ours ("not available in BurrTools.Ui 255.255": a property
   * revision the module's types do not take up) */
  Q_PROPERTY(bool tabStop READ activeFocusOnTab WRITE setActiveFocusOnTab NOTIFY activeFocusOnTabChanged FINAL)

public:

  explicit VoxelViewport(QQuickItem * parent = nullptr);

  SceneController * controller(void) const { return m_controller.data(); }
  void setController(SceneController * c);

  /* the multisampling to draw with, of what the graphics device offers:
   * the most of 8x, 4x and 2x it can that is no more than `wanted` (Settings
   * ▸ 3D view ▸ Anti-aliasing), else none -- smooth voxel edges (C06) */
  static int bestSampleCount(const QList<int> & supported, int wanted = 8);

  /* the multisampling the scene is drawn with: the setting's (4x by
   * default), as far as the device goes once it is known */
  int msaaSamples(void) const;

  /* the device's sample counts, set by the renderer during synchronize()
   * (the render thread, the GUI thread waiting): only that thread may ask
   * the window's QRhi. True when they changed. */
  bool setSupportedSampleCounts(const QList<int> & counts);

signals:

  void controllerChanged(void);

protected:

  QQuickRhiItemRenderer * createRenderer(void) override;

  void geometryChange(const QRectF & newGeometry, const QRectF & oldGeometry) override;
  void mousePressEvent(QMouseEvent * e) override;
  void mouseMoveEvent(QMouseEvent * e) override;
  void mouseReleaseEvent(QMouseEvent * e) override;
  void mouseUngrabEvent(void) override;
  void wheelEvent(QWheelEvent * e) override;
  void hoverEnterEvent(QHoverEvent * e) override;
  void hoverMoveEvent(QHoverEvent * e) override;
  void hoverLeaveEvent(QHoverEvent * e) override;
  bool eventFilter(QObject * watched, QEvent * e) override;
  void updatePolish(void) override;
  /* a move to a screen of another scale composes the edit cursor again */
  void itemChange(ItemChange change, const ItemChangeData & value) override;

private:

  void updateCursor(void);

  QPointer<SceneController> m_controller;
  QMetaObject::Connection m_frameConnection;
  QPointF m_hover{ -1, -1 };       ///< the pointer, while over the item
  Qt::KeyboardModifiers m_hoverMods;
  bool m_hoverPending = false;     ///< a hover to pick at the next polish
  bool m_filtering = false;        ///< watching the modifier keys
  quint64 m_cursorKey = 0;         ///< the cursor set (updateCursor): none, the tool, or a grab hand; 0 before any
  QList<int> m_supported;          ///< the device's sample counts; empty until known
};

#endif
