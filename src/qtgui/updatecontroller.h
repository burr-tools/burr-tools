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
#ifndef BTQT_UPDATECONTROLLER_H
#define BTQT_UPDATECONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <memory>

class SettingsController;

/* Help ▸ Check for updates, and the same check at launch (at most daily,
 * Settings ▸ General ▸ Check for updates at launch): legacy's update check
 * (#137, design/2026-10-04-update-check-design.md) for this GUI. The rules
 * -- when to ask, what a reply means -- are the shared src/tools/updatecheck;
 * the HTTPS request is the platform's (src/gui/httpget: WinHTTP,
 * NSURLSession, libcurl), made on a worker thread and polled from this one,
 * so a reply that comes after the window has gone reaches nothing.
 */
class UpdateController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  /* a check is waiting for GitHub's answer */
  Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged FINAL)

public:

  explicit UpdateController(SettingsController * settings, QObject * parent = nullptr);
  ~UpdateController() override;

  bool checking(void) const { return m_inFlight != nullptr; }

  /* Help ▸ Check for updates: always asks, and always says what it found */
  Q_INVOKABLE void checkNow(void);
  /* at launch: asks only when the setting is on, the build is a release,
   * and the last answer is a day old; says something only for a newer,
   * not skipped release */
  void checkAtLaunch(void);

  /* the offer's buttons */
  Q_INVOKABLE void openPage(const QString & url);
  Q_INVOKABLE void skipOffered(void);

  /* the installed version as the check sees it: the build's, or
   * BURRTOOLS_UPDATE_VERSION_OVERRIDE (testing) */
  static QString installedVersion(void);

signals:

  void checkingChanged(void);
  /* a release to offer (or, for a build whose version cannot be compared,
   * the releases page): what the update dialog shows */
  void offer(const QString & heading, const QString & notes, const QString & url, bool canSkip);
  /* up to date, or why the check failed (manual checks only) */
  void message(const QString & title, const QString & text);

private:

  struct Shared;

  void start(bool manual);
  void poll(void);

  SettingsController * m_settings;
  std::shared_ptr<Shared> m_inFlight;
  bool m_manual = false;
  int m_offered = 0;              ///< the offered release, packed (skipOffered)
  QTimer m_poll;
};

#endif
