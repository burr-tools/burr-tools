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
#ifndef BTQT_COMMANDCONTROLLER_H
#define BTQT_COMMANDCONTROLLER_H

#include "../uicore/commands.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class DocumentController;
class LayoutController;
class SettingsController;
class ViewportController;

/* The application commands for QML: the menu bar and its menus, the
 * keyboard shortcuts, enabled states, and what each command does.
 *
 * Everything comes from btui::commandTable(); QML only renders it. A command
 * whose work needs a dialog QML owns (About, Edit comment, Settings) is
 * answered with a signal.
 */
class CommandController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  /* bumps whenever an enabled state may have changed; menu bindings read it
   * so they re-evaluate */
  Q_PROPERTY(int revision READ revision NOTIFY revisionChanged FINAL)
  /* a modal dialog is open: menu commands wait (Main.qml sets it) */
  Q_PROPERTY(bool blocked READ blocked WRITE setBlocked NOTIFY revisionChanged FINAL)
  /* The menu items handle their own first shortcut: macOS, where the menu
   * bar is the system's and its items are key equivalents. Elsewhere the
   * menus only display keys and Main.qml binds them all. */
  Q_PROPERTY(bool menuItemsOwnShortcuts READ menuItemsOwnShortcuts CONSTANT FINAL)
  /* View > Show menu bar exists: not on macOS, whose menu bar is the
   * system's and stays */
  Q_PROPERTY(bool menuBarHideable READ menuBarHideable CONSTANT FINAL)

public:

  CommandController(SettingsController * settings, DocumentController * doc, LayoutController * layout,
                    ViewportController * viewport, QObject * parent = nullptr);

  int revision(void) const { return m_revision; }
  bool blocked(void) const { return m_blocked; }
  void setBlocked(bool b);
  static bool menuItemsOwnShortcuts(void);
  static bool menuBarHideable(void);

  /* [{ key, label }] left to right; labels as the platform shows them */
  Q_INVOKABLE QVariantList menuBar(void) const;

  /* [{ key, label, shortcut (portable text, when the item owns it),
   * shortcutText (up to two, native text), checkable, separatorBefore }]
   * for one menu */
  Q_INVOKABLE QVariantList menuItems(const QString & menuKey) const;

  /* The key bindings the menu items do not own: commands outside the menus,
   * and the second and later sequences of those in them (F3 for Open,
   * Ctrl+Y for Redo): [{ key, sequences: [portable text] }]. Where menu
   * items only display their shortcuts (all but macOS), every sequence. */
  Q_INVOKABLE QVariantList shortcuts(void) const;

  /* a label as this platform shows it: access keys on Windows and Linux,
   * none on macOS; Windows' own wording where the table has one */
  static QString platformLabel(std::string_view label);
  static QString platformLabel(const btui::CommandInfo & c);

  Q_INVOKABLE bool isEnabled(const QString & key) const;

  /* a menu item with a check mark (View > Show menu bar), and its state */
  Q_INVOKABLE bool isCheckable(const QString & key) const;
  Q_INVOKABLE bool isChecked(const QString & key) const;

  /* Help ▸ Keyboard shortcuts (C22), the groups that work in this build:
   * [{ title, sub, rows: [{ action, keys: [alternative] }] }], where an
   * alternative is a list of parts { text, mouse } -- key caps, or mouse
   * gestures drawn as dashed tokens. Keys of commands come from the command
   * table in the platform's own notation. */
  Q_INVOKABLE QVariantList shortcutHelp(void) const;

  /* the key caps of one portable key sequence as this platform writes them:
   * "Ctrl+Shift+F" -> [Ctrl, Shift, F]; on macOS one cap, "⇧⌘F" */
  static QStringList keyCaps(std::string_view portable);

  /* run a command by its stable key; unknown keys are ignored */
  Q_INVOKABLE void trigger(const QString & key);

  /* the shortcuts a menu shows, up to two, in the platform's own text:
   * "Ctrl+O / F3" on Windows, "⌘O" on macOS */
  static QString nativeShortcutText(const btui::CommandInfo & c);

  /* the shortcut list for this platform */
  static std::span<const std::string_view> platformShortcuts(const btui::CommandInfo & c);

signals:

  void revisionChanged(void);

  void aboutRequested(void);
  void commentRequested(void);
  void settingsRequested(void);
  void shortcutsHelpRequested(void);
  void fullScreenToggleRequested(void);
  /* the legacy menu tools whose dialogs QML owns */
  void convertRequested(void);
  void importAssembliesRequested(void);
  void statusRequested(void);
  void stlExportRequested(void);
  void vectorExportRequested(void);
  void imageExportRequested(void);

private:

  void run(btui::Command c);

  SettingsController * m_settings;
  DocumentController * m_doc;
  LayoutController * m_layout;
  ViewportController * m_viewport;
  int m_revision = 0;
  bool m_blocked = false;
};

#endif
