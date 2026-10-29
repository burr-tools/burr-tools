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
#ifndef BTQT_WINDOWFRAME_H
#define BTQT_WINDOWFRAME_H

class QWindow;

/* The window manager's frame (title bar) dark while the theme is, on
 * Linux and the BSDs, even on a light desktop. A light theme leaves the
 * frame to the desktop: a dark desktop keeps its dark frames.
 *
 *  - X11: the window's `_GTK_THEME_VARIANT` ("dark"), which Mutter,
 *    Metacity, xfwm4, Marco and Budgie read for the frame they draw;
 *  - KDE, X11 and Wayland: the application's colour scheme
 *    (`KDE_COLOR_SCHEME_PATH`, Breeze Dark), from which the KDE platform
 *    theme colours KWin's decoration;
 *  - GNOME on Wayland: nothing can -- Qt draws the frame there (its
 *    Adwaita decoration) after the desktop's own scheme alone.
 * Windows and macOS: nothing to do; Theme's QStyleHints::setColorScheme
 * reaches their frames.
 */
namespace windowframe {

  /* follows Theme::dark from now on, for this window */
  void follow(QWindow * window);
}

#endif
