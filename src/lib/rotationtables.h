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
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class problem_c;
class puzzle_c;
class stlExporter_c;
class disassemblerNode_c;

/**
 * Solver-side setup of rotation moves: which physical model and which
 * rotate-and-shift paths a solver uses, and where its tables come from.
 *
 * Rotation moves are optional. A solver that does not want them never asks
 * for tables and passes an empty pointer down to its disassemblers, so it
 * neither builds nor touches anything here.
 */
/** The wideAmounts of ROTATION_FAMILY_WIDE, the default family. */
inline const std::vector<double> WIDE_AMOUNTS = { 0.05, 0.1, 0.15, 0.2, 0.25, 0.3, 0.35, 0.4, 0.45, 0.5, 0.55 };

struct rotationSettings_t {
  /** The model of a puzzle that stores none (puzzle_c::getRotationModel):
   *  the STL export's defaults from before rotation moves (offset 0.01 mm,
   *  bevel 0.3 mm on 10 mm cubes), so a puzzle that never turns exports
   *  and draws as it always did. Turns usually need more clearance than
   *  that; the GUI and burrTxt say so, and Optimize Turns / burrTxt -M
   *  find what a puzzle needs. */
  rotationGeometry::model_t model = { 0.002, 0.03 };
  /** shift amounts of the compass wiggles and slides: every 0.05 from 0.05
   *  to 0.45 (see the TIC results in design/2026-10-19-rotation-moves.md) */
  std::vector<double> amounts = { 0.05, 0.1, 0.15, 0.2, 0.25, 0.3, 0.35, 0.4, 0.45 };
  /** amounts of the 16-direction extension after them
   *  (rotationGeometry::wideExtension), empty: compass family only. The
   *  extension finds turns the compass family misses at about 2.5 times
   *  the table cost; see design/2026-10-19-rotation-moves.md. */
  std::vector<double> wideAmounts = WIDE_AMOUNTS;
  /** Turns no family path clears are planned (rotationGeometry::planTurn)
   *  if the family clears them with `slack` more play: the planner finds
   *  the motions the family misses near the threshold. Off by default: it
   *  makes a solve 5 to 40 times slower. See
   *  design/2026-10-19-rotation-moves.md. */
  bool planner = false;
  double slack = 0.02;
  rotationGeometry::planSettings_t plan = {};
};

/** The +90° paths of the settings' family, in table order. */
std::vector<rotationGeometry::pathSpec_t> rotationFamily(const rotationSettings_t & settings);

/** Path families rotationConfig_t can choose. */
enum rotationFamily_e {
  ROTATION_FAMILY_COMPASS,  ///< 8 directions, wiggles and slides, amounts 0.05 to 0.45 (145 paths)
  ROTATION_FAMILY_WIDE      ///< the default: compass plus 16 directions and ramps, amounts to 0.55 (881 paths)
};

/**
 * What a front end chooses for rotation moves: the rotation part of
 * solverConfig_t (solver_config.h), which says how unset fields are filled
 * (environment, then rotationSettings_t's initializers) and lists the
 * variables. resolve() is defined in solver_config.cpp with the rest.
 */
struct rotationConfig_t {
  std::optional<rotationFamily_e> family;
  std::optional<bool> planner;
  std::optional<double> slack;

  rotationSettings_t resolve(void) const;
};


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
 * get the same object, built on the first request; a table with a larger
 * radius also serves smaller ones. The two most recently used sets stay
 * alive after their last user drops them, so solving designs of one size
 * one after another builds the tables once (releaseRotationTables frees
 * them); others are freed with their last user.
 * Safe to call from several threads. Building takes up to a few seconds, so
 * callers do it once, before starting worker threads.
 */
std::shared_ptr<const rotationGeometry::rotationTables_c>
rotationTablesFor(const problem_c & problem, const rotationSettings_t & settings);

/** Same, with an explicit radius; for callers without a problem and tests. */
std::shared_ptr<const rotationGeometry::rotationTables_c>
sharedRotationTables(const rotationSettings_t & settings, int radius);

/** Stop keeping the most recently used tables alive; tables still in use
 *  stay until their last user drops them. */
void releaseRotationTables(void);

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

    /** Does not wait for tables still being built in the background. */
    ~rotationContext_c();

    const problem_c & getProblem(void) const { return problem; }

    /** The problem's own table radius (rotationRadiusFor). The tables may
     *  be larger, served from the cache; turns are only looked for within
     *  this radius, so results do not depend on what was solved before. */
    int radius(void) const { return problemRadius; }

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
      SKIP_GROUPS = 4,   ///< more than 16 closures: only single closures were tried, not their unions
      SKIP_PLANNER = 8   ///< the planner gave up on a turn (planSettings_t::budget)
    };
    void noteSkipped(skip_e s) { skipped.fetch_or(s, std::memory_order_relaxed); }
    unsigned int getSkipped(void) const { return skipped.load(std::memory_order_relaxed); }

    // no copying and assigning
    rotationContext_c(const rotationContext_c &) = delete;
    rotationContext_c & operator=(const rotationContext_c &) = delete;

  private:

    const problem_c & problem;
    const int problemRadius;
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
  /** upper bound: every turn is proven possible at this play, along its
   *  recorded path, so the disassembly works at it */
  double upper = 0;
  /** lower bound: some turn is proven impossible below this (for shifts up
   *  to the planner's maxShift), so this disassembly does not work below
   *  it; another disassembly of the puzzle might. 0 if nothing could be
   *  proven */
  double lower = 0;
  /** a turn needed more than the planner's largest play: upper is not
   *  valid; re-planning (optimizeControl_t::measure false): a turn got no
   *  path proven at the model's play and kept one that is not */
  bool failed = false;
  /** stopped (optimizeControl_t::stop) before every turn was done: nothing valid */
  bool stopped = false;
  /** measuring: per turn, in the order optimizeTurns visits them, a path
   *  of the turning pieces (their own sense, not inverted) proven at the
   *  turn's measured play, empty if the planner gave none; re-planning for
   *  a smaller play takes it when its search finds nothing better
   *  (optimizeControl_t::proven) */
  std::vector<rotationGeometry::path_t> proven;
};

/** How optimizeTurns runs, for a front end that shows its progress. */
struct optimizeControl_t {
  /** set from another thread to stop, within a planner run at the latest */
  const std::atomic<bool> * stop = nullptr;
  /** counts the turns done (separation_c::sumRotations of them in all) */
  std::atomic<unsigned int> * done = nullptr;
  /** false: only re-plan each turn's path, with the most room at the
   *  model's play (after the play changed), without measuring the play */
  bool measure = true;
  /** plan for this model instead of the problem's: a front end re-plans
   *  for a play before making it the problem's */
  std::optional<rotationGeometry::model_t> model;
  /** re-planning: paths proven before (turnPlay_t::proven of a measuring
   *  run on the same disassembly). The planner's grid can miss a path that
   *  is clear by a hair, so a turn it finds none for takes this one, or
   *  keeps its recorded one, when that is certified at the model's play */
  const std::vector<rotationGeometry::path_t> * proven = nullptr;
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
turnPlay_t optimizeTurns(separation_c * s, const assembly_c * a, const problem_c & problem, const rotationSettings_t & settings,
                         const optimizeControl_t & control = {});

/**
 * The settings a problem's rotation moves use: the given ones with the
 * puzzle's model when it stores one (puzzle_c::getRotationModel). The model
 * describes the pieces, so it belongs to the puzzle; the rest is how the
 * solver searches. rotationContextFor and optimizeTurns apply it
 * themselves.
 */
rotationSettings_t rotationSettingsFor(const problem_c & problem, rotationSettings_t settings);

/** The model a problem's rotation moves are checked for: its puzzle's, else
 *  the default (rotationSettings_t::model). */
rotationGeometry::model_t rotationModelOf(const problem_c & problem);

/** The model the pieces of a shape are checked for: the puzzle's, else the
 *  default. */
std::optional<rotationGeometry::model_t> rotationModelForShape(const puzzle_c & puzzle, unsigned int shape);

/** The same, but only a stored model: nothing while the puzzle stores none.
 *  The 3D views' STL style draws such pieces; without one a shape keeps the
 *  view's own look. */
std::optional<rotationGeometry::model_t> storedRotationModelForShape(const puzzle_c & puzzle, unsigned int shape);

/** For an STL export dialog: whether the exporter's bevel and offset make
 *  the pieces the turns were checked for (`expected`, from
 *  rotationModelOf or rotationModelForShape; nothing when the problems
 *  disagree). */
struct stlModelState_t {
  enum match_e {
    NO_CUBES,      ///< the exporter does not make cubes: nothing to say
    UNDECIDED,     ///< the problems using the shape check different models
    MATCHES,       ///< the pieces are the ones the turns are checked for
    DIFFERS        ///< they are not: `now` against `expected`
  } match = NO_CUBES;
  rotationGeometry::model_t now, expected;
};
stlModelState_t stlModelState(const stlExporter_c & stl, const std::optional<rotationGeometry::model_t> & expected);

/** stlModelState as an English sentence, for the FLTK dialogs (the Qt one
 *  formats it with its translations); empty for NO_CUBES */
std::string stlModelNote(const stlExporter_c & stl, const std::optional<rotationGeometry::model_t> & expected);

/**
 * A rotation context for a problem, or nullptr when rotation moves are not
 * possible for it (only cube grids with a valid result shape have them).
 * With background set, the tables are built on another thread and the
 * first disassembler that needs them waits; otherwise they are built (or
 * taken from the cache) before returning. model: check against these
 * pieces instead of the puzzle's, for a what-if question (burrTxt -M); it
 * is an argument, never stored, so the question does not change the pieces.
 */
std::shared_ptr<rotationContext_c>
rotationContextFor(const problem_c & problem, const rotationSettings_t & settings, bool background = false,
                   const std::optional<rotationGeometry::model_t> & model = std::nullopt);

#endif
