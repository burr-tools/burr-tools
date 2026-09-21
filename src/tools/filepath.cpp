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
#include "filepath.h"

namespace {

  /* Both separators are recognised everywhere rather than only the host's.
   * A path can outlive the machine that produced it — it reaches us from a
   * puzzle file, a configuration entry, or the command line — and a Windows
   * path examined with only '/' in mind looks exactly like a bare filename,
   * which would send the export dialogs to the home directory instead of
   * the folder the puzzle came from.
   */
  bool isSeparator(char c) {
    return c == '/' || c == '\\';
  }
}

std::string directoryOfFile(const std::string & path) {

  if (path.empty())
    return std::string();

  /* A path that already ends in a separator names a directory, so it is its
   * own answer rather than its parent's. Strip the separator (or several)
   * and hand back what is left.
   */
  size_t end = path.size();
  while (end > 0 && isSeparator(path[end-1]))
    end--;

  if (end == 0)
    return path.substr(0, 1);       // nothing but separators: the root

  if (end < path.size())
    return path.substr(0, end);

  const size_t slash = path.find_last_of("/\\");

  if (slash == std::string::npos)
    return std::string();           // no directory part at all

  if (slash == 0)
    return path.substr(0, 1);       // the root directory, not an empty one

  return path.substr(0, slash);
}
