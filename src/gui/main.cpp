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
