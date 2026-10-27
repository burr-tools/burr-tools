/* Runner for the Qt Quick Test cases in test/qtgui/qml (tst_*.qml).
 *
 * The QML side needs what main() sets up for the real program: an App (here
 * on a scratch settings file, so the user's own is never touched), the icon
 * provider and the Basic style -- plus the Snapshots singleton
 * (BurrTools.Test) for reference-image checks. The cases run headless with
 * QT_QPA_PLATFORM=offscreen, drawn by Qt Quick's software renderer.
 */
#include <QtQuickTest/quicktest.h>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QGuiApplication>
#include <QStyleHints>
#include <QFile>
#include <QTemporaryDir>

#include <cstring>
#include <vector>

#include "app.h"
#include "iconprovider.h"
#include "snapshots.h"

#include "../../src/lib/bt_assert.h"

#include <memory>

class Setup : public QObject {
  Q_OBJECT

public slots:

  void applicationAvailable() {
    bt_assert_init();
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    // a steady text cursor: a blinking one would make reference images flaky
    QGuiApplication::styleHints()->setCursorFlashTime(0);
    qmlRegisterSingletonInstance("BurrTools.Test", 1, 0, "Snapshots", &m_snapshots);
    m_app = std::make_unique<App>(m_dir.filePath(QStringLiteral("settings.rc")),
                                  m_dir.filePath(QStringLiteral("legacy.rc")));
  }

  void qmlEngineAvailable(QQmlEngine * engine) {
    engine->addImageProvider(QStringLiteral("icon"), new IconProvider);
  }

private:
  QTemporaryDir m_dir;
  Snapshots m_snapshots;
  std::unique_ptr<App> m_app;
};

/* QUICK_TEST_MAIN_WITH_SETUP, plus one thing: QTest's console output does
 * not reach a piped stdout with the MSYS2 MinGW Qt (see test_qtgui.cpp), so
 * unless the caller chose an output with -o, the results go to a scratch
 * file that is copied to stdout afterwards.
 */
int main(int argc, char ** argv) {
  QTEST_SET_MAIN_SOURCE_PATH
  Setup setup;

  bool ownOutput = true;
  for (int i = 1; i < argc; i++)
    if (strcmp(argv[i], "-o") == 0)
      ownOutput = false;

  QTemporaryDir logDir;
  const QByteArray log = (logDir.filePath(QStringLiteral("qml.txt")) + QStringLiteral(",txt")).toLocal8Bit();
  std::vector<char *> args(argv, argv + argc);
  char dashO[] = "-o";
  if (ownOutput) {
    args.push_back(dashO);
    args.push_back(const_cast<char *>(log.constData()));
  }

  int r = quick_test_main_with_setup(int(args.size()), args.data(), "burrtools_qml", nullptr, &setup);

  if (ownOutput) {
    QFile f(logDir.filePath(QStringLiteral("qml.txt")));
    if (f.open(QIODevice::ReadOnly))
      fwrite(f.readAll().constData(), 1, size_t(f.size()), stdout);
  }
  return r;
}

#include "test_qtgui_qml.moc"
