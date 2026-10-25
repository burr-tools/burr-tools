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
#ifndef __ROTATION_TABLES_H__
#define __ROTATION_TABLES_H__

#include "rotationgeometry.h"

#include <atomic>
#include <cstdint>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

class problem_c;
class disassemblerNode_c;

/**
 * Solver-side setup of rotation moves: which physical model and which
 * rotate-and-shift paths a solver uses, and where its tables come from.
 *
 * Rotation moves are optional. A solver that does not want them never asks
 * for tables and passes an empty pointer down to its disassemblers, so it
 * neither builds nor touches anything here.
 */
struct rotationSettings_t {
  rotationGeometry::model_t model = {};
  std::vector<double> amounts;   ///< shift amounts of the wiggle and slide paths
  /** Turns no family path clears are planned (rotationGeometry::planTurn)
   *  if the family clears them with `slack` more play: the planner finds
   *  the motions the family misses near the threshold. Off by default: it
   *  makes a solve 5 to 40 times slower. BURRTOOLS_NO_ROT_PLANNER=1 turns it
   *  off where it is on. See design/2026-10-19-rotation-moves.md. */
  bool planner = false;
  double slack = 0.02;
  rotationGeometry::planSettings_t plan = {};
};

/** The settings used when the caller has no others: PR #50's implicit
 *  model (play 0.081, sharp edges) until physical calibration provides
 *  better defaults, and shift amounts every 0.05 from 0.05 to 0.45 (145
 *  paths per sense; see the TIC results in design/2026-10-19-rotation-moves.md). */
rotationSettings_t defaultRotationSettings(void);

/** Table radius for a problem: the largest filled extent of its result and
 *  of its pieces, plus one. That covers every turn of a single piece and of
 *  every group that stays within the assembled shape; turns of groups
 *  spread wider are skipped, and reported (rotationContext_c::getSkipped). */
int rotationRadiusFor(const problem_c & problem);

/**
 * The rotation tables for a problem, or nullptr when rotation moves are not
 * possible for it (only cube grids with a valid result shape have them).
 *
 * Tables are immutable and shared: all solvers asking for the same settings
 * get the same object, built on the first request and freed when the last
 * user drops it; a table with a larger radius also serves smaller ones.
 * Safe to call from several threads. Building takes up to a few seconds, so
 * callers do it once, before starting worker threads.
 */
std::shared_ptr<const rotationGeometry::rotationTables_c>
rotationTablesFor(const problem_c & problem, const rotationSettings_t & settings);

/** Same, with an explicit radius; for callers without a problem and tests. */
std::shared_ptr<const rotationGeometry::rotationTables_c>
sharedRotationTables(const rotationSettings_t & settings, int radius);

/**
 * The pivots, relative to a piece's hotspot in doubled in-plane units, at
 * which another piece blocks it on every path; a bitmap over a box.
 */
struct rotationPivotSet_t {
  int u0 = 0, v0 = 0, width = 0, height = 0;
  std::vector<uint64_t> bits;
  bool test(int u, int v) const {
    u -= u0;
    v -= v0;
    if (u < 0 || v < 0 || u >= width || v >= height)
      return false;
    const size_t b = (size_t)u * height + v;
    return (bits[b >> 6] >> (b & 63)) & 1;
  }
};

/** What a rotationPivotSet_t depends on: both parts, their orientations,
 *  their relative position, the axis and the sense. */
struct rotationPairKey_t {
  uint64_t a, b;
  bool operator==(const rotationPairKey_t &) const = default;
};

struct rotationPairKeyHash_t {
  size_t operator()(const rotationPairKey_t & k) const { return k.a * 0x9E3779B97F4A7C15ull ^ (k.b + (k.a >> 29)); }
};

/**
 * Everything the disassemblers of one solve share for rotation moves of one
 * problem: the tables (possibly still being built in the background), the
 * cache of blocking pivots per piece pair, and which limits made the search
 * skip turns. Thread-safe; one per problem and solve, handed to every
 * disassembler; none at all when rotation moves are off.
 */
class rotationContext_c {

  public:

    /** relaxed: the tables of the model with settings.slack more play,
     *  empty when the planner is off */
    rotationContext_c(const problem_c & problem, const rotationSettings_t & settings,
                      std::shared_future<std::shared_ptr<const rotationGeometry::rotationTables_c>> tables,
                      std::shared_future<std::shared_ptr<const rotationGeometry::rotationTables_c>> relaxed = {});

    const problem_c & getProblem(void) const { return problem; }

    /** The tables; waits for them if they are still being built. */
    const rotationGeometry::rotationTables_c & tables(void) const { return *tableFuture.get(); }

    /** True if turns the family misses are planned. */
    bool planner(void) const { return relaxedFuture.valid(); }

    /** The tables whose "blocked on every path" relation decides which
     *  groups may turn: with the planner those of the model with `slack`
     *  more play (every turn the planner tries is one of them), else
     *  tables(). Waits for them if needed. */
    const rotationGeometry::rotationTables_c & closureTables(void) const {
      return planner() ? *relaxedFuture.get() : tables();
    }

    /** With the planner: the planned path of a turn the family cannot make
     *  but closureTables() can, as a path index after the family's (see
     *  pathSpec), or -1 if the planner finds none. Results are cached by
     *  the turn's scene and shared by all workers. A turn and its reverse
     *  are planned once, from the same side, so a turn is found exactly
     *  when its reverse is (the search needs undirected edges). */
    int plannedPath(const std::vector<rotationGeometry::cell_t> & moving, const std::vector<rotationGeometry::cell_t> & stat,
                    unsigned int axis, int pivotHx, int pivotHy, int pivotHz, unsigned int sense);

    /** The path a node's path index names: a family path of sense, or a
     *  planned one. */
    rotationGeometry::pathSpec_t pathSpec(unsigned int sense, unsigned int index) const;

    /** The model and planner settings of this context. */
    const rotationSettings_t & getSettings(void) const { return settings; }

    /** Cached blocking pivots of a pair: true and the set (nullptr: never
     *  blocks) if known. */
    bool lookup(const rotationPairKey_t & key, const rotationPivotSet_t ** set);

    /** Publish a computed set. Returns the cached set (another worker may
     *  have been first); when the cache is full the set stays with the
     *  caller, who must then keep it alive, and set.get() is returned. */
    const rotationPivotSet_t * store(const rotationPairKey_t & key, std::unique_ptr<rotationPivotSet_t> & set);

    /**
     * Canonical orientations. A piece with symmetries occupies the same
     * cells (colours included) in several orientations; a turn can then
     * reach a state that equals one reached by translations but carries
     * another orientation id, and the search would visit it twice. So every
     * piece of a search state is stored in the smallest orientation with
     * the same cells, its hotspot moved by `dx, dy, dz` to keep them in
     * place.
     */
    struct canonical_t {
      unsigned char trans;
      int dx, dy, dz;
    };
    canonical_t canonical(unsigned int piece, unsigned int trans) const;

    /** Bring every piece of the node into its canonical orientation;
     *  `pieces` names the node's pieces. True if something changed. */
    bool canonicalize(disassemblerNode_c * node, const std::vector<unsigned int> & pieces) const;

    /** Limits that made the search skip turns, a bit set of skip_e. */
    enum skip_e {
      SKIP_RADIUS = 1,   ///< a group reached beyond the tables' radius
      SKIP_PIECES = 2,   ///< a (sub)problem had more pieces than groups can address
      SKIP_PLANNER = 4   ///< the planner gave up on a turn (planSettings_t::budget)
    };
    void noteSkipped(skip_e s) { skipped.fetch_or(s, std::memory_order_relaxed); }
    unsigned int getSkipped(void) const { return skipped.load(std::memory_order_relaxed); }

    // no copying and assigning
    rotationContext_c(const rotationContext_c &) = delete;
    rotationContext_c & operator=(const rotationContext_c &) = delete;

  private:

    const problem_c & problem;
    const rotationSettings_t settings;
    std::shared_future<std::shared_ptr<const rotationGeometry::rotationTables_c>> tableFuture;
    std::shared_future<std::shared_ptr<const rotationGeometry::rotationTables_c>> relaxedFuture;

    /* planner results by scene key (see plannedPath): the store indices of
     * the path seen from the key's side and from the other, -1 for none */
    struct planned_t { int forward, backward; };
    mutable std::mutex planMutex;
    std::map<std::vector<int>, planned_t> plans;
    std::vector<std::unique_ptr<rotationGeometry::path_t>> plannedPaths;

    /* [part][trans], and the part of every piece */
    std::vector<std::vector<canonical_t>> canonicalOf;
    std::vector<unsigned int> piecePart;

    static constexpr size_t SHARDS = 64;
    static constexpr size_t MAX_ENTRIES = size_t(1) << 22;
    struct shard_t {
      std::mutex mutex;
      std::unordered_map<rotationPairKey_t, std::unique_ptr<rotationPivotSet_t>, rotationPairKeyHash_t> sets;
    };
    shard_t shards[SHARDS];
    std::atomic<size_t> entries{0};
    std::atomic<unsigned int> skipped{0};
};

class separation_c;
class assembly_c;

/** What optimizeTurns found. */
struct turnPlay_t {
  bool rotations = false;   ///< the disassembly turns at all
  /** every turn is proven possible at this play (along its recorded path)... */
  double needs = 0;
  /** ...and some turn is proven impossible below this (for shifts up to
   *  the planner's maxShift), 0 if nothing could be proven */
  double atLeast = 0;
  /** a turn needed more than the planner's largest play: needs is not valid */
  bool failed = false;
};

/**
 * The play a disassembly needs: for every turn, the smallest play at which
 * some motion gets the turning pieces through (rotationGeometry::requiredPlay);
 * the disassembly needs the largest of them. Every turn's recorded path is
 * replaced by the path with the most clearance at the model's play: the
 * motion with the most room, which is also the one to animate. Slow (a
 * bisection of planner runs per turn), meant for the few disassemblies that
 * are reported. See design/2026-10-19-rotation-moves.md.
 */
turnPlay_t optimizeTurns(separation_c * s, const assembly_c * a, const problem_c & problem, const rotationSettings_t & settings);

/**
 * A rotation context for a problem, or nullptr when rotation moves are not
 * possible for it (only cube grids with a valid result shape have them).
 * With background set, the tables are built on another thread and the
 * first disassembler that needs them waits; otherwise they are built (or
 * taken from the cache) before returning.
 */
std::shared_ptr<rotationContext_c>
rotationContextFor(const problem_c & problem, const rotationSettings_t & settings, bool background = false);

#endif
