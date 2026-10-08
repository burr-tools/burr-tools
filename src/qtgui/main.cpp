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

/* burrtools-qt: the redesigned BurrTools GUI (Qt 6 Quick). */

#include "app.h"
#include "pipelinecache.h"
#include "settingscontroller.h"
#include "documentcontroller.h"
#include "iconprovider.h"
#include "selfcheck.h"
#include "theme.h"

#include "../lib/bt_assert.h"

#ifdef QT_QML_DEBUG
#include <QtQml/qqmldebug.h>      // its enabler lets qmlprofiler attach (-Dqml_debug=true)
#endif
#include <QFileOpenEvent>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QTranslator>

#include <cstdio>
#include <cstring>
#include <memory>

namespace {

  /* Exceptions must not unwind through Qt's event dispatch. An internal error
   * raised in C++ code Qt calls (a timer, an event filter) is caught here and
   * handed to the same rescue path the QML entry points use.
   *
   * It also receives the macOS "open this document" events (a Finder
   * double-click, a drop on the Dock icon), which go to the application
   * object.
   */
  class BurrToolsApplication : public QGuiApplication {
  public:
    using QGuiApplication::QGuiApplication;

    bool notify(QObject * receiver, QEvent * event) override {
      try {
        return QGuiApplication::notify(receiver, event);
      } catch (const assert_exception & e) {
        if (App * app = App::instance())
          app->handleInternalError(e);
        return true;
      }
    }

  protected:
    bool event(QEvent * e) override {
      if (e->type() == QEvent::FileOpen) {
        if (App * app = App::instance())
          app->document()->requestOpenPath(static_cast<QFileOpenEvent *>(e)->file());
        return true;
      }
      return QGuiApplication::event(e);
    }
  };

#ifdef Q_OS_MACOS
  /* File > Settings… moves into the application menu (Qt matches its text),
   * where Qt names it "Preferences...". macOS since 13, and the legacy app,
   * say Settings…; the name is Qt's translatable string, renamed here.
   */
  class MacMenuNames : public QTranslator {
  public:
    QString translate(const char * context, const char * source, const char *, int) const override {
      if (qstrcmp(context, "MAC_APPLICATION_MENU") == 0 && qstrcmp(source, "Preferences...") == 0)
        return QStringLiteral("Settings…");
      return QString();
    }
    bool isEmpty(void) const override { return false; }
  };
#endif

  /* BURRTOOLS_RHI=d3d11|d3d12|vulkan|opengl|metal picks the 3D graphics API,
   * for A/B comparisons from one build (the repository's env-toggle
   * convention); unset, Qt picks the platform default (Direct3D 11 on
   * Windows, Metal on macOS, OpenGL or Vulkan on Linux).
   */
  void applyGraphicsApiOverride(void) {
    const QByteArray v = qgetenv("BURRTOOLS_RHI").toLower();
    if (v.isEmpty())
      return;
    struct { const char * name; QSGRendererInterface::GraphicsApi api; } table[] = {
      { "d3d11", QSGRendererInterface::Direct3D11 },
      { "d3d12", QSGRendererInterface::Direct3D12 },
      { "vulkan", QSGRendererInterface::Vulkan },
      { "opengl", QSGRendererInterface::OpenGL },
      { "metal", QSGRendererInterface::Metal },
    };
    for (const auto & t : table)
      if (v == t.name) {
        QQuickWindow::setGraphicsApi(t.api);
        return;
      }
    fprintf(stderr, "BURRTOOLS_RHI=%s is not one of d3d11, d3d12, vulkan, opengl, metal; ignored\n", v.constData());
  }
}

int main(int argc, char ** argv) {

  bt_assert_init();

  // a headless invariant check for CI, before any window or QML exists
  if (argc == 2 && strcmp(argv[1], "--self-check") == 0) {
    QString problems = selfCheck();
    if (!problems.isEmpty()) {
      fprintf(stderr, "self-check FAILED:\n%s\n", qPrintable(problems));
      return 1;
    }
    printf("self-check OK\n");
    return 0;
  }

  // fractional scales (125 %, 150 %) render at their real size, matching the
  // spec's dp model (implementation guide section 4)
  QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
  applyGraphicsApiOverride();
  /* A scripted run on a scratch settings file keeps out of the user's
   * caches: ours follow the settings file (PipelineCache), and Qt Quick's
   * own automatic one -- in the user's cache folder, used by windows other
   * than the main one -- is switched off. */
  if (!qEnvironmentVariableIsEmpty("BURRTOOLS_QT_SETTINGS") && !qEnvironmentVariableIsSet("QSG_RHI_DISABLE_DISK_CACHE"))
    qputenv("QSG_RHI_DISABLE_DISK_CACHE", "1");

  BurrToolsApplication qapp(argc, argv);
  QGuiApplication::setApplicationName(QStringLiteral("BurrTools"));
  QGuiApplication::setOrganizationName(QStringLiteral("BurrTools"));
  QGuiApplication::setApplicationDisplayName(QStringLiteral("BurrTools"));
#ifdef Q_OS_MACOS
  MacMenuNames macMenuNames;
  QCoreApplication::installTranslator(&macMenuNames);
#endif

  // the Basic style is the one the spec re-skins completely
  QQuickStyle::setStyle(QStringLiteral("Basic"));

  /* BURRTOOLS_QT_SETTINGS names another settings file, so scripted runs
   * (screenshots, CI) never touch the user's own. */
  App app(QString::fromLocal8Bit(qgetenv("BURRTOOLS_QT_SETTINGS")), QString());

  QQmlApplicationEngine engine;
  engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
  QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &qapp,
                   [] { QCoreApplication::exit(2); }, Qt::QueuedConnection);
  // --gallery: the component gallery (every primitive in every state)
  // instead of the main window, for checking the look by eye
  const bool gallery = QCoreApplication::arguments().contains(QStringLiteral("--gallery"));
  engine.loadFromModule("BurrTools.Ui", gallery ? "Gallery" : "Main");
  if (engine.rootObjects().isEmpty())
    return 2;

  /* --screenshot=<file.png>: render the window once, save it and quit --
   * for documentation and for checking the GUI in CI without a person at
   * the screen. --command=<key> runs a command once the window is up (a
   * command table key such as "export.stl"), so a screenshot can show a
   * dialog. Every other argument (but --gallery) is a puzzle file; like
   * legacy, the first one that loads wins.
   */
  QString screenshot, command;
  const QStringList args = QCoreApplication::arguments();
  bool loaded = false;
  for (qsizetype i = 1; i < args.size(); i++) {
    if (args.at(i).startsWith(QLatin1String("--screenshot=")))
      screenshot = args.at(i).mid(13);
    else if (args.at(i).startsWith(QLatin1String("--command=")))
      command = args.at(i).mid(10);
    else if (args.at(i).startsWith(QLatin1String("-qmljsdebugger")))
      continue;           // qmlprofiler's, for Qt (a -Dqml_debug=true build), not a puzzle
    else if (!loaded && args.at(i) != QLatin1String("--gallery"))
      loaded = app.document()->loadPath(args.at(i));
  }

  auto * mainWindow = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
  /* the compiled pipelines kept between runs, beside the settings file
   * (a scratch one under BURRTOOLS_QT_SETTINGS); before the first frame,
   * which comes from the event loop */
  PipelineCache::configureWindow(mainWindow, PipelineCache::windowFile(app.settings()->file()));

  // the file the window shows: macOS gives the title its document icon
  auto showFile = [mainWindow, &app] { if (mainWindow) mainWindow->setFilePath(app.document()->filePath()); };
  QObject::connect(app.document(), &DocumentController::fileChanged, mainWindow, showFile);
  showFile();

  if (!command.isEmpty())
    QTimer::singleShot(300, &qapp, [&app, command] { app.commands()->trigger(command); });

  if (!screenshot.isEmpty()) {
    auto * window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QTimer::singleShot(1500, &qapp, [window, screenshot] {
      if (!window->grabWindow().save(screenshot))
        fprintf(stderr, "could not write %s\n", qPrintable(screenshot));
      QCoreApplication::exit(0);
    });
  }

  return qapp.exec();
}
