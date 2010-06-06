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

