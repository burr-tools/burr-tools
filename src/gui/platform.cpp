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
   */
  Fl::scheme("gleam");

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

void platform::installOpenHandler(void (*handler)(const char *)) {
#ifdef __APPLE__
  fl_open_callback(handler);
#else
  (void)handler;
#endif
}

std::string platform::windowTitle(const char * filename, bool /*edited*/) {

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

void platform::setDocumentEdited(Fl_Window * /*win*/, bool /*edited*/) {
  /* Task 7 */
}

void platform::openHelp(void) {
  char msg[512];
  fl_open_uri(USER_GUIDE_URL, msg, sizeof(msg));
}
