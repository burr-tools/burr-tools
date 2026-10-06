/* Qt Test cases for the redesigned GUI's C++ side: the controllers, the
 * theme and the icon provider, driven without any window
 * (QT_QPA_PLATFORM=offscreen). The toolkit-free logic underneath them is
 * covered by the [ui] cases in test_burrtools; these check the Qt layer:
 * signals, persistence and the asynchronous document flows.
 *
 * Example files are read relative to the project root, the test's working
 * directory.
 */
#include "app.h"
#include "commandcontroller.h"
#include "documentcontroller.h"
#include "iconprovider.h"
#include "keyboardcues.h"
#include "layoutcontroller.h"
#include "offscreenrenderer.h"
#include "test_gpu.h"
#include "selfcheck.h"
#include "settingscontroller.h"
#include "shapesmodel.h"
#include "statuscontroller.h"
#include "theme.h"
#include "test_viewport.h"

#include "../../src/uicore/commands.h"
#include "../../src/lib/puzzle.h"
#include "../../src/lib/voxel.h"
#include "../../src/lib/bt_assert.h"

#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QScreen>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

namespace {

  /* An App on scratch settings files: settings.rc (new GUI) and legacy.rc
   * (what it may seed from), both in a temporary directory.
   */
  struct Fixture {
    QTemporaryDir dir;
    std::unique_ptr<App> app;

    QString file(const char * name) const { return dir.filePath(QString::fromLatin1(name)); }

    void writeLegacy(const QByteArray & text) {
      QFile f(file("legacy.rc"));
      QVERIFY(f.open(QIODevice::WriteOnly));
      f.write(text);
    }

    void make() {
      app = std::make_unique<App>(file("settings.rc"), file("legacy.rc"));
    }
  };
}

// ---------------------------------------------------------------------------

class TestTheme : public QObject {
  Q_OBJECT

private slots:

  void parsesTheTokenColourForms() {
    QCOMPARE(Theme::parseColor(QStringLiteral("#2f6df6")), QColor(0x2f, 0x6d, 0xf6));
    QColor c = Theme::parseColor(QStringLiteral("rgba(47,109,246,0.12)"));
    QCOMPARE(c.red(), 47);
    QCOMPARE(c.blue(), 246);
    QVERIFY(qAbs(c.alphaF() - 0.12f) < 0.01f);
    QVERIFY(!Theme::parseColor(QStringLiteral("nonsense")).isValid());
  }

  void theAppFontIsTheTokensStack() {
    // design-tokens §2: -apple-system, Segoe UI, Inter, Roboto, Helvetica
    // Neue, Arial -- the system UI font only on macOS, no generic family
    const QStringList stack = Theme::instance()->fontFamilies();
#ifdef Q_OS_MACOS
    QCOMPARE(stack.size(), 6);
    QCOMPARE(stack.at(1), QStringLiteral("Segoe UI"));
#else
    QCOMPARE(stack, QStringList({ QStringLiteral("Segoe UI"), QStringLiteral("Inter"), QStringLiteral("Roboto"),
                                  QStringLiteral("Helvetica Neue"), QStringLiteral("Arial") }));
#endif
    QTemporaryDir dir;
    App app(dir.filePath(QStringLiteral("s.rc")), dir.filePath(QStringLiteral("l.rc")));
    QCOMPARE(QGuiApplication::font().families(), stack);
  }

  void theMonoFontIsTheTokensStack() {
    // font.mono: ui-monospace, Menlo, Consolas, monospace -- the first one
    // installed, not whatever the system names its fixed font, so the key
    // badges look (and snapshot) the same on every Windows edition
    const QString mono = Theme::instance()->monoFamily();
    const QString fixed = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
#if defined(Q_OS_MACOS)
    QCOMPARE(mono, fixed);
#else
    if (QFontDatabase::hasFamily(QStringLiteral("Menlo")))
      QCOMPARE(mono, QStringLiteral("Menlo"));
    else if (QFontDatabase::hasFamily(QStringLiteral("Consolas")))
      QCOMPARE(mono, QStringLiteral("Consolas"));
    else
      QCOMPARE(mono, fixed);
#endif
#ifdef Q_OS_WIN
    QCOMPARE(mono, QStringLiteral("Consolas"));   // on every Windows edition
#endif
  }

  void requestsTheForcedSchemeFromThePlatform() {
    // AC-C12-08: Light and Dark are asked of the platform (title bar, native
    // dialogs follow), System hands the choice back. Whether the platform
    // then redraws its frame needs a real one; the request is checked here.
    Theme * t = Theme::instance();
    t->setMode(QStringLiteral("dark"));
    QCOMPARE(t->requestedColorScheme(), Qt::ColorScheme::Dark);
    t->setMode(QStringLiteral("light"));
    QCOMPARE(t->requestedColorScheme(), Qt::ColorScheme::Light);
    t->setMode(QStringLiteral("system"));
    QCOMPARE(t->requestedColorScheme(), Qt::ColorScheme::Unknown);
  }

  void systemFollowsTheOperatingSystemAndFallsBackToLight() {
    Theme * t = Theme::instance();
    t->setMode(QStringLiteral("system"));
    const Qt::ColorScheme before = t->systemColorScheme();

    t->setSystemColorScheme(Qt::ColorScheme::Unknown);    // no preference given
    QVERIFY(!t->dark());

    QSignalSpy changed(t, &Theme::changed);
    t->setSystemColorScheme(Qt::ColorScheme::Dark);       // the OS turns dark
    QVERIFY(t->dark());
    QCOMPARE(changed.count(), 1);
    t->setSystemColorScheme(Qt::ColorScheme::Light);      // and back
    QVERIFY(!t->dark());

    // a forced theme ignores the operating system
    t->setMode(QStringLiteral("light"));
    t->setSystemColorScheme(Qt::ColorScheme::Dark);
    QVERIFY(!t->dark());
    t->setMode(QStringLiteral("system"));                 // System applies what it reported meanwhile
    QVERIFY(t->dark());

    t->setSystemColorScheme(before);
    t->setMode(QStringLiteral("light"));
  }

  void switchesPaletteAndDensity() {
    Theme * t = Theme::instance();
    QVERIFY(t);
    QSignalSpy changed(t, &Theme::changed);

    t->setMode(QStringLiteral("light"));
    QCOMPARE(t->dark(), false);
    QCOMPARE(t->panel(), QColor(QStringLiteral("#ffffff")));
    QCOMPARE(t->glyphFolder(), QStringLiteral("light"));

    t->setMode(QStringLiteral("dark"));
    QCOMPARE(t->dark(), true);
    QCOMPARE(t->panel(), QColor(QStringLiteral("#1a1d24")));
    QCOMPARE(t->glyphFolder(), QStringLiteral("dark"));
    QVERIFY(changed.count() >= 1);

    // density.md section 1
    t->setDensity(QStringLiteral("standard"));
    QCOMPARE(t->topBar(), 36.0);
    QCOMPARE(t->workspaceRail(), 60.0);
    QCOMPARE(t->collapsedRail(), 48.0);
    t->setDensity(QStringLiteral("minimal"));
    QCOMPARE(t->topBar(), 32.0);
    QCOMPARE(t->workspaceRail(), 44.0);
    QCOMPARE(t->cardRadius(), 0.0);
    t->setDensity(QStringLiteral("standard"));

    // anything that is not light or dark means "system"
    t->setMode(QStringLiteral("bogus"));
    QCOMPARE(t->mode(), QStringLiteral("system"));
  }
};

// ---------------------------------------------------------------------------

class TestSettings : public QObject {
  Q_OBJECT

private slots:

  void seedsFromTheLegacyFileOnFirstStart() {
    Fixture f;
    f.writeLegacy("tooltips = false\n"
                  "lightning = false\n"
                  "rotator = false\n"
                  "showviewcube = false\n"
                  "undodepth = 2\n"
                  "numthreads = 1\n"
                  "fadeout = false\n"
                  "displaylists = true\n"
                  "renderstyle = 2\n");
    f.make();
    SettingsController * s = f.app->settings();
    QCOMPARE(s->tooltips(), false);
    QCOMPARE(s->lighting(), false);
    QCOMPARE(s->rotationMethod(), QStringLiteral("arcball"));
    QCOMPARE(s->showViewCube(), false);
    QCOMPARE(s->undoDepth(), 100);
    QCOMPARE(s->workerThreads(), 1);
    QCOMPARE(s->fadePieces(), false);
    QCOMPARE(s->displayLists(), true);     // carried over, though unused
    // legacy's render style is not taken over: Voxel style is a setting of
    // the new GUI's own, Flat unless chosen
    QVERIFY(!s->store().contains("renderstyle"));
    QCOMPARE(s->voxelStyle(), QStringLiteral("flat"));
    QVERIFY(QFile::exists(f.file("settings.rc")));
  }

  void startsInTheLightTheme() {
    // Light, as legacy looks, whatever the system's scheme (the spec's
    // default is System); restoring the defaults comes back to it
    Fixture f;
    f.make();
    QCOMPARE(f.app->settings()->theme(), QStringLiteral("light"));
    QCOMPARE(Theme::instance()->mode(), QStringLiteral("light"));
    QVERIFY(!Theme::instance()->dark());
    f.app->settings()->setTheme(QStringLiteral("system"));
    f.app->settings()->restoreAllDefaults();
    QCOMPARE(f.app->settings()->theme(), QStringLiteral("light"));
  }

  void neverWritesTheLegacyFile() {
    Fixture f;
    f.writeLegacy("tooltips = true\n");
    f.make();
    f.app->settings()->setTooltips(false);
    QFile legacy(f.file("legacy.rc"));
    QVERIFY(legacy.open(QIODevice::ReadOnly));
    QCOMPARE(legacy.readAll(), QByteArray("tooltips = true\n"));
  }

  void seedsOnlyOnce() {
    Fixture f;
    f.writeLegacy("lightning = false\n");
    f.make();
    f.app->settings()->setLighting(true);
    f.app.reset();
    f.make();
    // the new GUI's own choice wins over the legacy file from now on
    QCOMPARE(f.app->settings()->lighting(), true);
  }

  void changesPersistImmediately() {
    Fixture f;
    f.make();
    f.app->settings()->setTheme(QStringLiteral("dark"));
    f.app->settings()->setReverseScroll(true);
    f.app.reset();
    f.make();
    QCOMPARE(f.app->settings()->theme(), QStringLiteral("dark"));
    QCOMPARE(f.app->settings()->reverseScroll(), true);
    QCOMPARE(Theme::instance()->dark(), true);
  }

  void undoDepthSnapsToTheOfferedValues() {
    // C12 / T-C12-4: 30 loads as 25, 80 as 100
    QCOMPARE(SettingsController::snapUndoDepth(30), 25);
    QCOMPARE(SettingsController::snapUndoDepth(80), 100);
    QCOMPARE(SettingsController::snapUndoDepth(500), 500);
    QCOMPARE(SettingsController::snapUndoDepth(10000), 500);
  }

  void resetSectionTouchesOnlyItsPage() {
    Fixture f;
    f.make();
    SettingsController * s = f.app->settings();
    s->setLighting(false);
    s->setTooltips(false);
    s->resetSection(QStringLiteral("view3d"));
    QCOMPARE(s->lighting(), true);
    QCOMPARE(s->tooltips(), false);
    s->restoreAllDefaults();
    QCOMPARE(s->tooltips(), true);

    s->setDisplayLists(true);
    s->setWorkerThreads(1);
    s->resetSection(QStringLiteral("performance"));
    QCOMPARE(s->displayLists(), false);
  }

  void aSmallerUndoDepthTrimsTheHistory() {
    // AC-C12-08 end to end: the setting reaches the document's history
    Fixture f;
    f.make();
    SettingsController * s = f.app->settings();
    DocumentController * d = f.app->document();
    s->setUndoDepth(100);
    d->session().puzzle().addShape(2, 2, 2);
    for (int i = 0; i < 60; i++)
      d->session().record(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);
    s->setUndoDepth(25);
    int undos = 0;
    while (d->canUndo()) {
      d->undo();
      undos++;
    }
    QCOMPARE(undos, 25);

    s->setUndoDepth(500);                  // the largest choice is honoured, not capped
    for (int i = 0; i < 300; i++)
      d->session().record(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);
    undos = 0;
    while (d->canUndo()) {
      d->undo();
      undos++;
    }
    QCOMPARE(undos, 300);
  }

  void placementUsesTheSavedPlaceOnlyOnAScreen() {
    // proposal 3, through App: the saved place comes back while it is on
    // one of this run's screens, else the window is centred
    Fixture f;
    f.make();
    QVariantMap p = f.app->windowPlacement(960, 640);
    QVERIFY(!p.value(QStringLiteral("restored")).toBool());       // nothing saved yet
    const QRect a = f.app->availableGeometry();
    QCOMPARE(p.value(QStringLiteral("x")).toInt(), a.x() + (a.width() - p.value(QStringLiteral("width")).toInt()) / 2);

    const QRect screen = QGuiApplication::primaryScreen()->geometry();
    f.app->settings()->saveWindowGeometry(screen.x() + 10, screen.y() + 20, 970, 650, false);
    p = f.app->windowPlacement(960, 640);
    QVERIFY(p.value(QStringLiteral("restored")).toBool());
    QCOMPARE(p.value(QStringLiteral("x")).toInt(), screen.x() + 10);
    QCOMPARE(p.value(QStringLiteral("height")).toInt(), 650);

    // left on a monitor that is gone, then maximised (which keeps that rect)
    f.app->settings()->saveWindowGeometry(screen.right() + 5000, 0, 970, 650, false);
    f.app->settings()->saveWindowGeometry(0, 0, 0, 0, true);
    p = f.app->windowPlacement(960, 640);
    QVERIFY(!p.value(QStringLiteral("restored")).toBool());       // that monitor is gone
    QVERIFY(p.value(QStringLiteral("maximized")).toBool());       // still maximised
  }

  void theWindowComesBackWhereItWas() {
    Fixture f;
    f.make();
    QVERIFY(f.app->settings()->windowGeometry().isEmpty());     // first start: centred
    f.app->settings()->saveWindowGeometry(40, 50, 1200, 800, false);
    f.app->settings()->saveWindowGeometry(0, 0, 3000, 2000, true);   // maximised keeps the restore size
    f.app.reset();
    f.make();
    const QVariantMap g = f.app->settings()->windowGeometry();
    QCOMPARE(g.value(QStringLiteral("x")).toInt(), 40);
    QCOMPARE(g.value(QStringLiteral("width")).toInt(), 1200);
    QCOMPARE(g.value(QStringLiteral("maximized")).toBool(), true);
  }
};

// ---------------------------------------------------------------------------

class TestDocument : public QObject {
  Q_OBJECT

  static void makeModified(DocumentController * d) {
    d->session().puzzle().addShape(2, 2, 2);
    d->session().record(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);
    d->notifyEdited();
  }

private slots:

  void newWithoutChangesGoesStraightToTheTypeChoice() {
    Fixture f;
    f.make();
    DocumentController * d = f.app->document();
    QSignalSpy confirm(d, &DocumentController::confirmDiscardRequested);
    QSignalSpy type(d, &DocumentController::newFileTypeRequested);
    d->requestNew();
    QCOMPARE(confirm.count(), 0);
    QCOMPARE(type.count(), 1);
    QVERIFY(d->flowPending());

    QSignalSpy replaced(d, &DocumentController::documentReplaced);
    d->newDocument(gridType_c::GT_SPHERES);
    QCOMPARE(replaced.count(), 1);
    QVERIFY(!d->flowPending());
    QCOMPARE(d->gridTypeName(), QStringLiteral("Spheres"));
  }

  void unsavedChangesAreAskedAboutAndCancelKeepsThem() {
    Fixture f;
    f.make();
    DocumentController * d = f.app->document();
    makeModified(d);
    QSignalSpy confirm(d, &DocumentController::confirmDiscardRequested);
    QSignalSpy open(d, &DocumentController::openFileRequested);
    d->requestOpen();
    QCOMPARE(confirm.count(), 1);
    QCOMPARE(confirm.at(0).at(0).toString(), QStringLiteral("open another puzzle"));
    d->resolveDiscard(DocumentController::Cancel);
    QCOMPARE(open.count(), 0);
    QVERIFY(!d->flowPending());
    QVERIFY(d->modified());
  }

  void discardContinuesTheFlow() {
    Fixture f;
    f.make();
    DocumentController * d = f.app->document();
    makeModified(d);
    QSignalSpy open(d, &DocumentController::openFileRequested);
    d->requestOpen();
    d->resolveDiscard(DocumentController::Discard);
    QCOMPARE(open.count(), 1);

    d->openFile(QUrl::fromLocalFile(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    QVERIFY(!d->modified());
    QCOMPARE(d->fileName(), QStringLiteral("PelikanBurr.xmpuzzle"));
  }

  void saveWithoutANameAsksForOneThenContinues() {
    Fixture f;
    f.make();
    DocumentController * d = f.app->document();
    makeModified(d);
    QSignalSpy saveAs(d, &DocumentController::saveAsRequested);
    QSignalSpy type(d, &DocumentController::newFileTypeRequested);

    d->requestNew();
    d->resolveDiscard(DocumentController::Save);
    QCOMPARE(saveAs.count(), 1);
    QCOMPARE(type.count(), 0);

    d->saveAsFile(QUrl::fromLocalFile(f.file("saved")));
    QVERIFY(QFile::exists(f.file("saved.xmpuzzle")));
    QCOMPARE(type.count(), 1);
  }

  void cancellingTheSaveAsDialogEndsTheFlow() {
    Fixture f;
    f.make();
    DocumentController * d = f.app->document();
    makeModified(d);
    QSignalSpy quit(d, &DocumentController::quitApproved);
    d->requestQuit();
    d->resolveDiscard(DocumentController::Save);
    d->cancelFlow();
    QCOMPARE(quit.count(), 0);
    QVERIFY(!d->flowPending());
    QVERIFY(d->modified());
  }

  void quitWithoutChangesIsApproved() {
    Fixture f;
    f.make();
    QSignalSpy quit(f.app->document(), &DocumentController::quitApproved);
    f.app->document()->requestQuit();
    QCOMPARE(quit.count(), 1);
  }

  void aFailedOpenReportsAndKeepsTheDocument() {
    Fixture f;
    f.make();
    DocumentController * d = f.app->document();
    QVERIFY(d->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    QSignalSpy msg(d, &DocumentController::messageRequested);
    QVERIFY(!d->loadPath(QStringLiteral("examples/nope.xmpuzzle")));
    QCOMPARE(msg.count(), 1);
    QCOMPARE(d->fileName(), QStringLiteral("PelikanBurr.xmpuzzle"));
  }

  void aFailedSaveIsReported() {
    Fixture f;
    f.make();
    DocumentController * d = f.app->document();
    QSignalSpy msg(d, &DocumentController::messageRequested);
    d->saveAsFile(QUrl::fromLocalFile(f.file("no/such/dir/x")));
    QCOMPARE(msg.count(), 1);
    QVERIFY(d->fileName().isEmpty());
  }

  void undoAndRedoReportWhereTheChangeWas() {
    Fixture f;
    f.make();
    DocumentController * d = f.app->document();
    makeModified(d);
    QVERIFY(d->canUndo());
    QSignalSpy applied(d, &DocumentController::historyApplied);
    d->undo();
    QCOMPARE(applied.count(), 1);
    QCOMPARE(d->session().puzzle().getNumberOfShapes(), 0u);
    QVERIFY(d->canRedo());
    d->redo();
    QCOMPARE(d->session().puzzle().getNumberOfShapes(), 1u);
  }

  void theCommentIsSavedState() {
    Fixture f;
    f.make();
    DocumentController * d = f.app->document();
    d->setComment(QStringLiteral("hello"));
    QCOMPARE(d->comment(), QStringLiteral("hello"));
    QVERIFY(d->modified());
  }

  void theTitleNamesTheDocumentAsThePlatformDoes() {
    Fixture f;
    f.make();
    DocumentController * d = f.app->document();
    QSignalSpy state(d, &DocumentController::stateChanged);
    QVERIFY(d->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
#if defined(Q_OS_WIN)
    QCOMPARE(d->windowTitle(), QStringLiteral("PelikanBurr - BurrTools"));
#elif defined(Q_OS_MACOS)
    QCOMPARE(d->windowTitle(), QStringLiteral("PelikanBurr"));
#else
    QCOMPARE(d->windowTitle(), QStringLiteral("PelikanBurr – BurrTools"));
#endif
    makeModified(d);
    QVERIFY(state.count() > 0);            // the title notifies with the modified state
#if defined(Q_OS_MACOS)
    QCOMPARE(d->windowTitle(), QStringLiteral("PelikanBurr — Edited"));
#else
    QVERIFY(d->windowTitle().startsWith(QLatin1Char('*')));
#endif
    d->newDocument(0);
    QVERIFY(d->windowTitle().startsWith(QLatin1String("Untitled")));
  }

  void voxelTypeNamesFollowTheOfficialList() {
    QCOMPARE(DocumentController::gridTypeDisplayName(0), QStringLiteral("Brick"));
    QCOMPARE(DocumentController::gridTypeDisplayName(1), QStringLiteral("Triangular Prism"));
    QCOMPARE(DocumentController::gridTypeDisplayName(2), QStringLiteral("Spheres"));
    QCOMPARE(DocumentController::gridTypeDisplayName(3), QStringLiteral("Rhombic Tetrahedra"));
    QCOMPARE(DocumentController::gridTypeDisplayName(4), QStringLiteral("Tetrahedra-Octahedra"));
  }
};

// ---------------------------------------------------------------------------

class TestLayout : public QObject {
  Q_OBJECT

private slots:

  void collapsePersistsPerWorkspaceAndFocusDoesNot() {
    Fixture f;
    f.make();
    LayoutController * l = f.app->layout();
    l->setRightCollapsed(true);
    l->setWorkspace(LayoutController::Solver);
    l->setLeftCollapsed(true);
    l->toggleFocus3d();
    QCOMPARE(l->focus(), int(LayoutController::Focus3d));

    f.app.reset();
    f.make();
    l = f.app->layout();
    QCOMPARE(l->workspace(), int(LayoutController::Entities));
    QCOMPARE(l->focus(), int(LayoutController::FocusNone));
    QCOMPARE(l->rightCollapsed(), true);
    QCOMPARE(l->leftCollapsed(), false);
    l->setWorkspace(LayoutController::Solver);
    QCOMPARE(l->leftCollapsed(), true);
  }

  void widthsFollowWindowAndDensity() {
    Fixture f;
    f.make();
    LayoutController * l = f.app->layout();
    l->setWindowWidth(1600);
    QCOMPARE(l->leftWidth(), 320.0);
    QCOMPARE(l->centreWidth(), 864.0);
    QCOMPARE(l->rightWidth(), 340.0);
    QSignalSpy changed(l, &LayoutController::changed);
    f.app->settings()->setDensity(QStringLiteral("minimal"));
    QVERIFY(changed.count() >= 1);
    QCOMPARE(l->centreWidth(), 970.0);
    f.app->settings()->setDensity(QStringLiteral("standard"));
  }

  void escapeLeavesFocusOnly() {
    Fixture f;
    f.make();
    LayoutController * l = f.app->layout();
    QVERIFY(!l->escape());
    l->toggleFocus2d();
    QVERIFY(l->escape());
    QCOMPARE(l->focus(), int(LayoutController::FocusNone));
  }
};

// ---------------------------------------------------------------------------

class TestCommands : public QObject {
  Q_OBJECT

private slots:

  void menuBarAndItemsComeFromTheTable() {
    Fixture f;
    f.make();
    CommandController * c = f.app->commands();
    const QVariantList bar = c->menuBar();
    QStringList keys;
    for (const QVariant & v : bar)
      keys << v.toMap().value(QStringLiteral("key")).toString();
    QCOMPARE(keys, QStringList({ "file", "edit", "view", "export", "help" }));

    const QVariantList file = c->menuItems(QStringLiteral("file"));
    QCOMPARE(file.first().toMap().value(QStringLiteral("key")).toString(), QStringLiteral("file.new"));
    QVERIFY(c->menuItems(QStringLiteral("nosuchmenu")).isEmpty());

    // labels as the platform writes them: Exit on Windows, no access keys on macOS
    QString quit;
    for (const QVariant & v : file)
      if (v.toMap().value(QStringLiteral("key")).toString() == QLatin1String("file.quit"))
        quit = v.toMap().value(QStringLiteral("label")).toString();
#if defined(Q_OS_WIN)
    QCOMPARE(quit, QStringLiteral("E&xit"));
#elif defined(Q_OS_MACOS)
    QCOMPARE(quit, QStringLiteral("Quit"));
#else
    QCOMPARE(quit, QStringLiteral("&Quit"));
#endif

    bool saveHasShortcut = false;
    for (const QVariant & v : file)
      if (v.toMap().value(QStringLiteral("key")).toString() == QLatin1String("file.save"))
        saveHasShortcut = !v.toMap().value(QStringLiteral("shortcutText")).toString().isEmpty();
    QVERIFY(saveHasShortcut);
  }

  void menuItemsOwnTheirFirstShortcut() {
    // on macOS the menu item owns Ctrl+O (Cmd+O, a key equivalent) and F3
    // stays a separate binding; elsewhere the menus only display keys.
    Fixture f;
    f.make();
    CommandController * c = f.app->commands();
    QStringList openKeys;
    for (const QVariant & v : c->shortcuts())
      if (v.toMap().value(QStringLiteral("key")).toString() == QLatin1String("file.open"))
        openKeys = v.toMap().value(QStringLiteral("sequences")).toStringList();
    if (CommandController::menuItemsOwnShortcuts())
      QCOMPARE(openKeys, QStringList({ "F3" }));
    else
      QCOMPARE(openKeys, QStringList({ "Ctrl+O", "F3" }));

    // the menu shows up to two of them
    QString openText;
    for (const QVariant & v : c->menuItems(QStringLiteral("file")))
      if (v.toMap().value(QStringLiteral("key")).toString() == QLatin1String("file.open"))
        openText = v.toMap().value(QStringLiteral("shortcutText")).toString();
#ifdef Q_OS_MACOS
    QCOMPARE(openText.count(QStringLiteral(" / ")), 1);
#else
    QCOMPARE(openText, QStringLiteral("Ctrl+O / F3"));
#endif

    QVERIFY(!c->blocked());
    c->setBlocked(true);
    QVERIFY(c->blocked());
    c->setBlocked(false);
  }

  void showMenuBarIsACheckableToggle() {
    Fixture f;
    f.make();
    CommandController * c = f.app->commands();
    QVERIFY(c->isCheckable(QStringLiteral("view.menuBar")));
    QVERIFY(!c->isCheckable(QStringLiteral("file.save")));
    QVERIFY(c->isChecked(QStringLiteral("view.menuBar")));
    QSignalSpy rev(c, &CommandController::revisionChanged);
    c->trigger(QStringLiteral("view.menuBar"));
    QVERIFY(!f.app->settings()->showMenuBar());
    QVERIFY(!c->isChecked(QStringLiteral("view.menuBar")));
    QVERIFY(rev.count() > 0);              // menus re-read the check mark
    f.app.reset();
    f.make();                              // it is remembered
    QVERIFY(!f.app->settings()->showMenuBar());
    f.app->settings()->restoreAllDefaults();
    QVERIFY(!f.app->settings()->showMenuBar());   // window state, not a Settings row

    // the item exists only where the menu bar can be hidden
    bool listed = false;
    for (const QVariant & v : f.app->commands()->menuItems(QStringLiteral("view")))
      if (v.toMap().value(QStringLiteral("key")).toString() == QLatin1String("view.menuBar"))
        listed = v.toMap().value(QStringLiteral("checkable")).toBool();
    QCOMPARE(listed, CommandController::menuBarHideable());
  }

  void theShortcutsWindowListsEveryKeyThatIsNotInAMenu() {
    // C22: the help window is the user-facing copy of the command table.
    // Menu commands show their keys in the menus; every other key must be
    // in the window, written as the window writes it.
    Fixture f;
    f.make();
    QSet<QString> listed;
    for (const QVariant & g : f.app->commands()->shortcutHelp()) {
      const QVariantMap group = g.toMap();
      QVERIFY(!group.value(QStringLiteral("title")).toString().isEmpty());
      const QVariantList rows = group.value(QStringLiteral("rows")).toList();
      QVERIFY(!rows.isEmpty());
      for (const QVariant & r : rows) {
        const QVariantMap row = r.toMap();
        QVERIFY(!row.value(QStringLiteral("action")).toString().isEmpty());
        const QVariantList keys = row.value(QStringLiteral("keys")).toList();
        QVERIFY2(!keys.isEmpty(), qPrintable(row.value(QStringLiteral("action")).toString()));
        for (const QVariant & alt : keys) {
          QStringList caps;
          for (const QVariant & part : alt.toList()) {
            QVERIFY(!part.toMap().value(QStringLiteral("text")).toString().isEmpty());
            caps << part.toMap().value(QStringLiteral("text")).toString();
          }
          listed.insert(caps.join(QLatin1Char('+')));
        }
      }
    }
    for (const auto & c : btui::commandTable()) {
      if (btui::menuOn(c, btui::currentPlatform()) != btui::Menu::None)
        continue;
      for (auto s : CommandController::platformShortcuts(c)) {
        const QString text = CommandController::keyCaps(s).join(QLatin1Char('+'));
        QVERIFY2(listed.contains(text), qPrintable(QStringLiteral("%1 (%2) is missing from Help > Keyboard shortcuts")
                                                      .arg(QString::fromUtf8(c.key.data(), qsizetype(c.key.size())), text)));
      }
    }
    // and the global keys the spec puts under "Everywhere", though they are in menus
    for (const char * key : { "settings", "help.shortcuts", "view.focus3d", "view.fullScreen", "workspace.entities" }) {
      const btui::CommandInfo * c = btui::findCommand(key);
      QVERIFY(c);
      for (auto s : CommandController::platformShortcuts(*c))
        QVERIFY2(listed.contains(CommandController::keyCaps(s).join(QLatin1Char('+'))), key);
    }
  }

  void keyCapsFollowThePlatform() {
#ifdef Q_OS_MACOS
    QCOMPARE(CommandController::keyCaps("Ctrl+Shift+F").size(), 1);
#else
    QCOMPARE(CommandController::keyCaps("Ctrl+Shift+F"), QStringList({ "Ctrl", "Shift", "F" }));
    QCOMPARE(CommandController::keyCaps("Ctrl++").last(), QStringLiteral("+"));
#endif
  }

  void undoIsDisabledWithNothingToUndo() {
    Fixture f;
    f.make();
    CommandController * c = f.app->commands();
    QVERIFY(!c->isEnabled(QStringLiteral("edit.undo")));
    QSignalSpy applied(f.app->document(), &DocumentController::historyApplied);
    c->trigger(QStringLiteral("edit.undo"));
    QCOMPARE(applied.count(), 0);
  }

  void exportNeedsAShape() {
    // legacy updateInterface(): Image and STL grey out on an empty puzzle;
    // the vector export draws the current view and stays available
    Fixture f;
    f.make();
    CommandController * c = f.app->commands();
    QVERIFY(!c->isEnabled(QStringLiteral("export.image")));
    QVERIFY(!c->isEnabled(QStringLiteral("export.stl")));
    QVERIFY(c->isEnabled(QStringLiteral("export.vector")));
    QSignalSpy stl(c, &CommandController::stlExportRequested);
    c->trigger(QStringLiteral("export.stl"));
    QCOMPARE(stl.count(), 0);

    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    QVERIFY(c->isEnabled(QStringLiteral("export.image")));
    QVERIFY(c->isEnabled(QStringLiteral("export.stl")));
    c->trigger(QStringLiteral("export.stl"));
    QCOMPARE(stl.count(), 1);
  }

  void menuToolsOpenTheirDialogs() {
    Fixture f;
    f.make();
    CommandController * c = f.app->commands();
    QSignalSpy convert(c, &CommandController::convertRequested);
    QSignalSpy import(c, &CommandController::importAssembliesRequested);
    QSignalSpy status(c, &CommandController::statusRequested);
    c->trigger(QStringLiteral("file.convert"));
    c->trigger(QStringLiteral("file.importAssemblies"));
    c->trigger(QStringLiteral("status"));
    QSignalSpy vector(c, &CommandController::vectorExportRequested);
    c->trigger(QStringLiteral("export.vector"));
    QCOMPARE(vector.count(), 1);
    QCOMPARE(convert.count(), 1);
    QCOMPARE(import.count(), 1);
    QCOMPARE(status.count(), 1);

    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    QSignalSpy image(c, &CommandController::imageExportRequested);
    c->trigger(QStringLiteral("export.image"));
    QCOMPARE(image.count(), 1);
  }

  void layoutCommandsReachTheLayout() {
    Fixture f;
    f.make();
    CommandController * c = f.app->commands();
    c->trigger(QStringLiteral("workspace.puzzle"));
    QCOMPARE(f.app->layout()->workspace(), int(LayoutController::Puzzle));
    c->trigger(QStringLiteral("view.focus3d"));
    QCOMPARE(f.app->layout()->focus(), int(LayoutController::Focus3d));
    c->trigger(QStringLiteral("layout.toggleRight"));
    QCOMPARE(f.app->layout()->focus(), int(LayoutController::FocusNone));
  }

  void dialogCommandsAskQml() {
    Fixture f;
    f.make();
    CommandController * c = f.app->commands();
    QSignalSpy about(c, &CommandController::aboutRequested);
    QSignalSpy comment(c, &CommandController::commentRequested);
    QSignalSpy settings(c, &CommandController::settingsRequested);
    QSignalSpy full(c, &CommandController::fullScreenToggleRequested);
    c->trigger(QStringLiteral("about"));
    c->trigger(QStringLiteral("editcomment"));
    c->trigger(QStringLiteral("settings"));
    c->trigger(QStringLiteral("view.fullScreen"));
    QCOMPARE(about.count(), 1);
    QCOMPARE(comment.count(), 1);
    QCOMPARE(settings.count(), 1);
    QCOMPARE(full.count(), 1);
  }

  void selfCheckPasses() {
    QCOMPARE(selfCheck(), QString());
  }
};

// ---------------------------------------------------------------------------

class TestKeyboardCues : public QObject {
  Q_OBJECT

  static void press(QObject * target, int key) {
    QKeyEvent e(QEvent::KeyPress, key, key == Qt::Key_Alt ? Qt::AltModifier : Qt::NoModifier);
    QCoreApplication::sendEvent(target, &e);
  }

private slots:

  void altShowsTheAccessKeysUntilTheKeyboardIsDone() {
    // Windows convention: underlines appear from an Alt press until a click,
    // Esc, leaving the application or running a menu command
    if (KeyboardCues::alwaysShown())
      QSKIP("this system underlines access keys always");
    KeyboardCues cues;
    QObject target;          // any receiver: the cues watch the whole application
    QSignalSpy changed(&cues, &KeyboardCues::changed);
    QVERIFY(!cues.showAccessKeys());

    press(&target, Qt::Key_Alt);
    QVERIFY(cues.showAccessKeys());
    QCOMPARE(changed.count(), 1);
    press(&target, Qt::Key_F);              // Alt+F opening a menu keeps them
    QVERIFY(cues.showAccessKeys());
    press(&target, Qt::Key_Escape);
    QVERIFY(!cues.showAccessKeys());

    press(&target, Qt::Key_Alt);
    QMouseEvent click(QEvent::MouseButtonPress, QPointF(1, 1), QPointF(1, 1), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&target, &click);
    QVERIFY(!cues.showAccessKeys());

    press(&target, Qt::Key_Alt);
    QEvent away(QEvent::ApplicationDeactivate);
    QCoreApplication::sendEvent(qApp, &away);
    QVERIFY(!cues.showAccessKeys());

    press(&target, Qt::Key_Alt);
    cues.reset();                           // a menu command ran
    QVERIFY(!cues.showAccessKeys());
    QCOMPARE(changed.count(), 8);
  }

  void escTakenByAShortcutEndsThemToo() {
    // the layout's Esc shortcut takes the key: it arrives only as the
    // ShortcutOverride sent ahead of it, never as a key press
    if (KeyboardCues::alwaysShown())
      QSKIP("this system underlines access keys always");
    KeyboardCues cues;
    QObject target;
    press(&target, Qt::Key_Alt);
    QVERIFY(cues.showAccessKeys());
    QKeyEvent overrideEsc(QEvent::ShortcutOverride, Qt::Key_Escape, Qt::NoModifier);
    QCoreApplication::sendEvent(&target, &overrideEsc);
    QVERIFY(!cues.showAccessKeys());
  }

  void otherKeysLeaveThemAlone() {
    if (KeyboardCues::alwaysShown())
      QSKIP("this system underlines access keys always");
    KeyboardCues cues;
    QObject target;
    press(&target, Qt::Key_A);
    press(&target, Qt::Key_Escape);
    QVERIFY(!cues.showAccessKeys());
  }

  void onlyWindowsCanAskForThemAlways() {
#ifndef Q_OS_WIN
    QVERIFY(!KeyboardCues::alwaysShown());
#endif
    KeyboardCues cues;
    QCOMPARE(cues.showAccessKeys(), KeyboardCues::alwaysShown());
  }
};

// ---------------------------------------------------------------------------

class TestShapesAndStatus : public QObject {
  Q_OBJECT

private slots:

  void shapesListFollowsTheDocument() {
    Fixture f;
    f.make();
    ShapesModel * m = f.app->shapes();
    QCOMPARE(m->count(), 0);
    QCOMPARE(m->selected(), -1);

    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    const int n = int(f.app->document()->session().puzzle().getNumberOfShapes());
    QCOMPARE(m->count(), n);
    QCOMPARE(m->selected(), 0);
    QCOMPARE(m->data(m->index(1), ShapesModel::IdTextRole).toString(), QStringLiteral("S2"));
    QCOMPARE(m->data(m->index(0), ShapesModel::ColorRole).value<QColor>(), QColor(0, 0, 255));

    m->select(n + 10);
    QCOMPARE(m->selected(), n - 1);
    m->select(-5);
    QCOMPARE(m->selected(), -1);
  }

  void statusTextIsTheLegacySentence() {
    Fixture f;
    f.make();
    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    f.app->shapes()->select(1);
    const voxel_c * v = f.app->document()->session().puzzle().getShape(1);
    const unsigned fx = v->countState(voxel_c::VX_FILLED), vr = v->countState(voxel_c::VX_VARIABLE);
    QCOMPARE(f.app->status()->text(),
             QStringLiteral("Shape <b>S2</b> has <b>%1</b> voxels (%2 fixed, %3 variable)").arg(fx + vr).arg(fx).arg(vr));

    // Puzzle and Solver have their own sentences (later phases)
    f.app->layout()->setWorkspace(LayoutController::Puzzle);
    QCOMPARE(f.app->status()->text(), QString());
  }

  void theCursorReadoutShowsWhatItIsGiven() {
    // C10's cursor readout ("Cursor X1 Y2 · Z0"); the 2D grid feeds it (P4)
    StatusController s;
    QSignalSpy changed(&s, &StatusController::changed);
    s.setCursorText(QStringLiteral("Cursor X1 Y2 · Z0"));
    QCOMPARE(s.cursorText(), QStringLiteral("Cursor X1 Y2 · Z0"));
    QCOMPARE(changed.count(), 1);
    s.setCursorText(QStringLiteral("Cursor X1 Y2 · Z0"));    // the same: nothing to tell
    QCOMPARE(changed.count(), 1);
    s.flash(QStringLiteral("Saved"));                                 // a flash leaves it alone
    QCOMPARE(s.cursorText(), QStringLiteral("Cursor X1 Y2 · Z0"));
    s.setCursorText(QString());
    QVERIFY(s.cursorText().isEmpty());
  }

  void aFlashReplacesTheTextThenReverts() {
    StatusController s;
    s.setFlashDuration(50);
    s.setLiveText(QStringLiteral("live"));
    s.flash(QStringLiteral("Blocked: <x>"));
    QCOMPARE(s.text(), QStringLiteral("Blocked: &lt;x&gt;"));
    QVERIFY(s.flashing());
    QTRY_COMPARE_WITH_TIMEOUT(s.text(), QStringLiteral("live"), 2000);
  }
};

// ---------------------------------------------------------------------------

class TestIcons : public QObject {
  Q_OBJECT

  /* width / height of an SVG as declared, and of its viewBox */
  static bool proportions(const QString & file, double * declared, double * box) {
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
      return false;
    const QString svg = QString::fromUtf8(f.read(4096));
    // the <svg> element's own width and height (not stroke-width)
    const QRegularExpression w(QStringLiteral("<svg[^>]*\\swidth=\"([0-9.]+)\"")),
                             h(QStringLiteral("<svg[^>]*\\sheight=\"([0-9.]+)\"")),
                             vb(QStringLiteral("viewBox=\"[-0-9.]+ [-0-9.]+ ([0-9.]+) ([0-9.]+)\""));
    const auto mw = w.match(svg), mh = h.match(svg), mv = vb.match(svg);
    if (!mv.hasMatch())
      return false;
    *box = mv.captured(1).toDouble() / mv.captured(2).toDouble();
    *declared = (mw.hasMatch() && mh.hasMatch()) ? mw.captured(1).toDouble() / mh.captured(1).toDouble() : *box;
    return true;
  }

private slots:

  void everyAssetIsDeclaredInItsOwnProportions() {
    // Qt's SVG renderer stretches a drawing to the size the file declares;
    // the edit-tool glyphs once declared 48 x 48 round a 28 x 24 drawing and
    // came out squeezed. Every glyph and icon keeps its viewBox's shape.
    Fixture f;
    f.make();
    int checked = 0;
    for (const char * folder : { "glyphs/light", "glyphs/dark", "icons" })
      for (const QString & name : f.app->assetNames(QString::fromLatin1(folder))) {
        const QString file = QStringLiteral(":/burrtools/%1/%2.svg").arg(QString::fromLatin1(folder), name);
        double declared = 0, box = 0;
        QVERIFY2(proportions(file, &declared, &box), qPrintable(file));
        QVERIFY2(std::abs(declared - box) < 0.01 * box,
                 qPrintable(QStringLiteral("%1: declared %2, drawn %3").arg(file).arg(declared).arg(box)));
        checked++;
      }
    QVERIFY(checked > 100);
  }

  void tintsCurrentColor() {
    QByteArray svg = IconProvider::tintedSvg(QStringLiteral("gear"), QStringLiteral("#ff0000"));
    QVERIFY(!svg.isEmpty());
    QVERIFY(!svg.contains("currentColor"));
    QVERIFY(svg.contains("#ff0000"));
  }

  void refusesPathsOutsideTheIconFolder() {
    QVERIFY(IconProvider::tintedSvg(QStringLiteral("../design-tokens"), QStringLiteral("#000")).isEmpty());
    QVERIFY(IconProvider::tintedSvg(QStringLiteral("glyphs/light/tool-fixed"), QStringLiteral("#000")).isEmpty());
    QVERIFY(IconProvider::tintedSvg(QStringLiteral("no-such-icon"), QStringLiteral("#000")).isEmpty());
  }

  void rendersAtTheRequestedSize() {
    IconProvider p;
    QSize size;
    QImage img = p.requestImage(QStringLiteral("gear?color=2f6df6"), &size, QSize(48, 48));
    QCOMPARE(size, QSize(48, 48));
    QCOMPARE(img.size(), QSize(48, 48));
    // some pixel is the requested accent colour
    bool found = false;
    for (int y = 0; y < img.height() && !found; y++)
      for (int x = 0; x < img.width() && !found; x++) {
        QColor c = img.pixelColor(x, y);
        if (c.alpha() == 255 && c.red() == 0x2f && c.blue() == 0xf6)
          found = true;
      }
    QVERIFY(found);
  }
};

// ---------------------------------------------------------------------------

class TestTools : public QObject {
  Q_OBJECT

  /* a brick document with S1, S2 = S1, S3 hollow, S4 two loose voxels */
  static void fourShapes(Fixture & f) {
    f.app->document()->newDocument(0);
    puzzle_c & p = f.app->document()->session().puzzle();
    for (int i = 0; i < 3; i++) {
      unsigned s = p.addShape(3, 3, 3);
      p.getShape(s)->setAll(voxel_c::VX_FILLED);
    }
    p.getShape(2)->setState(1, 1, 1, voxel_c::VX_EMPTY);
    unsigned s = p.addShape(3, 1, 1);
    p.getShape(s)->setState(0, 0, 0, voxel_c::VX_FILLED);
    p.getShape(s)->setState(2, 0, 0, voxel_c::VX_FILLED);
    f.app->document()->recordStructuralEdit();
  }

private slots:

  void convertReplacesThePuzzle() {
    Fixture f;
    f.make();
    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    const int shapes = f.app->shapes()->count();
    const QVariantList targets = f.app->tools()->convertTargets();
    QVERIFY(!targets.isEmpty());
    const QVariantMap first = targets.first().toMap();
    QVERIFY(!first.value(QStringLiteral("name")).toString().isEmpty());

    QSignalSpy replaced(f.app->document(), &DocumentController::documentReplaced);
    QVERIFY(f.app->tools()->convert(first.value(QStringLiteral("type")).toInt()));
    QCOMPARE(replaced.count(), 1);
    QCOMPARE(f.app->document()->gridType(), first.value(QStringLiteral("type")).toInt());
    QVERIFY(f.app->document()->modified());
    QVERIFY(!f.app->document()->canUndo());
    QCOMPARE(f.app->shapes()->count(), shapes);
  }

  void aFailedConvertSaysSo() {
    Fixture f;
    f.make();
    QSignalSpy message(f.app->document(), &DocumentController::messageRequested);
    QVERIFY(!f.app->tools()->convert(99));
    QCOMPARE(message.count(), 1);
  }

  void problemsAreListedTheLegacyWay() {
    Fixture f;
    f.make();
    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/SolidSixPieceBurrs.xmpuzzle")));
    const QVariantList probs = f.app->tools()->problems();
    QCOMPARE(probs.size(), int(f.app->document()->session().puzzle().getNumberOfProblems()));
    const QVariantMap p0 = probs.first().toMap();
    QVERIFY(p0.value(QStringLiteral("label")).toString().startsWith(QLatin1String("P1")));
    QCOMPARE(p0.value(QStringLiteral("color")).value<QColor>(), ShapesModel::chipColor(0));
  }

  void importOptionsKeepTheLegacyDefaults() {
    btui::ImportAssembliesOptions o = ToolsController::importOptions({});
    QVERIFY(o.destination == btui::ImportAssembliesOptions::Destination::JustAddShapes);
    QVERIFY(o.dropDisconnected && o.dropIdentical && !o.dropMirror);
    QCOMPARE(o.rangeMax, 1u);
    QCOMPARE(o.shapeMax, 1000000u);

    o = ToolsController::importOptions({ { "destination", "existing" }, { "target", 2 }, { "rangeMin", -4 },
                                         { "dropIdentical", false }, { "shapeMin", 7 } });
    QVERIFY(o.destination == btui::ImportAssembliesOptions::Destination::ExistingProblem);
    QCOMPARE(o.destinationProblem, 2u);
    QCOMPARE(o.rangeMin, 0u);
    QVERIFY(!o.dropIdentical);
    QCOMPARE(o.shapeMin, 7u);
  }

  void importAddsShapesAsOneUndoStep() {
    Fixture f;
    f.make();
    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/SolidSixPieceBurrs.xmpuzzle")));
    const int shapes = f.app->shapes()->count();
    const unsigned problems = f.app->document()->session().puzzle().getNumberOfProblems();

    const int added = f.app->tools()->importAssemblies({ { "destination", "new" } });
    QVERIFY(added > 0);
    QCOMPARE(f.app->shapes()->count(), shapes + added);
    QCOMPARE(f.app->document()->session().puzzle().getNumberOfProblems(), problems + 1);
    QVERIFY(f.app->document()->canUndo());
    QVERIFY(f.app->document()->modified());

    f.app->document()->undo();
    QCOMPARE(f.app->shapes()->count(), shapes);
    QCOMPARE(f.app->document()->session().puzzle().getNumberOfProblems(), problems);
  }

  void anImportThatAddsNothingLeavesNoUndoStep() {
    Fixture f;
    f.make();
    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/SolidSixPieceBurrs.xmpuzzle")));
    QCOMPARE(f.app->tools()->importAssemblies({ { "shapeMin", 1000000 } }), 0);
    QVERIFY(!f.app->document()->canUndo());
    QCOMPARE(f.app->tools()->importAssemblies({ { "source", 999 } }), 0);
  }

  void statusTableFillsInStepByStep() {
    Fixture f;
    f.make();
    fourShapes(f);
    ShapeStatusModel * m = f.app->tools()->shapeStatus();
    m->start();
    QVERIFY(m->busy());
    QVERIFY(m->bricks());
    QTRY_VERIFY(!m->busy());
    QCOMPARE(m->rowCount(), 4);
    QCOMPARE(m->progress(), 1.0);

    auto at = [m](int row, int role) { return m->data(m->index(row), role); };
    QCOMPARE(at(0, ShapeStatusModel::IdTextRole).toString(), QStringLiteral("S1"));
    QCOMPARE(at(0, ShapeStatusModel::FixedRole).toInt(), 27);
    QCOMPARE(at(0, ShapeStatusModel::ShapeRole).toInt(), 0);
    QCOMPARE(at(1, ShapeStatusModel::ShapeRole).toInt(), 1);     // equals S1
    QCOMPARE(at(2, ShapeStatusModel::Holes3dRole).toBool(), true);
    QCOMPARE(at(3, ShapeStatusModel::FaceRole).toBool(), false);
    QCOMPARE(at(3, ShapeStatusModel::TotalRole).toInt(), 2);
    QVERIFY(!at(0, ShapeStatusModel::SymmetryRole).toString().isEmpty());
  }

  void cancelKeepsTheRowsSoFar() {
    Fixture f;
    f.make();
    fourShapes(f);
    ShapeStatusModel * m = f.app->tools()->shapeStatus();
    m->start();
    m->cancel();
    QVERIFY(!m->busy());
    QVERIFY(m->rowCount() < 4);
    m->selectHoles();     // acts on computed rows only: nothing to do
    QCOMPARE(m->selectedCount(), 0);
  }

  void selectAndRemoveShapes() {
    Fixture f;
    f.make();
    fourShapes(f);
    ShapeStatusModel * m = f.app->tools()->shapeStatus();
    m->start();
    m->finish();

    m->selectIdentical(QStringLiteral("shape"));
    QCOMPARE(m->selectedCount(), 1);
    QVERIFY(m->data(m->index(1), ShapeStatusModel::SelectedRole).toBool());
    m->selectHoles();
    QCOMPARE(m->selectedCount(), 2);
    m->setSelected(2, false);
    QCOMPARE(m->selectedCount(), 1);

    QCOMPARE(m->removeSelected(), 1);
    QCOMPARE(f.app->shapes()->count(), 3);
    QVERIFY(m->busy());                  // computed again, as legacy reopens
    m->finish();
    QCOMPARE(m->rowCount(), 3);
    QCOMPARE(m->selectedCount(), 0);
    QCOMPARE(m->removeSelected(), 0);

    f.app->document()->undo();
    QCOMPARE(f.app->shapes()->count(), 4);
    QCOMPARE(m->rowCount(), 0);          // the table was about another puzzle
  }

  void aNewDocumentClearsTheTable() {
    Fixture f;
    f.make();
    fourShapes(f);
    ShapeStatusModel * m = f.app->tools()->shapeStatus();
    m->start();
    m->finish();
    QCOMPARE(m->rowCount(), 4);
    f.app->document()->newDocument(2);
    QCOMPARE(m->rowCount(), 0);
    QVERIFY(!m->busy());
  }
};

// ---------------------------------------------------------------------------

class TestStlExport : public QObject {
  Q_OBJECT

private slots:

  void beginBuildsThePreviewOfTheSelectedShape() {
    Fixture f;
    f.make();
    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    f.app->shapes()->select(2);
    StlExportController * s = f.app->stl();
    s->setViewportSize(320, 240);
    s->begin();
    QCOMPARE(s->shape(), 2);
    QCOMPARE(s->parameterCount(), 6);     // the brick exporter's
    QVERIFY(s->hasMesh());
    QVERIFY(s->error().isEmpty());
    QVERIFY(s->volumeText().startsWith(QLatin1String("Volume: ")));
    const SceneFrame fr = s->frame(1);
    QVERIFY(fr.mesh && !fr.mesh->opaque.empty());
    QVERIFY(!fr.xray);
    QCOMPARE(s->suggestedName(), QStringLiteral("PelikanBurr-S3.stl"));

    s->end();
    QVERIFY(!s->hasMesh());
    QCOMPARE(s->parameterCount(), 0);
    QVERIFY(!s->frame(1).mesh);
  }

  void parametersAreTypedAndRebuildThePreview() {
    Fixture f;
    f.make();
    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    StlExportController * s = f.app->stl();
    s->begin();
    s->setShape(2);
    const QVariantMap unit = s->parameter(0);
    QCOMPARE(unit.value(QStringLiteral("name")).toString(), QStringLiteral("Unit Size"));
    QVERIFY(!unit.value(QStringLiteral("tooltip")).toString().isEmpty());
    QVERIFY(s->parameter(99).isEmpty());

    const QString volumeBefore = s->volumeText();
    QSignalSpy values(s, &StlExportController::valuesChanged);
    QSignalSpy count(s, &StlExportController::parametersChanged);
    s->setParameter(0, unit.value(QStringLiteral("value")).toDouble() * 2);
    QCOMPARE(values.count(), 1);
    QCOMPARE(count.count(), 0);           // the fields stay; only values change
    QVERIFY(s->volumeText() != volumeBefore);

    // a positive parameter does not take a negative value
    for (int i = 0; i < s->parameterCount(); i++) {
      const QVariantMap p = s->parameter(i);
      if (p.value(QStringLiteral("type")).toString() == QLatin1String("posDouble")) {
        s->setParameter(i, -3);
        QCOMPARE(s->parameter(i).value(QStringLiteral("value")).toDouble(), 0.0);
        break;
      }
    }
  }

  void insidesMakesTheMeshSeeThrough() {
    Fixture f;
    f.make();
    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    StlExportController * s = f.app->stl();
    s->begin();
    s->setShape(2);
    s->setInsides(true);
    const SceneFrame fr = s->frame(1);
    QVERIFY(fr.xray);
    QVERIFY(fr.mesh->opaque.empty());
    QVERIFY(!fr.mesh->translucent.empty());
    s->begin();                           // a new dialog starts normal
    QVERIFY(!s->insides());
  }

  void exportWritesBinaryOrTextStl() {
    Fixture f;
    f.make();
    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    StlExportController * s = f.app->stl();
    s->begin();
    s->setShape(2);

    QSignalSpy exported(s, &StlExportController::exported);
    const QString bin = f.file("shape");
    QVERIFY(s->exportTo(QUrl::fromLocalFile(bin)));
    QCOMPARE(exported.count(), 1);
    QCOMPARE(exported.first().first().toString(), QStringLiteral("shape.stl"));
    QFile b(bin + QStringLiteral(".stl"));
    QVERIFY(b.open(QIODevice::ReadOnly));
    const QByteArray data = b.readAll();
    QVERIFY(data.size() > 84);
    QCOMPARE((data.size() - 84) % 50, 0);   // header, count, 50 bytes a triangle

    s->setBinary(false);
    const QString text = f.file("shape-text.stl");
    QVERIFY(s->exportTo(QUrl::fromLocalFile(text)));
    QFile t(text);
    QVERIFY(t.open(QIODevice::ReadOnly));
    QVERIFY(t.readAll().startsWith("solid "));
  }

  void aShapeThatCannotBeExportedSaysWhy() {
    Fixture f;
    f.make();
    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    StlExportController * s = f.app->stl();
    s->begin();
    s->setShape(0);                       // S1 has variable voxels
    QVERIFY(!s->hasMesh());
    QCOMPARE(s->error(), QStringLiteral("Shapes with variable voxels cannot be exported"));
    QVERIFY(s->volumeText().isEmpty());
    QVERIFY(!s->frame(1).mesh);           // the preview does not keep the last mesh
    s->setShape(2);
    QVERIFY(s->hasMesh());
    QVERIFY(s->error().isEmpty());
  }

  void anUnwritableFileSaysSo() {
    Fixture f;
    f.make();
    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    StlExportController * s = f.app->stl();
    s->begin();
    s->setShape(2);
    QSignalSpy failed(s, &StlExportController::failed);
    QVERIFY(!s->exportTo(QUrl::fromLocalFile(f.file("no/such/folder/x.stl"))));
    QCOMPARE(failed.count(), 1);
  }

  void theVectorExportWritesTheView() {
    Fixture f;
    f.make();
    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    ViewportController * v = f.app->viewport();
    v->setViewportSize(400, 300);
    for (int fmt = 0; fmt <= 5; fmt++) {
      const QString path = f.file("view.") + ViewportController::vectorExtension(fmt);
      QVERIFY2(v->exportVector(QUrl::fromLocalFile(path), fmt), qPrintable(path));
      QFile o(path);
      QVERIFY(o.open(QIODevice::ReadOnly));
      QVERIFY(o.size() > 50);
    }
    QFile svg(f.file("view.svg"));
    QVERIFY(svg.open(QIODevice::ReadOnly));
    const QByteArray s = svg.readAll();
    QVERIFY(s.contains("width=\"400pt\" height=\"300pt\""));
    QVERIFY(s.contains("<polygon"));
    QVERIFY(!v->exportVector(QUrl::fromLocalFile(f.file("no/such/dir/x.svg")), 4));
    QVERIFY(!v->exportVector(QUrl::fromLocalFile(f.file("x.svg")), 9));
  }

  void anUnsavedPuzzleExportsToTheHomeFolder() {
    Fixture f;
    f.make();
    f.app->document()->newDocument(0);
    QCOMPARE(f.app->stl()->folder(), QUrl::fromLocalFile(QDir::homePath()));
    QCOMPARE(f.app->stl()->suggestedName(), QStringLiteral("S1.stl"));
  }
};

// ---------------------------------------------------------------------------

class TestImageExport : public QObject {
  Q_OBJECT

  static bool haveGraphics(void) {
    static const bool ok = OffscreenRenderer().isValid();
    return ok;
  }

private slots:

  void startsWithLegacyDefaultsAndFallsBack() {
    Fixture f;
    f.make();
    ImageExportController * e = f.app->images();
    e->begin();
    QCOMPARE(e->mode(), QStringLiteral("shape"));     // nothing else on an empty puzzle
    QVERIFY(!e->canProblem());

    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    e->begin();
    QCOMPARE(e->mode(), QStringLiteral("solution"));
    QVERIFY(e->canSolution());
    QCOMPARE(e->supersampling(), 3);
    QCOMPARE(e->pixelX(), 300);
    QCOMPARE(e->pages(), 1);
    QVERIFY(e->frame(1).mesh);                        // the preview shows the assembly
    e->setMode(QStringLiteral("bogus"));
    QCOMPARE(e->mode(), QStringLiteral("solution"));
  }

  void paperSizesSetThePixels() {
    Fixture f;
    f.make();
    ImageExportController * e = f.app->images();
    e->begin();
    e->setPaper(QStringLiteral("a4p"));
    QCOMPARE(e->sizeXmm(), 210);
    QCOMPARE(e->pixelX(), 2480);
    QCOMPARE(e->pixelY(), 3508);
    e->setDpi(100);
    QCOMPARE(e->pixelX(), 827);
    e->setPaper(QStringLiteral("manual"));
    e->setSizeXmm(100);
    QCOMPARE(e->pixelX(), 394);
    e->setPixelX(50);                                 // pixels can be typed directly
    QCOMPARE(e->pixelX(), 50);
  }

  void aShapeBecomesOnePage() {
    GPU_OR_SKIP(haveGraphics());
    Fixture f;
    f.make();
    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    ImageExportController * e = f.app->images();
    e->begin();
    e->setMode(QStringLiteral("shape"));
    e->setShape(2);
    e->setSupersampling(2);
    e->setPixelX(400);
    e->setPixelY(200);
    QSignalSpy done(e, &ImageExportController::finished);
    e->start(QUrl::fromLocalFile(f.file("piece.png")));
    QVERIFY(e->busy());
    e->finish();
    QCOMPARE(done.count(), 1);
    QCOMPARE(e->writtenFiles(), QStringList{ f.file("piece000.png") });
    QImage page(f.file("piece000.png"));
    QCOMPARE(page.size(), QSize(400, 200));
    QCOMPARE(page.pixelColor(399, 199), QColor(Qt::white));
    // the picture starts at the left edge and is the piece's red
    bool red = false;
    for (int x = 0; x < 200 && !red; x++)
      for (int y = 0; y < 200 && !red; y++) {
        const QColor c = page.pixelColor(x, y);
        red = c.red() > c.blue() + 80 && c.red() > c.green() + 80;
      }
    QVERIFY(red);
    QCOMPARE(e->progressText(), QStringLiteral("Done"));
  }

  void aSolutionFillsThePagesAskedFor() {
    GPU_OR_SKIP(haveGraphics());
    Fixture f;
    f.make();
    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    ImageExportController * e = f.app->images();
    e->begin();
    e->setTransparent(true);
    e->setSupersampling(1);
    e->setDimStatic(true);
    e->setPixelX(600);
    e->setPixelY(400);
    e->setPages(2);
    e->start(QUrl::fromLocalFile(f.file("steps")));
    e->finish();
    const QStringList files = e->writtenFiles();
    QVERIFY(!files.isEmpty());
    QVERIFY(files.size() <= 2);
    QImage first(files.first());
    QCOMPARE(first.pixelColor(599, 399).alpha(), 0);  // transparent background
  }

  void anUnwritablePlaceFailsOnce() {
    GPU_OR_SKIP(haveGraphics());
    Fixture f;
    f.make();
    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    ImageExportController * e = f.app->images();
    e->begin();
    e->setMode(QStringLiteral("problem"));
    e->setSupersampling(1);
    e->setPages(3);
    QSignalSpy failed(e, &ImageExportController::failed);
    e->start(QUrl::fromLocalFile(f.file("no/such/dir/x.png")));
    e->finish();
    QCOMPARE(failed.count(), 1);
    QVERIFY(e->writtenFiles().isEmpty());
    QCOMPARE(e->progressText(), QStringLiteral("Failed"));
  }

  void cancelStopsTheExport() {
    GPU_OR_SKIP(haveGraphics());
    Fixture f;
    f.make();
    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    ImageExportController * e = f.app->images();
    e->begin();
    e->start(QUrl::fromLocalFile(f.file("c.png")));
    QVERIFY(e->busy());
    e->cancel();
    QVERIFY(!e->busy());
    QTest::qWait(50);
    QVERIFY(e->writtenFiles().isEmpty());
  }

  void bigPicturesAreDrawnInTiles() {
    OffscreenRenderer whole(OffscreenRenderer::Backend::Software), tiled(OffscreenRenderer::Backend::Software);
    GPU_OR_SKIP(whole.isValid());
    tiled.setMaxTile(64);
    Fixture f;
    f.make();
    QVERIFY(f.app->document()->loadPath(QStringLiteral("examples/PelikanBurr.xmpuzzle")));
    f.app->viewport()->setViewportSize(300, 200);
    const SceneFrame fr = f.app->viewport()->frame(1);
    const QImage a = whole.render(fr, QSize(300, 200)).convertToFormat(QImage::Format_ARGB32);
    const QImage b = tiled.render(fr, QSize(300, 200)).convertToFormat(QImage::Format_ARGB32);
    QCOMPARE(a.size(), b.size());
    int differ = 0;
    for (int y = 0; y < a.height(); y++)
      for (int x = 0; x < a.width(); x++) {
        const QColor p = a.pixelColor(x, y), q = b.pixelColor(x, y);
        if (std::abs(p.red() - q.red()) + std::abs(p.green() - q.green()) + std::abs(p.blue() - q.blue()) > 12)
          differ++;
      }
    // only the odd pixel on a tile seam may differ
    QVERIFY2(differ < a.width() * a.height() / 200, qPrintable(QString::number(differ)));
  }
};

// ---------------------------------------------------------------------------

int main(int argc, char ** argv) {
  /* Headless by default. BURRTOOLS_TEST_QPA picks another platform plugin:
   * Linux CI runs this binary as "xcb" under Xvfb, because Vulkan (Mesa's
   * lavapipe, for the render tests) needs an instance from a windowing
   * plugin and "offscreen" cannot provide one. */
  const QByteArray qpa = qgetenv("BURRTOOLS_TEST_QPA");
  if (!qpa.isEmpty())
    qputenv("QT_QPA_PLATFORM", qpa);
  else if (qgetenv("QT_QPA_PLATFORM").isEmpty())
    qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication app(argc, argv);
  bt_assert_init();
  Theme theme(nullptr);
  /* Qt Quick's RHI renderer even on the headless platform, which would pick
   * the software one: TestViewportItem draws the real 3D view item into an
   * offscreen QRhi. Chosen once per process, before any Qt Quick item. */
  QQuickWindow::setSceneGraphBackend(QStringLiteral("rhi"));

  /* QTest's console output does not reach a piped stdout with the MSYS2
   * MinGW Qt (meson's test log stayed empty, failures included), so each
   * class writes its plain-text results to a scratch file that is then
   * copied to stdout. With an explicit -o the caller's choice is used as is.
   */
  const QStringList baseArgs = QCoreApplication::arguments();
  const bool ownOutput = !baseArgs.contains(QStringLiteral("-o"));
  QTemporaryDir logDir;

  int failures = 0, classes = 0;
  auto run = [&](QObject && t) {
    classes++;
    QStringList args = baseArgs;
    const QString log = logDir.filePath(QStringLiteral("class%1.txt").arg(classes));
    if (ownOutput)
      args << QStringLiteral("-o") << log + QStringLiteral(",txt");
    failures += QTest::qExec(&t, args);
    if (ownOutput) {
      QFile f(log);
      if (f.open(QIODevice::ReadOnly))
        fwrite(f.readAll().constData(), 1, size_t(f.size()), stdout);
    }
  };
  {
    // which graphics API the render tests use here, for the CI log
    OffscreenRenderer probe(OffscreenRenderer::Backend::Software);
    printf("test_qtgui: platform %s, render tests on %s\n", qPrintable(QGuiApplication::platformName()),
           probe.isValid() ? qPrintable(probe.backendName() + QStringLiteral(" (") + probe.deviceName() + QStringLiteral(")"))
                           : "no graphics backend");
  }
  run(TestTheme());
  run(TestSettings());
  run(TestDocument());
  run(TestLayout());
  run(TestCommands());
  run(TestKeyboardCues());
  run(TestShapesAndStatus());
  run(TestIcons());
  run(TestTools());
  run(TestStlExport());
  run(TestImageExport());
  run(TestViewportController());
  run(TestViewCubePaint());
  run(TestRender());
  run(TestViewportItem());
  run(TestPipelineCache());
  printf("test_qtgui: %d test classes, %d failing test function(s)\n", classes, failures);
  return failures ? 1 : 0;
}

#include "test_qtgui.moc"
