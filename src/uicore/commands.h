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
#ifndef BTUI_COMMANDS_H
#define BTUI_COMMANDS_H

#include <span>
#include <string>
#include <string_view>
#include <vector>

/* The application commands of the redesigned GUI and the menus that show
 * them.
 *
 * One table is the single source for menu labels, menu order, keyboard
 * shortcuts and the stable ids that tests and the QML side use, so a command
 * cannot exist in a menu without a shortcut entry being considered, or be
 * renamed in one place and not another. The toolkit layer turns each entry
 * into a menu item and a shortcut; it never invents a command of its own.
 */
namespace btui {

  enum class Command {
    // File
    New, Open, Import, Close, Save, SaveAs, Convert, ImportAssemblies, Settings, Quit,
    // Edit
    Undo, Redo, EditComment,
    // View
    ToggleMenuBar, Status, FullScreen, Focus3d,
    WorkspaceEntities, WorkspacePuzzle, WorkspaceSolver,
    ToggleLeftCard, ToggleRightCard,
    // Export
    ExportImage, ExportVector, ExportStl,
    // Help
    HelpGuide, HelpShortcuts, About,
    // not in a menu: the 3D view (every workspace) and the active layer (Entities)
    ViewOrbit, ViewPan, ViewHome, ViewFit,
    LayerUp, LayerDown,
  };

  /* Where a command appears. Menu::None means keyboard / buttons only. */
  enum class Menu { None, File, Edit, View, Export, Help };

  /* The menus differ a little per platform, each following its own
   * conventions (labels, places, shortcuts). */
  enum class Platform { Windows, Mac, Linux };

  /* the platform this build runs on */
  Platform currentPlatform(void);

  struct CommandInfo {
    Command id = Command::New;
    std::string_view key;          ///< stable id, e.g. "file.new"; also the QML objectName suffix
    std::string_view label;        ///< menu text, sentence case per the spec, "&" marks the access key
    Menu menu = Menu::None;
    bool separatorBefore = false;  ///< draw a divider above this item in its menu
    /* Shortcuts in Qt's portable text form. On macOS Qt reads "Ctrl" as
     * Command and "Meta" as Control, so "Ctrl+S" becomes Cmd+S there. The
     * mac list replaces the portable one when it is non-empty.
     */
    std::span<const std::string_view> shortcuts;
    std::span<const std::string_view> macShortcuts;
    /* The legacy FLTK menu callback this command replaces (the name between
     * "cb_" and "_stub" in src/gui/mainmenu.cpp), or empty if it is new.
     */
    std::string_view legacyCallback;
    std::string_view winLabel;     ///< the label on Windows when it differs ("E&xit")
    std::string_view macLabel;     ///< the label on macOS when it differs ("BurrTools Help")
    /* the menu on macOS when it differs (Status, Close: File); Menu::None
     * means the same menu as elsewhere */
    Menu macMenu = Menu::None;
  };

  /* The menu, label and shortcuts of a command on a platform. */
  Menu menuOn(const CommandInfo & c, Platform p);
  std::string_view labelOn(const CommandInfo & c, Platform p);
  std::span<const std::string_view> shortcutsOn(const CommandInfo & c, Platform p);

  /* One menu's items on a platform, in order. A divider belonging to an
   * item the platform leaves out moves to the next item shown. */
  struct MenuEntry {
    const CommandInfo * command = nullptr;
    bool separatorBefore = false;
  };
  std::vector<MenuEntry> menuEntries(Menu m, Platform p);

  /* Every command, in menu order. */
  std::span<const CommandInfo> commandTable(void);

  /* Lookup by id; never fails for a valid enumerator. */
  const CommandInfo & commandInfo(Command c);

  /* Lookup by stable key; nullptr when there is no such command. */
  const CommandInfo * findCommand(std::string_view key);

  /* The menus of the menu bar, in order. */
  struct MenuBarEntry {
    std::string_view key;          ///< "file", "edit", ... -> objectName "shell.menu." + key
    std::string_view label;        ///< with its access key, "&File"
    Menu menu = Menu::None;
  };
  std::span<const MenuBarEntry> menuBar(void);

  /* a label without its access-key markers ("Save &as…" -> "Save as…") */
  std::string plainLabel(std::string_view label);

  /* Legacy menu callbacks the redesign has no command for. The menu-table
   * drift check skips exactly these: Toggle 3D, removed on purpose -- the
   * Focus modes now do its job (product decision, 2026-10-05) -- and Check
   * for Updates, added to the legacy GUI in #137 and not ported yet.
   */
  std::span<const std::string_view> droppedLegacyCallbacks(void);
}

#endif
