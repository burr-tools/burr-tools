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
   *
   * The title deliberately says nothing about unsaved changes: that is
   * setDocumentEdited()'s job below, which uses the platform's own
   * affordance rather than decorating the title text.
   */
  std::string windowTitle(const char * filename);

  /* Reflect unsaved changes in the window chrome (the dot in the macOS
   * close button). No-op where the platform has no such affordance.
   */
  void setDocumentEdited(Fl_Window * win, bool edited);

  /* Open the user guide in the user's browser. */
  void openHelp(void);

  /* The wash the window system is currently drawing over win's own
   * contents, which anything drawing outside those contents has to
   * reproduce for itself.
   *
   * This exists for one case: macOS presents the native file dialog as a
   * sheet and dims the window it is attached to, but FLTK gives every
   * subwindow a child window of its own, and the dim does not reach a
   * child window. An Fl_Gl_Window therefore stays at full brightness --
   * a bright rectangle in an otherwise dimmed window.
   *
   * win may be any widget's window; the window the sheet is attached to
   * is found from it. Returns false when nothing is being dimmed, which
   * is always the case off macOS. Otherwise grey and alpha describe a
   * source-over composite the caller should paint over everything it
   * drew:  out = in * (1 - alpha) + grey * alpha.
   */
  bool modalDimWash(Fl_Window * win, float * grey, float * alpha);
}

#endif
