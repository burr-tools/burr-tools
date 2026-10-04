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
#include "updatechecker.h"
#include "httpget.h"

#include "version.h"

#include <cstdio>
#include <cstdlib>
#include <variant>

using namespace updatecheck;

static constexpr int TIMEOUT_SECONDS = 10;

namespace updatechecker {

  std::string userAgent(void) {
    return std::string("BurrTools/") + BURRTOOLS_VERSION;
  }

  std::string installedVersionString(void) {
    const char * o = std::getenv("BURRTOOLS_UPDATE_VERSION_OVERRIDE");
    return (o && *o) ? std::string(o) : std::string(BURRTOOLS_VERSION);
  }

  std::optional<Version> installedVersion(void) {
    return parseVersion(installedVersionString());
  }

  std::string describeFailure(const HttpResult & r) {
    switch (r.kind) {
      case HttpResult::Kind::Transport:
        return "Couldn't reach GitHub: " + r.error;
      case HttpResult::Kind::Status:
        if (r.status == 403 || r.status == 429)
          return "GitHub returned HTTP " + std::to_string(r.status) +
                 " (rate limited - try again later).";
        return "GitHub returned HTTP " + std::to_string(r.status) + ".";
      case HttpResult::Kind::TooLarge:
        return "Unexpected response from GitHub.";
      case HttpResult::Kind::Unsupported:
        return "Update checking isn't supported in this build.";
      case HttpResult::Kind::Ok:
        break;
    }
    return "";
  }

  int runCli(void) {
    std::string installedStr = installedVersionString();
    std::optional<Version> installed = installedVersion();
    printf("installed: %s\n", installedStr.c_str());

    HttpResult http = httpGet(LATEST_RELEASE_API, userAgent(), TIMEOUT_SECONDS);
    if (http.kind != HttpResult::Kind::Ok) {
      printf("error: %s\n", describeFailure(http).c_str());
      return 1;
    }

    auto parsed = parseLatestRelease(http.body);
    if (auto err = std::get_if<std::string>(&parsed)) {
      printf("error: unexpected response from GitHub: %s\n", err->c_str());
      return 1;
    }
    const Release & rel = std::get<Release>(parsed);
    printf("latest: %s (%s)\n", rel.tag.c_str(), rel.htmlUrl.c_str());

    if (!installed) {
      printf("result: installed version cannot be compared\n");
      return 0;
    }

    switch (evaluate(Mode::Manual, *installed, rel, Settings())) {
      case Outcome::UpdateAvailable: printf("result: update available\n"); break;
      case Outcome::UpToDate:        printf("result: up to date\n"); break;
      case Outcome::SkippedByUser:   printf("result: skipped\n"); break;
    }
    return 0;
  }
}
