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
#include <chrono>
#include <mutex>

using rotationGeometry::rotationTables_c;

rotationSettings_t defaultRotationSettings(void) {
  return { { 0.081 }, { 0.1, 0.2, 0.3, 0.4 } };
}

int rotationRadiusFor(const problem_c & problem) {
  auto extent = [](const voxel_c * v) {
    return (int)std::max({ v->boundX2() - v->boundX1(), v->boundY2() - v->boundY1(), v->boundZ2() - v->boundZ1() }) + 1;
  };
  int largest = extent(getResultShape(problem));
  for (unsigned int i = 0; i < problem.getNumberOfParts(); i++)
    largest = std::max(largest, extent(problem.getPartShape(i)));
  return largest + 1;
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

typedef std::shared_future<std::shared_ptr<const rotationTables_c>> tableFuture_t;

/* The tables shared between solvers, and the background builds no context
 * waits for any more. Dropping the last reference to a std::async future
 * blocks until its build is done, which could be the GUI thread stopping a
 * solve right after starting it; such futures are kept here instead. They
 * are declared after the cache, so at exit they are destroyed first and a
 * build still running finishes while the cache it writes to exists. */
struct shared_t {
  std::mutex mutex;
  std::vector<cacheEntry_t> cache;
  std::mutex parkedMutex;
  std::vector<tableFuture_t> parked;
};

shared_t & shared(void) {
  static shared_t s;
  return s;
}

void park(tableFuture_t f) {
  if (!f.valid() || f.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    return;
  shared_t & s = shared();
  std::lock_guard<std::mutex> lock(s.parkedMutex);
  std::erase_if(s.parked, [](const tableFuture_t & p) { return p.wait_for(std::chrono::seconds(0)) == std::future_status::ready; });
  s.parked.push_back(std::move(f));
}

}

std::shared_ptr<const rotationTables_c>
sharedRotationTables(const rotationSettings_t & settings, int radius) {

  std::vector<cacheEntry_t> & cache = shared().cache;

  /* Building under the lock keeps two solvers from building the same
   * tables twice; requests for other settings wait, which is rare. */
  std::lock_guard<std::mutex> lock(shared().mutex);

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

rotationContext_c::rotationContext_c(const problem_c & p,
                                     std::shared_future<std::shared_ptr<const rotationTables_c>> tables) :
  problem(p),
  tableFuture(std::move(tables))
{
}

rotationContext_c::~rotationContext_c() {
  park(std::move(tableFuture));
}

bool rotationContext_c::lookup(const rotationPairKey_t & key, const rotationPivotSet_t ** set) {
  shard_t & sh = shards[rotationPairKeyHash_t()(key) % SHARDS];
  std::lock_guard<std::mutex> lock(sh.mutex);
  auto it = sh.sets.find(key);
  if (it == sh.sets.end())
    return false;
  *set = it->second.get();
  return true;
}

const rotationPivotSet_t * rotationContext_c::store(const rotationPairKey_t & key, std::unique_ptr<rotationPivotSet_t> & set) {
  shard_t & sh = shards[rotationPairKeyHash_t()(key) % SHARDS];
  std::lock_guard<std::mutex> lock(sh.mutex);
  auto it = sh.sets.find(key);
  if (it != sh.sets.end())
    return it->second.get();
  if (entries.load(std::memory_order_relaxed) >= MAX_ENTRIES)
    return set.get();
  entries.fetch_add(1, std::memory_order_relaxed);
  return (sh.sets[key] = std::move(set)).get();
}

std::shared_ptr<rotationContext_c>
rotationContextFor(const problem_c & problem, const rotationSettings_t & settings, bool background) {

  if (problem.getPuzzle().getGridType()->getType() != gridType_c::GT_BRICKS || !problem.resultValid())
    return nullptr;

  const int radius = rotationRadiusFor(problem);
  std::shared_future<std::shared_ptr<const rotationTables_c>> tables;
  if (background)
    tables = std::async(std::launch::async, [settings, radius] { return sharedRotationTables(settings, radius); }).share();
  else {
    std::promise<std::shared_ptr<const rotationTables_c>> ready;
    ready.set_value(sharedRotationTables(settings, radius));
    tables = ready.get_future().share();
  }
  return std::make_shared<rotationContext_c>(problem, std::move(tables));
}
