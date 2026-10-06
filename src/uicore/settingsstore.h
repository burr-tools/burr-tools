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
#ifndef BTUI_SETTINGSSTORE_H
#define BTUI_SETTINGSSTORE_H

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

/* The redesigned GUI's settings file.
 *
 * burrtools-qt keeps its settings in a file of its own (.burrtools-qt.rc,
 * next to the legacy .burrtools.rc) and never writes the legacy one. The
 * legacy GUI rewrites its file on exit with only the keys it knows, so a
 * shared file would lose every key the new GUI added each time the old GUI
 * ran; a separate file sidesteps that without touching the legacy code.
 *
 * The format is the legacy one -- one "name = value" per line, value a bare
 * true/false, a bare integer, or a double-quoted string -- so parseFile() can
 * also read the legacy file, which is how the new GUI takes over the user's
 * existing choices on its first start. Keys are free-form; the new GUI uses
 * dotted names ("view.lighting").
 *
 * This class only stores values. What a key means, its default and its legal
 * range belong to the caller.
 */
namespace btui {

  class SettingsStore {

    public:

      using Value = std::variant<bool, long long, std::string>;
      using Values = std::map<std::string, Value, std::less<>>;

      explicit SettingsStore(std::filesystem::path file);

      const std::filesystem::path & file(void) const { return path; }

      /* Read the file, replacing whatever is held. Returns false (leaving the
       * store empty) when the file does not exist or cannot be read.
       */
      bool load(void);

      /* Write every value. Returns false if the file could not be written. */
      bool save(void) const;

      /* Parse a settings file without loading it into a store. Unreadable
       * files give an empty map; lines that do not parse are skipped, so a
       * hand-edited or newer file never stops the program.
       */
      static Values parseFile(const std::filesystem::path & file);

      bool contains(std::string_view key) const;
      void remove(std::string_view key);
      const Values & values(void) const { return vals; }

      /* Typed reads. A key holding a value of another type reads as absent:
       * the caller falls back to its default rather than guessing.
       */
      std::optional<bool> getBool(std::string_view key) const;
      std::optional<long long> getInt(std::string_view key) const;
      std::optional<std::string> getString(std::string_view key) const;

      bool getBool(std::string_view key, bool def) const { return getBool(key).value_or(def); }
      long long getInt(std::string_view key, long long def) const { return getInt(key).value_or(def); }
      std::string getString(std::string_view key, std::string_view def) const;

      void set(std::string_view key, Value v);

    private:

      std::filesystem::path path;
      Values vals;
  };
}

#endif
