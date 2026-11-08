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
#ifndef BTQT_QMLOUTLINES_H
#define BTQT_QMLOUTLINES_H

#include <QPointer>
#include <QQuickPaintedItem>
#include <QTimer>

class QQuickWindow;

/* BURRTOOLS_QML_OUTLINES=1: a window's map from the screen back to the QML.
 * Every visible item the QML gave an objectName gets a thin outline; the
 * one under the mouse pointer is highlighted and labelled with its
 * objectName and the QML file and line that declare it, and its chain of
 * named parents is printed on stderr as the pointer moves onto it. A tool
 * for reading the QML: it takes no input (the pointer is polled, so hover
 * and clicks still reach the UI) and costs nothing when off.
 */
class QmlOutlines : public QQuickPaintedItem {
public:

  /* over `window`, if BURRTOOLS_QML_OUTLINES is set; else nothing */
  static void installIfAsked(QQuickWindow * window);

  explicit QmlOutlines(QQuickWindow * window);

  void paint(QPainter * p) override;

  /* "ShapesCard.qml:42": where an object made from QML was declared;
   * empty for one made in C++ */
  static QString declaredAt(const QObject * object);

  /* the visible items under `root` that QML declared with an objectName,
   * parents first */
  static QList<QQuickItem *> namedItems(QQuickItem * root);

private:

  void poll(void);

  QPointer<QQuickWindow> m_window;
  QTimer m_timer;
  QPointer<QQuickItem> m_hot;
};

#endif
