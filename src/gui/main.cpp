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


#include "mainwindow.h"
#include "assertwindow.h"
#include "mainmenu.h"
#include "platform.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define GL_SILENCE_DEPRECATION 1

#include <FL/Fl.H>
#pragma GCC diagnostic pop

#include <time.h>
#include <string.h>

#include "../lib/bt_assert.h"
#include "../lib/gridtype.h"
#include "../lib/puzzle.h"

#include "../tools/xml.h"
#include "../tools/gzstream.h"

/* fl_open_callback() takes a plain function pointer with no user data, so
 * the window it should forward to is reached through this file-scope
 * pointer instead. Cleared before the window is destroyed so a late Apple
 * Event can never reach a dangling pointer.
 */
static mainWindow_c * g_ui = 0;

static void handleSystemOpen(const char * filename) {
  if (g_ui)
    g_ui->openFromSystem(filename);
}

class my_Fl : public Fl {

public:

  static int run(mainWindow_c * ui) {

    time_t start = time(0);

    while (Fl::first_window()) {
      wait(0.5);
      if (time(0)-start >= 1) {
        ui->update();
        start = time(0);
      }
    }

    return 0;
  }
};

int main(int argc, char ** argv) {

  // A headless invariant check, so CI can catch menu-table drift without
  // linking FLTK into test_burrtools. Must run before any window exists.
  if (argc == 2 && strcmp(argv[1], "--self-check") == 0) {
    bt_assert_init();
    mainmenu::assertTablesConsistent();
    printf("self-check OK\n");
    return 0;
  }

  bt_assert_init();

  /* And again on the normal path, so that simply running the program is
   * enough to catch menu-table drift -- the --self-check entry point above
   * exists for CI, not as the only place this runs. The check is a linear
   * walk of a couple of dozen table entries, which costs nothing next to
   * building the main window, and it must happen before that window is
   * constructed so a failure surfaces before any menu is copied.
   */
  mainmenu::assertTablesConsistent();

  platform::applyLookAndFeel();

  mainWindow_c *ui = new mainWindow_c(new gridType_c());

  /* Must come after the window exists and before it is shown: launching by
   * double-clicking a puzzle delivers the open event almost immediately.
   */
  g_ui = ui;
  platform::installOpenHandler(handleSystemOpen);

  int res = 0;

  try {

    ui->show(argc, argv);

    res = my_Fl::run(ui);
  }

  catch (assert_exception& a) {

    assertWindow_c * aw = new assertWindow_c("I'm sorry there is a bug in this program. It needs to be closed.\n"
                                             "I try to save the current puzzle in '__rescue.xmpuzzle'\n",
                                             &a);

    aw->show();

    while (aw->visible())
      Fl::wait();

    delete aw;

    ogzstream ostr("__rescue.xmpuzzle");

    if (ostr)
    {
      xmlWriter_c xml(ostr);
      ui->getPuzzle()->save(xml);
    }

    return -1;
  }

  catch (...) {
    printf(" exception\n");
  }

  g_ui = 0;
  delete ui;
  return res;
}
