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
#include "mainmenu.h"
#include "platform.h"

#include "../lib/bt_assert.h"

#ifdef __APPLE__
#include <FL/Fl_Sys_Menu_Bar.H>
#include <FL/Fl_Group.H>
#include <FL/platform.H>
#endif

namespace {

  /* The historical in-window menu. Unchanged from the table that lived in
   * mainwindow.cpp, so Linux and Windows see exactly what they always have.
   */
  Fl_Menu_Item menu_Portable[] = {
    { "&File",           0, 0, 0, FL_SUBMENU, 0, 0, 0, 0 },
      {"New",            0, cb_New_stub,         0, 0, 0, 0, 14, 56},
      {"Load",    FL_F + 3, cb_Load_stub,        0, 0, 0, 0, 14, 56},
      {"Import",         0, cb_Load_Ps3d_stub,   0, 0, 0, 0, 14, 56},
      {"Save",    FL_F + 2, cb_Save_stub,        0, 0, 0, 0, 14, 56},
      {"Save As",        0, cb_SaveAs_stub,      0, FL_MENU_DIVIDER, 0, 0, 14, 56},
      {"Convert",        0, cb_Convert_stub,     0, 0, 0, 0, 14, 56},
      {"Import Assms",   0, cb_AssembliesToShapes_stub,     0, 0, 0, 0, 14, 56},
      {"Quit",           0, cb_Quit_stub,        0, 0, 3, 0, 14, 56},
      { },
    {"Toggle 3D", FL_F + 4, cb_Toggle3D_stub,    0, 0, 0, 0, 14, 56},
    { "&Export",         0, 0, 0, FL_SUBMENU, 0, 0, 0, 0 },
      {"Images",             0, cb_ImageExport_stub, 0, 0, 0, 0, 14, 56},
      {"Vector Image",       0, cb_ImageExportVector_stub, 0, 0, 0, 0, 14, 56},
      {"STL",             0, cb_STLExport_stub, 0, 0, 0, 0, 14, 56},
      { },
    {"Status",           0, cb_StatusWindow_stub,  0, 0, 0, 0, 14, 56},
    {"Edit Comment",     0, cb_Comment_stub,     0, 0, 0, 0, 14, 56},
    {"Settings",         0, cb_Config_stub,      0, 0, 0, 0, 14, 56},
    {"About",            0, cb_About_stub,       0, 0, 3, 0, 14, 56},
    { }
  };

#ifdef __APPLE__

  /* The macOS table. Restructured for the system menu bar: Export becomes a
   * File submenu rather than a top-level menu, the document-level actions
   * gather under Puzzle, and every item that opens a dialog gains an
   * ellipsis.
   *
   * There is no Edit menu. The only candidate for one is Edit Comment --
   * BurrTools has no Undo, Cut, Copy or Paste -- and a one-item Edit menu
   * reads worse than none.
   *
   * About, Settings and Quit are deliberately absent: they belong to the
   * application menu, built in installApplicationMenu() below.
   */
  Fl_Menu_Item menu_Mac[] = {
    { "&File",             0, 0, 0, FL_SUBMENU, 0, 0, 0, 0 },
      {"New",              FL_COMMAND + 'n', cb_New_stub,       0, 0, 0, 0, 14, 56},
      {"Open...",          FL_COMMAND + 'o', cb_Load_stub,      0, 0, 0, 0, 14, 56},
      {"Import...",        0,                cb_Load_Ps3d_stub, 0, FL_MENU_DIVIDER, 0, 0, 14, 56},
      {"Save",             FL_COMMAND + 's', cb_Save_stub,      0, 0, 0, 0, 14, 56},
      {"Save As...",       FL_COMMAND + FL_SHIFT + 's', cb_SaveAs_stub, 0, FL_MENU_DIVIDER, 0, 0, 14, 56},
      { "Export",          0, 0, 0, FL_SUBMENU | FL_MENU_DIVIDER, 0, 0, 0, 0 },
        {"Image...",        0, cb_ImageExport_stub,       0, 0, 0, 0, 14, 56},
        {"Vector Image...", 0, cb_ImageExportVector_stub, 0, 0, 0, 0, 14, 56},
        {"STL...",          0, cb_STLExport_stub,         0, 0, 0, 0, 14, 56},
        { },
      {"Close",            FL_COMMAND + 'w', cb_Quit_stub,      0, 0, 0, 0, 14, 56},
      { },
    { "&Puzzle",           0, 0, 0, FL_SUBMENU, 0, 0, 0, 0 },
      {"Edit Comment...",      0, cb_Comment_stub,            0, 0, 0, 0, 14, 56},
      {"Convert...",           0, cb_Convert_stub,            0, 0, 0, 0, 14, 56},
      {"Import Assemblies...", 0, cb_AssembliesToShapes_stub, 0, FL_MENU_DIVIDER, 0, 0, 14, 56},
      {"Status",           FL_COMMAND + 'i', cb_StatusWindow_stub, 0, 0, 0, 0, 14, 56},
      { },
    { "&View",             0, 0, 0, FL_SUBMENU, 0, 0, 0, 0 },
      {"Toggle 3D",        FL_COMMAND + '3', cb_Toggle3D_stub, 0, 0, 0, 0, 14, 56},
      { },
    { "Help",              0, 0, 0, FL_SUBMENU, 0, 0, 0, 0 },
      {"BurrTools User Guide", FL_COMMAND + '?', cb_Help_stub, 0, 0, 0, 0, 14, 56},
      { },
    { }
  };

#endif

  Fl_Menu_Item * activeTable(void) {
#ifdef __APPLE__
    return menu_Mac;
#else
    return menu_Portable;
#endif
  }

  size_t activeTableSize(void) {
#ifdef __APPLE__
    return sizeof(menu_Mac) / sizeof(menu_Mac[0]);
#else
    return sizeof(menu_Portable) / sizeof(menu_Portable[0]);
#endif
  }
}

const Fl_Menu_Item * mainmenu::table(void) {
  return activeTable();
}

void cb_Help_stub(Fl_Widget*, void*) {
  platform::openHelp();
}

void mainmenu::assertTablesConsistent(void) {

  /* Walk the portable table and require each of its callbacks to appear in
   * the table this build actually uses. Labels and shortcuts differ by
   * design; a missing callback does not.
   *
   * This is deliberately ONE-DIRECTIONAL, portable -> platform, rather than
   * a set equality: cb_Help_stub is macOS-only (the portable menu has never
   * had a Help item), so requiring the reverse containment would fail every
   * macOS build. The consequence is that an item added ONLY to menu_Mac is
   * not caught here -- only the portable-table-forgotten-on-macOS direction
   * is, which is the direction that actually loses functionality.
   *
   * Off macOS activeTable() is menu_Portable, so this compares the portable
   * table with itself and is a tautology. The check can only ever fire on a
   * macOS build; that is where the second table exists.
   *
   * The macOS table intentionally omits About, Settings and Quit, which
   * live in the application menu, so those are excluded from the
   * comparison.
   */
  static Fl_Callback * const appMenuOnly[] = {
    cb_About_stub, cb_Config_stub
  };

  const size_t portableSize = sizeof(menu_Portable) / sizeof(menu_Portable[0]);

  for (size_t i = 0; i < portableSize; i++) {

    Fl_Callback * cb = menu_Portable[i].callback();
    if (!cb) continue;

    bool skip = false;
    for (size_t k = 0; k < sizeof(appMenuOnly)/sizeof(appMenuOnly[0]); k++)
      if (cb == appMenuOnly[k]) skip = true;
    if (skip) continue;

    bool found = false;
    for (size_t j = 0; j < activeTableSize(); j++)
      if (activeTable()[j].callback() == cb) found = true;

    /* A menu item exists on one platform but not the other. Add it to the
     * table that is missing it, or add it to appMenuOnly if it genuinely
     * belongs only in the macOS application menu.
     */
    bt_assert(found);
  }
}

void mainmenu::installApplicationMenu(mainWindow_c * win) {
#ifdef __APPLE__

  /* mainWindow_c's constructor never closes the Fl_Group scope its
   * LFl_Double_Window base opens for its master layouter_c (every widget
   * the constructor builds needs to land in that layout tree), so
   * Fl_Group::current() is still that layouter_c for the whole of the
   * constructor, including this call.
   *
   * Fl_Mac_App_Menu::custom_application_menu_items() below allocates an
   * internal Fl_Menu_Bar helper of its own and -- being ordinary FLTK code
   * with no idea our "current" group is actually a layout container --
   * adds it to whatever group is current. Left alone, that stray widget
   * lands in the layout tree, fails the layouter's dynamic_cast to
   * layoutable_c, and crashes the first resize. Clearing "current" for the
   * duration of this call keeps FLTK's own bookkeeping out of our layout.
   */
  Fl_Group * savedCurrent = Fl_Group::current();
  Fl_Group::current(0);

  /* The About item is a property of the application menu, not of our
   * table, so FLTK wants it separately.
   */
  Fl_Sys_Menu_Bar::about(cb_About_stub, win);

  /* Settings belongs in the application menu on macOS. The array must
   * outlive the call, hence the static; user_data cannot be set in the
   * initialiser because the window does not exist until runtime.
   */
  static Fl_Menu_Item appItems[] = {
    { "Settings...", FL_COMMAND + ',', cb_Config_stub, 0, 0, 0, 0, 14, 56 },
    { }
  };
  appItems[0].user_data(win);

  Fl_Mac_App_Menu::custom_application_menu_items(appItems);

  /* Gives us Minimize and the window list for free. There is no Zoom. */
  Fl_Sys_Menu_Bar::window_menu_style(Fl_Sys_Menu_Bar::tabbing_mode_none);

  Fl_Group::current(savedCurrent);

#else
  (void)win;
#endif
}
