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
#include "solveprogresscache.h"

#include <algorithm>

void solveProgressCache_c::remember(const void * problem, const solveSnapshot_c & snapshot) {
  entries[problem] = snapshot;
}

const solveSnapshot_c * solveProgressCache_c::recall(const void * problem) const {

  auto i = entries.find(problem);

  if (i == entries.end())
    return nullptr;

  return &i->second;
}

void solveProgressCache_c::forget(const void * problem) {
  entries.erase(problem);
}

void solveProgressCache_c::keepOnly(const std::vector<const void *> & live) {

  /* Erasing while walking, so the iterator is advanced by erase() on the way
   * out. A puzzle has few problems and the map is smaller still, so the linear
   * scan of `live` per entry is cheaper than building a set for it.
   */
  for (auto i = entries.begin(); i != entries.end(); ) {
    if (std::find(live.begin(), live.end(), i->first) == live.end())
      i = entries.erase(i);
    else
      ++i;
  }
}

void solveProgressCache_c::clear(void) {
  entries.clear();
}
