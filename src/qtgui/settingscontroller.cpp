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
#include "settingscontroller.h"
#include "theme.h"

#include <QDir>
#include <QStandardPaths>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <thread>

namespace {

  // keys of the settings file; the legacy names they are seeded from are in
  // seedFromLegacy()
  const char * const kTheme       = "ui.theme";
  const char * const kDensity     = "ui.density";
  const char * const kTooltips    = "ui.tooltips";
  const char * const kUndoDepth   = "ui.undoDepth";
  const char * const kViewCube    = "view.cube";
  const char * const kReverse     = "view.reverseScroll";
  const char * const kRotation    = "view.rotationMethod";
  const char * const kVoxelStyle  = "view.voxelStyle";
  const char * const kLighting    = "view.lighting";
  const char * const kFade        = "view.fadePieces";
  const char * const kThreads     = "solver.threads";
  const char * const kDisplayLists = "view.displayLists";
  const char * const kMenuBar     = "ui.showMenuBar";
  const char * const kWinX        = "ui.window.x";
  const char * const kWinY        = "ui.window.y";
  const char * const kWinW        = "ui.window.width";
  const char * const kWinH        = "ui.window.height";
  const char * const kWinMax      = "ui.window.maximized";

  constexpr std::array<int, 5> undoDepths = { 25, 50, 100, 200, 500 };

  unsigned int hardwareThreads(void) {
    unsigned int hw = std::thread::hardware_concurrency();
    return hw ? std::min(hw, 256u) : 1u;
  }

  /* legacy configuration_c: 60 % of the cores, at least one */
  int defaultThreads(void) {
    return int(std::max(1u, hardwareThreads() * 6 / 10));
  }

  std::filesystem::path toPath(const QString & s) {
    return std::filesystem::path(s.toStdU16String());
  }

  /* Where legacy keeps .burrtools.rc: the "Personal" (Documents) shell
   * folder on Windows, the home directory elsewhere (src/tools/homedir.cpp).
   */
  QString settingsDir(void) {
#ifdef Q_OS_WIN
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#else
    return QDir::homePath();
#endif
  }
}

SettingsController::SettingsController(QString file, QString legacyFile, QObject * parent) :
  QObject(parent),
  m_store(toPath(file.isEmpty() ? defaultFile() : file))
{
  if (!m_store.load()) {
    seedFromLegacy(legacyFile.isEmpty() ? defaultLegacyFile() : legacyFile);
    write();
  }
  applyToTheme();
}

QString SettingsController::file(void) const {
  return QString::fromStdU16String(m_store.file().u16string());
}

QString SettingsController::defaultFile(void) {
  return QDir(settingsDir()).filePath(QStringLiteral(".burrtools-qt.rc"));
}

QString SettingsController::defaultLegacyFile(void) {
  return QDir(settingsDir()).filePath(QStringLiteral(".burrtools.rc"));
}

/* Take over what the legacy GUI's settings mean for this one. renderstyle
 * is deliberately not carried over: the main 3D view always uses the voxel
 * style (product decision 2026-10-05). displaylists is carried over though
 * the new renderer has no use for it, so every legacy setting is present
 * (C12 AC-01). The window position is not: legacy measured it with FLTK,
 * whose frame and scaling differ.
 */
void SettingsController::seedFromLegacy(const QString & legacyFile) {
  const auto legacy = btui::SettingsStore::parseFile(toPath(legacyFile));

  auto copyBool = [&](const char * from, const char * to) {
    auto it = legacy.find(from);
    if (it != legacy.end())
      if (auto b = std::get_if<bool>(&it->second))
        m_store.set(to, *b);
  };

  copyBool("tooltips", kTooltips);
  copyBool("lightning", kLighting);
  copyBool("fadeout", kFade);
  copyBool("reversescrollzoom", kReverse);
  copyBool("showviewcube", kViewCube);
  copyBool("displaylists", kDisplayLists);

  if (auto it = legacy.find("rotator"); it != legacy.end())
    if (auto b = std::get_if<bool>(&it->second))
      m_store.set(kRotation, std::string(*b ? "drag" : "arcball"));

  if (auto it = legacy.find("numthreads"); it != legacy.end())
    if (auto n = std::get_if<long long>(&it->second))
      m_store.set(kThreads, *n);

  // legacy stores an index into {25, 50, 100, 200}
  if (auto it = legacy.find("undodepth"); it != legacy.end())
    if (auto n = std::get_if<long long>(&it->second))
      if (*n >= 0 && *n < 4)
        m_store.set(kUndoDepth, (long long)undoDepths[size_t(*n)]);
}

void SettingsController::write(void) {
  m_store.save();
}

void SettingsController::applyToTheme(void) {
  if (Theme * t = Theme::instance()) {
    t->setMode(theme());
    t->setDensity(density());
  }
}

QString SettingsController::theme(void) const {
  // Light by default, as legacy looks (the spec's default is System)
  return QString::fromStdString(m_store.getString(kTheme, "light"));
}

void SettingsController::setTheme(const QString & v) {
  QString t = (v == QLatin1String("light") || v == QLatin1String("dark")) ? v : QStringLiteral("system");
  if (t == theme())
    return;
  m_store.set(kTheme, t.toStdString());
  write();
  applyToTheme();
  emit changed();
}

QString SettingsController::density(void) const {
  return QString::fromStdString(m_store.getString(kDensity, "standard"));
}

void SettingsController::setDensity(const QString & v) {
  QString d = (v == QLatin1String("minimal")) ? v : QStringLiteral("standard");
  if (d == density())
    return;
  m_store.set(kDensity, d.toStdString());
  write();
  applyToTheme();
  emit changed();
}

#define BOOL_SETTING(getter, setter, key, def)            \
  bool SettingsController::getter(void) const {           \
    return m_store.getBool(key, def);                     \
  }                                                       \
  void SettingsController::setter(bool v) {               \
    if (v == getter()) return;                            \
    m_store.set(key, v);                                  \
    write();                                              \
    emit changed();                                       \
  }

BOOL_SETTING(tooltips, setTooltips, kTooltips, true)
BOOL_SETTING(showViewCube, setShowViewCube, kViewCube, true)
BOOL_SETTING(reverseScroll, setReverseScroll, kReverse, false)
BOOL_SETTING(lighting, setLighting, kLighting, true)
BOOL_SETTING(fadePieces, setFadePieces, kFade, true)
BOOL_SETTING(displayLists, setDisplayLists, kDisplayLists, false)
BOOL_SETTING(showMenuBar, setShowMenuBar, kMenuBar, true)

#undef BOOL_SETTING

QString SettingsController::rotationMethod(void) const {
  return QString::fromStdString(m_store.getString(kRotation, "drag"));
}

void SettingsController::setRotationMethod(const QString & v) {
  QString m = (v == QLatin1String("arcball")) ? v : QStringLiteral("drag");
  if (m == rotationMethod())
    return;
  m_store.set(kRotation, m.toStdString());
  write();
  emit changed();
}

QString SettingsController::voxelStyle(void) const {
  return QString::fromStdString(m_store.getString(kVoxelStyle, "flat"));
}

void SettingsController::setVoxelStyle(const QString & v) {
  QString s = (v == QLatin1String("legacy")) ? v : QStringLiteral("flat");
  if (s == voxelStyle())
    return;
  m_store.set(kVoxelStyle, s.toStdString());
  write();
  emit changed();
}

int SettingsController::snapUndoDepth(int v) {
  int best = undoDepths[0];
  for (int d : undoDepths)
    if (std::abs(d - v) < std::abs(best - v))
      best = d;
  return best;
}

int SettingsController::undoDepth(void) const {
  return snapUndoDepth(int(m_store.getInt(kUndoDepth, 25)));
}

void SettingsController::setUndoDepth(int v) {
  int d = snapUndoDepth(v);
  if (d == undoDepth() && m_store.contains(kUndoDepth))
    return;
  m_store.set(kUndoDepth, (long long)d);
  write();
  emit changed();
}

int SettingsController::maxWorkerThreads(void) const {
  return int(hardwareThreads());
}

int SettingsController::workerThreads(void) const {
  long long n = m_store.getInt(kThreads, defaultThreads());
  return int(std::clamp<long long>(n, 1, maxWorkerThreads()));
}

void SettingsController::setWorkerThreads(int v) {
  int n = std::clamp(v, 1, maxWorkerThreads());
  if (n == workerThreads() && m_store.contains(kThreads))
    return;
  m_store.set(kThreads, (long long)n);
  write();
  emit changed();
}

bool SettingsController::boolValue(const QString & key, bool def) const {
  return m_store.getBool(key.toStdString(), def);
}

void SettingsController::setBoolValue(const QString & key, bool v) {
  const std::string k = key.toStdString();
  if (m_store.getBool(k) == v)
    return;
  m_store.set(k, v);
  write();
}

QString SettingsController::stringValue(const QString & key, const QString & def) const {
  return QString::fromStdString(m_store.getString(key.toStdString(), def.toStdString()));
}

void SettingsController::setStringValue(const QString & key, const QString & v) {
  const std::string k = key.toStdString();
  if (m_store.getString(k) == v.toStdString())
    return;
  m_store.set(k, v.toStdString());
  write();
}

void SettingsController::resetSection(const QString & page) {
  if (page == QLatin1String("general")) {
    for (const char * k : { kTheme, kDensity, kTooltips, kUndoDepth })
      m_store.remove(k);
  } else if (page == QLatin1String("view3d")) {
    for (const char * k : { kViewCube, kReverse, kRotation, kVoxelStyle, kLighting, kFade })
      m_store.remove(k);
  } else if (page == QLatin1String("performance")) {
    m_store.remove(kThreads);
    m_store.remove(kDisplayLists);
  } else {
    return;
  }
  write();
  applyToTheme();
  emit changed();
}

void SettingsController::restoreAllDefaults(void) {
  for (const char * p : { "general", "view3d", "performance" })
    resetSection(QString::fromLatin1(p));
}

QVariantMap SettingsController::windowGeometry(void) const {
  QVariantMap m;
  if (!m_store.contains(kWinW) || !m_store.contains(kWinH))
    return m;
  m.insert(QStringLiteral("x"), int(m_store.getInt(kWinX, 0)));
  m.insert(QStringLiteral("y"), int(m_store.getInt(kWinY, 0)));
  m.insert(QStringLiteral("width"), int(m_store.getInt(kWinW, 0)));
  m.insert(QStringLiteral("height"), int(m_store.getInt(kWinH, 0)));
  m.insert(QStringLiteral("maximized"), m_store.getBool(kWinMax, false));
  return m;
}

void SettingsController::saveWindowGeometry(int x, int y, int width, int height, bool maximized) {
  m_store.set(kWinMax, maximized);
  // a maximised window keeps the size it returns to
  if (!maximized && width > 0 && height > 0) {
    m_store.set(kWinX, (long long)x);
    m_store.set(kWinY, (long long)y);
    m_store.set(kWinW, (long long)width);
    m_store.set(kWinH, (long long)height);
  }
  write();
}
