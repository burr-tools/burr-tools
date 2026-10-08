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
#include "settingsstore.h"

#include <charconv>
#include <fstream>

namespace btui {

  namespace {

    std::string_view trim(std::string_view s) {
      const char * ws = " \t\r\n";
      size_t b = s.find_first_not_of(ws);
      if (b == std::string_view::npos)
        return {};
      size_t e = s.find_last_not_of(ws);
      return s.substr(b, e - b + 1);
    }

    /* A quoted value: the legacy writer never escapes anything (it printf's
     * the string between quotes), so a backslash is taken literally unless it
     * precedes a quote or another backslash -- the only two escapes save()
     * writes. Returns nothing when the closing quote is missing.
     */
    std::optional<std::string> unquote(std::string_view s) {
      if (s.size() < 2 || s.front() != '"')
        return std::nullopt;
      std::string out;
      for (size_t i = 1; i < s.size(); i++) {
        char c = s[i];
        if (c == '\\' && i + 1 < s.size() && (s[i+1] == '"' || s[i+1] == '\\')) {
          out += s[++i];
        } else if (c == '"') {
          return (i == s.size() - 1) ? std::optional<std::string>(out) : std::nullopt;
        } else {
          out += c;
        }
      }
      return std::nullopt;
    }

    std::optional<SettingsStore::Value> parseValue(std::string_view v) {
      if (v == "true") return SettingsStore::Value(true);
      if (v == "false") return SettingsStore::Value(false);
      if (!v.empty() && v.front() == '"') {
        auto s = unquote(v);
        if (!s) return std::nullopt;
        return SettingsStore::Value(std::move(*s));
      }
      long long n = 0;
      auto [p, ec] = std::from_chars(v.data(), v.data() + v.size(), n);
      if (ec == std::errc() && p == v.data() + v.size())
        return SettingsStore::Value(n);
      return std::nullopt;
    }

    std::string quote(const std::string & s) {
      std::string out = "\"";
      for (char c : s) {
        if (c == '"' || c == '\\')
          out += '\\';
        out += c;
      }
      out += '"';
      return out;
    }
  }

  SettingsStore::SettingsStore(std::filesystem::path file) : path(std::move(file)) {}

  SettingsStore::Values SettingsStore::parseFile(const std::filesystem::path & file) {
    Values out;
    std::ifstream in(file);
    if (!in)
      return out;

    std::string line;
    while (std::getline(in, line)) {
      std::string_view l = trim(line);
      if (l.empty() || l.starts_with("--") || l.starts_with('#'))
        continue;
      size_t eq = l.find('=');
      if (eq == std::string_view::npos)
        continue;
      std::string_view key = trim(l.substr(0, eq));
      std::string_view val = trim(l.substr(eq + 1));
      if (key.empty())
        continue;
      if (auto v = parseValue(val))
        out.insert_or_assign(std::string(key), std::move(*v));
    }
    return out;
  }

  bool SettingsStore::load(void) {
    vals.clear();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
      return false;
    vals = parseFile(path);
    return true;
  }

  /* Written beside the file and renamed over it, so a crash or a full disk
   * part way leaves the settings as they were, never half a file. */
  bool SettingsStore::save(void) const {
    std::filesystem::path tmp = path;
    tmp += ".tmp";
    std::error_code ec;
    {
      std::ofstream out(tmp, std::ios::trunc);
      if (!out)
        return false;
      for (const auto & [k, v] : vals) {
        out << k << " = ";
        if (auto b = std::get_if<bool>(&v))
          out << (*b ? "true" : "false");
        else if (auto n = std::get_if<long long>(&v))
          out << *n;
        else
          out << quote(std::get<std::string>(v));
        out << "\n";
      }
      out.flush();
      if (!out) {
        out.close();
        std::filesystem::remove(tmp, ec);
        return false;
      }
    }
    std::filesystem::rename(tmp, path, ec);       // replaces the old file
    if (ec) {
      std::filesystem::remove(tmp, ec);
      return false;
    }
    return true;
  }

  bool SettingsStore::contains(std::string_view key) const {
    return vals.find(key) != vals.end();
  }

  void SettingsStore::remove(std::string_view key) {
    auto it = vals.find(key);
    if (it != vals.end())
      vals.erase(it);
  }

  std::optional<bool> SettingsStore::getBool(std::string_view key) const {
    auto it = vals.find(key);
    if (it == vals.end()) return std::nullopt;
    if (auto b = std::get_if<bool>(&it->second)) return *b;
    return std::nullopt;
  }

  std::optional<long long> SettingsStore::getInt(std::string_view key) const {
    auto it = vals.find(key);
    if (it == vals.end()) return std::nullopt;
    if (auto n = std::get_if<long long>(&it->second)) return *n;
    return std::nullopt;
  }

  std::optional<std::string> SettingsStore::getString(std::string_view key) const {
    auto it = vals.find(key);
    if (it == vals.end()) return std::nullopt;
    if (auto s = std::get_if<std::string>(&it->second)) return *s;
    return std::nullopt;
  }

  std::string SettingsStore::getString(std::string_view key, std::string_view def) const {
    auto s = getString(key);
    return s ? *s : std::string(def);
  }

  void SettingsStore::set(std::string_view key, Value v) {
    vals.insert_or_assign(std::string(key), std::move(v));
  }
}
