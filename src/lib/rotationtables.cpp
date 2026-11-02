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

#include "assembly.h"
#include "disassemblernode.h"
#include "disassembly.h"
#include "gridtype.h"
#include "problem.h"
#include "puzzle.h"
#include "symmetries.h"
#include "voxel.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <numbers>
#include <map>
#include <mutex>
#include <string>
#include <tuple>

using rotationGeometry::rotationTables_c;

std::vector<rotationGeometry::pathSpec_t> rotationFamily(const rotationSettings_t & settings) {
  std::vector<rotationGeometry::pathSpec_t> family = rotationGeometry::compassFamily(settings.amounts);
  for (rotationGeometry::pathSpec_t & s : rotationGeometry::wideExtension(settings.amounts, settings.wideAmounts))
    family.push_back(std::move(s));
  return family;
}

rotationSettings_t rotationConfig_t::resolve(void) const {

  auto fromEnv = [](const char * name) -> std::optional<std::string> {
    const char * v = std::getenv(name);
    if (!v || !*v)
      return std::nullopt;
    return std::string(v);
  };

  /* each field: explicit, else the environment, else the default */
  rotationFamily_e fam = ROTATION_FAMILY_COMPASS;
  if (family)
    fam = *family;
  else if (auto v = fromEnv("BURRTOOLS_ROT_FAMILY")) {
    if (*v == "compass")
      fam = ROTATION_FAMILY_COMPASS;
    else if (*v == "wide")
      fam = ROTATION_FAMILY_WIDE;
    else
      bt_te("BURRTOOLS_ROT_FAMILY must be compass or wide");
  }

  bool plan = false;
  if (planner)
    plan = *planner;
  else if (auto v = fromEnv("BURRTOOLS_ROT_PLANNER")) {
    if (*v != "0" && *v != "1")
      bt_te("BURRTOOLS_ROT_PLANNER must be 0 or 1");
    plan = (*v == "1");
  }

  rotationSettings_t s;
  s.model = model ? *model : rotationGeometry::model_t{ 0.081, 0 };
  s.amounts = { 0.05, 0.1, 0.15, 0.2, 0.25, 0.3, 0.35, 0.4, 0.45 };
  if (fam == ROTATION_FAMILY_WIDE)
    s.wideAmounts = { 0.05, 0.1, 0.15, 0.2, 0.25, 0.3, 0.35, 0.4, 0.45, 0.5, 0.55 };
  s.planner = plan;
  if (slack)
    s.slack = *slack;
  return s;
}

rotationSettings_t defaultRotationSettings(void) {
  return rotationConfig_t{}.resolve();
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
  rotationSettings_t settings = {};
  int radius = 0;
  std::weak_ptr<const rotationTables_c> tables;
};

bool sameSettings(const rotationSettings_t & a, const rotationSettings_t & b) {
  return a.model.play == b.model.play && a.model.chamfer == b.model.chamfer &&
         a.amounts == b.amounts && a.wideAmounts == b.wideAmounts;
}

/* Tables kept alive after their last solver is done, most recent first:
 * solving one design after another (GUI, Python) then builds them once.
 * Two, so that a solve with the planner keeps both of its sets. */
constexpr size_t KEPT_TABLES = 2;

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
  std::vector<std::shared_ptr<const rotationTables_c>> kept;
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

/* with shared().mutex held */
void keep(const std::shared_ptr<const rotationTables_c> & tables) {
  std::vector<std::shared_ptr<const rotationTables_c>> & kept = shared().kept;
  std::erase(kept, tables);
  kept.insert(kept.begin(), tables);
  if (kept.size() > KEPT_TABLES)
    kept.pop_back();
}

}

std::shared_ptr<const rotationTables_c>
sharedRotationTables(const rotationSettings_t & settings, int radius) {

  std::vector<cacheEntry_t> & cache = shared().cache;
  std::vector<std::shared_ptr<const rotationTables_c>> & kept = shared().kept;

  /* Building under the lock keeps two solvers from building the same
   * tables twice; requests for other settings wait, which is rare. */
  std::lock_guard<std::mutex> lock(shared().mutex);

  std::erase_if(cache, [](const cacheEntry_t & e) { return e.tables.expired(); });

  for (const cacheEntry_t & e : cache)
    if (e.radius >= radius && sameSettings(e.settings, settings))
      if (auto tables = e.tables.lock()) {
        keep(tables);
        return tables;
      }

  auto tables = std::make_shared<const rotationTables_c>(settings.model, radius, rotationFamily(settings));

  /* the new tables serve every request smaller ones of the same settings
   * did, so those need not be kept */
  for (const cacheEntry_t & e : cache)
    if (sameSettings(e.settings, settings))
      if (auto smaller = e.tables.lock())
        std::erase(kept, smaller);

  cache.push_back({ settings, radius, tables });
  keep(tables);
  return tables;
}

void releaseRotationTables(void) {
  std::lock_guard<std::mutex> lock(shared().mutex);
  shared().kept.clear();
}

std::shared_ptr<const rotationTables_c>
rotationTablesFor(const problem_c & problem, const rotationSettings_t & settings) {

  if (problem.getPuzzle().getGridType()->getType() != gridType_c::GT_BRICKS || !problem.resultValid())
    return nullptr;

  return sharedRotationTables(settings, rotationRadiusFor(problem));
}

rotationContext_c::rotationContext_c(const problem_c & p, const rotationSettings_t & s,
                                     std::shared_future<std::shared_ptr<const rotationTables_c>> tables,
                                     std::shared_future<std::shared_ptr<const rotationTables_c>> relaxed) :
  problem(p),
  problemRadius(rotationRadiusFor(p)),
  settings(s),
  tableFuture(std::move(tables)),
  relaxedFuture(std::move(relaxed))
{
  const gridType_c * gt = problem.getPuzzle().getGridType();
  const unsigned int orientations = gt->getSymmetries()->getNumTransformations();

  for (unsigned int part = 0; part < problem.getNumberOfParts(); part++) {

    for (unsigned int j = 0; j < problem.getPartMaximum(part); j++)
      piecePart.push_back(part);

    /* every orientation's cells (and colours) relative to their lowest
     * corner, and that corner relative to the hotspot */
    typedef std::vector<std::tuple<int, int, int, unsigned int>> form_t;
    std::map<form_t, unsigned int> first;
    std::vector<canonical_t> & canon = canonicalOf.emplace_back(orientations);
    std::vector<std::array<int, 3>> corner(orientations);

    for (unsigned int t = 0; t < orientations; t++) {
      std::unique_ptr<voxel_c> v(gt->getVoxel(*problem.getPartShape(part)));
      bt_assert2(v->transform(t));
      form_t form;
      int mx = INT_MAX, my = INT_MAX, mz = INT_MAX;
      for (unsigned int z = 0; z < v->getZ(); z++)
        for (unsigned int y = 0; y < v->getY(); y++)
          for (unsigned int x = 0; x < v->getX(); x++)
            if (v->isFilled(x, y, z)) {
              const int cx = (int)x - v->getHx(), cy = (int)y - v->getHy(), cz = (int)z - v->getHz();
              mx = std::min(mx, cx); my = std::min(my, cy); mz = std::min(mz, cz);
              form.push_back({ cx, cy, cz, v->getColor(x, y, z) });
            }
      for (auto & [x, y, z, c] : form) {
        x -= mx; y -= my; z -= mz;
      }
      std::sort(form.begin(), form.end());
      corner[t] = { mx, my, mz };

      /* the first orientation with this form is the canonical one; the
       * hotspot moves by the difference of the corners */
      auto [it, inserted] = first.insert({ form, t });
      const unsigned int c = it->second;
      canon[t] = { (unsigned char)c, corner[t][0] - corner[c][0], corner[t][1] - corner[c][1], corner[t][2] - corner[c][2] };
    }
  }
}

rotationContext_c::~rotationContext_c() {
  park(std::move(tableFuture));
  park(std::move(relaxedFuture));
}

rotationContext_c::canonical_t rotationContext_c::canonical(unsigned int piece, unsigned int trans) const {
  bt_assert(piece < piecePart.size());
  const std::vector<canonical_t> & canon = canonicalOf[piecePart[piece]];
  /* mirrored placements keep their orientation */
  if (trans >= canon.size())
    return { (unsigned char)trans, 0, 0, 0 };
  return canon[trans];
}

bool rotationContext_c::canonicalize(disassemblerNode_c * node, const std::vector<unsigned int> & pieces) const {
  bool changed = false;
  for (unsigned int i = 0; i < node->getPiecenumber(); i++) {
    if (node->is_piece_removed(i))
      continue;
    const canonical_t c = canonical(pieces[i], node->getTrans(i));
    if (c.trans == node->getTrans(i))
      continue;
    node->set(i, node->getX(i) + c.dx, node->getY(i) + c.dy, node->getZ(i) + c.dz, c.trans);
    changed = true;
  }
  return changed;
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

namespace {

/* In-plane doubled offsets from the pivot and axial layer of every cell. */
std::vector<std::array<int, 3>> pivotOffsets(const std::vector<rotationGeometry::cell_t> & cells,
                                             unsigned int axis, int hx, int hy, int hz) {
  const int pu = (axis == 0) ? hy : ((axis == 1) ? hz : hx);
  const int pv = (axis == 0) ? hz : ((axis == 1) ? hx : hy);
  std::vector<std::array<int, 3>> r;
  for (const rotationGeometry::cell_t & c : cells) {
    int u, v;
    rotationGeometry::inPlaneDoubled(c.x, c.y, c.z, axis, &u, &v);
    r.push_back({ u - pu, v - pv, rotationGeometry::axialCoordinate(c.x, c.y, c.z, axis) });
  }
  return r;
}

/* The key of a turn's scene: sense, then the moving and the static cubes as
 * offsets from the pivot, layers relative to the lowest moving one. Equal
 * keys are equal scenes, whatever the axis and position. */
std::vector<int> sceneKey(unsigned int sense, std::vector<std::array<int, 3>> moving, std::vector<std::array<int, 3>> stat, int layer0) {
  std::sort(moving.begin(), moving.end());
  std::sort(stat.begin(), stat.end());
  std::vector<int> key = { (int)sense, (int)moving.size() };
  for (const auto * list : { &moving, &stat })
    for (const auto & c : *list) {
      key.push_back(c[0]);
      key.push_back(c[1]);
      key.push_back(c[2] - layer0);
    }
  return key;
}

}

int rotationContext_c::plannedPath(const std::vector<rotationGeometry::cell_t> & moving, const std::vector<rotationGeometry::cell_t> & stat,
                                   unsigned int axis, int hx, int hy, int hz, unsigned int sense) {

  using rotationGeometry::cell_t;
  bt_assert(planner());

  /* the static cubes that can matter: in a layer of a moving cube and
   * within reach of its sweep, shifts and cube size included */
  const std::vector<std::array<int, 3>> mo = pivotOffsets(moving, axis, hx, hy, hz);
  const std::vector<std::array<int, 3>> so = pivotOffsets(stat, axis, hx, hy, hz);
  double reach = 0;
  int layer0 = INT_MAX;
  for (const auto & c : mo) {
    reach = std::max(reach, 0.5 * std::hypot(c[0], c[1]));
    layer0 = std::min(layer0, c[2]);
  }
  reach += std::numbers::sqrt2 * (settings.plan.maxShift + 1) + 0.1;
  std::vector<std::array<int, 3>> near;
  std::vector<cell_t> nearCells;
  for (size_t i = 0; i < so.size(); i++)
    if (0.5 * std::hypot(so[i][0], so[i][1]) <= reach &&
        std::any_of(mo.begin(), mo.end(), [&](const auto & m) { return m[2] == so[i][2]; })) {
      near.push_back(so[i]);
      nearCells.push_back(stat[i]);
    }

  /* the reverse turn: the moving cubes turned, the other sense */
  std::vector<std::array<int, 3>> turned;
  std::vector<cell_t> turnedCells;
  for (size_t i = 0; i < mo.size(); i++) {
    turned.push_back(sense == 0 ? std::array<int, 3>{ -mo[i][1], mo[i][0], mo[i][2] }
                                : std::array<int, 3>{ mo[i][1], -mo[i][0], mo[i][2] });
    cell_t c = moving[i];
    rotationGeometry::turnAbout(&c.x, &c.y, &c.z, hx, hy, hz, axis, sense);
    turnedCells.push_back(c);
  }

  std::vector<int> forward = sceneKey(sense, mo, near, layer0);
  std::vector<int> backward = sceneKey(sense ^ 1, turned, near, layer0);
  const bool fromHere = forward <= backward;
  const std::vector<int> & key = fromHere ? forward : backward;

  const unsigned int family = tables().size();
  {
    std::lock_guard<std::mutex> lock(planMutex);
    auto it = plans.find(key);
    if (it != plans.end()) {
      const int id = fromHere ? it->second.forward : it->second.backward;
      return (id < 0) ? -1 : (int)family + id;
    }
  }

  /* plan from the key's side, outside the lock: the result depends only on
   * the key, so a worker computing it twice gets the same */
  rotationGeometry::planSettings_t ps = settings.plan;
  ps.floor = 0;
  ps.upper = false;
  ps.widest = false;
  const rotationGeometry::planarScene_c scene(fromHere ? moving : turnedCells, nearCells, axis, hx, hy, hz, settings.model);
  const rotationGeometry::turnPlan_t plan = rotationGeometry::planTurn(scene, fromHere ? sense : sense ^ 1, ps);
  /* the planner's bound may be 0 at contact; recorded turns must replay
   * as clear, like the family's */
  const bool found = plan.found && plan.clearance >= 0 && scene.certifyPath(plan.path) == rotationGeometry::PATH_CLEAR;
  if (plan.gaveUp)
    noteSkipped(SKIP_PLANNER);

  std::lock_guard<std::mutex> lock(planMutex);
  auto [it, inserted] = plans.try_emplace(key, planned_t{ -1, -1 });
  if (inserted && found) {
    it->second.forward = (int)plannedPaths.size();
    plannedPaths.push_back(std::make_unique<rotationGeometry::path_t>(plan.path));
    it->second.backward = (int)plannedPaths.size();
    plannedPaths.push_back(std::make_unique<rotationGeometry::path_t>(rotationGeometry::reversePath(plan.path)));
  }
  const int id = fromHere ? it->second.forward : it->second.backward;
  return (id < 0) ? -1 : (int)family + id;
}

rotationGeometry::pathSpec_t rotationContext_c::pathSpec(unsigned int sense, unsigned int index) const {
  const unsigned int family = tables().size();
  if (index < family)
    return tables().table(sense, index).getSpec();
  std::lock_guard<std::mutex> lock(planMutex);
  bt_assert(index - family < plannedPaths.size());
  return { rotationGeometry::PATH_PLANNED, sense, 0, 0, *plannedPaths[index - family] };
}

std::shared_ptr<rotationContext_c>
rotationContextFor(const problem_c & problem, const rotationSettings_t & settings, bool background) {

  if (problem.getPuzzle().getGridType()->getType() != gridType_c::GT_BRICKS || !problem.resultValid())
    return nullptr;

  const int radius = rotationRadiusFor(problem);
  auto tablesOf = [background, radius](const rotationSettings_t & s) {
    if (background)
      return std::async(std::launch::async, [s, radius] { return sharedRotationTables(s, radius); }).share();
    std::promise<std::shared_ptr<const rotationTables_c>> ready;
    ready.set_value(sharedRotationTables(s, radius));
    return ready.get_future().share();
  };

  std::shared_future<std::shared_ptr<const rotationTables_c>> relaxed;
  if (settings.planner) {
    rotationSettings_t r = settings;
    /* at most the largest play that leaves the cubes larger than their
     * bevel: the slack only picks which turns are worth planning */
    r.model.play = std::min(settings.model.play + settings.slack, 1.0 - 2.0 * settings.model.chamfer - 1e-6);
    relaxed = tablesOf(r);
  }
  return std::make_shared<rotationContext_c>(problem, settings, tablesOf(settings), std::move(relaxed));
}

namespace {

void optimizeRec(separation_c * s, const assembly_c * a, const problem_c & problem,
                 const rotationSettings_t & settings, turnPlay_t & result) {

  using rotationGeometry::cell_t;
  const gridType_c * gt = problem.getPuzzle().getGridType();

  /* the cells of piece k of the separation in state st */
  auto cellsOf = [&](const state_c * st, unsigned int k) {
    const unsigned int piece = s->getPieceName(k);
    std::unique_ptr<voxel_c> v(gt->getVoxel(problem.getPartShape(problem.getPartIdToPieceId(piece))));
    bt_assert2(v->transform(st->hasTrans() ? st->getTrans(k) : a->getTransformation(piece)));
    std::vector<cell_t> cells;
    for (unsigned int z = 0; z < v->getZ(); z++)
      for (unsigned int y = 0; y < v->getY(); y++)
        for (unsigned int x = 0; x < v->getX(); x++)
          if (v->isFilled(x, y, z))
            cells.push_back({ (int)x - v->getHx() + st->getX(k), (int)y - v->getHy() + st->getY(k), (int)z - v->getHz() + st->getZ(k) });
    return cells;
  };

  for (unsigned int i = 1; i <= s->getMoves(); i++) {
    state_c * st = s->editState(i);
    const stateRotation_t * r = st->getRotation();
    if (!r)
      continue;
    result.rotations = true;

    const state_c * before = s->getState(i - 1);
    std::vector<cell_t> moving, stat;
    for (unsigned int k = 0; k < s->getPieceNumber(); k++) {
      if (before->pieceRemoved(k))
        continue;
      const std::vector<cell_t> c = cellsOf(before, k);
      std::vector<cell_t> & into = r->turns(k) ? moving : stat;
      into.insert(into.end(), c.begin(), c.end());
    }

    /* the turning pieces' own motion, whichever side the search recorded */
    const unsigned int sense = r->turnSense();
    int hx, hy, hz;
    rotationGeometry::pivotOf(r->axis, r->pivotU, r->pivotV, &hx, &hy, &hz);

    const rotationGeometry::playBound_t pb =
      rotationGeometry::requiredPlay(moving, stat, r->axis, hx, hy, hz, sense, settings.model, settings.plan);
    if (!pb.found) {
      result.failed = true;
      continue;
    }
    result.needs = std::max(result.needs, pb.needs);
    result.atLeast = std::max(result.atLeast, pb.atLeast);

    /* the path with the most room at the model's play */
    rotationGeometry::planSettings_t widest = settings.plan;
    widest.floor = 0;
    widest.upper = false;
    widest.widest = true;
    const rotationGeometry::planarScene_c scene(moving, stat, r->axis, hx, hy, hz, settings.model);
    const rotationGeometry::turnPlan_t best = rotationGeometry::planTurn(scene, sense, widest);
    if (!best.found || best.clearance < 0 || scene.certifyPath(best.path) != rotationGeometry::PATH_CLEAR)
      continue;   // keep the recorded path, it is certified at this play

    stateRotation_t nr = *r;
    nr.path = { rotationGeometry::PATH_PLANNED, sense, 0, 0, best.path };
    nr.inverse = false;
    st->setRotation(nr);
  }

  if (s->editRemoved())
    optimizeRec(s->editRemoved(), a, problem, settings, result);
  if (s->editLeft())
    optimizeRec(s->editLeft(), a, problem, settings, result);
}

}

turnPlay_t optimizeTurns(separation_c * s, const assembly_c * a, const problem_c & problem, const rotationSettings_t & settings) {
  turnPlay_t result;
  optimizeRec(s, a, problem, settings, result);
  return result;
}
