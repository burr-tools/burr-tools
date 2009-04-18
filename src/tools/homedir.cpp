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
