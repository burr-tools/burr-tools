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
#include <tuple>

using rotationGeometry::rotationTables_c;

const char * rotationModeName(rotationMode_e mode) {
  switch (mode) {
    case ROTATION_OFF: return "off";
    case ROTATION_FAST: return "fast";
    case ROTATION_BALANCED: return "balanced";
    case ROTATION_SLOW: return "slow";
  }
  bt_te("rotationModeName: no such mode");
}

std::optional<rotationMode_e> rotationModeFromName(const std::string & name) {
  for (rotationMode_e m : { ROTATION_OFF, ROTATION_FAST, ROTATION_BALANCED, ROTATION_SLOW })
    if (name == rotationModeName(m))
      return m;
  return std::nullopt;
}

double rotationGatePlay(const rotationGeometry::model_t & model) {
  /* at most the largest play that leaves the cubes larger than their
   * bevel: the gate only picks which turns are worth planning */
  const double play = std::min(model.play + std::max(model.play, 0.04), 1.0 - 2.0 * model.chamfer - 1e-6);
  return std::max(play, model.play);
}

std::vector<rotationGeometry::pathSpec_t> rotationFamily(const rotationSettings_t & settings) {
  std::vector<rotationGeometry::pathSpec_t> family = rotationGeometry::compassFamily(settings.amounts);
  for (rotationGeometry::pathSpec_t & s : rotationGeometry::wideExtension(settings.amounts, settings.wideAmounts))
    family.push_back(std::move(s));
  return family;
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
 * Two, so that a balanced solve keeps both of its sets. */
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
  return sharedRotationTables(settings, radius, nullptr);
}

std::shared_ptr<const rotationTables_c>
sharedRotationTables(const rotationSettings_t & settings, int radius, const std::shared_ptr<const rotationTables_c> & tighter) {

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

  std::shared_ptr<const rotationTables_c> tables;
  if (tighter && tighter->getRadius() >= radius) {
    radius = tighter->getRadius();
    tables = std::make_shared<const rotationTables_c>(settings.model, *tighter);
  } else
    tables = std::make_shared<const rotationTables_c>(settings.model, radius, rotationFamily(settings));

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
  reach += std::numbers::sqrt2 * (settings.decide.maxShift + 1) + 0.1;
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
  const rotationGeometry::planarScene_c scene(fromHere ? moving : turnedCells, nearCells, axis, hx, hy, hz, settings.model);
  const auto t0 = std::chrono::steady_clock::now();
  const rotationGeometry::turnVerdict_t verdict = rotationGeometry::answerTurn(scene, fromHere ? sense : sense ^ 1, settings.decide);
  const uint64_t nanos = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
  plannerNanos.fetch_add(nanos, std::memory_order_relaxed);
  if (verdict.answer == rotationGeometry::TURN_IMPOSSIBLE)
    impossibleNanos.fetch_add(nanos, std::memory_order_relaxed);
  for (uint64_t w = worstNanos.load(std::memory_order_relaxed);
       nanos > w && !worstNanos.compare_exchange_weak(w, nanos, std::memory_order_relaxed); )
    ;
  nPlanned.fetch_add(1, std::memory_order_relaxed);
  (verdict.answer == rotationGeometry::TURN_POSSIBLE ? nPossible
   : verdict.answer == rotationGeometry::TURN_IMPOSSIBLE ? nImpossible : nUndecided).fetch_add(1, std::memory_order_relaxed);
  const bool found = verdict.answer == rotationGeometry::TURN_POSSIBLE;
  const rotationGeometry::path_t & path = verdict.path;
  /* neither proven: the turn may be possible, and the search skips it */
  if (verdict.answer == rotationGeometry::TURN_UNDECIDED)
    noteSkipped(SKIP_PLANNER);

  std::lock_guard<std::mutex> lock(planMutex);
  auto [it, inserted] = plans.try_emplace(key, planned_t{ -1, -1 });
  if (inserted && found) {
    it->second.forward = (int)plannedPaths.size();
    plannedPaths.push_back(std::make_unique<rotationGeometry::path_t>(path));
    it->second.backward = (int)plannedPaths.size();
    plannedPaths.push_back(std::make_unique<rotationGeometry::path_t>(rotationGeometry::reversePath(path)));
  }
  const int id = fromHere ? it->second.forward : it->second.backward;
  return (id < 0) ? -1 : (int)family + id;
}

int rotationContext_c::turnPath(const std::vector<rotationGeometry::cell_t> & moving, const std::vector<rotationGeometry::cell_t> & stat,
                                const rotationGeometry::occupancy_c & occupied,
                                unsigned int axis, int hx, int hy, int hz, unsigned int sense) {
  nCandidates.fetch_add(1, std::memory_order_relaxed);
  const int path = tables().firstClear(moving, occupied, axis, hx, hy, hz, sense);
  if (path >= 0) {
    nFamily.fetch_add(1, std::memory_order_relaxed);
    return path;
  }
  switch (settings.mode) {
    case ROTATION_BALANCED:
      /* No family path: if the family gets through with more play, the
       * turn is near the threshold, where some other motion may still get
       * through at this play. */
      if (closureTables().firstClear(moving, occupied, axis, hx, hy, hz, sense) < 0)
        return -1;
      break;
    case ROTATION_SLOW:
      /* every candidate: its end pose is free (listRotations) */
      break;
    default:
      return -1;
  }
  nGate.fetch_add(1, std::memory_order_relaxed);
  return plannedPath(moving, stat, axis, hx, hy, hz, sense);
}

rotationContext_c::stats_t rotationContext_c::getStats(void) const {
  stats_t s;
  s.candidates = nCandidates.load(std::memory_order_relaxed);
  s.family = nFamily.load(std::memory_order_relaxed);
  s.gate = nGate.load(std::memory_order_relaxed);
  s.possible = nPossible.load(std::memory_order_relaxed);
  s.impossible = nImpossible.load(std::memory_order_relaxed);
  s.undecided = nUndecided.load(std::memory_order_relaxed);
  s.planned = nPlanned.load(std::memory_order_relaxed);
  s.plannerSeconds = 1e-9 * (double)plannerNanos.load(std::memory_order_relaxed);
  s.impossibleSeconds = 1e-9 * (double)impossibleNanos.load(std::memory_order_relaxed);
  s.worstSeconds = 1e-9 * (double)worstNanos.load(std::memory_order_relaxed);
  return s;
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

  if (settings.mode == ROTATION_OFF ||
      problem.getPuzzle().getGridType()->getType() != gridType_c::GT_BRICKS || !problem.resultValid())
    return nullptr;

  const int radius = rotationRadiusFor(problem);
  auto tablesOf = [background, radius](const rotationSettings_t & s) {
    if (background)
      return std::async(std::launch::async, [s, radius] { return sharedRotationTables(s, radius); }).share();
    std::promise<std::shared_ptr<const rotationTables_c>> ready;
    ready.set_value(sharedRotationTables(s, radius));
    return ready.get_future().share();
  };

  const tableFuture_t tables = tablesOf(settings);
  tableFuture_t relaxed;
  if (settings.mode == ROTATION_BALANCED) {
    rotationSettings_t r = settings;
    r.model.play = rotationGatePlay(settings.model);
    /* from the model's tables: only their blockers are checked again */
    auto derive = [r, radius, tables] { return sharedRotationTables(r, radius, tables.get()); };
    if (background)
      relaxed = std::async(std::launch::async, derive).share();
    else {
      std::promise<std::shared_ptr<const rotationTables_c>> ready;
      ready.set_value(derive());
      relaxed = ready.get_future().share();
    }
  }
  return std::make_shared<rotationContext_c>(problem, settings, tables, std::move(relaxed));
}

namespace {

/* One turn of a disassembly, as optimizeTurns sees it:
 * its state, the cells that turn and the others before it, the turning
 * pieces' own sense and the pivot. */
struct turnScene_t {
  state_c * state;
  const stateRotation_t * rotation;
  std::vector<rotationGeometry::cell_t> moving, stat;
  unsigned int sense;
  int hx, hy, hz;
};

/* f(turnScene_t &) for every turn of the disassembly, in a fixed order
 * (the states of s, then the removed part, then the left one); f returns
 * false to stop. False if stopped. */
template <class F>
bool walkTurns(separation_c * s, const assembly_c * a, const problem_c & problem, F & f) {

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

    turnScene_t t{ st, r, {}, {}, r->turnSense(), 0, 0, 0 };
    const state_c * before = s->getState(i - 1);
    for (unsigned int k = 0; k < s->getPieceNumber(); k++) {
      if (before->pieceRemoved(k))
        continue;
      const std::vector<cell_t> c = cellsOf(before, k);
      std::vector<cell_t> & into = r->turns(k) ? t.moving : t.stat;
      into.insert(into.end(), c.begin(), c.end());
    }
    rotationGeometry::pivotOf(r->axis, r->pivotU, r->pivotV, &t.hx, &t.hy, &t.hz);
    if (!f(t))
      return false;
  }

  if (s->editRemoved() && !walkTurns(s->editRemoved(), a, problem, f))
    return false;
  if (s->editLeft() && !walkTurns(s->editLeft(), a, problem, f))
    return false;
  return true;
}

/* the turn's path replaced by a planned one, of the turning pieces */
void setPath(const turnScene_t & t, const rotationGeometry::path_t & path) {
  stateRotation_t nr = *t.rotation;
  nr.path = { rotationGeometry::PATH_PLANNED, t.sense, 0, 0, path };
  nr.inverse = false;
  t.state->setRotation(nr);
}

/* the recorded path, in the frame it was recorded in (inverted, the other
 * pieces followed it) */
rotationGeometry::knownPath_t recordedPath(const turnScene_t & t) {
  return { rotationGeometry::makePath(t.rotation->path), t.rotation->inverse };
}

}

turnPlay_t optimizeTurns(separation_c * s, const assembly_c * a, const problem_c & problem, const rotationSettings_t & given,
                         const optimizeControl_t & control) {
  turnPlay_t result;
  rotationSettings_t settings = given;
  settings.decide.stop = control.stop;

  auto each = [&](turnScene_t & t) {
    if (control.stop && control.stop->load(std::memory_order_relaxed)) {
      result.stopped = true;
      return false;
    }
    result.rotations = true;
    const stateRotation_t * r = t.rotation;
    const std::vector<rotationGeometry::cell_t> & moving = t.moving, & stat = t.stat;
    const unsigned int sense = t.sense;
    const int hx = t.hx, hy = t.hy, hz = t.hz;
    const std::vector<rotationGeometry::knownPath_t> recorded = { recordedPath(t) };

    /* when the recorded path is clear at the model's play, the turn needs
     * no more, whatever the planner finds */
    const rotationGeometry::playBound_t pb =
      rotationGeometry::requiredPlay(moving, stat, r->axis, hx, hy, hz, sense, settings.model, settings.decide,
                                     0.0005, 0.4, recorded);
    /* and the bevel it needs at the model's play */
    const rotationGeometry::playBound_t bb =
      rotationGeometry::requiredBevel(moving, stat, r->axis, hx, hy, hz, sense, settings.model, settings.decide,
                                      0.0005, recorded);
    if (bb.found)
      result.bevelUpper = std::max(result.bevelUpper, bb.upper);
    else
      result.bevelFailed = true;
    result.bevelLower = std::max(result.bevelLower, bb.lower);
    /* also proven when no play up to the largest does */
    result.lower = std::max(result.lower, pb.lower);
    if (!pb.found) {
      result.failed = true;
      if (control.done)
        control.done->fetch_add(1, std::memory_order_relaxed);
      return true;
    }
    result.upper = std::max(result.upper, pb.upper);

    /* the path with the most room at the model's play */
    const rotationGeometry::planarScene_c scene(moving, stat, r->axis, hx, hy, hz, settings.model);
    const rotationGeometry::turnPlan_t best = rotationGeometry::widestTurn(scene, sense, settings.decide);
    if (control.done)
      control.done->fetch_add(1, std::memory_order_relaxed);
    /* otherwise the recorded path stays, the model's play is where it was found */
    if (best.found && best.clearance >= 0 && scene.certifyPath(best.path) == rotationGeometry::PATH_CLEAR)
      setPath(t, best.path);
    return true;
  };
  walkTurns(s, a, problem, each);
  /* a stop during a turn's planner runs leaves them undecided, which looks
   * like a turn that needs more: that turn's bounds are not valid */
  if (control.stop && control.stop->load(std::memory_order_relaxed))
    result.stopped = true;
  return result;
}
