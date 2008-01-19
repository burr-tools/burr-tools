#include "Fl_Help_Dialog.h"
#include "../flu/Flu_File_Chooser.h"

#include <FL/Fl_Shared_Image.H>

int main(int  argc, char *argv[]) {

  Fl_Help_Dialog help;

  if (argc < 2) {
    const char * fname = flu_file_chooser("File to open", "*.html", "");
    if (!fname) exit(0);
    help.load(fname);
  } else
    help.load(argv[1]);
    
  fl_register_images();

  help.show(1, argv);

  Fl::run();

  return (0);
}

