/* Burr Solver
 * Copyright (C) 2003-2007  Andreas Röver
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
#include "gzstream.h"

#include <FL/Fl.H>

#include <time.h>

#include <xmlwrapp/xmlwrapp.h>

#include "../lib/bt_assert.h"
#include "../lib/gridtype.h"
#include "../lib/puzzle.h"

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

  xml::init xmlinit;
  bt_assert_init();

  Fl::set_boxtype(FL_UP_BOX, FL_THIN_UP_BOX);
  Fl::set_boxtype(FL_DOWN_BOX, FL_THIN_DOWN_BOX);
  Fl::set_boxtype(FL_UP_FRAME, FL_THIN_UP_FRAME);
  Fl::set_boxtype(FL_DOWN_FRAME, FL_THIN_DOWN_FRAME);

  mainWindow_c *ui = new mainWindow_c(new gridType_c());

  int res = 0;

  try {

    ui->show(argc, argv);

    res = my_Fl::run(ui);
  }

  catch (assert_exception *a) {

    assertWindow_c * aw = new assertWindow_c("I'm sorry there is a bug in this program. It needs to be closed.\n"
                                             "I try to save the current puzzle in '__rescue.xmpuzzle'\n",
                                             a);

    aw->show();

    while (aw->visible())
      Fl::wait();

    delete aw;

    ogzstream ostr("__rescue.xmpuzzle");

    if (ostr)
      ostr << ui->getPuzzle()->save();

    return -1;
  }

  catch (...) {
    printf(" exception\n");
  }

  delete ui;
  return res;
}
