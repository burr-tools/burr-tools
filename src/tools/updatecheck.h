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
#ifndef __UPDATECHECK_H__
#define __UPDATECHECK_H__

#include <optional>
#include <string>
#include <string_view>
#include <variant>

/* The decision logic of the update check: version strings, the GitHub
 * release response, and whether to check and what to tell the user. Pure
 * functions only -- no network, no GUI, no clock -- so all of it is unit
 * tested; src/gui/updatechecker.cpp supplies the I/O.
 */
namespace updatecheck {

  /* Field names avoid major/minor, which older glibc defines as macros. */
  struct Version {
    unsigned vMajor = 0;
    unsigned vMinor = 0;
    unsigned vPatch = 0;
    bool isDev = false;   ///< commits past the tag, or a dirty worktree
  };

  /* Components are capped so packVersion() fits a 32-bit int. */
  inline constexpr unsigned MAX_COMPONENT = 999;

  /* Accepts "X.Y.Z" with an optional leading "v", optionally followed by
   * git describe's "-N-gHASH" and/or "-dirty" (either makes it a dev
   * version). Anything else -- "temp-64-bit", "0.7.0-unknown", a bare
   * hash -- has no comparable version and yields nullopt.
   */
  std::optional<Version> parseVersion(std::string_view s);

  /* <0, 0, >0 by (major, minor, patch). isDev does not participate: a dev
   * build is compared as the release it was built from.
   */
  int compareVersions(const Version & a, const Version & b);

  /* "X.Y.Z" */
  std::string toString(const Version & v);

  /* A version as one int, for the configuration file, which can only store
   * bools and numbers. 0 is reserved for "none"; 0.0.0 is not a release.
   */
  int packVersion(const Version & v);
  std::optional<Version> unpackVersion(int packed);

  inline constexpr const char * LATEST_RELEASE_API =
      "https://api.github.com/repos/burr-tools/burr-tools/releases/latest";

  /* Every URL the update check opens in a browser starts with this. */
  inline constexpr const char * RELEASES_URL_PREFIX =
      "https://github.com/burr-tools/burr-tools/";

  inline constexpr const char * RELEASES_PAGE =
      "https://github.com/burr-tools/burr-tools/releases";

  struct Release {
    std::string tag;       ///< "v0.7.2"
    Version version;       ///< parsed from tag; never isDev
    std::string name;      ///< "BurrTools 0.7.2"; may be empty
    std::string htmlUrl;   ///< validated against RELEASES_URL_PREFIX
    std::string body;      ///< release notes, LF line endings; may be empty
  };

  /* Parses a GitHub "get the latest release" response. On failure the
   * string alternative holds a short, user-presentable reason. html_url
   * is checked here because it is handed to the OS to open: it must start
   * with RELEASES_URL_PREFIX and contain no whitespace, quotes or control
   * characters.
   */
  std::variant<Release, std::string> parseLatestRelease(std::string_view json);
}

#endif
