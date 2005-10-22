/* Burr Solver
 * Copyright (C) 2003-2005  Andreas Röver
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


#include "MainWindow.h"
#include "WindowWidgets.h"
#include "gzstream.h"

#include <FL/Fl.h>

#include <time.h>

#include <xmlwrapp/xmlwrapp.h>

#include "../lib/bt_assert.h"

class my_Fl : public Fl {

public:

  static int run(UserInterface * ui) {

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

  UserInterface *ui = new UserInterface();

  int res = 0;

  try {

    ui->show(argc, argv);

    res = my_Fl::run(ui);
  }

  catch (assert_exception *a) {

    assertWindow * aw = new assertWindow("I'm sorry there is a bug in this program. It needs to be closed.\n"
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
