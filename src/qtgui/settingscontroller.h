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
#ifndef BTQT_SETTINGSCONTROLLER_H
#define BTQT_SETTINGSCONTROLLER_H

#include "../uicore/settingsstore.h"

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

/* The persisted settings (spec C12 catalogue) and the UI state the spec
 * says to persist (layout collapse, display options), all in the new GUI's
 * own file, .burrtools-qt.rc (see btui::SettingsStore for why it is not the
 * legacy file).
 *
 * On the very first start -- when that file does not exist -- the user's
 * choices are taken over from the legacy .burrtools.rc, which is only ever
 * read. Every change is written straight away; there is no Apply.
 */
class SettingsController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY changed)
  Q_PROPERTY(QString density READ density WRITE setDensity NOTIFY changed)
  Q_PROPERTY(bool tooltips READ tooltips WRITE setTooltips NOTIFY changed)
  Q_PROPERTY(bool showViewCube READ showViewCube WRITE setShowViewCube NOTIFY changed)
  Q_PROPERTY(bool reverseScroll READ reverseScroll WRITE setReverseScroll NOTIFY changed)
  Q_PROPERTY(QString rotationMethod READ rotationMethod WRITE setRotationMethod NOTIFY changed)
  Q_PROPERTY(bool lighting READ lighting WRITE setLighting NOTIFY changed)
  /* "flat" (the redesign's outlined faces) or "legacy" (bevelled voxels in
   * a light / dark checker); a new setting, no legacy counterpart */
  Q_PROPERTY(QString voxelStyle READ voxelStyle WRITE setVoxelStyle NOTIFY changed)
  Q_PROPERTY(bool fadePieces READ fadePieces WRITE setFadePieces NOTIFY changed)
  /* legacy "Use openGL display lists": kept and carried over, but the new
   * renderer has no display lists, so nothing reads it */
  Q_PROPERTY(bool displayLists READ displayLists WRITE setDisplayLists NOTIFY changed)
  /* View > Show menu bar; off puts the menus behind the rail's menu button.
   * Window state, not a C12 setting: no reset touches it */
  Q_PROPERTY(bool showMenuBar READ showMenuBar WRITE setShowMenuBar NOTIFY changed)
  Q_PROPERTY(int undoDepth READ undoDepth WRITE setUndoDepth NOTIFY changed)
  Q_PROPERTY(int workerThreads READ workerThreads WRITE setWorkerThreads NOTIFY changed)
  Q_PROPERTY(int maxWorkerThreads READ maxWorkerThreads CONSTANT)

public:

  /* file: the settings file to use; legacyFile: the legacy settings to seed
   * it from when it does not exist yet. Both default to the locations the
   * two GUIs really use; tests pass their own.
   */
  explicit SettingsController(QString file = QString(), QString legacyFile = QString(), QObject * parent = nullptr);

  static QString defaultFile(void);
  /* the settings file in use (BURRTOOLS_QT_SETTINGS or the default);
   * caches that belong to it are kept beside it */
  QString file(void) const;
  static QString defaultLegacyFile(void);

  QString theme(void) const;
  void setTheme(const QString & v);
  QString density(void) const;
  void setDensity(const QString & v);
  bool tooltips(void) const;
  void setTooltips(bool v);
  bool showViewCube(void) const;
  void setShowViewCube(bool v);
  bool reverseScroll(void) const;
  void setReverseScroll(bool v);
  QString rotationMethod(void) const;
  void setRotationMethod(const QString & v);
  QString voxelStyle(void) const;
  void setVoxelStyle(const QString & v);
  bool lighting(void) const;
  void setLighting(bool v);
  bool fadePieces(void) const;
  void setFadePieces(bool v);
  bool displayLists(void) const;
  void setDisplayLists(bool v);
  bool showMenuBar(void) const;
  void setShowMenuBar(bool v);
  int undoDepth(void) const;
  void setUndoDepth(int v);
  int workerThreads(void) const;
  void setWorkerThreads(int v);
  int maxWorkerThreads(void) const;

  /* The undo depths the dropdown offers (C12): 25 50 100 200 500. Any other
   * value snaps to the nearest of these.
   */
  static int snapUndoDepth(int v);

  /* Generic access for the UI state other controllers persist (layout,
   * display options). Keys are dotted names.
   */
  bool boolValue(const QString & key, bool def) const;
  void setBoolValue(const QString & key, bool v);
  QString stringValue(const QString & key, const QString & def) const;
  void setStringValue(const QString & key, const QString & v);

  /* Settings ▸ Reset section / Restore all defaults. page is "general",
   * "view3d" or "performance".
   */
  Q_INVOKABLE void resetSection(const QString & page);

  /* The main window's place, as legacy's windowpos* keys kept it: { x, y,
   * width, height, maximized }, or an empty map before the first save. */
  Q_INVOKABLE QVariantMap windowGeometry(void) const;
  Q_INVOKABLE void saveWindowGeometry(int x, int y, int width, int height, bool maximized);
  Q_INVOKABLE void restoreAllDefaults(void);

  const btui::SettingsStore & store(void) const { return m_store; }

signals:

  void changed(void);

private:

  void seedFromLegacy(const QString & legacyFile);
  void write(void);
  void applyToTheme(void);

  btui::SettingsStore m_store;
};

#endif
