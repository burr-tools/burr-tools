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

#include "platform.h"

#include <FL/Fl.H>
#include <FL/filename.H>
#include <FL/Fl_Window.H>
#include <FL/platform.H>

#include <string.h>

#ifdef __APPLE__
#include <objc/message.h>
#include <objc/runtime.h>
#include <objc/objc.h>
#endif

namespace {
  const char * const USER_GUIDE_URL =
    "https://burrtools.sourceforge.net/gui-doc/toc.html";

#ifdef __APPLE__
  /* basename without the directory part; returns the whole string when
   * there is no separator.
   *
   * Guarded because it is only reachable on macOS -- left unguarded it is
   * an unused static function everywhere else, which just build-werror
   * rejects.
   */
  const char * baseName(const char * path) {
    const char * slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
  }

  /* What AppKit's sheet dim does to the window underneath it, as a
   * source-over composite:  out = in * (1 - alpha) + grey * alpha.
   *
   * Measured rather than assumed, because AppKit exposes no API for it and
   * the value is not documented. A probe window painted four known grey
   * levels twice -- once with FLTK widgets, once in an Fl_Gl_Window, which
   * the dim does not reach -- and was screenshotted with a sheet attached.
   * Fitting the dimmed side against the undimmed one gives:
   *
   *     in    255  216  128   64
   *     out   111   97   62   38      ->  out = 0.385 * in + 13.2
   *
   * which is grey 21.4/255 at alpha 0.615, reproducing all four levels to
   * better than one part in 255. The same constants predict the 216 -> 97
   * measured in the screenshot on the reporter's machine, so this is a
   * property of the compositor rather than of one display profile.
   *
   * If a future macOS changes the dim these will be slightly off, which
   * costs a faint seam at the edge of the 3D view. That is a far smaller
   * error than the undimmed rectangle they exist to remove.
   */
  const float MODAL_DIM_GREY  = 0.084f;
  const float MODAL_DIM_ALPHA = 0.615f;
#endif
}

void platform::applyLookAndFeel(void) {

  /* A scheme replaces the default Motif-derived boxes. This is deliberately
   * applied on every platform: the default scheme looks equally dated on
   * Linux and Windows.
   *
   * Note that this must not be combined with the FL_THIN_* boxtype
   * overrides main() used to install -- schemes install their own box
   * drawing functions and the two fight, leaving the scheme half applied.
   *
   * "oxy" over "gleam", which this started with: it is the flatter and more
   * restrained of the two, which suits a macOS-facing build. Both remap the
   * FL_THIN_* variants as well as the plain boxes, which matters because
   * separator.cpp draws with FL_THIN_DOWN_BOX and would otherwise be the one
   * widget left rendering in the old style. Unlike gleam, oxy also narrows
   * the scrollbars (Fl::scrollbar_size(15) -- see FLTK's
   * Fl_get_system_colors.cxx), so anything that assumed the default 16 will
   * shift by a pixel.
   */
  Fl::scheme("oxy");

#ifdef __APPLE__
  /* Render labels in the system UI font (SF Pro) rather than Helvetica, and
   * match the 13pt macOS control text size rather than FLTK's 14.
   */
  Fl::set_font(FL_HELVETICA, ".AppleSystemUIFont");
  FL_NORMAL_SIZE = 13;
#endif

  Fl::get_system_colors();
}

bool platform::usesSystemMenuBar(void) {
#ifdef __APPLE__
  return true;
#else
  return false;
#endif
}

void platform::installOpenHandler(void (*handler)(const char * filename)) {
#ifdef __APPLE__
  fl_open_callback(handler);
#else
  (void)handler;
#endif
}

std::string platform::windowTitle(const char * filename) {

  const bool untitled = !filename || !filename[0];

#ifdef __APPLE__
  /* macOS titles a document window with the document's name and nothing
   * else; the application name belongs in the menu bar.
   */
  return untitled ? std::string("Untitled") : std::string(baseName(filename));
#else
  return untitled ? std::string("BurrTools - Untitled")
                  : std::string("BurrTools - ") + filename;
#endif
}

void platform::setDocumentEdited(Fl_Window * win, bool edited) {
#ifdef __APPLE__

  if (!win || !win->shown())
    return;

  /* [NSWindow setDocumentEdited:] draws the dot in the close button. This
   * needs no Objective-C source file: <objc/message.h> is a plain C API,
   * and fl_xid() hands back the FLWindow (an NSWindow subclass) FLTK
   * created for us. libobjc arrives with the Cocoa framework we already
   * link.
   */
  id nsWindow = (id)fl_xid(win);
  if (!nsWindow)
    return;

  typedef void (*SetEditedFn)(id, SEL, BOOL);
  ((SetEditedFn)objc_msgSend)(nsWindow,
                              sel_registerName("setDocumentEdited:"),
                              edited ? YES : NO);
#else
  (void)win;
  (void)edited;
#endif
}

void platform::openHelp(void) {
  char msg[512];
  fl_open_uri(USER_GUIDE_URL, msg, sizeof(msg));
}

bool platform::modalDimWash(Fl_Window * win, float * grey, float * alpha) {
#ifdef __APPLE__

  if (!win)
    return false;

  /* The sheet is attached to the top level window, not to the subwindow
   * the caller is drawing into.
   */
  Fl_Window * top = win->top_window();
  if (!top || !top->shown())
    return false;

  id nsWindow = (id)fl_xid(top);
  if (!nsWindow)
    return false;

  /* Same plain-C runtime call as setDocumentEdited() above: no
   * Objective-C source file is needed for a message with an object
   * return and no arguments.
   */
  typedef id (*AttachedSheetFn)(id, SEL);
  id sheet = ((AttachedSheetFn)objc_msgSend)(nsWindow,
                                             sel_registerName("attachedSheet"));
  if (!sheet)
    return false;

  if (grey)  *grey  = MODAL_DIM_GREY;
  if (alpha) *alpha = MODAL_DIM_ALPHA;
  return true;

#else
  (void)win;
  (void)grey;
  (void)alpha;
  return false;
#endif
}
