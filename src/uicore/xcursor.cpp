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
#include "xcursor.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>

namespace btui::xcursor {

  namespace {

    // the file format (libXcursor's xcursor.c): little-endian 32-bit words
    constexpr uint32_t kMagic = 0x72756358;          // "Xcur"
    constexpr uint32_t kImageType = 0xfffd0002;
    constexpr uint32_t kMaxSide = 0x7fff;

    bool word(std::string_view b, size_t at, uint32_t & out) {
      if (b.size() < 4 || at > b.size() - 4)
        return false;
      const std::string_view w = b.substr(at, 4);
      out = uint32_t(uint8_t(w[0])) | uint32_t(uint8_t(w[1])) << 8 | uint32_t(uint8_t(w[2])) << 16 |
            uint32_t(uint8_t(w[3])) << 24;
      return true;
    }

    std::string trim(std::string s) {
      const auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
      while (!s.empty() && isSpace(s.back()))
        s.pop_back();
      size_t i = 0;
      while (i < s.size() && isSpace(s[i]))
        i++;
      return s.substr(i);
    }

    /* the value of `key=value` anywhere in an ini-style file; empty when absent */
    std::string iniValue(const std::filesystem::path & file, const std::string & key) {
      std::ifstream in(file);
      std::string line;
      while (std::getline(in, line)) {
        const size_t eq = line.find('=');
        if (eq != std::string::npos && trim(line.substr(0, eq)) == key)
          return trim(line.substr(eq + 1));
      }
      return {};
    }

    int positive(const std::string & s) {
      char * end = nullptr;
      const long v = std::strtol(s.c_str(), &end, 10);
      return end != s.c_str() && v > 0 && v <= 512 ? int(v) : 0;
    }

    std::string home(const Env & env) {
      const char * h = env("HOME");
      return h ? h : "";
    }
  }

  std::optional<Image> parse(std::string_view b, int size) {
    uint32_t magic = 0, headerSize = 0, version = 0, ntoc = 0;
    if (!word(b, 0, magic) || magic != kMagic || !word(b, 4, headerSize) || !word(b, 8, version) || !word(b, 12, ntoc))
      return std::nullopt;
    if (headerSize < 16 || ntoc > 0x10000 || headerSize > b.size() || (b.size() - headerSize) / 12 < ntoc)
      return std::nullopt;

    // the image entries' nominal sizes: the nearest, the larger on a tie
    long best = -1;
    for (uint32_t i = 0; i < ntoc; i++) {
      uint32_t type = 0, subtype = 0;
      word(b, headerSize + i * 12, type);
      word(b, headerSize + i * 12 + 4, subtype);
      if (type != kImageType)
        continue;
      const long d = std::labs(long(subtype) - size), bd = std::labs(best - size);
      if (best < 0 || d < bd || (d == bd && long(subtype) > best))
        best = long(subtype);
    }
    if (best < 0)
      return std::nullopt;

    // its first image (an animation's first frame)
    for (uint32_t i = 0; i < ntoc; i++) {
      uint32_t type = 0, subtype = 0, position = 0;
      word(b, headerSize + i * 12, type);
      word(b, headerSize + i * 12 + 4, subtype);
      word(b, headerSize + i * 12 + 8, position);
      if (type != kImageType || long(subtype) != best)
        continue;
      uint32_t chunkHeader = 0, chunkType = 0, nominal = 0, chunkVersion = 0, w = 0, h = 0, xhot = 0, yhot = 0, delay = 0;
      if (!word(b, position, chunkHeader) || !word(b, size_t(position) + 4, chunkType) ||
          !word(b, size_t(position) + 8, nominal) || !word(b, size_t(position) + 12, chunkVersion) ||
          !word(b, size_t(position) + 16, w) || !word(b, size_t(position) + 20, h) ||
          !word(b, size_t(position) + 24, xhot) || !word(b, size_t(position) + 28, yhot) ||
          !word(b, size_t(position) + 32, delay))
        return std::nullopt;
      if (chunkType != kImageType || chunkHeader < 36 || w == 0 || h == 0 || w > kMaxSide || h > kMaxSide ||
          xhot > w || yhot > h)
        return std::nullopt;
      const size_t pixels = size_t(position) + chunkHeader;
      const size_t count = size_t(w) * h;
      if (pixels > b.size() || (b.size() - pixels) / 4 < count)
        return std::nullopt;
      Image img;
      img.width = int(w);
      img.height = int(h);
      img.xhot = int(xhot);
      img.yhot = int(yhot);
      img.nominal = int(nominal);
      img.argb.resize(count);
      for (size_t k = 0; k < count; k++)
        word(b, pixels + k * 4, img.argb[k]);
      return img;
    }
    return std::nullopt;
  }

  Settings settings(const Env & env) {
    Settings s;
    const char * xdg = env("XDG_CONFIG_HOME");
    const std::filesystem::path config = xdg && *xdg ? std::filesystem::path(xdg)
                                                     : std::filesystem::path(home(env)) / ".config";
    const std::filesystem::path kde = config / "kcminputrc";
    const std::filesystem::path gtk3 = config / "gtk-3.0" / "settings.ini";
    const std::filesystem::path gtk4 = config / "gtk-4.0" / "settings.ini";

    if (const char * t = env("XCURSOR_THEME"); t && *t)
      s.theme = t;
    for (const auto & [file, key] : { std::pair{ kde, "cursorTheme" }, std::pair{ gtk4, "gtk-cursor-theme-name" },
                                      std::pair{ gtk3, "gtk-cursor-theme-name" } })
      if (s.theme.empty())
        s.theme = iniValue(file, key);

    int size = 0;
    if (const char * v = env("XCURSOR_SIZE"))
      size = positive(v);
    for (const auto & [file, key] : { std::pair{ kde, "cursorSize" }, std::pair{ gtk4, "gtk-cursor-theme-size" },
                                      std::pair{ gtk3, "gtk-cursor-theme-size" } })
      if (size == 0)
        size = positive(iniValue(file, key));
    if (size > 0)
      s.size = size;
    return s;
  }

  std::vector<std::filesystem::path> searchPath(const Env & env) {
    const std::string h = home(env);
    std::string list;
    if (const char * p = env("XCURSOR_PATH"); p && *p)
      list = p;
    else {
      const char * data = env("XDG_DATA_HOME");
      list = (data && *data ? std::string(data) : h + "/.local/share") + "/icons:~/.icons:/usr/share/icons:/usr/share/pixmaps";
    }
    std::vector<std::filesystem::path> dirs;
    std::stringstream in(list);
    std::string part;
    while (std::getline(in, part, ':')) {
      if (part.empty())
        continue;
      if (part[0] == '~')
        part = h + part.substr(1);
      dirs.emplace_back(part);
    }
    return dirs;
  }

  std::filesystem::path find(const std::string & theme, const std::vector<std::string> & names,
                             const std::vector<std::filesystem::path> & dirs) {
    std::vector<std::string> queue{ theme.empty() ? std::string("default") : theme };
    std::set<std::string> seen;
    std::error_code ec;
    // breadth first, as libXcursor: the theme, then what it inherits
    for (size_t q = 0; q < queue.size() && q < 32; q++) {
      const std::string t = queue[q];
      if (t.empty() || t.find('/') != std::string::npos || t == ".." || !seen.insert(t).second)
        continue;
      for (const std::string & name : names)
        for (const auto & dir : dirs) {
          const std::filesystem::path f = dir / t / "cursors" / name;
          if (std::filesystem::is_regular_file(f, ec))
            return f;
        }
      for (const auto & dir : dirs) {
        // "Inherits=a,b" (some themes separate with ';')
        std::string list = iniValue(dir / t / "index.theme", "Inherits");
        std::replace(list.begin(), list.end(), ';', ',');
        std::stringstream inherits(list);
        std::string parent;
        while (std::getline(inherits, parent, ','))
          queue.push_back(trim(parent));
      }
    }
    return {};
  }
}
