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
#ifndef BTQT_SHAPEPICTURE_H
#define BTQT_SHAPEPICTURE_H

#include "documentcontroller.h"   // a Q_PROPERTY type: moc needs it complete

#include "../uicore/vectorexport.h"

#include <QColor>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

/* A shape as a small picture, to tell shapes apart in a list (the Result's
 * Change… menu): its faces seen from the 3D view's Home angle, in `colour`
 * (btui::shapePicture). Drawn with QPainter, made again when the shape's
 * voxels change, an undo or redo applies, or the document is replaced. */
class ShapePicture : public QQuickPaintedItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(DocumentController * document READ document WRITE setDocument NOTIFY documentChanged FINAL)
  Q_PROPERTY(int shape READ shape WRITE setShape NOTIFY shapeChanged FINAL)
  Q_PROPERTY(QColor colour READ colour WRITE setColour NOTIFY colourChanged FINAL)
  /* how many faces the picture holds: 0 for none (an empty shape); for tests */
  Q_PROPERTY(int faces READ faces NOTIFY drawn FINAL)

public:
  explicit ShapePicture(QQuickItem * parent = nullptr);

  DocumentController * document(void) const { return m_doc; }
  void setDocument(DocumentController * d);
  int shape(void) const { return m_shape; }
  void setShape(int s);
  QColor colour(void) const { return m_colour; }
  void setColour(const QColor & c);
  int faces(void) const { return int(m_page.prims.size()); }

  void paint(QPainter * painter) override;

signals:
  void documentChanged(void);
  void shapeChanged(void);
  void colourChanged(void);
  void drawn(void);

protected:
  void geometryChange(const QRectF & newGeometry, const QRectF & oldGeometry) override;

private:
  void stale(void);
  void build(void);

  QPointer<DocumentController> m_doc;
  int m_shape = -1;
  QColor m_colour{ Qt::gray };
  btui::VectorPage m_page;
  bool m_stale = true;
};

#endif
