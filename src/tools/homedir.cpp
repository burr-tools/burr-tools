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
#include "homedir.h"

#include <FL/filename.H>

#ifdef WIN32
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <lmcons.h>
#else
#define MAX_PATH 1024
#endif

#if defined WIN32 && !defined CYGWIN
#include <direct.h>
#else
#include <unistd.h>
#endif

// returns either the home directory or the current
// directory on system that don't know home directories
std::string homedir()
{
  char userHome[MAX_PATH+5];

#ifdef WIN32

  HKEY key;
  DWORD size = MAX_PATH;

  if (RegOpenKeyEx(HKEY_CURRENT_USER,
                   "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",
                   0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
  {
    userHome[0] = '\0';
  }
  else if (RegQueryValueEx(key, "Personal", 0, 0, (LPBYTE)userHome, &size ) != ERROR_SUCCESS)
  {
    userHome[0] = '\0';
  }
  else
  {
    RegCloseKey(key);
  }

  size = strlen(userHome);
  userHome[size] = '\\';
  userHome[size+1] = '\0';

#else

  fl_filename_expand( userHome, MAX_PATH, "~/" );

#endif

  return userHome;
}
