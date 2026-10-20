#include <catch2/catch_test_macros.hpp>

#include "lib/assembler.h"
#include "lib/assembly.h"
#include "lib/disassembler_0.h"
#include "lib/disassembly.h"
#include "lib/gridtype.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/rotationgeometry.h"
#include "lib/rotationtables.h"
#include "lib/voxel.h"
#include "tools/xml.h"

#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <tuple>
#include <vector>

using namespace rotationGeometry;

namespace {

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
  std::unique_ptr<assembler_c> assm = problem.getPuzzle().getGridType()->findAssembler(problem);
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

        if (st->getTrans(pc) == prev->getTrans(pc)) {
          /* not turning: stays exactly where it is */
          CHECK(keys(after) == keys(before));
          stat.insert(stat.end(), before.begin(), before.end());
          continue;
        }

        /* turning: lands on its start cells turned about the pivot */
        turned++;
        std::vector<cell_t> expected;
        for (cell_t c : before) {
          int dx = 2 * c.x - hx, dy = 2 * c.y - hy, dz = 2 * c.z - hz;
          quarterTurn(&dx, &dy, &dz, r->axis, r->path.sense);
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
       * rather than through the tables the solver used */
      planarScene_c scene(moving, stat, r->axis, hx, hy, hz, model);
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
    r.withoutRotations = disassembler_0_c(problem).disassemble(r.assemblies[0].get());
    r.withRotations = disassembler_0_c(problem, rotationTablesFor(problem, defaultRotationSettings()))
                        .disassemble(r.assemblies[0].get());
    return r;
  }();
  return result;
}

}

TEST_CASE("AAA_most_rotations comes apart only with rotation moves", "[rotations][disasm]") {

  const problem_c & problem = *aaa().puzzle->getProblem(0);
  const rotationSettings_t settings = defaultRotationSettings();

  /* translations alone cannot take it apart */
  CHECK(aaa().withoutRotations == nullptr);

  const separation_c * da = aaa().withRotations.get();
  REQUIRE(da != nullptr);

  /* the shortest disassembly the breadth-first search finds; changes when
   * the default model or the path family changes */
  CHECK(da->movesText() == "2.4");
  CHECK(da->sumRotations() == 2);

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
}

TEST_CASE("disassemblies without rotation moves record no orientations", "[rotations][disasm]") {

  /* the default solver output, and its saved files, do not change */
  auto puzzle = puzzle_c::load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(puzzle != nullptr);
  problem_c & problem = *puzzle->getProblem(0);

  for (const auto & a : assembleAll(problem)) {
    std::unique_ptr<separation_c> da = disassembler_0_c(problem).disassemble(a.get());
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
