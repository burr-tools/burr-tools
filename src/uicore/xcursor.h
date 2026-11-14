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
#ifndef BTUI_XCURSOR_H
#define BTUI_XCURSOR_H

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/* The desktop's own arrow on Linux and the BSDs (PR-20, OQ-17): the edit
 * cursors draw their badge on it. X11 and Wayland desktops keep cursors as
 * Xcursor themes -- folders of files, each holding a cursor at several
 * sizes -- and say which theme and size through the environment
 * (XCURSOR_THEME, XCURSOR_SIZE) or their settings files. This reads them;
 * the Qt side turns the pixels into an image.
 */
namespace btui::xcursor {

  /* One image of a cursor file: premultiplied ARGB, rows top first. */
  struct Image {
    int width = 0, height = 0;
    int xhot = 0, yhot = 0;            ///< the hotspot, in pixels from the top left
    int nominal = 0;                   ///< the size the file says it was drawn for
    std::vector<uint32_t> argb;
  };

  /* The image whose nominal size is nearest `size` (the larger on a tie)
   * in an Xcursor file's bytes; nothing when they are not one, or are cut
   * short. Animated cursors give their first frame. */
  std::optional<Image> parse(std::string_view bytes, int size);

  /* What the desktop asks for. `env` reads an environment variable (null
   * when unset); the settings files are KDE's kcminputrc and GTK's
   * settings.ini under XDG_CONFIG_HOME (~/.config). */
  struct Settings {
    std::string theme;                 ///< empty: the "default" theme
    int size = 24;                     ///< logical pixels
  };
  using Env = std::function<const char *(const char *)>;
  Settings settings(const Env & env);

  /* Where themes are: XCURSOR_PATH, or Xcursor's own default list
   * (~/.local/share/icons, ~/.icons, /usr/share/icons, /usr/share/pixmaps). */
  std::vector<std::filesystem::path> searchPath(const Env & env);

  /* The file of the first of `names` in a theme, looked for in each folder
   * of the path, then in the themes its index.theme inherits; empty when
   * there is none. */
  std::filesystem::path find(const std::string & theme, const std::vector<std::string> & names,
                             const std::vector<std::filesystem::path> & dirs);
}

#endif
