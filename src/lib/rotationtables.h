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
/** The amounts of the wide family's 16-direction extension (the only
 *  family the front ends use: the compass paths and this extension). */
inline const std::vector<double> WIDE_AMOUNTS = { 0.05, 0.1, 0.15, 0.2, 0.25, 0.3, 0.35, 0.4, 0.45, 0.5, 0.55 };

/**
 * How a solve decides turns (design/2026-10-19-rotation-moves.md, "Rotation
 * modes"). Every mode accepts only turns certified at the puzzle's play, so
 * results from any of them hold at that play; the modes differ in how many
 * candidate turns they look at, and so in what they find and what it costs.
 */
enum rotationMode_e : unsigned char {
  ROTATION_OFF,        ///< no rotation moves: rotationContextFor gives no context
  ROTATION_FAST,       ///< the fixed path family only, at the puzzle's play
  /** the family at play + max(play, 0.04) (rotationGatePlay) proposes the
   *  turns the family misses at the play, the planner (answerTurn) accepts
   *  those possible at the play */
  ROTATION_BALANCED,
  /** a family path clear at the play is taken; every other turn of a group
   *  whose end pose is free goes to the planner, without a gate */
  ROTATION_SLOW
};

/** "off", "fast", "balanced", "slow" */
const char * rotationModeName(rotationMode_e mode);
/** the mode a name of rotationModeName stands for; none for anything else */
std::optional<rotationMode_e> rotationModeFromName(const std::string & name);

struct rotationSettings_t {
  /** The model of a puzzle that stores none (puzzle_c::getRotationModel):
   *  the STL export's defaults from before rotation moves (offset 0.01 mm,
   *  bevel 0.3 mm on 10 mm cubes), so a puzzle that never turns exports
   *  and draws as it always did. Turns usually need more clearance than
   *  that; the GUI and burrTxt say so, and Optimize Turns / burrTxt -P
   *  measure what a disassembly needs. */
  rotationGeometry::model_t model = { 0.002, 0.03 };
  /** shift amounts of the compass wiggles and slides: every 0.05 from 0.05
   *  to 0.45 (see the TIC results in design/2026-10-19-rotation-moves.md) */
  std::vector<double> amounts = { 0.05, 0.1, 0.15, 0.2, 0.25, 0.3, 0.35, 0.4, 0.45 };
  /** amounts of the 16-direction extension after them
   *  (rotationGeometry::wideExtension), the wide family. Empty leaves the
   *  compass part alone, which tests use for its smaller tables; the
   *  front ends always use the wide family. */
  std::vector<double> wideAmounts = WIDE_AMOUNTS;
  /** how turns are decided; the default is what a front end that turns
   *  rotation moves on without choosing gets */
  rotationMode_e mode = ROTATION_BALANCED;
  /** how a turn is decided: by the solver, and for the play a turn needs
   *  and the path with the most room (requiredPlay, widestTurn; Optimize
   *  Turns and burrTxt -P) */
  rotationGeometry::decideSettings_t decide = {};
  /** BURRTOOLS_ROT_STATS=1: a front end reports how the turns were decided
   *  (rotationContext_c::getStats; benchmarking) */
  bool stats = false;
};

/** The +90° paths of the settings' family, in table order. */
std::vector<rotationGeometry::pathSpec_t> rotationFamily(const rotationSettings_t & settings);

/** The play of ROTATION_BALANCED's gate tables for a model: play +
 *  max(play, 0.04), so twice the play but never less than 0.04 more (twice
 *  a tiny measured play would gate out the turns it was measured on), at
 *  most what leaves the cubes larger than their bevel. */
double rotationGatePlay(const rotationGeometry::model_t & model);

/**
 * What a front end chooses for rotation moves: the rotation part of
 * solverConfig_t (solver_config.h), which says how unset fields are filled
 * (environment, then rotationSettings_t's initializers) and lists the
 * variables. resolve() is defined in solver_config.cpp with the rest.
 */
struct rotationConfig_t {
  std::optional<rotationMode_e> mode;
  std::optional<bool> stats;

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

/** Same, built if needed from tables of the same family and bevel, at most
 *  this play and at least this radius (rotationTables_c's constructor from
 *  tighter tables): the planner's gate tables from the model's. */
std::shared_ptr<const rotationGeometry::rotationTables_c>
sharedRotationTables(const rotationSettings_t & settings, int radius,
                     const std::shared_ptr<const rotationGeometry::rotationTables_c> & tighter);

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

    /** relaxed: ROTATION_BALANCED's gate tables, the model's at
     *  rotationGatePlay; empty in the other modes, which need none (a
     *  balanced context given none uses the model's tables as its gate) */
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

    /** How turns are decided (never ROTATION_OFF: then there is no context). */
    rotationMode_e mode(void) const { return settings.mode; }

    /** True if turns the family misses are planned (balanced and slow). */
    bool planner(void) const { return settings.mode == ROTATION_BALANCED || settings.mode == ROTATION_SLOW; }

    /** Which groups may turn is decided by a relation "blocks at this
     *  pivot" between pieces (movementAnalysator_c::listRotations). Fast
     *  and balanced take it from tables, "blocks on every path" of
     *  closureTables(); slow from the end pose alone (a cube of one piece
     *  turns onto a cube of the other, rotationGeometry::endPosePivot),
     *  which every motion of the planner's class has to respect. */
    bool endPoseClosure(void) const { return settings.mode == ROTATION_SLOW; }

    /** The tables whose "blocked on every path" relation decides which
     *  groups may turn when endPoseClosure() is false: balanced's gate
     *  tables (every turn the planner tries is one they clear), else
     *  tables(). Waits for them if needed. */
    const rotationGeometry::rotationTables_c & closureTables(void) const {
      return relaxedFuture.valid() ? *relaxedFuture.get() : tables();
    }

    /** With the planner: the planned path of a turn the family cannot make
     *  (balanced: but closureTables() can), as a path index after the
     *  family's (see pathSpec), or -1 if the planner finds none. Results are cached by
     *  the turn's scene and shared by all workers. A turn and its reverse
     *  are planned once, from the same side, so a turn is found exactly
     *  when its reverse is (the search needs undirected edges). */
    int plannedPath(const std::vector<rotationGeometry::cell_t> & moving, const std::vector<rotationGeometry::cell_t> & stat,
                    unsigned int axis, int pivotHx, int pivotHy, int pivotHz, unsigned int sense);

    /** The path index of a turn of the moving cubes among the static
     *  ones (occupied: the same cubes as stat), -1 if it is not made: the
     *  first family path that clears it; else balanced: for a turn
     *  closureTables() clears, plannedPath; slow: plannedPath. What listRotations asks for
     *  every candidate turn. */
    int turnPath(const std::vector<rotationGeometry::cell_t> & moving, const std::vector<rotationGeometry::cell_t> & stat,
                 const rotationGeometry::occupancy_c & occupied,
                 unsigned int axis, int pivotHx, int pivotHy, int pivotHz, unsigned int sense);

    /** The path a node's path index names: a family path of sense, or a
     *  planned one. */
    rotationGeometry::pathSpec_t pathSpec(unsigned int sense, unsigned int index) const;

    /** The model, mode and planner settings of this context. */
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

    /** How the turns asked for were decided (turnPath), for measuring. */
    struct stats_t {
      uint64_t candidates = 0;        ///< turns asked for
      uint64_t family = 0;            ///< a family path clears them
      uint64_t gate = 0;              ///< no family path, handed to the planner: balanced, the gate's tables clear it; slow, all
      uint64_t planned = 0;           ///< planner runs (the rest of `gate` were answered by the cache)
      uint64_t possible = 0, impossible = 0, undecided = 0;   ///< the runs' answers
      double plannerSeconds = 0;      ///< in planner runs, summed over threads
      double impossibleSeconds = 0;   ///< of them in runs that proved a turn impossible
      double worstSeconds = 0;        ///< the longest run
    };
    stats_t getStats(void) const;

    /** Limits that made the search skip turns, a bit set of skip_e. */
    enum skip_e {
      SKIP_RADIUS = 1,   ///< a group reached beyond the tables' radius
      SKIP_PIECES = 2,   ///< a (sub)problem had more pieces than groups can address
      SKIP_GROUPS = 4,   ///< more than 16 closures: only single closures were tried, not their unions
      SKIP_PLANNER = 8   ///< a turn was left undecided (decideSettings_t::resolution, maxBoxes)
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

    /* stats_t, counted with relaxed atomics */
    std::atomic<uint64_t> nCandidates{0}, nFamily{0}, nGate{0}, nPossible{0}, nImpossible{0}, nUndecided{0}, nPlanned{0};
    std::atomic<uint64_t> plannerNanos{0}, impossibleNanos{0}, worstNanos{0};
};

class separation_c;
class assembly_c;

/** What optimizeTurns found. */
struct turnPlay_t {
  bool rotations = false;   ///< the disassembly turns at all
  /** upper bound: every turn is proven possible at this play, so the
   *  disassembly works at it. The motion that proves it is not kept: the
   *  recorded paths are the ones with the most room at the model's play
   *  and are certified only there */
  double upper = 0;
  /** lower bound: some turn is proven impossible below this (for shifts up
   *  to the planner's maxShift), so this disassembly does not work below
   *  it; another disassembly of the puzzle might. 0 if nothing could be
   *  proven */
  double lower = 0;
  /** a turn needed more than the planner's largest play: upper is not
   *  valid */
  bool failed = false;
  /** the same for the bevel at the model's play (requiredBevel): every
   *  turn is proven possible with bevel bevelUpper, some turn impossible
   *  below bevelLower; bevelFailed: a turn is not possible with any bevel
   *  this play allows, bevelUpper is not valid */
  double bevelUpper = 0;
  double bevelLower = 0;
  bool bevelFailed = false;
  /** stopped (optimizeControl_t::stop) before every turn was done: nothing valid */
  bool stopped = false;
};

/** How optimizeTurns runs, for a front end that shows its progress. */
struct optimizeControl_t {
  /** set from another thread to stop, within a planner run at the latest */
  const std::atomic<bool> * stop = nullptr;
  /** counts the turns done (separation_c::sumRotations of them in all) */
  std::atomic<unsigned int> * done = nullptr;
};

/**
 * The play a disassembly needs: for every turn, the smallest play at which
 * some motion gets the turning pieces through (rotationGeometry::requiredPlay);
 * the disassembly needs the largest of them; likewise the bevel it needs at
 * the model's play (rotationGeometry::requiredBevel). Every turn's recorded path is
 * replaced by the path with the most clearance at the model's play: the
 * motion with the most room, which is also the one to animate. At a play
 * below the model's (down to `upper`) a turn takes some other motion, which
 * is not recorded. Slow (a
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

/** The pieces of a puzzle: its model, else the default. The same for every
 *  shape and problem; the 3D views' STL style draws only a stored one
 *  (puzzle_c::getRotationModel), so a puzzle without keeps their own look. */
rotationGeometry::model_t rotationModelOf(const puzzle_c & puzzle);

/** A solve that turns pieces stores the model it checks them for when the
 *  puzzle has none, so that a saved file says which pieces its
 *  disassemblies are for. The front ends call it before such a solve. */
void storeSolveRotationModel(puzzle_c & puzzle, const rotationSettings_t & settings);

/** For an STL export dialog: whether the exporter's bevel and offset make
 *  the pieces the turns are checked for (`expected`, from rotationModelOf). */
struct stlModelState_t {
  enum match_e {
    NO_CUBES,      ///< the exporter does not make cubes: nothing to say
    MATCHES,       ///< the pieces are the ones the turns are checked for
    DIFFERS        ///< they are not: `now` against `expected`
  } match = NO_CUBES;
  rotationGeometry::model_t now, expected;
};
stlModelState_t stlModelState(const stlExporter_c & stl, const rotationGeometry::model_t & expected);

/** stlModelState as an English sentence, for the FLTK dialogs (the Qt one
 *  formats it with its translations); empty for NO_CUBES */
std::string stlModelNote(const stlExporter_c & stl, const rotationGeometry::model_t & expected);

/**
 * A rotation context for a problem, or nullptr when rotation moves are not
 * possible for it (only cube grids with a valid result shape have them).
 * With background set, the tables are built on another thread and the
 * first disassembler that needs them waits; otherwise they are built (or
 * taken from the cache) before returning. ROTATION_OFF gives nullptr too.
 * Only balanced builds a second set of tables (its gate, rotationGatePlay).
 */
std::shared_ptr<rotationContext_c>
rotationContextFor(const problem_c & problem, const rotationSettings_t & settings, bool background = false);

#endif
