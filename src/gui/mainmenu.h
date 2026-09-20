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
#ifndef __MAINMENU_H__
#define __MAINMENU_H__

#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Widget.H>

class mainWindow_c;

/* The main menu lives here rather than in mainwindow.cpp because there is
 * more than one of it: macOS gets a table restructured for the system menu
 * bar, every other platform keeps the historical in-window table. Both are
 * built from the same callbacks.
 */
namespace mainmenu {

  /* the table for the platform this build targets */
  const Fl_Menu_Item * table(void);

  /* the same table, writable, for activate()/deactivate() */
  Fl_Menu_Item * mutableTable(void);

  /* Index of the entry with the given callback.
   *
   * Deliberately keyed on the callback rather than the label: the macOS
   * table uses different wording ("Image..." where the portable table says
   * "Images"), so a label-keyed lookup would assert on one platform and
   * not the other.
   */
  int findEntry(Fl_Callback * cb);

  /* Assert that both tables expose the same set of callbacks.
   *
   * Adding an item to one table and forgetting the other is the failure
   * mode this design creates; this is the check that catches it. Labels and
   * shortcuts are intentionally not compared, as they legitimately differ.
   */
  void assertTablesConsistent(void);

  /* Populate the macOS application menu (About, Settings). No-op elsewhere. */
  void installApplicationMenu(mainWindow_c * win);
}

/* The menu callbacks. Defined in mainwindow.cpp next to the methods they
 * forward to; declared here because the tables above reference them.
 */
void cb_New_stub(Fl_Widget*, void*);
void cb_Load_stub(Fl_Widget*, void*);
void cb_Load_Ps3d_stub(Fl_Widget*, void*);
void cb_Save_stub(Fl_Widget*, void*);
void cb_SaveAs_stub(Fl_Widget*, void*);
void cb_Convert_stub(Fl_Widget*, void*);
void cb_AssembliesToShapes_stub(Fl_Widget*, void*);
void cb_Quit_stub(Fl_Widget*, void*);
void cb_Toggle3D_stub(Fl_Widget*, void*);
void cb_ImageExport_stub(Fl_Widget*, void*);
void cb_ImageExportVector_stub(Fl_Widget*, void*);
void cb_STLExport_stub(Fl_Widget*, void*);
void cb_StatusWindow_stub(Fl_Widget*, void*);
void cb_Comment_stub(Fl_Widget*, void*);
void cb_Config_stub(Fl_Widget*, void*);
void cb_About_stub(Fl_Widget*, void*);
void cb_Help_stub(Fl_Widget*, void*);

#endif
