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
#ifndef BTQT_EDITCURSOR_H
#define BTQT_EDITCURSOR_H

#include "../uicore/voxeledit.h"

#include <QColor>
#include <QCursor>
#include <QImage>
#include <QPoint>

/* The edit cursors (PR-20, glyph catalog ▸ Cursors), composed at run time:
 * Fixed and Variable are the pointer with a square badge at its upper
 * right, filled with the drawing colour; Erase and Paint are their icons
 * alone, hotspot at the centre. The Default colour (an invalid QColor) is
 * drawn in the theme's contrast tones. The OS pointer is not read back:
 * the bundled arrow (resources/cursors/arrow-fallback.svg) stands in for it
 * on every platform (OQ-17's fallback).
 *
 * Sizes follow a 32 px canvas at scale 1 (48 at 1.5, 64 at 2); `scale` is
 * the screen's device pixel ratio, the nearest the toolkit offers to the OS
 * cursor size.
 */
namespace editcursor {

  struct Composed {
    QImage image;                  ///< device pixels, with the scale as its device pixel ratio
    QPoint hotspot;                ///< in logical (scale 1) pixels
  };

  Composed compose(btui::Tool tool, const QColor & colour, bool dark, qreal scale);

  /* compose(), cached per tool, colour, theme and scale */
  QCursor cursor(btui::Tool tool, const QColor & colour, bool dark, qreal scale);

  /* The same drawing as an icon (C06 "The Edit button": "the same drawing
   * as the cursor"): cropped to what it draws and centred in a square of
   * px device pixels, so it fills a button as a glyph does */
  QImage glyph(btui::Tool tool, const QColor & colour, bool dark, int px);
}

#endif
