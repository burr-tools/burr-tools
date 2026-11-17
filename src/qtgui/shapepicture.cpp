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
#include "shapepicture.h"

#include "guarded.h"

#include "../lib/puzzle.h"

#include <QPainter>
#include <QPolygonF>

ShapePicture::ShapePicture(QQuickItem * parent) : QQuickPaintedItem(parent) {
  setAntialiasing(true);
}

void ShapePicture::setDocument(DocumentController * d) {
  if (d == m_doc)
    return;
  if (m_doc)
    disconnect(m_doc, nullptr, this, nullptr);
  m_doc = d;
  if (m_doc) {
    connect(m_doc, &DocumentController::puzzleEdited, this,
            [this](int, DocumentController::Change change, int shape) {
              using C = DocumentController::Change;
              if (change == C::Structure || (change == C::Voxels && (shape < 0 || shape == m_shape)))
                stale();
            });
    connect(m_doc, &DocumentController::historyApplied, this, &ShapePicture::stale);
    connect(m_doc, &DocumentController::documentReplaced, this, &ShapePicture::stale);
  }
  stale();
  emit documentChanged();
}

void ShapePicture::setShape(int s) {
  if (s == m_shape)
    return;
  m_shape = s;
  stale();
  emit shapeChanged();
}

void ShapePicture::setColour(const QColor & c) {
  if (c == m_colour)
    return;
  m_colour = c;
  stale();
  emit colourChanged();
}

void ShapePicture::geometryChange(const QRectF & newGeometry, const QRectF & oldGeometry) {
  QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
  if (newGeometry.size() != oldGeometry.size())
    stale();
}

void ShapePicture::stale(void) {
  m_stale = true;
  update();
}

void ShapePicture::build(void) {
  m_stale = false;
  const float size = float(std::min(width(), height()));
  btui::VectorPage page;
  if (m_doc && size > 0)
    page = guarded([&] {
      const puzzle_c & p = m_doc->session().puzzle();
      if (m_shape < 0 || unsigned(m_shape) >= p.getNumberOfShapes())
        return btui::VectorPage{};
      return btui::shapePicture(*p.getShape(unsigned(m_shape)),
                                btui::Rgb{ float(m_colour.redF()), float(m_colour.greenF()), float(m_colour.blueF()) },
                                size);
    });
  m_page = std::move(page);
  emit drawn();
}

void ShapePicture::paint(QPainter * painter) {
  if (m_stale)
    build();
  if (m_page.prims.empty())
    return;
  painter->setRenderHint(QPainter::Antialiasing);
  // centred in a box that is not square
  const float size = float(std::min(width(), height()));
  painter->translate((width() - size) / 2, (height() - size) / 2);
  QPolygonF poly;
  for (const btui::VectorPrimitive & p : m_page.prims) {
    poly.clear();
    for (size_t i = 0; i + 1 < p.xy.size(); i += 2)
      poly << QPointF(p.xy[i], p.xy[i + 1]);
    const QColor c = QColor::fromRgbF(p.r, p.g, p.b, p.a);
    // a hairline of the face's own colour closes the seams antialiasing leaves
    painter->setPen(QPen(c, 0.5));
    painter->setBrush(c);
    painter->drawPolygon(poly);
  }
}
