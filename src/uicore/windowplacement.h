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
#ifndef BTUI_WINDOWPLACEMENT_H
#define BTUI_WINDOWPLACEMENT_H

#include <optional>
#include <vector>

/* Where the main window opens: where the user left it (legacy kept
 * windowpos*), if that is still on a screen; otherwise -- the first start,
 * or a monitor since unplugged -- centred on the primary screen's free area,
 * clear of a taskbar or dock, at most 1600 x 1000 and 92 % of that area.
 */
namespace btui {

  struct WindowRect { int x = 0, y = 0, width = 0, height = 0; };

  struct WindowPlacement {
    WindowRect rect;
    bool maximized = false;
    bool restored = false;      ///< the saved place was used
  };

  /* saved: the last place (empty before the first save); screens: every
   * screen's geometry; available: the primary screen's free area. The size
   * never goes below the window's minimum. */
  WindowPlacement placeWindow(std::optional<WindowRect> saved, bool savedMaximized,
                              const std::vector<WindowRect> & screens, WindowRect available,
                              int minWidth, int minHeight);
}

#endif
