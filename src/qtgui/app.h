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
#ifndef BTQT_APP_H
#define BTQT_APP_H

// the controllers are Q_PROPERTY types, which moc needs complete
#include "commandcontroller.h"
#include "documentcontroller.h"
#include "imageexportcontroller.h"
#include "keyboardcues.h"
#include "popupwindowstyle.h"
#include "layoutcontroller.h"
#include "settingscontroller.h"
#include "shapesmodel.h"
#include "statuscontroller.h"
#include "stlexportcontroller.h"
#include "toolscontroller.h"
#include "viewportcontroller.h"

#include <QObject>
#include <QRect>
#include <QVariantMap>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <exception>

class QJSEngine;
class QQmlEngine;
class Theme;

/* The root of the redesigned GUI's controllers, reachable from QML as the
 * singleton App (App.document, App.layout, ...).
 *
 * One App is created in main() before QML loads and owns every controller,
 * so their lifetime is C++'s, not the QML engine's. Tests create their own
 * with a scratch settings file.
 */
class App : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(SettingsController * settings READ settings CONSTANT)
  Q_PROPERTY(DocumentController * document READ document CONSTANT)
  Q_PROPERTY(LayoutController * layout READ layout CONSTANT)
  Q_PROPERTY(StatusController * status READ status CONSTANT)
  Q_PROPERTY(ShapesModel * shapes READ shapes CONSTANT)
  Q_PROPERTY(CommandController * commands READ commands CONSTANT)
  Q_PROPERTY(ViewportController * viewport READ viewport CONSTANT)
  Q_PROPERTY(ToolsController * tools READ tools CONSTANT)
  Q_PROPERTY(StlExportController * stl READ stl CONSTANT)
  Q_PROPERTY(ImageExportController * images READ images CONSTANT)
  Q_PROPERTY(KeyboardCues * keyboardCues READ keyboardCues CONSTANT)
  Q_PROPERTY(PopupWindowStyle * popupStyle READ popupStyle CONSTANT)
  Q_PROPERTY(QString version READ version CONSTANT)

public:

  /* settingsFile / legacySettingsFile: empty for the real locations.
   *
   * Deliberately not default-constructible: for a QML_SINGLETON Qt prefers
   * a default constructor over create(), and would then build a second App
   * of its own instead of using this one.
   */
  App(const QString & settingsFile, const QString & legacySettingsFile, QObject * parent = nullptr);
  ~App() override;

  static App * instance(void);
  static App * create(QQmlEngine *, QJSEngine *);

  SettingsController * settings(void) const { return m_settings; }
  DocumentController * document(void) const { return m_document; }
  LayoutController * layout(void) const { return m_layout; }
  StatusController * status(void) const { return m_status; }
  ShapesModel * shapes(void) const { return m_shapes; }
  CommandController * commands(void) const { return m_commands; }
  ViewportController * viewport(void) const { return m_viewport; }
  ToolsController * tools(void) const { return m_tools; }
  StlExportController * stl(void) const { return m_stl; }
  ImageExportController * images(void) const { return m_images; }
  KeyboardCues * keyboardCues(void) const { return m_cues; }
  PopupWindowStyle * popupStyle(void) const { return m_popupStyle; }

  /* the primary screen's area for windows (without taskbar or dock) */
  Q_INVOKABLE QRect availableGeometry(void) const;

  /* Where the main window opens (btui::placeWindow): the saved place if it
   * is still on a screen, else centred on the free area.
   * { x, y, width, height, maximized, restored } */
  Q_INVOKABLE QVariantMap windowPlacement(int minWidth, int minHeight) const;

  /* The SVG assets in one resource folder ("icons", "glyphs/light"), by
   * name without ".svg", sorted -- the component gallery's contact sheets. */
  Q_INVOKABLE QStringList assetNames(const QString & folder) const;
  QString version(void) const;

  /* Called after an internal error (a bt_assert) reached the GUI: save the
   * puzzle to __rescue.xmpuzzle the way legacy main() does, so no work is
   * lost. Returns the path written, or empty if that failed.
   */
  QString rescueSave(void);

  /* An internal error (a bt_assert) reached the GUI. Exceptions must not
   * unwind through Qt or the QML engine, so every entry point from QML that
   * reaches the library catches them (guarded.h) and lands here:
   * rescue-save, then tell the user, after which QML quits (legacy main()
   * does the same in its catch block).
   */
  void handleInternalError(const std::exception & e);

signals:

  void internalError(const QString & message);

private:

  void updateStatusText(void);

  Theme * m_theme;
  SettingsController * m_settings;
  DocumentController * m_document;
  LayoutController * m_layout;
  StatusController * m_status;
  ShapesModel * m_shapes;
  CommandController * m_commands;
  ViewportController * m_viewport;
  ToolsController * m_tools;
  StlExportController * m_stl;
  ImageExportController * m_images;
  KeyboardCues * m_cues;
  PopupWindowStyle * m_popupStyle;
};

#endif
