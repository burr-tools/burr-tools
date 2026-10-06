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
#include "app.h"
#include "commandcontroller.h"
#include "documentcontroller.h"
#include "layoutcontroller.h"
#include "settingscontroller.h"
#include "shapesmodel.h"
#include "statuscontroller.h"
#include "theme.h"

#include "../uicore/windowplacement.h"

#include "../lib/puzzle.h"
#include "../lib/voxel.h"
#include "../tools/gzstream.h"
#include "../tools/xml.h"

#include <QDir>
#include <QFont>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QScreen>

#ifndef BURRTOOLS_VERSION
#define BURRTOOLS_VERSION "unknown"
#endif

namespace {
  App * s_app = nullptr;
}

App::App(const QString & settingsFile, const QString & legacySettingsFile, QObject * parent) :
  QObject(parent)
{
  s_app = this;

  // the theme first: the settings push the saved theme and density into it
  m_theme = Theme::instance() ? Theme::instance() : new Theme(this);

  // the spec's font stack (Segoe UI on Windows), not whatever the platform
  // plugin defaults to -- the headless test platform has no sensible default
  if (qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
    QFont font = QGuiApplication::font();
    font.setFamilies(m_theme->fontFamilies());
    QGuiApplication::setFont(font);
  }
  m_settings = new SettingsController(settingsFile, legacySettingsFile, this);
  m_document = new DocumentController(this);
  m_layout = new LayoutController(m_settings, this);
  m_status = new StatusController(this);
  m_shapes = new ShapesModel(m_document, this);
  m_viewport = new ViewportController(m_settings, m_document, m_shapes, m_layout, this);
  m_commands = new CommandController(m_settings, m_document, m_layout, m_viewport, this);
  m_tools = new ToolsController(m_document, this);
  m_stl = new StlExportController(m_settings, m_document, m_shapes, this);
  m_images = new ImageExportController(m_settings, m_document, m_shapes, this);
  m_cues = new KeyboardCues(this);
  m_popupStyle = new PopupWindowStyle(this);

  m_document->session().setUndoDepth(unsigned(m_settings->undoDepth()));
  connect(m_settings, &SettingsController::changed, this, [this] {
    m_document->session().setUndoDepth(unsigned(m_settings->undoDepth()));
  });

  connect(m_shapes, &ShapesModel::selectedChanged, this, &App::updateStatusText);
  connect(m_shapes, &ShapesModel::countChanged, this, &App::updateStatusText);
  connect(m_document, &DocumentController::documentReplaced, this, &App::updateStatusText);
  connect(m_document, &DocumentController::historyApplied, this, &App::updateStatusText);
  connect(m_document, &DocumentController::puzzleEdited, this, &App::updateStatusText);
  connect(m_layout, &LayoutController::changed, this, &App::updateStatusText);
  updateStatusText();
}

App::~App() {
  if (s_app == this)
    s_app = nullptr;
}

QVariantMap App::windowPlacement(int minWidth, int minHeight) const {
  std::optional<btui::WindowRect> saved;
  const QVariantMap g = m_settings->windowGeometry();
  if (!g.isEmpty())
    saved = btui::WindowRect{ g.value(QStringLiteral("x")).toInt(), g.value(QStringLiteral("y")).toInt(),
                              g.value(QStringLiteral("width")).toInt(), g.value(QStringLiteral("height")).toInt() };
  std::vector<btui::WindowRect> screens;
  for (const QScreen * s : QGuiApplication::screens()) {
    const QRect r = s->geometry();
    screens.push_back({ r.x(), r.y(), r.width(), r.height() });
  }
  const QRect a = availableGeometry();
  const btui::WindowPlacement p = btui::placeWindow(saved, g.value(QStringLiteral("maximized")).toBool(), screens,
                                                    { a.x(), a.y(), a.width(), a.height() }, minWidth, minHeight);
  return {
    { QStringLiteral("x"), p.rect.x }, { QStringLiteral("y"), p.rect.y },
    { QStringLiteral("width"), p.rect.width }, { QStringLiteral("height"), p.rect.height },
    { QStringLiteral("maximized"), p.maximized }, { QStringLiteral("restored"), p.restored },
  };
}

QStringList App::assetNames(const QString & folder) const {
  QStringList out;
  for (const QString & f : QDir(QStringLiteral(":/burrtools/") + folder).entryList({ QStringLiteral("*.svg") }, QDir::Files, QDir::Name))
    out.append(f.chopped(4));
  return out;
}

QRect App::availableGeometry(void) const {
  const QScreen * s = QGuiApplication::primaryScreen();
  return s ? s->availableGeometry() : QRect(0, 0, 1280, 800);
}

App * App::instance(void) {
  return s_app;
}

App * App::create(QQmlEngine *, QJSEngine *) {
  QJSEngine::setObjectOwnership(s_app, QJSEngine::CppOwnership);
  return s_app;
}

QString App::version(void) const {
  return QStringLiteral(BURRTOOLS_VERSION);
}

/* The live status sentence. Entities shows the selected shape's counts
 * (legacy StatPieceInfo); the Puzzle and Solver sentences arrive with those
 * workspaces.
 */
void App::updateStatusText(void) {
  const puzzle_c & p = m_document->session().puzzle();
  const int s = m_shapes->selected();
  if (m_layout->workspace() == LayoutController::Entities && s >= 0 && unsigned(s) < p.getNumberOfShapes()) {
    const voxel_c * v = p.getShape(unsigned(s));
    m_status->setLiveText(StatusController::shapeText(unsigned(s), v->countState(voxel_c::VX_FILLED),
                                                      v->countState(voxel_c::VX_VARIABLE)));
  } else {
    m_status->setLiveText(QString());
  }
}

QString App::rescueSave(void) {
  const char * name = "__rescue.xmpuzzle";
  ogzstream ostr(name);
  if (!ostr.rdbuf()->is_open())
    return QString();
  xmlWriter_c xml(ostr);
  m_document->session().puzzle().save(xml);
  ostr.close();
  return ostr ? QString::fromLatin1(name) : QString();
}

void App::handleInternalError(const std::exception & e) {
  QString saved = rescueSave();
  QString msg = QStringLiteral("I'm sorry there is a bug in this program. It needs to be closed.\n\n%1\n\n")
                  .arg(QString::fromLocal8Bit(e.what()));
  msg += saved.isEmpty() ? QStringLiteral("The puzzle could not be saved.")
                         : QStringLiteral("The current puzzle was saved to '%1'.").arg(saved);
  emit internalError(msg);
}
