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

#include <algorithm>
#include <array>
#include <map>
#include <cctype>

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
    constexpr sv scEdit3d[]      = { "E" };
    // the focused 3D view's own keys (Scope::Viewport)
    constexpr sv scOrbitLeft[]   = { "Left" };
    constexpr sv scOrbitRight[]  = { "Right" };
    constexpr sv scOrbitUp[]     = { "Up" };
    constexpr sv scOrbitDown[]   = { "Down" };
    constexpr sv scPanLeft[]     = { "Shift+Left" };
    constexpr sv scPanRight[]    = { "Shift+Right" };
    constexpr sv scPanUp[]       = { "Shift+Up" };
    constexpr sv scPanDown[]     = { "Shift+Down" };
    constexpr sv scZoomIn[]      = { "+", "=" };
    constexpr sv scZoomOut[]     = { "-" };
    constexpr sv scTurnLeft[]    = { "Ctrl+Left" };
    constexpr sv scTurnRight[]   = { "Ctrl+Right" };
    constexpr sv scTurnUp[]      = { "Ctrl+Up" };
    constexpr sv scTurnDown[]    = { "Ctrl+Down" };
    // macOS keeps Control+arrows for Mission Control and its spaces
    constexpr sv scTurnLeftMac[]  = { "Alt+Left" };
    constexpr sv scTurnRightMac[] = { "Alt+Right" };
    constexpr sv scTurnUpMac[]    = { "Alt+Up" };
    constexpr sv scTurnDownMac[]  = { "Alt+Down" };
    constexpr sv scRollLeft[]    = { "Ctrl+Shift+Left" };
    constexpr sv scRollRight[]   = { "Ctrl+Shift+Right" };
    constexpr sv scRollLeftMac[]  = { "Alt+Shift+Left" };
    constexpr sv scRollRightMac[] = { "Alt+Shift+Right" };
    constexpr sv scFront[]       = { "Alt+1" };
    constexpr sv scBack[]        = { "Alt+2" };
    constexpr sv scLeft[]        = { "Alt+3" };
    constexpr sv scRight[]       = { "Alt+4" };
    constexpr sv scTop[]         = { "Alt+5" };
    constexpr sv scBottom[]      = { "Alt+6" };
    constexpr sv scLayerUp[]     = { "PgUp" };
    constexpr sv scLayerDown[]   = { "PgDown" };
    // the tool numbers used everywhere (C07): keys, badges, quick tools
    constexpr sv scTool1[]       = { "1" };
    constexpr sv scTool2[]       = { "2" };
    constexpr sv scTool3[]       = { "3" };
    constexpr sv scTool4[]       = { "4" };
    /* The Shapes list's. Rename is Enter -- the main key (Qt's "Return")
     * and the keypad's alike -- not the spec's F2, which stays Save (product
     * decision, 2026-10-27). macOS deletes with Backspace, as its lists do. */
    constexpr sv scShapeRename[] = { "Enter" };
    // macOS names that key Return (↩); its "Enter" is the keypad's ⌤
    constexpr sv scShapeRenameMac[] = { "Return" };
    constexpr sv scShapeDup[]    = { "Ctrl+D" };
    constexpr sv scShapeDel[]    = { "Del" };
    constexpr sv scShapeDelMac[] = { "Backspace", "Del" };
    constexpr sv scShapeEarlier[] = { "Alt+Up" };
    constexpr sv scShapeLater[]  = { "Alt+Down" };

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
      { Command::Import,           "file.import",           "&Import…",              Menu::File,  false, none,         none,       "Load_Ps3d", "", "", Menu::None },
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
      { Command::ToggleLeftCard,   "layout.toggleLeft",     "Collapse or expand the &left sidebar",  Menu::View, true,  scLeftCard,  none, "", "", "", Menu::None },
      { Command::ToggleRightCard,  "layout.toggleRight",    "Collapse or expand the &right sidebar", Menu::View, false, scRightCard, none, "", "", "", Menu::None },

      { Command::ExportImage,      "export.image",          "&Image…",                Menu::Export, false, none,         none,       "ImageExport", "", "", Menu::None },
      { Command::ExportVector,     "export.vector",         "&Vector image…",         Menu::Export, false, none,         none,       "ImageExportVector", "", "", Menu::None },
      { Command::ExportStl,        "export.stl",            "S&TL…",                  Menu::Export, false, none,         none,       "STLExport", "", "", Menu::None },

      // macOS: the app's own help, first in Help with Cmd+? (legacy: "BurrTools User Guide", Cmd+?)
      { Command::HelpGuide,        "help.guide",            "&User guide",            Menu::Help,   false, none,         scGuideMac, "Help", "", "BurrTools Help", Menu::None },
      { Command::HelpShortcuts,    "help.shortcuts",        "&Keyboard shortcuts",    Menu::Help,   false, scShortcuts,  none,       "", "", "", Menu::None },
      { Command::CheckForUpdates,  "help.checkForUpdates",  "&Check for updates…",   Menu::Help,   true,  none,         none,       "CheckForUpdates", "", "", Menu::None },
      { Command::About,            "about",                 "&About BurrTools",       Menu::Help,   false, none,         none,       "About", "", "", Menu::None },

      { Command::ViewOrbit,        "view.orbit",            "Orbit mode",             Menu::None,   false, scOrbit,      none,       "", "", "", Menu::None },
      { Command::ViewPan,          "view.pan",              "Pan mode",               Menu::None,   false, scPan,        none,       "", "", "", Menu::None },
      { Command::ViewHome,         "view.home",             "Home view",              Menu::None,   false, scHome,       none,       "", "", "", Menu::None },
      { Command::ViewFit,          "view.fit",              "Fit to view",            Menu::None,   false, scFit,        none,       "", "", "", Menu::None },
      // C06: Edit mode, the latched tool of the 3D view
      { Command::Edit3d,           "view.edit3d",           "3D edit mode",           Menu::None,   false, scEdit3d,     none,       "", "", "", Menu::None },
      { Command::LayerUp,          "editor.layerUp",        "Next layer",             Menu::None,   false, scLayerUp,    none,       "", "", "", Menu::None },
      { Command::LayerDown,        "editor.layerDown",      "Previous layer",         Menu::None,   false, scLayerDown,  none,       "", "", "", Menu::None },
      // replace legacy's tool buttons (L60), not menu items
      { Command::ToolFixed,        "editor.tool.fixed",     "Fixed voxel",            Menu::None,   false, scTool1,      none,       "", "", "", Menu::None },
      { Command::ToolVariable,     "editor.tool.variable",  "Variable voxel",         Menu::None,   false, scTool2,      none,       "", "", "", Menu::None },
      { Command::ToolErase,        "editor.tool.erase",     "Erase",                  Menu::None,   false, scTool3,      none,       "", "", "", Menu::None },
      { Command::ToolPaint,        "editor.tool.paint",     "Paint",                  Menu::None,   false, scTool4,      none,       "", "", "", Menu::None },

      // replace legacy's shape buttons (Label, Copy, Delete, <-, ->), not menu items
      { Command::ShapeRename,      "shapes.rename",         "Rename…",                Menu::None,   false, scShapeRename, scShapeRenameMac, "", "", "", Menu::None, Scope::ShapesList },
      { Command::ShapeDuplicate,   "shapes.duplicate",      "Duplicate",              Menu::None,   false, scShapeDup,   none,       "", "", "", Menu::None, Scope::ShapesList },
      { Command::ShapeDelete,      "shapes.delete",         "Delete",                 Menu::None,   false, scShapeDel,   scShapeDelMac, "", "", "", Menu::None, Scope::ShapesList },
      { Command::ShapeMoveEarlier, "shapes.moveEarlier",    "Move earlier",           Menu::None,   false, scShapeEarlier, none,     "", "", "", Menu::None, Scope::ShapesList },
      { Command::ShapeMoveLater,   "shapes.moveLater",      "Move later",             Menu::None,   false, scShapeLater, none,       "", "", "", Menu::None, Scope::ShapesList },

      // the focused 3D view: the mouse's orbit, pan and zoom, and the view cube's turns, rolls and faces
      { Command::ViewOrbitLeft,    "view.orbitLeft",        "Orbit left",             Menu::None,   false, scOrbitLeft,  none,       "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewOrbitRight,   "view.orbitRight",       "Orbit right",            Menu::None,   false, scOrbitRight, none,       "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewOrbitUp,      "view.orbitUp",          "Orbit up",               Menu::None,   false, scOrbitUp,    none,       "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewOrbitDown,    "view.orbitDown",        "Orbit down",             Menu::None,   false, scOrbitDown,  none,       "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewPanLeft,      "view.panLeft",          "Pan left",               Menu::None,   false, scPanLeft,    none,       "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewPanRight,     "view.panRight",         "Pan right",              Menu::None,   false, scPanRight,   none,       "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewPanUp,        "view.panUp",            "Pan up",                 Menu::None,   false, scPanUp,      none,       "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewPanDown,      "view.panDown",          "Pan down",               Menu::None,   false, scPanDown,    none,       "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewZoomIn,       "view.zoomIn",           "Zoom in",                Menu::None,   false, scZoomIn,     none,       "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewZoomOut,      "view.zoomOut",          "Zoom out",               Menu::None,   false, scZoomOut,    none,       "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewTurnLeft,     "view.turnLeft",         "Turn 90° left",          Menu::None,   false, scTurnLeft,   scTurnLeftMac,  "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewTurnRight,    "view.turnRight",        "Turn 90° right",         Menu::None,   false, scTurnRight,  scTurnRightMac, "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewTurnUp,       "view.turnUp",           "Turn 90° up",            Menu::None,   false, scTurnUp,     scTurnUpMac,    "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewTurnDown,     "view.turnDown",         "Turn 90° down",          Menu::None,   false, scTurnDown,   scTurnDownMac,  "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewRollLeft,     "view.rollLeft",         "Roll 90° counter-clockwise", Menu::None, false, scRollLeft, scRollLeftMac,  "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewRollRight,    "view.rollRight",        "Roll 90° clockwise",     Menu::None,   false, scRollRight,  scRollRightMac, "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewFront,        "view.front",            "View from the front (+Z)", Menu::None,   false, scFront,      none,       "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewBack,         "view.back",             "View from the back (−Z)", Menu::None,   false, scBack,       none,       "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewLeft,         "view.left",             "View from the left (−X)", Menu::None,   false, scLeft,       none,       "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewRight,        "view.right",            "View from the right (+X)", Menu::None,   false, scRight,      none,       "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewTop,          "view.top",              "View from the top (+Y)", Menu::None,   false, scTop,        none,       "", "", "", Menu::None, Scope::Viewport },
      { Command::ViewBottom,       "view.bottom",           "View from the bottom (−Y)", Menu::None,   false, scBottom,     none,       "", "", "", Menu::None, Scope::Viewport },
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

    // Toggle 3D: dropped
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

  bool isSingleCharacter(std::string_view shortcut) {
    // "+" alone is the plus key; any other "+" joins a modifier
    return shortcut.size() == 1 && std::isprint(static_cast<unsigned char>(shortcut[0])) && shortcut[0] != ' ';
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

  std::vector<std::string> shortcutClashes(Platform p) {
    std::vector<std::string> out;
    std::map<std::string_view, std::vector<Scope>> used;
    for (const CommandInfo & c : commandTable())
      for (std::string_view s : shortcutsOn(c, p)) {
        std::vector<Scope> & scopes = used[s];
        const bool clash = std::any_of(scopes.begin(), scopes.end(), [&](Scope o) {
          return o == c.scope || o == Scope::Window || c.scope == Scope::Window;
        });
        if (clash)
          out.push_back("shortcut '" + std::string(s) + "' is used twice");
        scopes.push_back(c.scope);
      }
    return out;
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
