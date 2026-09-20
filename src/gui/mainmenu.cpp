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

#include "../lib/bt_assert.h"

#include <FL/Fl.H>

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

  /* Task 3 adds menu_Mac[] here. */

  Fl_Menu_Item * activeTable(void) {
    return menu_Portable;
  }

  size_t activeTableSize(void) {
    return sizeof(menu_Portable) / sizeof(menu_Portable[0]);
  }
}

const Fl_Menu_Item * mainmenu::table(void) {
  return activeTable();
}

Fl_Menu_Item * mainmenu::mutableTable(void) {
  return activeTable();
}

int mainmenu::findEntry(Fl_Callback * cb) {

  bt_assert(cb);

  int found = -1;

  for (size_t i = 0; i < activeTableSize(); i++)
    if (activeTable()[i].callback() == cb) {
      bt_assert(found == -1);
      found = (int)i;
    }

  bt_assert(found >= 0);
  return found;
}

void mainmenu::assertTablesConsistent(void) {
  /* Task 3 fills this in once there is a second table to compare against. */
}

void mainmenu::installApplicationMenu(mainWindow_c * /*win*/) {
  /* Task 3 fills this in. */
}
