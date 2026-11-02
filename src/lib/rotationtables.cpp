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
#include "rotationtables.h"

#include "gridtype.h"
#include "problem.h"
#include "puzzle.h"
#include "voxel.h"

#include <algorithm>
#include <mutex>

using rotationGeometry::rotationTables_c;

rotationSettings_t defaultRotationSettings(void) {
  return { { 0.081 }, { 0.1, 0.2, 0.3, 0.4 } };
}

int rotationRadiusFor(const problem_c & problem) {
  const voxel_c * result = getResultShape(problem);
  return (int)std::max({ result->getX(), result->getY(), result->getZ() }) + 1;
}

namespace {

struct cacheEntry_t {
  rotationSettings_t settings;
  int radius;
  std::weak_ptr<const rotationTables_c> tables;
};

bool sameSettings(const rotationSettings_t & a, const rotationSettings_t & b) {
  return a.model.play == b.model.play && a.amounts == b.amounts;
}

}

std::shared_ptr<const rotationTables_c>
sharedRotationTables(const rotationSettings_t & settings, int radius) {

  static std::mutex mutex;
  static std::vector<cacheEntry_t> cache;

  /* Building under the lock keeps two solvers from building the same
   * tables twice; requests for other settings wait, which is rare. */
  std::lock_guard<std::mutex> lock(mutex);

  std::erase_if(cache, [](const cacheEntry_t & e) { return e.tables.expired(); });

  for (const cacheEntry_t & e : cache)
    if (e.radius >= radius && sameSettings(e.settings, settings))
      if (auto tables = e.tables.lock())
        return tables;

  auto tables = std::make_shared<const rotationTables_c>(settings.model, radius, settings.amounts);
  cache.push_back({ settings, radius, tables });
  return tables;
}

std::shared_ptr<const rotationTables_c>
rotationTablesFor(const problem_c & problem, const rotationSettings_t & settings) {

  if (problem.getPuzzle().getGridType()->getType() != gridType_c::GT_BRICKS || !problem.resultValid())
    return nullptr;

  return sharedRotationTables(settings, rotationRadiusFor(problem));
}
