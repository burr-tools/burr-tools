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
#include "updatecontroller.h"
#include "settingscontroller.h"

#include "../tools/httpget.h"
#include "../tools/updatecheck.h"

#include <QDesktopServices>
#include <QUrl>

#include <atomic>
#include <ctime>
#include <exception>
#include <thread>
#include <variant>

#ifndef BURRTOOLS_VERSION
#define BURRTOOLS_VERSION "unknown"     // meson defines it for the library
#endif

using namespace updatecheck;

namespace {

  constexpr int timeoutSeconds = 10;

  std::optional<Version> installed(void) {
    return parseVersion(UpdateController::installedVersion().toStdString());
  }

  updatecheck::Settings settingsOf(const SettingsController & s) {
    updatecheck::Settings out;
    out.autoCheckEnabled = s.checkForUpdates();
    out.lastCheck = s.updateLastCheck();
    out.skipped = unpackVersion(s.updateSkippedVersion());
    return out;
  }

  QString failure(const HttpResult & r) {
    switch (r.kind) {
      case HttpResult::Kind::Transport:
        return UpdateController::tr("Couldn't reach GitHub: %1").arg(QString::fromStdString(r.error));
      case HttpResult::Kind::Status:
        return r.status == 403 || r.status == 429
               ? UpdateController::tr("GitHub returned HTTP %1 (rate limited - try again later).").arg(r.status)
               : UpdateController::tr("GitHub returned HTTP %1.").arg(r.status);
      case HttpResult::Kind::TooLarge:
        return UpdateController::tr("Unexpected response from GitHub.");
      case HttpResult::Kind::Unsupported:
        return UpdateController::tr("Update checking isn't available: %1.").arg(QString::fromStdString(r.error));
      case HttpResult::Kind::Ok:
        break;
    }
    return {};
  }
}

/* What the worker hands back: written by it, read here once `done` is set. */
struct UpdateController::Shared {
  std::atomic<bool> done{ false };
  HttpResult http;
  std::variant<Release, std::string> parsed{ std::string("not fetched") };
};

UpdateController::UpdateController(SettingsController * settings, QObject * parent) :
  QObject(parent), m_settings(settings)
{
  m_poll.setInterval(250);
  connect(&m_poll, &QTimer::timeout, this, &UpdateController::poll);
}

// a worker still out keeps its own Shared; it touches nothing of this one
UpdateController::~UpdateController() = default;

QString UpdateController::installedVersion(void) {
  const QByteArray o = qgetenv("BURRTOOLS_UPDATE_VERSION_OVERRIDE");
  return o.isEmpty() ? QStringLiteral(BURRTOOLS_VERSION) : QString::fromUtf8(o);
}

void UpdateController::checkNow(void) {
  start(true);
}

void UpdateController::checkAtLaunch(void) {
  start(false);
}

void UpdateController::start(bool manual) {
  if (m_inFlight) {
    // a manual check during a launch check takes over its answer
    m_manual = m_manual || manual;
    return;
  }
  const Mode mode = manual ? Mode::Manual : Mode::Auto;
  switch (gate(mode, installed(), settingsOf(*m_settings), std::int64_t(std::time(nullptr)))) {
    case Gate::Skip:
      return;
    case Gate::UnknownVersion:
      // no release notes: the releases list itself, said in the heading
      emit offer(tr("This build's version (%1) can't be compared with published releases. "
                    "Open the releases page to look for yourself?").arg(installedVersion()),
                 QString(), QString::fromLatin1(RELEASES_PAGE), false);
      return;
    case Gate::Fetch:
      break;
  }

  auto shared = std::make_shared<Shared>();
  m_inFlight = shared;
  m_manual = manual;
  const std::string agent = std::string("BurrTools/") + BURRTOOLS_VERSION;
  std::thread([shared, agent, fetch = m_fetch] {
    try {
      shared->http = fetch ? fetch() : httpGet(LATEST_RELEASE_API, agent, timeoutSeconds);
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
  m_poll.start();
  emit checkingChanged();
}

void UpdateController::poll(void) {
  if (!m_inFlight || !m_inFlight->done.load(std::memory_order_acquire))
    return;
  m_poll.stop();
  // the slot is free before anything is shown: a dialog may start another check
  const std::shared_ptr<Shared> result = std::move(m_inFlight);
  const bool manual = m_manual;
  emit checkingChanged();

  if (result->http.kind != HttpResult::Kind::Ok) {
    if (manual)
      emit message(tr("Check for updates"), failure(result->http));
    return;
  }
  if (const auto * err = std::get_if<std::string>(&result->parsed)) {
    if (manual)
      emit message(tr("Check for updates"),
                   tr("Unexpected response from GitHub:\n%1").arg(QString::fromStdString(*err)));
    return;
  }

  m_settings->setUpdateLastCheck(std::int64_t(std::time(nullptr)));
  const Release & rel = std::get<Release>(result->parsed);
  const std::optional<Version> mine = installed();
  if (!mine)
    return;                        // gate() refused unknown versions already
  switch (evaluate(manual ? Mode::Manual : Mode::Auto, *mine, rel, settingsOf(*m_settings))) {
    case Outcome::UpToDate:
      if (manual)
        emit message(tr("Check for updates"),
                     tr("You're running the latest version of BurrTools (%1).")
                       .arg(QString::fromStdString(toString(*mine))));
      return;
    case Outcome::SkippedByUser:
      return;
    case Outcome::UpdateAvailable: {
      const QString title = rel.name.empty() ? tr("BurrTools %1").arg(QString::fromStdString(toString(rel.version)))
                                             : QString::fromStdString(rel.name);
      m_offered = packVersion(rel.version);
      emit offer(tr("%1 is available. You have %2.").arg(title, QString::fromStdString(toString(*mine))),
                 QString::fromStdString(rel.body), QString::fromStdString(rel.htmlUrl), !manual);
      return;
    }
  }
}

void UpdateController::openPage(const QString & url) {
  // only the release pages parseLatestRelease() vetted, or the releases list
  if (url.startsWith(QLatin1String(RELEASES_URL_PREFIX)) || url == QLatin1String(RELEASES_PAGE))
    QDesktopServices::openUrl(QUrl(url));
}

void UpdateController::skipOffered(void) {
  if (m_offered > 0)
    m_settings->setUpdateSkippedVersion(m_offered);
}
