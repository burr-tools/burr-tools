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
#include "filechooser.h"

#include <string>
#include <string.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

#if defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
#include <FL/Fl_Native_File_Chooser.H>
#else
#include <FL/Fl_File_Chooser.H>
#endif

#pragma GCC diagnostic pop

/* the file name the user most recently selected in a native save dialog,
 * where the system already asked whether an existing file may be replaced */
static std::string lastNativeSave;

bool fileChooserConfirmedOverwrite(const char * name)
{
  return name && !lastNativeSave.empty() && lastNativeSave == name;
}

const char * fileChooser(const char * title, const char * filterName, const char * pattern, const char * fname, bool save)
{
#if defined(__APPLE__) || defined(__linux__) || defined(_WIN32)

  static std::string result;

  Fl_Native_File_Chooser ch;

  ch.title(title);
  ch.type(save ? Fl_Native_File_Chooser::BROWSE_SAVE_FILE : Fl_Native_File_Chooser::BROWSE_FILE);

  std::string filter;
  if (pattern && pattern[0])
  {
    if (filterName && filterName[0])
    {
      filter = filterName;
      filter += "\t";
    }
    filter += pattern;
    ch.filter(filter.c_str());
  }

  if (save)
    ch.options(Fl_Native_File_Chooser::NEW_FOLDER | Fl_Native_File_Chooser::USE_FILTER_EXT);

  // preselect directory and file name when one is given
  if (fname && fname[0])
  {
    const char * div = strrchr(fname, '/');

    if (div)
    {
      std::string dir(fname, div - fname);
      ch.directory(dir.c_str());
      if (div[1])
        ch.preset_file(div + 1);
    }
    else
    {
      ch.preset_file(fname);
    }
  }

  if (ch.show() != 0)
    return 0;

  result = ch.filename();

  if (save)
    lastNativeSave = result;

  return result.c_str();

#else

  // on the other platforms keep the FLTK dialog as it always was
  (void)filterName;
  (void)save;
  return fl_file_chooser(title, (pattern && pattern[0]) ? pattern : 0, fname, 0);

#endif
}
