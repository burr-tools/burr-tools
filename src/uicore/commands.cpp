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
#include "commands.h"

#include "../lib/bt_assert.h"

#include <array>

namespace btui {

  namespace {

    using sv = std::string_view;

    /* Shortcut lists. The legacy F-keys stay (F2 save, F3 open) next to the
     * platform-standard accelerators; Toggle 3D's F4 is gone with Toggle 3D.
     *
     * Full screen follows each platform's own convention -- F11 on Windows
     * and Linux, Control+Command+F on macOS, where F11 shows the desktop --
     * and Focus 3D, a layout change rather than a window state, gets its own
     * key. The spec's Ctrl+Space is not used: macOS reserves it for switching
     * the input source and Windows input methods for Chinese and Japanese
     * take it too.
     */
    constexpr sv scNew[]         = { "Ctrl+N" };
    constexpr sv scOpen[]        = { "Ctrl+O", "F3" };
    constexpr sv scSave[]        = { "Ctrl+S", "F2" };
    constexpr sv scSaveAs[]      = { "Ctrl+Shift+S" };
    constexpr sv scQuit[]        = { "Ctrl+Q" };
    constexpr sv scUndo[]        = { "Ctrl+Z" };
    constexpr sv scRedo[]        = { "Ctrl+Shift+Z", "Ctrl+Y" };
    constexpr sv scRedoMac[]     = { "Ctrl+Shift+Z" };
    constexpr sv scShortcuts[]   = { "F1" };
    // legacy's macOS menu: Close Cmd+W, Status Cmd+I, User Guide Cmd+?
    constexpr sv scCloseMac[]    = { "Ctrl+W" };
    constexpr sv scStatusMac[]   = { "Ctrl+I" };
    constexpr sv scGuideMac[]    = { "Ctrl+?" };
    constexpr sv scSettings[]    = { "Ctrl+," };
    constexpr sv scFullScreen[]  = { "F11" };
    constexpr sv scFullScrMac[]  = { "Ctrl+Meta+F" };
    constexpr sv scFocus3d[]     = { "Ctrl+Shift+F" };
    constexpr sv scEntities[]    = { "Ctrl+1" };
    constexpr sv scPuzzle[]      = { "Ctrl+2" };
    constexpr sv scSolver[]      = { "Ctrl+3" };
    constexpr sv scLeftCard[]    = { "Ctrl+[" };
    constexpr sv scRightCard[]   = { "Ctrl+]" };
    constexpr sv scOrbit[]       = { "O" };
    constexpr sv scPan[]         = { "P" };
    constexpr sv scHome[]        = { "Home" };
    constexpr sv scFit[]         = { "F" };
    constexpr sv scLayerUp[]     = { "PgUp" };
    constexpr sv scLayerDown[]   = { "PgDown" };

    constexpr std::span<const sv> none;

    /* Labels carry Windows/Linux access keys ("&File"); macOS drops them.
     * winLabel replaces label on Windows, where Quit is "Exit".
     *
     * macOS moves three items into the application menu by their titles
     * (Qt's text heuristic): Settings becomes BurrTools > Preferences... with
     * Cmd+,, About BurrTools and Quit follow it. Keep those titles.
     */
    const CommandInfo table[] = {
      { Command::New,              "file.new",              "&New…",                  Menu::File,   false, scNew,        none,       "New", "", "", Menu::None },
      { Command::Open,             "file.open",             "&Open…",                 Menu::File,   false, scOpen,       none,       "Load", "", "", Menu::None },
      { Command::Import,           "file.import",           "&Import PuzzleSolver3D…", Menu::File,  false, none,         none,       "Load_Ps3d", "", "", Menu::None },
      // macOS only, where a window closes with Cmd+W; the one window closing quits, as legacy's Close did
      { Command::Close,            "file.close",            "Close",                  Menu::None,   true,  none,         scCloseMac, "", "", "", Menu::File },
      { Command::Save,             "file.save",             "&Save",                  Menu::File,   false,  scSave,       none,       "Save", "", "", Menu::None },
      { Command::SaveAs,           "file.saveAs",           "Save &as…",              Menu::File,   false, scSaveAs,     none,       "SaveAs", "", "", Menu::None },
      { Command::Convert,          "file.convert",          "&Convert grid type…",    Menu::File,   true,  none,         none,       "Convert", "", "", Menu::None },
      { Command::ImportAssemblies, "file.importAssemblies", "Import asse&mblies…",    Menu::File,   false, none,         none,       "AssembliesToShapes", "", "", Menu::None },
      { Command::Settings,         "settings",              "Se&ttings…",             Menu::File,   true,  scSettings,   none,       "Config", "", "", Menu::None },
      { Command::Quit,             "file.quit",             "&Quit",                  Menu::File,   true,  scQuit,       none,       "Quit", "E&xit", "", Menu::None },

      { Command::Undo,             "edit.undo",             "&Undo",                  Menu::Edit,   false, scUndo,       none,       "Undo", "", "", Menu::None },
      { Command::Redo,             "edit.redo",             "&Redo",                  Menu::Edit,   false, scRedo,       scRedoMac,  "Redo", "", "", Menu::None },
      { Command::EditComment,      "editcomment",           "Edit &comment…",         Menu::Edit,   true,  none,         none,       "Comment", "", "", Menu::None },

      { Command::ToggleMenuBar,    "view.menuBar",          "Show &menu bar",         Menu::View,   false, none,         none,       "", "", "", Menu::None },
      // macOS: in File with Cmd+I, the place and key of Get Info (legacy: Cmd+I)
      { Command::Status,           "status",                "&Status…",               Menu::View,   true,  none,         scStatusMac, "StatusWindow", "", "", Menu::File },
      { Command::FullScreen,       "view.fullScreen",       "&Full screen",           Menu::View,   true,  scFullScreen, scFullScrMac, "", "", "", Menu::None },
      { Command::Focus3d,          "view.focus3d",          "Focus &3D view",         Menu::View,   false, scFocus3d,    none,       "", "", "", Menu::None },
      { Command::WorkspaceEntities,"workspace.entities",    "&Entities",              Menu::View,   true,  scEntities,   none,       "", "", "", Menu::None },
      { Command::WorkspacePuzzle,  "workspace.puzzle",      "&Puzzle",                Menu::View,   false, scPuzzle,     none,       "", "", "", Menu::None },
      { Command::WorkspaceSolver,  "workspace.solver",      "S&olver",                Menu::View,   false, scSolver,     none,       "", "", "", Menu::None },
      { Command::ToggleLeftCard,   "layout.toggleLeft",     "Collapse or expand the &left card",  Menu::View, true,  scLeftCard,  none, "", "", "", Menu::None },
      { Command::ToggleRightCard,  "layout.toggleRight",    "Collapse or expand the &right card", Menu::View, false, scRightCard, none, "", "", "", Menu::None },

      { Command::ExportImage,      "export.image",          "&Image…",                Menu::Export, false, none,         none,       "ImageExport", "", "", Menu::None },
      { Command::ExportVector,     "export.vector",         "&Vector image…",         Menu::Export, false, none,         none,       "ImageExportVector", "", "", Menu::None },
      { Command::ExportStl,        "export.stl",            "S&TL…",                  Menu::Export, false, none,         none,       "STLExport", "", "", Menu::None },

      // macOS: the app's own help, first in Help with Cmd+? (legacy: "BurrTools User Guide", Cmd+?)
      { Command::HelpGuide,        "help.guide",            "&User guide",            Menu::Help,   false, none,         scGuideMac, "Help", "", "BurrTools Help", Menu::None },
      { Command::HelpShortcuts,    "help.shortcuts",        "&Keyboard shortcuts",    Menu::Help,   false, scShortcuts,  none,       "", "", "", Menu::None },
      { Command::About,            "about",                 "&About BurrTools",       Menu::Help,   true,  none,         none,       "About", "", "", Menu::None },

      { Command::ViewOrbit,        "view.orbit",            "Orbit mode",             Menu::None,   false, scOrbit,      none,       "", "", "", Menu::None },
      { Command::ViewPan,          "view.pan",              "Pan mode",               Menu::None,   false, scPan,        none,       "", "", "", Menu::None },
      { Command::ViewHome,         "view.home",             "Home view",              Menu::None,   false, scHome,       none,       "", "", "", Menu::None },
      { Command::ViewFit,          "view.fit",              "Fit to view",            Menu::None,   false, scFit,        none,       "", "", "", Menu::None },
      { Command::LayerUp,          "editor.layerUp",        "Next layer",             Menu::None,   false, scLayerUp,    none,       "", "", "", Menu::None },
      { Command::LayerDown,        "editor.layerDown",      "Previous layer",         Menu::None,   false, scLayerDown,  none,       "", "", "", Menu::None },
    };

    /* The menu bar, left to right. Legacy's File, Edit, Export and Help
     * stay; its menu-bar buttons that ran a command directly (Status, Edit
     * comment, About) became items, because native menu bars -- the macOS
     * one above all -- hold only menus: Status in View, Edit comment in
     * Edit, About in Help. Settings (legacy Config) is in File. View is new
     * and gathers the window and layout commands.
     */
    const MenuBarEntry menus[] = {
      { "file",   "&File",   Menu::File },
      { "edit",   "&Edit",   Menu::Edit },
      { "view",   "&View",   Menu::View },
      { "export", "E&xport", Menu::Export },
      { "help",   "&Help",   Menu::Help },
    };

    constexpr sv dropped[] = { "Toggle3D" };
  }

  std::span<const CommandInfo> commandTable(void) {
    return table;
  }

  const CommandInfo & commandInfo(Command c) {
    for (const auto & i : table)
      if (i.id == c)
        return i;
    bt_te("command missing from the command table");
  }

  const CommandInfo * findCommand(std::string_view key) {
    for (const auto & i : table)
      if (i.key == key)
        return &i;
    return nullptr;
  }

  std::span<const MenuBarEntry> menuBar(void) {
    return menus;
  }

  Platform currentPlatform(void) {
#if defined(_WIN32)
    return Platform::Windows;
#elif defined(__APPLE__)
    return Platform::Mac;
#else
    return Platform::Linux;
#endif
  }

  Menu menuOn(const CommandInfo & c, Platform p) {
    if (p == Platform::Mac && c.macMenu != Menu::None)
      return c.macMenu;
    // macOS keeps its system menu bar: there is nothing to hide
    if (p == Platform::Mac && c.id == Command::ToggleMenuBar)
      return Menu::None;
    return c.menu;
  }

  std::string_view labelOn(const CommandInfo & c, Platform p) {
    if (p == Platform::Windows && !c.winLabel.empty())
      return c.winLabel;
    if (p == Platform::Mac && !c.macLabel.empty())
      return c.macLabel;
    return c.label;
  }

  std::span<const std::string_view> shortcutsOn(const CommandInfo & c, Platform p) {
    if (p == Platform::Mac && !c.macShortcuts.empty())
      return c.macShortcuts;
    return c.shortcuts;
  }

  std::vector<MenuEntry> menuEntries(Menu m, Platform p) {
    std::vector<MenuEntry> out;
    bool pendingSeparator = false;
    for (const auto & c : table) {
      if (c.menu != m && c.macMenu != m)
        continue;
      if (menuOn(c, p) != m) {
        // left out here: its divider still separates what comes next
        pendingSeparator = pendingSeparator || c.separatorBefore;
        continue;
      }
      out.push_back({ &c, !out.empty() && (c.separatorBefore || pendingSeparator) });
      pendingSeparator = false;
    }
    return out;
  }

  std::string plainLabel(std::string_view label) {
    std::string out;
    for (size_t i = 0; i < label.size(); i++) {
      if (label[i] == '&') {
        if (i + 1 < label.size() && label[i + 1] == '&')
          out += label[++i];
        continue;
      }
      out += label[i];
    }
    return out;
  }

  std::span<const std::string_view> droppedLegacyCallbacks(void) {
    return dropped;
  }
}
