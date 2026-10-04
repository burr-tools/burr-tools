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
#include "configuration.h"
#include "updatewindow.h"

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>
#include <FL/filename.H>

#include <atomic>
#include <ctime>
#include <thread>

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

struct updateChecker_c::Shared {
  std::atomic<bool> done{false};
  HttpResult http;
  std::variant<Release, std::string> parsed{std::string("not fetched")};
};

static Settings currentSettings(void) {
  Settings s;
  s.autoCheckEnabled = config.checkForUpdates();
  s.lastCheck = std::int64_t(config.updateLastCheckMinutes()) * 60;
  s.skipped = unpackVersion(config.updateSkippedVersion());
  return s;
}

updateChecker_c::updateChecker_c(Fl_Window * p) : parent(p), inFlightMode(Mode::Auto) {}

updateChecker_c::~updateChecker_c() {
  Fl::remove_timeout(pollCb, this);
}

void updateChecker_c::start(Mode mode) {

  if (inFlight) {
    if (mode == Mode::Manual) {
      inFlightMode = Mode::Manual;
      parent->cursor(FL_CURSOR_WAIT);
    }
    return;
  }

  std::optional<Version> installed = updatechecker::installedVersion();

  switch (gate(mode, installed, currentSettings(), std::int64_t(time(nullptr)))) {

    case Gate::Skip:
      return;

    case Gate::UnknownVersion: {
      std::string v = updatechecker::installedVersionString();
      if (fl_choice("This build's version (%s) can't be compared with published releases.\n"
                    "Open the releases page to look for yourself?",
                    "Cancel", "Open Releases Page", nullptr, v.c_str()) == 1) {
        char msg[512];
        if (!fl_open_uri(RELEASES_PAGE, msg, sizeof(msg)))
          fl_alert("Couldn't open the browser: %s", msg);
      }
      return;
    }

    case Gate::Fetch:
      break;
  }

  auto shared = std::make_shared<Shared>();
  inFlight = shared;
  inFlightMode = mode;
  if (mode == Mode::Manual)
    parent->cursor(FL_CURSOR_WAIT);

  std::string ua = updatechecker::userAgent();
  std::thread([shared, ua] {
    try {
      shared->http = httpGet(LATEST_RELEASE_API, ua, TIMEOUT_SECONDS);
      if (shared->http.kind == HttpResult::Kind::Ok)
        shared->parsed = parseLatestRelease(shared->http.body);
    } catch (const std::exception & e) {
      shared->http.kind = HttpResult::Kind::Transport;
      shared->http.error = e.what();
    } catch (...) {
      shared->http.kind = HttpResult::Kind::Transport;
      shared->http.error = "unexpected error";
    }
    shared->done.store(true, std::memory_order_release);
  }).detach();

  Fl::add_timeout(0.25, pollCb, this);
}

void updateChecker_c::pollCb(void * v) {
  static_cast<updateChecker_c *>(v)->poll();
}

void updateChecker_c::poll(void) {
  if (!inFlight->done.load(std::memory_order_acquire)) {
    Fl::repeat_timeout(0.25, pollCb, this);
    return;
  }

  /* Release the in-flight slot before showing any dialog: the dialogs run
   * a nested event loop, during which a new check may be started.
   */
  std::shared_ptr<Shared> result;
  result.swap(inFlight);
  Mode mode = inFlightMode;
  if (mode == Mode::Manual)
    parent->cursor(FL_CURSOR_DEFAULT);

  finish(*result, mode);
}

void updateChecker_c::finish(const Shared & result, Mode mode) {

  const bool manual = mode == Mode::Manual;

  if (result.http.kind != HttpResult::Kind::Ok) {
    if (manual)
      fl_alert("%s", updatechecker::describeFailure(result.http).c_str());
    return;
  }

  if (auto err = std::get_if<std::string>(&result.parsed)) {
    if (manual)
      fl_alert("Unexpected response from GitHub:\n%s", err->c_str());
    return;
  }

  config.updateLastCheckMinutes(int(time(nullptr) / 60));
  config.save();

  const Release & rel = std::get<Release>(result.parsed);
  std::optional<Version> installed = updatechecker::installedVersion();
  if (!installed) return;   // gate() already refused unknown versions

  switch (evaluate(mode, *installed, rel, currentSettings())) {
    case Outcome::UpToDate:
      if (manual)
        fl_message("You're running the latest version of BurrTools (%s).",
                   toString(*installed).c_str());
      break;
    case Outcome::SkippedByUser:
      break;
    case Outcome::UpdateAvailable:
      offer(rel, *installed, mode);
      break;
  }
}

void updateChecker_c::offer(const Release & rel, const Version & installed, Mode mode) {

  std::string title = rel.name.empty() ? "BurrTools " + toString(rel.version) : rel.name;
  std::string heading = title + " is available. You have " + toString(installed) + ".";

  updateWindow_c win(heading, rel.body, mode == Mode::Auto);

  switch (win.run()) {
    case updateWindow_c::Choice::OpenPage: {
      char msg[512];
      if (!fl_open_uri(rel.htmlUrl.c_str(), msg, sizeof(msg)))
        fl_alert("Couldn't open the browser: %s", msg);
      break;
    }
    case updateWindow_c::Choice::Skip:
      config.updateSkippedVersion(packVersion(rel.version));
      config.save();
      break;
    case updateWindow_c::Choice::Later:
      break;
  }
}
