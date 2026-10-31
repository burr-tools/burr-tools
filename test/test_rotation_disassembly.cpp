#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"
#include "lib/assembler.h"
#include "lib/assembler_0.h"
#include "lib/assembly.h"
#include "lib/disasmtomoves.h"
#include "lib/disassembler_0.h"
#include "lib/disassembly.h"
#include "lib/disassemblernode.h"
#include "lib/disassemblerpool.h"
#include "lib/movementanalysator.h"
#include "lib/symmetries.h"
#include "lib/gridtype.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/rotationgeometry.h"
#include "lib/rotationtables.h"
#include "lib/solution.h"
#include "lib/voxel.h"
#include "tools/xml.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cmath>
#include <map>
#include <mutex>
#include <numbers>
#include <memory>
#include <set>
#include <sstream>
#include <tuple>
#include <vector>

using namespace rotationGeometry;

namespace {

/* the default settings with the planner on (it is off by default), and
 * with enough slack to reach the planned turn of AAA_most_rotations, which
 * the default slack of 0.02 does not: so its disassembly has a planned
 * path, which the replay and animation tests then cover. The model is
 * play 0.081 with a bevel of 0.032 (PR #50's play, puzzlecad's bevel),
 * which these tests were written for; the default is the STL export's
 * tight one, made for slides. */
rotationSettings_t withPlanner(void) {
  rotationSettings_t s = rotationSettings_t{};
  s.model = { 0.081, 0.032 };
  s.planner = true;
  s.slack = 0.04;
  return s;
}

typedef std::tuple<int, int, int> key_t;

class collect_c : public assembler_cb {
  public:
    std::vector<std::unique_ptr<assembly_c>> assemblies;
    bool assembly(std::unique_ptr<assembly_c> a) override {
      assemblies.push_back(std::move(a));
      return true;
    }
};

std::vector<std::unique_ptr<assembly_c>> assembleAll(problem_c & problem) {
  std::unique_ptr<assembler_c> assm = problem.getPuzzle().getGridType()->findAssembler(problem, bttest::settings());
  REQUIRE(assm != nullptr);
  REQUIRE(assm->createMatrix(false, false, false) == assembler_c::ERR_NONE);
  collect_c cb;
  assm->assemble(&cb);
  return std::move(cb.assemblies);
}

/* Independent replay of a disassembly: rebuilds every piece's cells from the
 * recorded positions and orientations and checks each step geometrically. */
class replay_c {

  public:

    replay_c(const problem_c & p, const model_t & m) : problem(p), model(m) {
      for (unsigned int i = 0; i < p.getNumberOfParts(); i++)
        for (unsigned int j = 0; j < p.getPartMaximum(i); j++)
          shapes.push_back(p.getPartShape(i));
    }

    unsigned int rotations = 0;

    void check(const separation_c & s) {
      for (unsigned int k = 0; k <= s.getMoves(); k++) {
        const state_c * st = s.getState(k);
        REQUIRE(st->hasTrans());
        checkDisjoint(s, k);
        if (k > 0)
          checkStep(s, k);
      }
      if (s.getRemoved())
        check(*s.getRemoved());
      if (s.getLeft())
        check(*s.getLeft());
    }

  private:

    const problem_c & problem;
    model_t model;
    std::vector<const voxel_c *> shapes;

    std::vector<cell_t> cells(const separation_c & s, unsigned int k, unsigned int pc) {
      const state_c * st = s.getState(k);
      std::unique_ptr<voxel_c> v(problem.getPuzzle().getGridType()->getVoxel(*shapes[s.getPieceName(pc)]));
      REQUIRE(v->transform(st->getTrans(pc)));
      std::vector<cell_t> out;
      for (unsigned int z = 0; z < v->getZ(); z++)
        for (unsigned int y = 0; y < v->getY(); y++)
          for (unsigned int x = 0; x < v->getX(); x++)
            if (v->isFilled(x, y, z))
              out.push_back({ st->getX(pc) + (int)x - v->getHx(),
                              st->getY(pc) + (int)y - v->getHy(),
                              st->getZ(pc) + (int)z - v->getHz() });
      return out;
    }

    static std::set<key_t> keys(const std::vector<cell_t> & cs) {
      std::set<key_t> out;
      for (const cell_t & c : cs)
        out.insert({ c.x, c.y, c.z });
      return out;
    }

    void checkDisjoint(const separation_c & s, unsigned int k) {
      std::set<key_t> seen;
      for (unsigned int pc = 0; pc < s.getPieceNumber(); pc++) {
        if (s.getState(k)->pieceRemoved(pc))
          continue;
        for (const cell_t & c : cells(s, k, pc)) {
          INFO("state " << k << " piece " << s.getPieceName(pc));
          CHECK(seen.insert({ c.x, c.y, c.z }).second);
        }
      }
    }

    void checkStep(const separation_c & s, unsigned int k) {

      const state_c * prev = s.getState(k - 1);
      const state_c * st = s.getState(k);
      const stateRotation_t * r = st->getRotation();

      if (!r) {
        /* translations never turn a piece */
        for (unsigned int pc = 0; pc < s.getPieceNumber(); pc++)
          CHECK(st->getTrans(pc) == prev->getTrans(pc));
        return;
      }

      rotations++;
      int hx, hy, hz;
      if (r->axis == 0)      { hx = 0; hy = r->pivotU; hz = r->pivotV; }
      else if (r->axis == 1) { hx = r->pivotV; hy = 0; hz = r->pivotU; }
      else                   { hx = r->pivotU; hy = r->pivotV; hz = 0; }

      std::vector<cell_t> moving, stat;
      unsigned int turned = 0;
      for (unsigned int pc = 0; pc < s.getPieceNumber(); pc++) {
        INFO("rotation into state " << k << " piece " << s.getPieceName(pc));
        REQUIRE_FALSE(st->pieceRemoved(pc));
        std::vector<cell_t> before = cells(s, k - 1, pc);
        std::vector<cell_t> after = cells(s, k, pc);

        if (!r->turns(pc)) {
          /* not turning: stays exactly where it is */
          CHECK(st->getTrans(pc) == prev->getTrans(pc));
          CHECK(keys(after) == keys(before));
          stat.insert(stat.end(), before.begin(), before.end());
          continue;
        }

        /* turning: lands on its start cells turned about the pivot */
        turned++;
        std::vector<cell_t> expected;
        for (cell_t c : before) {
          int dx = 2 * c.x - hx, dy = 2 * c.y - hy, dz = 2 * c.z - hz;
          quarterTurn(&dx, &dy, &dz, r->axis, r->turnSense());
          REQUIRE((hx + dx) % 2 == 0);
          REQUIRE((hy + dy) % 2 == 0);
          REQUIRE((hz + dz) % 2 == 0);
          expected.push_back({ (hx + dx) / 2, (hy + dy) / 2, (hz + dz) / 2 });
        }
        CHECK(keys(after) == keys(expected));
        moving.insert(moving.end(), before.begin(), before.end());
      }
      CHECK(turned > 0);

      /* the recorded path is collision-free, decided by the checker itself
       * rather than through the tables the solver used; inverted, the other
       * pieces followed it */
      planarScene_c scene(r->inverse ? stat : moving, r->inverse ? moving : stat, r->axis, hx, hy, hz, model);
      CHECK(scene.certifyPath(makePath(r->path)) == PATH_CLEAR);
    }
};

/* AAA_most_rotations solved once for all sections below */
struct aaa_t {
  std::unique_ptr<puzzle_c> puzzle;
  std::vector<std::unique_ptr<assembly_c>> assemblies;
  std::unique_ptr<separation_c> withoutRotations, withRotations;
};

const aaa_t & aaa(void) {
  static const aaa_t result = [] {
    aaa_t r;
    r.puzzle = puzzle_c::load("examples/AAA_most_rotations.xmpuzzle");
    REQUIRE(r.puzzle != nullptr);
    problem_c & problem = *r.puzzle->getProblem(0);
    r.assemblies = assembleAll(problem);
    REQUIRE(r.assemblies.size() == 1);
    r.withoutRotations = disassembler_0_c(problem, bttest::settings()).disassemble(r.assemblies[0].get());
    r.withRotations = disassembler_0_c(problem, bttest::settings(), rotationContextFor(problem, withPlanner()))
                        .disassemble(r.assemblies[0].get());
    return r;
  }();
  return result;
}

}

TEST_CASE("a turn of the group with piece 0 is saved as that group following the path", "[rotations][disasm]") {

  /* in memory pieces 1 and 3 turned and the others (0 and 2) followed the
   * path relative to them; the file lists the path's followers, 0 among
   * them, and reading it back gives the same rotation */
  state_c st(4);
  for (unsigned int i = 0; i < 4; i++)
    st.set(i, (int)i, 0, 0);
  stateRotation_t r = {};
  r.axis = 2;
  r.pivotU = 1;
  r.pivotV = 1;
  r.path.kind = rotationGeometry::PATH_PURE;
  r.path.sense = 1;
  r.pieces = { 1, 3 };
  r.inverse = true;
  st.setRotation(r);

  std::ostringstream out;
  {
    xmlWriter_c xml(out);
    st.save(xml, 4);
  }
  INFO(out.str());
  CHECK(out.str().find("pieces=\"0 2\"") != std::string::npos);
  CHECK(out.str().find("axis=\"z\"") != std::string::npos);
  CHECK(out.str().find("angle=\"-90\"") != std::string::npos);
  CHECK(out.str().find("inverse") == std::string::npos);

  std::istringstream in(out.str());
  xmlParser_c pars(in);
  pars.nextTag();
  const state_c back(pars, 4);
  REQUIRE(back.getRotation() != nullptr);
  CHECK(back.getRotation()->inverse);
  CHECK(back.getRotation()->pieces == std::vector<unsigned int>{ 1, 3 });
  CHECK(back.getRotation()->turnSense() == 0);
}

TEST_CASE("rotation moves do not grow the search node", "[rotations][disasm]") {
  /* pivot and turning group live in what was alignment padding */
  if (sizeof(void *) == 8)
    CHECK(sizeof(disassemblerNode_c) == 48);
}

TEST_CASE("the planner copes with family tables larger than its relaxed ones", "[rotations][disasm]") {

  /* The cache serves a request from any tables of the same settings with
   * at least the requested radius, so a context can get family tables
   * larger than its relaxed ones. The search must then only turn cubes
   * both cover (it threw "cube outside the tables" before), and find what
   * it finds with tables of the problem's own radius. */
  auto puzzle = puzzle_c::load("examples/AAA_most_rotations.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  problem_c & problem = *puzzle->getProblem(0);
  const rotationSettings_t settings = withPlanner();
  const int radius = rotationRadiusFor(problem);

  releaseRotationTables();
  auto larger = sharedRotationTables(settings, radius + 3);
  auto context = rotationContextFor(problem, settings);
  REQUIRE(context != nullptr);
  REQUIRE(context->tables().getRadius() > context->closureTables().getRadius());

  auto assemblies = assembleAll(problem);
  REQUIRE(assemblies.size() == 1);
  auto da = disassembler_0_c(problem, bttest::settings(), context).disassemble(assemblies[0].get());
  REQUIRE(da != nullptr);
  CHECK(da->movesText() == "2.3");

  releaseRotationTables();
}

TEST_CASE("AAA_most_rotations comes apart only with rotation moves", "[rotations][disasm]") {

  const problem_c & problem = *aaa().puzzle->getProblem(0);
  const rotationSettings_t settings = withPlanner();

  /* translations alone cannot take it apart */
  CHECK(aaa().withoutRotations == nullptr);

  const separation_c * da = aaa().withRotations.get();
  REQUIRE(da != nullptr);

  /* the shortest disassembly the breadth-first search finds, with the
   * planner; changes when the default model, the path family or the
   * planner changes (2.4 with the compass family alone: the planner, like
   * the wide family, finds a turn that saves a move) */
  CHECK(da->movesText() == "2.3");
  CHECK(da->sumRotations() == 2);

  /* one of its turns is planned: what the replay below covers */
  {
    unsigned int planned = 0;
    std::vector<const separation_c *> todo = { da };
    while (!todo.empty()) {
      const separation_c * s = todo.back();
      todo.pop_back();
      for (unsigned int i = 0; i <= s->getMoves(); i++)
        if (s->getState(i)->getRotation() && s->getState(i)->getRotation()->path.kind == PATH_PLANNED)
          planned++;
      for (const separation_c * c : { s->getLeft(), s->getRemoved() })
        if (c)
          todo.push_back(c);
    }
    CHECK(planned >= 1);
  }

  SECTION("every step replays as a valid motion") {
    replay_c replay(problem, settings.model);
    replay.check(*da);
    CHECK(replay.rotations == da->sumRotations());
  }

  SECTION("orientations and rotations survive saving and loading") {
    std::ostringstream out;
    {
      xmlWriter_c xml(out);
      da->save(xml);
    }
    std::istringstream in(out.str());
    xmlParser_c pars(in);
    pars.nextTag();
    separation_c back(pars, problem.getNumberOfPieces());

    CHECK(back.movesText() == da->movesText());
    CHECK(back.sumRotations() == da->sumRotations());

    replay_c replay(problem, settings.model);
    replay.check(back);
    CHECK(replay.rotations == da->sumRotations());
  }

  SECTION("a solution with an orientation the grid does not have is rejected") {
    solution_c sol(std::make_unique<assembly_c>(aaa().assemblies[0].get()), 0, std::make_unique<separation_c>(da), 0);
    std::ostringstream out;
    {
      xmlWriter_c xml(out);
      sol.save(xml);
    }
    const gridType_c * gt = problem.getPuzzle().getGridType();
    auto load = [&](const std::string & text) {
      std::istringstream in(text);
      xmlParser_c pars(in);
      pars.nextTag();
      solution_c(pars, problem.getNumberOfPieces(), gt);
    };
    CHECK_NOTHROW(load(out.str()));

    /* the first piece's orientation in the first state that has them */
    std::string bad = out.str();
    const size_t dt = bad.find("<dt>");
    REQUIRE(dt != std::string::npos);
    const size_t first = dt + 4, end = bad.find_first_of(" <", first);
    bad.replace(first, end - first, std::to_string(gt->getSymmetries()->getNumTransformationsMirror()));
    CHECK_THROWS(load(bad));
  }
}

namespace {

/* Centres of a piece's cubes as the GUI draws them at the current step of
 * the animation: cube (x, y, z) spans [x, x+1], a turning piece is turned
 * about the pivot and shifted, exactly like voxelFrame_c's transform. */
std::vector<std::array<double, 3>> drawnCentres(disasmToMoves_c & anim, const problem_c & problem,
                                                const std::vector<const voxel_c *> & shapes, unsigned int p) {
  std::unique_ptr<voxel_c> v(problem.getPuzzle().getGridType()->getVoxel(*shapes[p]));
  REQUIRE(anim.getTrans(p) >= 0);
  REQUIRE(v->transform(anim.getTrans(p)));

  piecePositions_c::turn_t turn;
  bool turning = anim.getTurn(p, &turn);
  const double a = turning ? turn.angle * std::numbers::pi / 180 : 0;
  const int u = (turning && turn.axis == 0) ? 1 : (turning && turn.axis == 1) ? 2 : 0;
  const int w = (turning && turn.axis == 0) ? 2 : (turning && turn.axis == 1) ? 0 : 1;

  std::vector<std::array<double, 3>> out;
  for (unsigned int z = 0; z < v->getZ(); z++)
    for (unsigned int y = 0; y < v->getY(); y++)
      for (unsigned int x = 0; x < v->getX(); x++)
        if (v->isFilled(x, y, z)) {
          std::array<double, 3> c = { anim.getX(p) + (int)x - v->getHx() + 0.5,
                                      anim.getY(p) + (int)y - v->getHy() + 0.5,
                                      anim.getZ(p) + (int)z - v->getHz() + 0.5 };
          if (turning) {
            double du = c[u] - turn.pivot[u], dv = c[w] - turn.pivot[w];
            c[u] = turn.pivot[u] + du * std::cos(a) - dv * std::sin(a) + turn.shift[u];
            c[w] = turn.pivot[w] + du * std::sin(a) + dv * std::cos(a) + turn.shift[w];
          }
          out.push_back(c);
        }
  std::sort(out.begin(), out.end());
  return out;
}

bool sameCentres(std::vector<std::array<double, 3>> a, std::vector<std::array<double, 3>> b) {
  if (a.size() != b.size())
    return false;
  /* match each centre of a to a centre of b within a small tolerance */
  for (const auto & c : a) {
    auto it = std::find_if(b.begin(), b.end(), [&c](const std::array<double, 3> & d) {
      return std::fabs(c[0] - d[0]) < 1e-3 && std::fabs(c[1] - d[1]) < 1e-3 && std::fabs(c[2] - d[2]) < 1e-3;
    });
    if (it == b.end())
      return false;
    b.erase(it);
  }
  return true;
}

}

TEST_CASE("a tighter rotation model drops the disassemblies that turn", "[rotations][disasm]") {

  /* turns are proven for the model they were found with; less play or a
   * smaller bevel adds material, more of either only removes some */
  auto solvedWithTurns = [] {
    auto puzzle = puzzle_c::load("examples/AAA_most_rotations.xmpuzzle");
    REQUIRE(puzzle != nullptr);
    problem_c & problem = *puzzle->getProblem(0);
    auto assemblies = assembleAll(problem);
    REQUIRE(assemblies.size() == 1);
    auto da = disassembler_0_c(problem, bttest::settings(), rotationContextFor(problem, rotationSettings_t{}))
                .disassemble(assemblies[0].get());
    REQUIRE(da != nullptr);
    REQUIRE(da->sumRotations() > 0);
    problem.setAssembler(std::make_unique<assembler_0_c>(problem, bttest::settings()));
    problem.addSolution(new assembly_c(assemblies[0].get()), da.release());
    problem.finishedSolving();
    return puzzle;
  };
  const rotationGeometry::model_t model = rotationSettings_t{}.model;

  for (rotationGeometry::model_t tighter : { rotationGeometry::model_t{ model.play / 2, model.chamfer },
                                             rotationGeometry::model_t{ model.play, model.chamfer / 2 } }) {
    auto puzzle = solvedWithTurns();
    problem_c & problem = *puzzle->getProblem(0);
    puzzle->setRotationModel(tighter);
    REQUIRE(problem.getNumberOfSavedSolutions() == 1);
    CHECK(problem.getSavedSolution(0)->getDisassembly() == nullptr);
    CHECK(problem.getSavedSolution(0)->getAssembly() != nullptr);
  }

  auto puzzle = solvedWithTurns();
  problem_c & problem = *puzzle->getProblem(0);
  puzzle->setRotationModel({ model.play * 2, model.chamfer });
  CHECK(problem.getSavedSolution(0)->getDisassembly() != nullptr);
  /* back to the default from more play: tighter again */
  puzzle->clearRotationModel();
  CHECK(problem.getSavedSolution(0)->getDisassembly() == nullptr);
}

TEST_CASE("the animation of every rotation starts and ends on the recorded states", "[rotations][disasm]") {

  const problem_c & problem = *aaa().puzzle->getProblem(0);
  const separation_c * da = aaa().withRotations.get();
  REQUIRE(da != nullptr);

  std::vector<const voxel_c *> shapes;
  for (unsigned int i = 0; i < problem.getNumberOfParts(); i++)
    for (unsigned int j = 0; j < problem.getPartMaximum(i); j++)
      shapes.push_back(problem.getPartShape(i));

  disasmToMoves_c anim(da, 20, problem.getNumberOfPieces());

  unsigned int turnSteps = 0;
  for (unsigned int step = 0; step < da->sumMoves(); step++) {

    anim.setStep(step + 0.5f);
    std::vector<unsigned int> turning;
    for (unsigned int p = 0; p < problem.getNumberOfPieces(); p++) {
      piecePositions_c::turn_t t;
      if (anim.getTurn(p, &t))
        turning.push_back(p);
    }
    if (turning.empty())
      continue;
    turnSteps++;

    for (unsigned int p : turning) {
      INFO("step " << step << " piece " << p);

      anim.setStep((float)step);
      auto start = drawnCentres(anim, problem, shapes, p);
      anim.setStep(step + 1.0f);
      auto end = drawnCentres(anim, problem, shapes, p);

      /* the turn starts where the piece is and ends where the next state has it */
      anim.setStep(step + 1e-6f);
      CHECK(sameCentres(drawnCentres(anim, problem, shapes, p), start));
      anim.setStep(step + 1 - 1e-6f);
      CHECK(sameCentres(drawnCentres(anim, problem, shapes, p), end));
    }
  }

  CHECK(turnSteps == da->sumRotations());
}

namespace {

/* The cells of every piece: the moves are compared by where the pieces
 * are, not by position and orientation, as the search keeps symmetric
 * pieces in a canonical orientation with its own hotspot. */
typedef std::vector<std::set<key_t>> config_t;

std::vector<std::vector<cell_t>> worldCells(const problem_c & problem, const disassemblerNode_c * n) {
  const gridType_c * gt = problem.getPuzzle().getGridType();
  std::vector<const voxel_c *> shapes;
  for (unsigned int i = 0; i < problem.getNumberOfParts(); i++)
    for (unsigned int j = 0; j < problem.getPartMaximum(i); j++)
      shapes.push_back(problem.getPartShape(i));

  std::vector<std::vector<cell_t>> world(n->getPiecenumber());
  for (unsigned int i = 0; i < n->getPiecenumber(); i++) {
    if (n->getTrans(i) == 0xFF)   // removed: no cells, unlike any turned state
      continue;
    std::unique_ptr<voxel_c> v(gt->getVoxel(*shapes[i]));
    REQUIRE(v->transform(n->getTrans(i)));
    for (unsigned int z = 0; z < v->getZ(); z++)
      for (unsigned int y = 0; y < v->getY(); y++)
        for (unsigned int x = 0; x < v->getX(); x++)
          if (v->isFilled(x, y, z))
            world[i].push_back({ n->getX(i) + (int)x - v->getHx(),
                                 n->getY(i) + (int)y - v->getHy(),
                                 n->getZ(i) + (int)z - v->getHz() });
  }
  return world;
}

/* relative to piece 0's first cell: a translation of piece 0 is the
 * opposite one of the rest, and a turn can end as either */
config_t configOf(const std::vector<std::vector<cell_t>> & world) {
  cell_t o = { 0, 0, 0 };
  if (!world.empty() && !world[0].empty())
    o = *std::min_element(world[0].begin(), world[0].end(), [](const cell_t & a, const cell_t & b) {
      return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
    });
  config_t c(world.size());
  for (size_t i = 0; i < world.size(); i++)
    for (const cell_t & cell : world[i])
      c[i].insert({ cell.x - o.x, cell.y - o.y, cell.z - o.z });
  return c;
}

config_t configOf(const problem_c & problem, const disassemblerNode_c * n) {
  return configOf(worldCells(problem, n));
}

/* Every rotation move of a node by brute force: all groups but the whole,
 * all centre and corner pivots in the group's footprint, both senses about
 * all axes, end pose free and not a no-op, some path of the family clear.
 * Piece 0 stays in place: a group with it ends as the rest turned the other
 * way. */
std::set<config_t> bruteRotations(const problem_c & problem, const rotationContext_c & context,
                                  const disassemblerNode_c * node, unsigned int * canonicalized) {
  const rotationTables_c & tables = context.tables();
  const symmetries_c * sym = problem.getPuzzle().getGridType()->getSymmetries();
  const unsigned int pn = node->getPiecenumber();
  const std::vector<std::vector<cell_t>> world = worldCells(problem, node);

  auto turn = [](int & x, int & y, int & z, int hx, int hy, int hz, unsigned int axis, unsigned int sense) {
    int dx = 2 * x - hx, dy = 2 * y - hy, dz = 2 * z - hz;
    quarterTurn(&dx, &dy, &dz, axis, sense);
    x = (hx + dx) / 2; y = (hy + dy) / 2; z = (hz + dz) / 2;
  };

  std::set<config_t> out;
  const int reach = 2 * tables.getRadius();
  for (uint32_t group = 1; group + 1 < (1u << pn); group++)
    for (unsigned int axis = 0; axis < 3; axis++) {
      std::vector<cell_t> moving, stat;
      std::vector<unsigned int> owner;
      for (unsigned int i = 0; i < pn; i++)
        for (const cell_t & c : world[i]) {
          if (group & (1u << i)) { moving.push_back(c); owner.push_back(i); }
          else stat.push_back(c);
        }
      auto uv = [axis](const cell_t & c, int & u, int & v) {
        if (axis == 0)      { u = 2 * c.y; v = 2 * c.z; }
        else if (axis == 1) { u = 2 * c.z; v = 2 * c.x; }
        else                { u = 2 * c.x; v = 2 * c.y; }
      };
      int umin = INT_MAX, umax = INT_MIN, vmin = INT_MAX, vmax = INT_MIN;
      for (const cell_t & c : moving) {
        int u, v;
        uv(c, u, v);
        umin = std::min(umin, u); umax = std::max(umax, u);
        vmin = std::min(vmin, v); vmax = std::max(vmax, v);
      }
      std::set<key_t> occupied = {}, start;
      for (const cell_t & c : stat) occupied.insert({ c.x, c.y, c.z });
      for (int pu = umin - 1; pu <= umax + 1; pu++)
        for (int pv = vmin - 1; pv <= vmax + 1; pv++) {
          if (((pu ^ pv) & 1) || umax - pu > reach || pu - umin > reach || vmax - pv > reach || pv - vmin > reach)
            continue;
          int hx, hy, hz;
          if (axis == 0)      { hx = 0; hy = pu; hz = pv; }
          else if (axis == 1) { hx = pv; hy = 0; hz = pu; }
          else                { hx = pu; hy = pv; hz = 0; }
          for (unsigned int sense = 0; sense < 2; sense++) {
            bool blocked = false, unchanged = true;
            for (size_t k = 0; k < moving.size(); k++) {
              cell_t e = moving[k];
              turn(e.x, e.y, e.z, hx, hy, hz, axis, sense);
              blocked = blocked || occupied.count({ e.x, e.y, e.z });
              bool own = false;
              for (const cell_t & c : world[owner[k]])
                own = own || (c.x == e.x && c.y == e.y && c.z == e.z);
              unchanged = unchanged && own;
            }
            if (blocked || unchanged || tables.firstClear(moving, stat, axis, hx, hy, hz, sense) < 0)
              continue;
            /* the cells turned about the pivot */
            std::vector<std::vector<cell_t>> after = world;
            const bool withFirst = group & 1u;
            bool symmetric = false;
            for (unsigned int i = 0; i < pn; i++)
              if (bool(group & (1u << i)) != withFirst) {
                for (cell_t & e : after[i])
                  turn(e.x, e.y, e.z, hx, hy, hz, axis, sense ^ (withFirst ? 1 : 0));
                /* the search stores this piece in another orientation */
                const unsigned char t = sym->transAdd((unsigned char)node->getTrans(i),
                                                      quarterTurnTrans(axis, sense ^ (withFirst ? 1 : 0)));
                symmetric |= context.canonical(i, t).trans != t;
              }
            *canonicalized += out.insert(configOf(after)).second && symmetric;
          }
        }
    }
  return out;
}

/* A 3x1x1 bar of a cube and a domino, problem 0: both pieces are
 * symmetric, and the domino can turn about its outer end. */
std::unique_ptr<puzzle_c> cubeAndDominoBar(void) {
  auto puzzle = std::make_unique<puzzle_c>(std::make_unique<gridType_c>(gridType_c::GT_BRICKS));
  const unsigned int result = puzzle->addShape(3, 1, 1);
  const unsigned int cube = puzzle->addShape(1, 1, 1);
  const unsigned int domino = puzzle->addShape(2, 1, 1);
  for (unsigned int x = 0; x < 3; x++)
    puzzle->getShape(result)->set(x, 0, 0, voxel_c::VX_FILLED);
  puzzle->getShape(cube)->set(0, 0, 0, voxel_c::VX_FILLED);
  puzzle->getShape(domino)->set(0, 0, 0, voxel_c::VX_FILLED);
  puzzle->getShape(domino)->set(1, 0, 0, voxel_c::VX_FILLED);
  problem_c & problem = *puzzle->getProblem(puzzle->addProblem());
  problem.setResultId(result);
  problem.setShapeMaximum(cube, 1);
  problem.setShapeMaximum(domino, 1);
  return puzzle;
}

/* Returns how many of the exhaustive moves turn a symmetric piece into an
 * orientation the search replaces by its canonical one. */
unsigned int checkAgainstBruteForce(const problem_c & problem, const assembly_c * assembly, unsigned int maxNodes,
                                    bool expectRotations) {

  auto context = rotationContextFor(problem, rotationSettings_t{});
  REQUIRE(context != nullptr);
  movementAnalysator_c analysator(problem, bttest::settings(), context);

  std::vector<unsigned int> pieces;
  for (unsigned int i = 0; i < assembly->placementCount(); i++)
    pieces.push_back(i);

  /* breadth-first over a few search states, rotations and translations */
  std::vector<disassemblerNode_c *> nodes = { new disassemblerNode_c(assembly) };
  unsigned int rotationsSeen = 0, canonicalized = 0;
  for (size_t at = 0; at < nodes.size() && at < maxNodes; at++) {
    std::vector<disassemblerNode_c *> found;
    analysator.completeFind(nodes[at], pieces, &found);

    std::set<config_t> all, generated;
    for (disassemblerNode_c * n : found) {
      all.insert(configOf(problem, n));
      if (n->isRotation())
        generated.insert(configOf(problem, n));
    }
    std::set<config_t> brute = bruteRotations(problem, *context, nodes[at], &canonicalized);
    rotationsSeen += brute.size();

    INFO("search state " << at);
    for (const config_t & c : generated)
      CHECK(brute.count(c) == 1);
    for (const config_t & c : brute)
      CHECK(all.count(c) == 1);

    for (disassemblerNode_c * n : found)
      if (!n->is_separation() && nodes.size() < maxNodes)
        nodes.push_back(n);
      else if (n->decRefCount())
        delete n;
  }
  for (disassemblerNode_c * n : nodes)
    if (n->decRefCount())
      delete n;

  CHECK((rotationsSeen > 0) == expectRotations);
  return canonicalized;
}

}

TEST_CASE("the closure-based rotation search finds exactly the moves of an exhaustive one", "[rotations][disasm]") {
  checkAgainstBruteForce(*aaa().puzzle->getProblem(0), aaa().assemblies[0].get(), 15, true);
}

/* the bar's pieces are symmetric: most turns end in an orientation that the
 * search replaces by its canonical one, with its own hotspot */
TEST_CASE("the closure-based rotation search agrees on symmetric pieces", "[rotations][disasm]") {
  auto puzzle = cubeAndDominoBar();
  problem_c & problem = *puzzle->getProblem(0);
  auto assemblies = assembleAll(problem);
  REQUIRE_FALSE(assemblies.empty());
  for (const auto & a : assemblies)
    CHECK(checkAgainstBruteForce(problem, a.get(), 15, true) > 0);
}

/* a tight burr: no piece can turn in the first states, and the closures must
 * agree with the exhaustive search on that; slow because of larger tables */
TEST_CASE("the closure-based rotation search agrees on a tight burr", "[rotations][disasm][stress]") {
  auto puzzle = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  problem_c & problem = *puzzle->getProblem(0);
  auto assemblies = assembleAll(problem);
  REQUIRE_FALSE(assemblies.empty());
  checkAgainstBruteForce(problem, assemblies[0].get(), 3, false);
}

TEST_CASE("disassemblies without rotation moves record no orientations", "[rotations][disasm]") {

  /* the default solver output, and its saved files, do not change */
  auto puzzle = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  problem_c & problem = *puzzle->getProblem(0);

  for (const auto & a : assembleAll(problem)) {
    std::unique_ptr<separation_c> da = disassembler_0_c(problem, bttest::settings()).disassemble(a.get());
    if (!da)
      continue;
    CHECK(da->sumRotations() == 0);
    CHECK_FALSE(da->getState(0)->hasTrans());

    std::ostringstream out;
    {
      xmlWriter_c xml(out);
      da->save(xml);
    }
    CHECK(out.str().find("<dt>") == std::string::npos);
    CHECK(out.str().find("<rotation") == std::string::npos);
  }
}

TEST_CASE("a disassembly that uses no rotation records no orientations, rotations allowed or not", "[rotations][disasm]") {

  /* a 3x1x1 bar of a cube and a domino: the first move takes it apart, so
   * no turn is part of the shortest disassembly */
  auto puzzle = cubeAndDominoBar();
  problem_c & problem = *puzzle->getProblem(0);

  auto assemblies = assembleAll(problem);
  REQUIRE_FALSE(assemblies.empty());
  auto context = rotationContextFor(problem, { { 0.081, 0 }, { 0.2 } });
  REQUIRE(context != nullptr);

  for (bool rotations : { false, true }) {
    INFO("rotation moves " << rotations);
    std::unique_ptr<separation_c> da =
      disassembler_0_c(problem, bttest::settings(), rotations ? context : nullptr).disassemble(assemblies[0].get());
    REQUIRE(da != nullptr);
    CHECK(da->sumRotations() == 0);
    CHECK_FALSE(da->getState(0)->hasTrans());

    std::ostringstream out;
    {
      xmlWriter_c xml(out);
      da->save(xml);
    }
    CHECK(out.str().find("<dt>") == std::string::npos);
  }
}

TEST_CASE("dropping a rotation context does not wait for its tables", "[rotations][disasm]") {

  /* a stop right after a start drops the context while its tables are
   * still being built; that must not block (the GUI thread does it) */
  const problem_c & problem = *aaa().puzzle->getProblem(0);
  rotationSettings_t measured = rotationSettings_t{}, dropped = measured;
  measured.model.play = 0.0817;   // tables no other test builds
  dropped.model.play = 0.0819;

  /* how long such tables take to build */
  auto t0 = std::chrono::steady_clock::now();
  REQUIRE(rotationTablesFor(problem, measured) != nullptr);
  const auto build = std::chrono::steady_clock::now() - t0;

  /* only the drop is timed: making the context (starting its thread,
   * the canonical orientations) can take longer than a fast build */
  auto context = rotationContextFor(problem, dropped, true);
  t0 = std::chrono::steady_clock::now();
  context.reset();
  const auto drop = std::chrono::steady_clock::now() - t0;
  CHECK(2 * drop < build);
}

TEST_CASE("rotation moves between identical pieces in different orientations", "[rotations][disasm]") {

  /* AlPackino has several copies of a piece: pairing two of them in
   * different orientations once read the cubes of one from a cache entry
   * that looking up the other had moved */
  auto puzzle = puzzle_c::load("examples/AlPackino.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  problem_c & problem = *puzzle->getProblem(0);
  auto assemblies = assembleAll(problem);
  REQUIRE(assemblies.size() == 1);

  const rotationSettings_t settings = rotationSettings_t{};
  std::unique_ptr<separation_c> da =
    disassembler_0_c(problem, bttest::settings(), rotationContextFor(problem, settings)).disassemble(assemblies[0].get());
  REQUIRE(da != nullptr);
  if (da->sumRotations() > 0) {
    replay_c replay(problem, settings.model);
    replay.check(*da);
  }
}

/* For the ThreadSanitizer job: pool workers share one rotation context,
 * its pair cache and its tables, which are still being built when they
 * start. AlPackino has identical pieces in different orientations, so the
 * cache holds pairs of symmetric pieces and the search canonicalizes. */
TEST_CASE("pool workers sharing a rotation context disassemble like one", "[rotations][disasm][parallel]") {
  auto puzzle = puzzle_c::load("examples/AlPackino.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  problem_c & problem = *puzzle->getProblem(0);
  auto assemblies = assembleAll(problem);
  REQUIRE(assemblies.size() == 1);

  auto text = [](const separation_c * s) {
    std::ostringstream out;
    if (s) {
      xmlWriter_c xml(out);
      s->save(xml);
    }
    return out.str();
  };
  const std::string serial = text(disassembler_0_c(problem, bttest::settings(), rotationContextFor(problem, rotationSettings_t{}))
                                    .disassemble(assemblies[0].get()).get());
  REQUIRE_FALSE(serial.empty());

  const unsigned int copies = 8;
  std::vector<std::string> results;
  std::mutex resultsMutex;
  {
    disassemblerPool_c pool(problem, bttest::withThreads(4),
      [&](uint64_t, std::unique_ptr<assembly_c>, std::unique_ptr<separation_c> s) {
        std::lock_guard<std::mutex> lock(resultsMutex);
        results.push_back(text(s.get()));
      },
      rotationContextFor(problem, rotationSettings_t{}, true));
    for (unsigned int i = 0; i < copies; i++)
      pool.submit(std::make_unique<assembly_c>(assemblies[0].get()));
    pool.finish();
  }

  REQUIRE(results.size() == copies);
  for (const std::string & r : results)
    CHECK(r == serial);
}

/* the cells piece p occupies in an assembly */
static std::set<std::tuple<int, int, int>> placedCells(const problem_c & problem, unsigned int p, const assembly_c & a) {
  std::unique_ptr<voxel_c> v(problem.getPuzzle().getGridType()->getVoxel(*problem.getPartShape(problem.getPartIdToPieceId(p))));
  bt_assert2(v->transform(a.getTransformation(p)));
  std::set<std::tuple<int, int, int>> cells;
  for (unsigned int z = 0; z < v->getZ(); z++)
    for (unsigned int y = 0; y < v->getY(); y++)
      for (unsigned int x = 0; x < v->getX(); x++)
        if (v->isFilled(x, y, z))
          cells.insert({ a.getX(p) + (int)x - v->getHx(), a.getY(p) + (int)y - v->getHy(), a.getZ(p) + (int)z - v->getHz() });
  return cells;
}

TEST_CASE("pieces placed in a symmetric orientation come back in it when no rotation is used", "[rotations][disasm]") {

  /* the bar of the test above, with every piece placed in another
   * orientation that has the same cells: the search holds them in their
   * canonical orientations, the result must not show it */
  auto puzzle = cubeAndDominoBar();
  problem_c & problem = *puzzle->getProblem(0);

  auto assemblies = assembleAll(problem);
  REQUIRE_FALSE(assemblies.empty());
  auto context = rotationContextFor(problem, { { 0.081, 0 }, { 0.2 } });
  REQUIRE(context != nullptr);
  const unsigned int orientations = puzzle->getGridType()->getSymmetries()->getNumTransformations();

  for (const auto & a : assemblies) {

    /* the last other orientation of each piece with the same cells */
    assembly_c turned(puzzle->getGridType());
    bool reoriented = false;
    for (unsigned int p = 0; p < a->placementCount(); p++) {
      const rotationContext_c::canonical_t ca = context->canonical(p, a->getTransformation(p));
      unsigned int t = a->getTransformation(p);
      rotationContext_c::canonical_t ct = ca;
      for (unsigned int u = 0; u < orientations; u++) {
        const rotationContext_c::canonical_t cu = context->canonical(p, u);
        if (u != a->getTransformation(p) && cu.trans == ca.trans) {
          t = u;
          ct = cu;
        }
      }
      reoriented |= t != a->getTransformation(p);
      /* same cells: the hotspot moves by the difference of the offsets */
      turned.addPlacement(t, a->getX(p) + ca.dx - ct.dx, a->getY(p) + ca.dy - ct.dy, a->getZ(p) + ca.dz - ct.dz);
    }
    REQUIRE(reoriented);
    for (unsigned int p = 0; p < a->placementCount(); p++)
      REQUIRE(placedCells(problem, p, turned) == placedCells(problem, p, *a));

    std::string saved[2];
    for (bool rotations : { false, true }) {
      INFO("rotation moves " << rotations);
      std::unique_ptr<separation_c> da =
        disassembler_0_c(problem, bttest::settings(), rotations ? context : nullptr).disassemble(&turned);
      REQUIRE(da != nullptr);
      CHECK(da->sumRotations() == 0);
      CHECK_FALSE(da->getState(0)->hasTrans());

      std::ostringstream out;
      {
        xmlWriter_c xml(out);
        da->save(xml);
      }
      saved[rotations] = out.str();
    }
    CHECK(saved[1] == saved[0]);
  }
}

TEST_CASE("rotation moves do not depend on which piece comes first", "[rotations][disasm][stress]") {

  /* Piece 0 stays in place in every search state, so a group with it turns
   * as the rest turning the other way. That rest is generally not in the
   * path family (an inverted wiggle or slide turns its shift along), and
   * its footprint decides pivots and reach: searched only as the rest,
   * PelikanBurr came apart in 5, 6 or 7 turns depending on the piece order.
   *
   * The disassembly itself may still depend on the order: when several
   * move sequences are equally short, the search keeps the first it finds.
   * What must not depend on it is which turns a search state offers. So for
   * every piece order, walk the states reachable by translations (they keep
   * piece 0 as the frame), written in the original piece order relative to
   * the original first piece, and count their distinct rotation moves; a
   * state reached under several orders must offer as many under each. */
  const char * file = "examples/AAA_most_rotations.xmpuzzle";
  const unsigned int parts = puzzle_c::load(file)->getProblem(0)->getNumberOfParts();
  std::vector<std::map<config_t, size_t>> turnsAt(parts);

  for (unsigned int first = 0; first < parts; first++) {
    auto puzzle = puzzle_c::load(file);
    REQUIRE(puzzle != nullptr);
    problem_c & problem = *puzzle->getProblem(0);
    /* exchangeParts swaps neighbours: bubble the part to the front */
    for (unsigned int j = first; j > 0; j--)
      problem.exchangeParts(j - 1, j);
    auto context = rotationContextFor(problem, rotationSettings_t{});
    REQUIRE(context != nullptr);

    /* it comes apart only with turns, so states reached by translations
     * offer some */
    auto assemblies = assembleAll(problem);
    REQUIRE(assemblies.size() == 1);
    const std::unique_ptr<assembly_c> & assembly = assemblies[0];
    CHECK(disassembler_0_c(problem, bttest::settings(), context).disassemble(assembly.get()) != nullptr);

    /* piece p of this order is original piece orig(p) */
    auto orig = [&](unsigned int p) { return p == 0 ? first : (p <= first ? p - 1 : p); };
    auto normalized = [&](const disassemblerNode_c * n) {
      const std::vector<std::vector<cell_t>> w = worldCells(problem, n);
      std::vector<std::vector<cell_t>> o(w.size());
      for (unsigned int i = 0; i < w.size(); i++)
        o[orig(i)] = w[i];
      return configOf(o);
    };

    movementAnalysator_c analysator(problem, bttest::settings(), context);
    std::vector<unsigned int> pieces;
    for (unsigned int i = 0; i < assembly->placementCount(); i++)
      pieces.push_back(i);

    std::vector<disassemblerNode_c *> nodes = { new disassemblerNode_c(assembly.get()) };
    std::set<config_t> seen = { normalized(nodes[0]) };
    for (size_t at = 0; at < nodes.size(); at++) {
      std::vector<disassemblerNode_c *> found;
      analysator.completeFind(nodes[at], pieces, &found);
      std::set<config_t> turns;
      for (disassemblerNode_c * n : found)
        if (n->isRotation())
          turns.insert(normalized(n));
      turnsAt[first][normalized(nodes[at])] = turns.size();
      for (disassemblerNode_c * n : found)
        if (!n->isRotation() && !n->is_separation() && nodes.size() < 400 && seen.insert(normalized(n)).second)
          nodes.push_back(n);
        else if (n->decRefCount())
          delete n;
    }
    for (disassemblerNode_c * n : nodes)
      if (n->decRefCount())
        delete n;
  }

  size_t compared = 0, withTurns = 0;
  for (unsigned int first = 1; first < parts; first++)
    for (const auto & [state, turns] : turnsAt[first]) {
      auto it = turnsAt[0].find(state);
      if (it == turnsAt[0].end())
        continue;
      INFO("piece order " << first);
      CHECK(turns == it->second);
      compared++;
      withTurns += (turns > 0);
    }
  /* the orders must share enough states, and some of them must offer turns */
  CHECK(compared >= 2 * (parts - 1));
  CHECK(withTurns > 0);
}

TEST_CASE("optimizeTurns never measures more play than the turns were found at", "[rotations][disasm]") {

  /* at the default model's small play the planner's conservative bound
   * misses paths the family's tables certified; the recorded path proves
   * the turn there, so the measurement stays at or below that play */
  auto puzzle = puzzle_c::load("examples/AAA_most_rotations.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  problem_c & problem = *puzzle->getProblem(0);
  auto assemblies = assembleAll(problem);
  REQUIRE(assemblies.size() == 1);
  const rotationSettings_t settings = rotationSettings_t{};
  auto da = disassembler_0_c(problem, bttest::settings(), rotationContextFor(problem, settings))
              .disassemble(assemblies[0].get());
  REQUIRE(da != nullptr);
  REQUIRE(da->sumRotations() > 0);

  const turnPlay_t play = optimizeTurns(da.get(), assemblies[0].get(), problem, settings);
  CHECK_FALSE(play.failed);
  CHECK(play.upper <= settings.model.play);
  CHECK(play.lower <= play.upper);
}

TEST_CASE("optimizeTurns reports its progress, stops when asked, and can only re-plan",
          "[rotations][disasm]") {

  const problem_c & problem = *aaa().puzzle->getProblem(0);
  const rotationSettings_t settings = withPlanner();
  const separation_c * found = aaa().withRotations.get();
  REQUIRE(found != nullptr);
  const unsigned int turns = found->sumRotations();
  REQUIRE(turns > 0);
  const assembly_c * assembly = aaa().assemblies[0].get();

  /* every turn counted, and bounds measured */
  double measured = 0;
  std::vector<rotationGeometry::path_t> provenPaths;
  {
    separation_c copy(found);
    std::atomic<unsigned int> done{ 0 };
    optimizeControl_t control;
    control.done = &done;
    const turnPlay_t play = optimizeTurns(&copy, assembly, problem, settings, control);
    CHECK(done == turns);
    CHECK_FALSE(play.stopped);
    CHECK(play.rotations);
    CHECK(play.upper > 0);
    CHECK(play.lower <= play.upper);
    measured = play.upper;
    provenPaths = play.proven;
    CHECK(provenPaths.size() == turns);
  }

  /* re-planned for exactly the measured play, which is not the problem's:
   * every turn gets a path that replays clear at that play, where the
   * grid misses one from the paths the measuring proved */
  {
    separation_c copy(found);
    optimizeControl_t control;
    control.measure = false;
    control.model = rotationGeometry::model_t{ measured, rotationModelOf(problem).chamfer };
    control.proven = &provenPaths;
    const turnPlay_t play = optimizeTurns(&copy, assembly, problem, settings, control);
    CHECK_FALSE(play.failed);
    replay_c replay(problem, *control.model);
    replay.check(copy);
    CHECK(replay.rotations == turns);
  }

  /* stopped before the first turn: nothing done, nothing valid */
  {
    separation_c copy(found);
    std::atomic<bool> stop{ true };
    std::atomic<unsigned int> done{ 0 };
    optimizeControl_t control;
    control.stop = &stop;
    control.done = &done;
    const turnPlay_t play = optimizeTurns(&copy, assembly, problem, settings, control);
    CHECK(play.stopped);
    CHECK(done == 0);
  }

  /* re-planning only: every turn gets a path, no play is measured */
  {
    separation_c copy(found);
    optimizeControl_t control;
    control.measure = false;
    const turnPlay_t play = optimizeTurns(&copy, assembly, problem, settings, control);
    CHECK(play.upper == 0);
    CHECK_FALSE(play.failed);
    CHECK(copy.sumRotations() == turns);
    CHECK(copy.movesText() == found->movesText());
  }

  /* re-planning with a planner that finds no path: at the play the turns
   * were found at, the recorded paths are certified and stay; at a much
   * smaller one they are not, nothing stands in, and that is reported */
  {
    rotationSettings_t pure = settings;
    pure.plan.maxShift = 0;
    pure.plan.steps = 2;
    optimizeControl_t control;
    control.measure = false;

    separation_c same(found);
    CHECK_FALSE(optimizeTurns(&same, assembly, problem, pure, control).failed);

    separation_c copy(found);
    control.model = rotationGeometry::model_t{ 0.005, settings.model.chamfer };
    const turnPlay_t play = optimizeTurns(&copy, assembly, problem, pure, control);
    CHECK(play.failed);
    CHECK(copy.movesText() == found->movesText());
  }
}
