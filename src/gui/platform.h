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

#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include <string>

class Fl_Window;

/* Everything BurrTools does differently on one operating system than
 * another. The point of this seam is that no caller branches: each function
 * has a meaningful body on macOS and a no-op or portable fallback
 * everywhere else, so mainwindow.cpp and main.cpp contain no platform
 * conditionals at all.
 */
namespace platform {

  /* Widget scheme, fonts and metrics. Call once, before any window is
   * constructed.
   */
  void applyLookAndFeel(void);

  /* True when the menu bar is drawn by the operating system rather than
   * inside the application window.
   */
  bool usesSystemMenuBar(void);

  /* Register a handler for documents the OS asks us to open -- a Finder
   * double-click, a drop on the Dock icon, "Open With". The handler may be
   * called before the first window is shown, so it must be installed after
   * the main window is constructed.
   */
  void installOpenHandler(void (*handler)(const char * filename));

  /* The window title for a document. macOS wants the bare file name;
   * everywhere else keeps the historical "BurrTools - <name>".
   * A null or empty file name yields the untitled form.
   */
  std::string windowTitle(const char * filename, bool edited);

  /* Reflect unsaved changes in the window chrome (the dot in the macOS
   * close button). No-op where the platform has no such affordance.
   */
  void setDocumentEdited(Fl_Window * win, bool edited);

  /* Open the user guide in the user's browser. */
  void openHelp(void);
}

#endif
