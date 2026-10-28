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
#include "editcursor.h"

#include <QFile>
#include <QHash>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

#include <algorithm>
#include <cmath>

namespace editcursor {

  namespace {

    // the glyph catalog's cursor tones: ink and its opposite, per theme
    QString ink(bool dark) { return dark ? QStringLiteral("#f2f4f8") : QStringLiteral("#1a1f29"); }
    QString opposite(bool dark) { return dark ? QStringLiteral("#14171c") : QStringLiteral("#ffffff"); }

    QString asset(const char * name) {
      QFile f(QStringLiteral(":/burrtools/cursors/") + QLatin1String(name));
      return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
    }

    void render(QPainter & p, const QString & svg, const QRectF & where) {
      QSvgRenderer r(svg.toUtf8());
      if (r.isValid())
        r.render(&p, where);
    }

    /* the Fixed or Variable badge, its placeholders filled */
    QString badge(btui::Tool tool, const QColor & colour, bool dark) {
      const bool def = !colour.isValid();
      QString svg;
      if (tool == btui::Tool::Fixed) {
        svg = asset("badge-fixed.template.svg");
        // Default: the opposite tone inside, a full-strength outline
        if (def)
          svg.replace(QStringLiteral(" stroke-opacity=\"0.55\""), QString());
        svg.replace(QStringLiteral("%COLOUR%"), def ? opposite(dark) : colour.name());
      } else {
        svg = asset("badge-variable.template.svg");
        // Default: the opposite tone inside, boundary and "+" in ink
        if (def)
          svg.replace(QStringLiteral("fill=\"%COLOUR%\" fill-opacity=\"0.32\""),
                      QStringLiteral("fill=\"%1\"").arg(opposite(dark)));
        svg.replace(QStringLiteral("%COLOUR%"), def ? ink(dark) : colour.name());
      }
      return svg.replace(QStringLiteral("%INK%"), ink(dark)).replace(QStringLiteral("%OPP%"), opposite(dark));
    }

    QString icon(btui::Tool tool, const QColor & colour, bool dark) {
      QString svg = asset(tool == btui::Tool::Erase ? "icon-cursor-erase.template.svg" : "icon-cursor-paint.template.svg");
      svg.replace(QStringLiteral("%COLOUR%"), colour.isValid() ? colour.name() : ink(dark));
      return svg.replace(QStringLiteral("%INK%"), ink(dark)).replace(QStringLiteral("%OPP%"), opposite(dark));
    }
  }

  Composed compose(btui::Tool tool, const QColor & colour, bool dark, qreal scale) {
    scale = std::max<qreal>(1, scale);
    const int px = int(std::lround(32 * scale));
    Composed c;
    c.image = QImage(px, px, QImage::Format_ARGB32_Premultiplied);
    c.image.fill(Qt::transparent);
    QPainter p(&c.image);
    p.setRenderHint(QPainter::Antialiasing);
    switch (tool) {
      case btui::Tool::Fixed:
      case btui::Tool::Variable:
        render(p, asset("arrow-fallback.svg"), QRectF(0, 0, px, px));
        // snug against the arrow's upper right: left edge at its width, top at its top
        render(p, badge(tool, colour, dark), QRectF(12 * scale, 0, 12 * scale, 12 * scale));
        c.hotspot = QPoint(1, 1);
        break;
      case btui::Tool::Erase:
      case btui::Tool::Paint:
        render(p, icon(tool, colour, dark), QRectF(0, 0, px, px));
        c.hotspot = QPoint(16, 16);
        break;
    }
    p.end();
    c.image.setDevicePixelRatio(scale);
    return c;
  }

  QImage glyph(btui::Tool tool, const QColor & colour, bool dark, int px) {
    px = std::max(1, px);
    // drawn large, then the box of what it draws taken and fitted
    const Composed c = compose(tool, colour, dark, qreal(px) / 12.0);
    const QImage & img = c.image;
    int x0 = img.width(), y0 = img.height(), x1 = -1, y1 = -1;
    for (int y = 0; y < img.height(); y++) {
      const QRgb * row = reinterpret_cast<const QRgb *>(img.constScanLine(y));
      for (int x = 0; x < img.width(); x++)
        if (qAlpha(row[x]) > 8) {
          x0 = std::min(x0, x); x1 = std::max(x1, x);
          y0 = std::min(y0, y); y1 = std::max(y1, y);
        }
    }
    QImage out(px, px, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    if (x1 < x0)
      return out;
    const QRect box(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
    const qreal k = qreal(px) / std::max(box.width(), box.height());
    const QSizeF size(box.width() * k, box.height() * k);
    QPainter p(&out);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    QImage part = img.copy(box);
    part.setDevicePixelRatio(1);
    p.drawImage(QRectF(QPointF((px - size.width()) / 2, (px - size.height()) / 2), size), part);
    return out;
  }

  QCursor cursor(btui::Tool tool, const QColor & colour, bool dark, qreal scale) {
    static QHash<QString, QCursor> cache;
    const QString key = QStringLiteral("%1/%2/%3/%4").arg(int(tool)).arg(colour.isValid() ? colour.name() : QString())
                            .arg(dark).arg(scale);
    auto it = cache.constFind(key);
    if (it != cache.constEnd())
      return *it;
    const Composed c = compose(tool, colour, dark, scale);
    QCursor made(QPixmap::fromImage(c.image), c.hotspot.x(), c.hotspot.y());
    cache.insert(key, made);
    return made;
  }
}
