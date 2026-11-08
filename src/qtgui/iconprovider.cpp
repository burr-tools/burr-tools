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
#include "iconprovider.h"

#include <QFile>
#include <QPainter>
#include <QSvgRenderer>
#include <QUrlQuery>

IconProvider::IconProvider(void) : QQuickImageProvider(QQuickImageProvider::Image) {}

QByteArray IconProvider::tintedSvg(const QString & name, const QString & color) {
  // names come from QML; refuse anything that could leave the icon folder
  if (name.isEmpty() || name.contains(QLatin1Char('/')) || name.contains(QLatin1String("..")))
    return {};
  QFile f(QStringLiteral(":/burrtools/icons/") + name + QStringLiteral(".svg"));
  if (!f.open(QIODevice::ReadOnly))
    return {};
  QByteArray svg = f.readAll();
  svg.replace("currentColor", color.toUtf8());
  return svg;
}

QImage IconProvider::requestImage(const QString & id, QSize * size, const QSize & requestedSize) {
  /* id is "<name>?color=<hex>". The hex comes without its '#', which would
   * start a URL fragment and never reach the provider; the colour defaults
   * to black.
   */
  const qsizetype q = id.indexOf(QLatin1Char('?'));
  const QString name = q < 0 ? id : id.left(q);
  QString color = QStringLiteral("#000000");
  if (q >= 0) {
    QUrlQuery query(id.mid(q + 1));
    if (query.hasQueryItem(QStringLiteral("color")))
      color = QLatin1Char('#') + query.queryItemValue(QStringLiteral("color"));
  }

  QSvgRenderer renderer(tintedSvg(name, color));
  QSize target = requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : QSize(24, 24);
  if (size)
    *size = target;

  QImage img(target, QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  if (renderer.isValid()) {
    QPainter p(&img);
    renderer.render(&p);
  }
  return img;
}
