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
#include "updatecheck.h"

#include <nlohmann/json.hpp>

namespace updatecheck {

  namespace {

    bool isDigit(char c) { return c >= '0' && c <= '9'; }

    bool isHex(char c) {
      return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    /* Consumes one decimal component at pos. Fails on no digits or a value
     * above MAX_COMPONENT; the overflow check runs per digit, so a long
     * run of digits cannot wrap.
     */
    bool component(std::string_view s, size_t & pos, unsigned & out) {
      size_t start = pos;
      unsigned val = 0;
      while (pos < s.size() && isDigit(s[pos])) {
        val = val * 10 + unsigned(s[pos] - '0');
        if (val > MAX_COMPONENT) return false;
        pos++;
      }
      out = val;
      return pos > start;
    }

    /* The git describe suffix: "-N-gHASH", then optionally "-dirty"; or
     * just "-dirty".
     */
    bool devSuffix(std::string_view rest) {
      if (rest == "-dirty") return true;

      size_t pos = 0;
      if (pos >= rest.size() || rest[pos] != '-') return false;
      pos++;
      size_t start = pos;
      while (pos < rest.size() && isDigit(rest[pos])) pos++;
      if (pos == start) return false;

      if (rest.substr(pos, 2) != "-g") return false;
      pos += 2;
      start = pos;
      while (pos < rest.size() && isHex(rest[pos])) pos++;
      if (pos == start) return false;

      rest.remove_prefix(pos);
      return rest.empty() || rest == "-dirty";
    }
  }

  std::optional<Version> parseVersion(std::string_view s) {
    if (!s.empty() && s[0] == 'v') s.remove_prefix(1);

    Version v;
    size_t pos = 0;
    if (!component(s, pos, v.vMajor)) return std::nullopt;
    if (pos >= s.size() || s[pos] != '.') return std::nullopt;
    pos++;
    if (!component(s, pos, v.vMinor)) return std::nullopt;
    if (pos >= s.size() || s[pos] != '.') return std::nullopt;
    pos++;
    if (!component(s, pos, v.vPatch)) return std::nullopt;

    std::string_view rest = s.substr(pos);
    if (rest.empty()) return v;
    if (!devSuffix(rest)) return std::nullopt;
    v.isDev = true;
    return v;
  }

  int compareVersions(const Version & a, const Version & b) {
    if (a.vMajor != b.vMajor) return a.vMajor < b.vMajor ? -1 : 1;
    if (a.vMinor != b.vMinor) return a.vMinor < b.vMinor ? -1 : 1;
    if (a.vPatch != b.vPatch) return a.vPatch < b.vPatch ? -1 : 1;
    return 0;
  }

  std::string toString(const Version & v) {
    return std::to_string(v.vMajor) + "." + std::to_string(v.vMinor) + "." +
           std::to_string(v.vPatch);
  }

  int packVersion(const Version & v) {
    return int(v.vMajor * 1000000u + v.vMinor * 1000u + v.vPatch);
  }

  std::optional<Version> unpackVersion(int packed) {
    if (packed <= 0 || packed > 999999999) return std::nullopt;
    Version v;
    v.vMajor = unsigned(packed / 1000000);
    v.vMinor = unsigned(packed / 1000 % 1000);
    v.vPatch = unsigned(packed % 1000);
    return v;
  }

  namespace {

    bool flagSet(const nlohmann::json & j, const char * key) {
      auto it = j.find(key);
      return it != j.end() && it->is_boolean() && it->get<bool>();
    }

    const std::string * stringField(const nlohmann::json & j, const char * key) {
      auto it = j.find(key);
      if (it == j.end() || !it->is_string()) return nullptr;
      return it->get_ptr<const std::string *>();
    }

    bool safeUrl(const std::string & url) {
      if (url.rfind(RELEASES_URL_PREFIX, 0) != 0) return false;
      for (unsigned char c : url)
        if (c <= ' ' || c == 0x7f || c == '"' || c == '\'' || c == '<' ||
            c == '>' || c == '\\' || c == '`')
          return false;
      return true;
    }

    std::string foldCrlf(const std::string & s) {
      std::string out;
      out.reserve(s.size());
      for (size_t i = 0; i < s.size(); i++)
        if (!(s[i] == '\r' && i + 1 < s.size() && s[i + 1] == '\n'))
          out += s[i];
      return out;
    }
  }

  std::variant<Release, std::string> parseLatestRelease(std::string_view json) {
    nlohmann::json j = nlohmann::json::parse(json.begin(), json.end(), nullptr, false);
    if (j.is_discarded() || !j.is_object())
      return std::string("the response is not a JSON object");

    if (flagSet(j, "draft") || flagSet(j, "prerelease"))
      return std::string("the latest release is a draft or pre-release");

    const std::string * tag = stringField(j, "tag_name");
    if (!tag) return std::string("the response has no tag_name");

    std::optional<Version> v = parseVersion(*tag);
    if (!v || v->isDev)
      return std::string("the release tag \"" + *tag + "\" is not a version number");

    const std::string * url = stringField(j, "html_url");
    if (!url || !safeUrl(*url))
      return std::string("the release page address is missing or not on GitHub");

    Release r;
    r.tag = *tag;
    r.version = *v;
    r.htmlUrl = *url;
    if (const std::string * name = stringField(j, "name")) r.name = *name;

    auto body = j.find("body");
    if (body != j.end() && !body->is_null()) {
      if (!body->is_string()) return std::string("the release notes are not text");
      r.body = foldCrlf(body->get<std::string>());
    }
    return r;
  }

  Gate gate(Mode mode, const std::optional<Version> & installed,
            const Settings & settings, std::int64_t now) {
    if (mode == Mode::Manual)
      return installed ? Gate::Fetch : Gate::UnknownVersion;

    if (!settings.autoCheckEnabled || !installed || installed->isDev)
      return Gate::Skip;

    std::int64_t elapsed = now - settings.lastCheck;
    if (settings.lastCheck > 0 && elapsed >= 0 && elapsed < CHECK_INTERVAL_SECONDS)
      return Gate::Skip;

    return Gate::Fetch;
  }

  Outcome evaluate(Mode mode, const Version & installed, const Release & release,
                   const Settings & settings) {
    if (compareVersions(release.version, installed) <= 0)
      return Outcome::UpToDate;

    if (mode == Mode::Auto && settings.skipped &&
        compareVersions(*settings.skipped, release.version) == 0)
      return Outcome::SkippedByUser;

    return Outcome::UpdateAvailable;
  }
}
