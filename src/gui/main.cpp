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

#include <FL/Fl.h>

#include <time.h>

class my_Fl : public Fl {

public:

  static int run(UserInterface * ui) {

//    time_t start = time(0);

    while (Fl::first_window()) {
      wait(0.1);
//      if (time(0)-start >= 1) {
        ui->update();
//        start = time(0);
//      }
    }
  }
};

int main(int argc, char ** argv) {

  UserInterface *ui = new UserInterface();
  
  ui->show(argc, argv);

  return my_Fl::run(ui);
}
