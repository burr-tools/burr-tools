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
#ifndef BTQT_ICONPROVIDER_H
#define BTQT_ICONPROVIDER_H

#include <QQuickImageProvider>

/* Renders the monochrome UI icons (foundations/ui-icons) in a given colour.
 *
 * The icons draw with stroke="currentColor", which an SVG renderer resolves
 * to black; the spec tints them muted / text / accent by state (glyph
 * catalog, asset rules). QML asks for "image://icon/<name>?color=<hex>" and
 * gets the SVG with currentColor replaced, rasterised at the requested size,
 * which Image passes in device pixels when sourceSize is set -- so the icon
 * stays crisp at every scale.
 */
class IconProvider : public QQuickImageProvider {

public:

  IconProvider(void);

  QImage requestImage(const QString & id, QSize * size, const QSize & requestedSize) override;

  /* The SVG text of a UI icon with currentColor replaced; empty when there is
   * no such icon. Exposed for tests.
   */
  static QByteArray tintedSvg(const QString & name, const QString & color);
};

#endif
