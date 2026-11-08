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
#include "windowplacement.h"

#include <algorithm>

namespace btui {

  namespace {

    bool contains(const WindowRect & r, int x, int y) {
      return x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
    }
  }

  WindowPlacement placeWindow(std::optional<WindowRect> saved, bool savedMaximized,
                              const std::vector<WindowRect> & screens, WindowRect available,
                              int minWidth, int minHeight) {
    WindowPlacement p;
    p.maximized = savedMaximized;

    // the saved place counts while its centre is on some screen
    if (saved && saved->width > 0 && saved->height > 0) {
      const int cx = saved->x + saved->width / 2, cy = saved->y + saved->height / 2;
      for (const WindowRect & s : screens)
        if (contains(s, cx, cy)) {
          p.rect = *saved;
          p.rect.width = std::max(minWidth, saved->width);
          p.rect.height = std::max(minHeight, saved->height);
          p.restored = true;
          return p;
        }
    }

    p.rect.width = std::max(minWidth, std::min(1600, int(available.width * 0.92)));
    p.rect.height = std::max(minHeight, std::min(1000, int(available.height * 0.92)));
    p.rect.x = available.x + (available.width - p.rect.width) / 2;
    p.rect.y = available.y + (available.height - p.rect.height) / 2;
    return p;
  }
}
