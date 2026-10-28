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
#ifndef BTQT_SYSTEMPOINTER_H
#define BTQT_SYSTEMPOINTER_H

#include <QImage>
#include <QPoint>

/* The operating system's own arrow and pointer size (PR-20, OQ-17), for
 * the Fixed and Variable edit cursors to draw their badge on:
 *
 *  - Windows: the arrow of the user's pointer scheme (Control Panel ▸
 *    Mouse ▸ Pointers, or a pointer colour from Settings), at the pointer
 *    size of Settings ▸ Accessibility ▸ Mouse pointer (`CursorBaseSize`);
 *    a custom cursor is shown at its own size there, so it is drawn at it;
 *  - macOS: `NSCursor.arrowCursor`; the system enlarges every cursor,
 *    custom ones too, by Accessibility ▸ Display ▸ Pointer size, so the
 *    size factor is 1;
 *  - Linux and the BSDs: the Xcursor theme's arrow at its size
 *    (`btui::xcursor`).
 * Anything unreadable gives a null image, and the caller draws the bundled
 * arrow (OQ-17's fallback).
 */
namespace systempointer {

  struct Arrow {
    QImage image;          ///< device pixels, the scale as its device pixel ratio; null when unknown
    QPoint hotspot;        ///< logical pixels from the top left
  };

  /* The pointer size the user chose, relative to the platform's default
   * (1: the default). */
  double sizeFactor(void);

  /* The arrow as the system draws it on a screen of this device pixel
   * ratio. Read anew each time: the user may change it while we run. */
  Arrow arrow(double scale);
}

#endif
