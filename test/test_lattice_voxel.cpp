/* Tests of the described grids (BurrTools NG's grids as data, step G3 of
 * design/2026-11-19-ng-merge-plan.md): voxel_lattice_c, symmetries_lattice_c
 * and the conversion of native puzzles to them.
 *
 * Random shapes are made of voxels of each grid; the checks are the
 * properties the assemblers rely on: transformations compose as transAdd
 * says, keep voxels voxels, and the symmetry tables agree with what
 * transforming the shape actually gives.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "lib/assembler.h"
#include "lib/assembly.h"
#include "lib/converter.h"
#include "lib/disassembler_0.h"
#include "lib/disassembly.h"
#include "lib/movementcache.h"
#include "lib/gridtype.h"
#include "lib/latticegridinfo.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/symmetries.h"
#include "lib/voxel.h"
#include "tools/xml.h"
#include "test_helpers.h"

#include <bit>
#include <cmath>
#include <memory>
#include <random>
#include <sstream>
#include <vector>

namespace {

const gridType_c::gridType DESCRIBED[] = {
  gridType_c::GT_HEXAGONAL_PRISM, gridType_c::GT_CAIRO_PRISM, gridType_c::GT_RHOMBIC_TETRAHEDRA,
  gridType_c::GT_OCTAHEDRAL_TETRAHEDRA, gridType_c::GT_DESCRIBED_BRICKS, gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM,
};

/* a random shape: up to `count` voxels of the grid in a box of `size`,
 * minimised; a few seeds also give symmetric shapes by chance */
std::unique_ptr<voxel_c> randomShape(const gridType_c & gt, std::mt19937 & rng, int size, int count) {
  std::unique_ptr<voxel_c> v(gt.getVoxel(size, size, size, voxel_c::VX_EMPTY));
  std::uniform_int_distribution<int> d(0, size - 1);
  for (int i = 0, placed = 0; i < 2000 && placed < count; i++) {
    const int x = d(rng), y = d(rng), z = d(rng);
    if (v->validCoordinate(x, y, z) && v->isEmpty(x, y, z)) {
      v->setState(x, y, z, voxel_c::VX_FILLED);
      placed++;
    }
  }
  v->minimizePiece();
  v->initHotspot();
  return v;
}

std::unique_ptr<voxel_c> transformed(const voxel_c & v, unsigned int t) {
  std::unique_ptr<voxel_c> c(v.getGridType()->getVoxel(&v));
  REQUIRE(c->transform(t));
  return c;
}

bool allFilledValid(const voxel_c & v) {
  for (unsigned int z = 0; z < v.getZ(); z++)
    for (unsigned int y = 0; y < v.getY(); y++)
      for (unsigned int x = 0; x < v.getX(); x++)
        if (!v.isEmpty(x, y, z) && !v.validCoordinate(x, y, z)) return false;
  return true;
}

const char * name(gridType_c::gridType t) {
  switch (t) {
    case gridType_c::GT_HEXAGONAL_PRISM: return "hexagonal prisms";
    case gridType_c::GT_CAIRO_PRISM: return "Cairo prisms";
    case gridType_c::GT_RHOMBIC_TETRAHEDRA: return "rhombic tetrahedra";
    case gridType_c::GT_OCTAHEDRAL_TETRAHEDRA: return "octahedral tetrahedra";
    case gridType_c::GT_DESCRIBED_BRICKS: return "described bricks";
    case gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM: return "described triangular prisms";
    default: return "?";
  }
}

} // namespace

TEST_CASE("described grids: the transformation counts are those of the grids", "[lattice]") {
  struct E { gridType_c::gridType t; unsigned int rot; };
  for (auto e : { E{gridType_c::GT_HEXAGONAL_PRISM, 12}, E{gridType_c::GT_CAIRO_PRISM, 8},
                  E{gridType_c::GT_RHOMBIC_TETRAHEDRA, 24}, E{gridType_c::GT_OCTAHEDRAL_TETRAHEDRA, 24},
                  E{gridType_c::GT_DESCRIBED_BRICKS, 24}, E{gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM, 12} }) {
    INFO(name(e.t));
    gridType_c gt(e.t);
    CHECK(gt.isDescribed());
    CHECK(gt.getSymmetries()->getNumTransformations() == e.rot);
    CHECK(gt.getSymmetries()->getNumTransformationsMirror() == 2 * e.rot);
    CHECK((gt.getCapabilities() & gridType_c::CAP_ASSEMBLE) != 0);
  }
}

TEST_CASE("described grids: transformations keep voxels voxels and compose as transAdd says", "[lattice]") {
  for (gridType_c::gridType type : DESCRIBED) {
    INFO(name(type));
    gridType_c gt(type);
    const symmetries_c * sym = gt.getSymmetries();
    const unsigned int n = sym->getNumTransformationsMirror();
    std::mt19937 rng(7);

    for (int s = 0; s < 3; s++) {
      auto v = randomShape(gt, rng, 9, 6);
      REQUIRE(v->countState(voxel_c::VX_FILLED) > 0);

      for (unsigned int t1 = 0; t1 < n; t1++) {
        auto a = transformed(*v, t1);
        INFO("t1 " << t1);
        CHECK(allFilledValid(*a));
        CHECK(a->validCoordinate(a->getHx(), a->getHy(), a->getHz()));
        CHECK(a->countState(voxel_c::VX_FILLED) == v->countState(voxel_c::VX_FILLED));

        for (unsigned int t2 = 0; t2 < n; t2 += 3) {
          const unsigned char p = sym->transAdd(t1, t2);
          INFO("t2 " << t2);
          REQUIRE(p != TND);  // the grid's transformations are a group
          auto b = transformed(*a, t2);
          auto c = transformed(*v, p);
          CHECK(b->identicalInBB(c.get()));
        }
      }

      // the identity, and a shape moved by a lattice vector is the same shape
      CHECK(transformed(*v, 0)->identicalInBB(v.get()));
    }
  }
}

TEST_CASE("described grids: the symmetry tables agree with transforming the shape", "[lattice]") {
  for (gridType_c::gridType type : DESCRIBED) {
    INFO(name(type));
    gridType_c gt(type);
    const symmetries_c * sym = gt.getSymmetries();
    const unsigned int n = sym->getNumTransformationsMirror();
    std::mt19937 rng(11);

    std::vector<std::unique_ptr<voxel_c>> shapes;
    // a single voxel is as symmetric as its kind gets
    shapes.push_back(randomShape(gt, rng, 6, 1));
    for (int s = 0; s < 4; s++) shapes.push_back(randomShape(gt, rng, 8, 2 + s));

    for (const auto & v : shapes) {
      const symmetries_t g = sym->calculateSymmetry(v.get());
      REQUIRE(!isSymmetryInvalid(g));
      CHECK(sym->symmetryKnown(v.get()));

      unsigned int orientations = 0, unique = 0;
      std::vector<std::unique_ptr<voxel_c>> seen;
      for (unsigned int t = 0; t < n; t++) {
        auto a = transformed(*v, t);
        INFO("t " << t);
        CHECK(sym->symmetrieContainsTransformation(g, t) == a->identicalInBB(v.get()));

        const unsigned char m = sym->minimizeTransformation(g, t);
        CHECK(m <= t);
        CHECK(transformed(*v, m)->identicalInBB(a.get()));

        bool isNew = true;
        for (const auto & o : seen)
          if (o->identicalInBB(a.get())) isNew = false;
        if (isNew) {
          orientations++;
          seen.push_back(std::move(a));
        }
        if (sym->isTransformationUnique(g, t)) unique++;
      }
      CHECK(unique == orientations);
      CHECK(sym->countSymmetryIntersection(g, g) >= 1);
    }
  }
}

TEST_CASE("described bricks: random shapes have as many orientations as on the native cube grid", "[lattice]") {
  gridType_c native(gridType_c::GT_BRICKS), described(gridType_c::GT_DESCRIBED_BRICKS);
  const symmetries_c * ns = native.getSymmetries();
  const symmetries_c * ds = described.getSymmetries();
  std::mt19937 rng(3);

  for (int s = 0; s < 20; s++) {
    auto v = randomShape(native, rng, 4, 1 + s % 7);
    std::unique_ptr<voxel_c> w(described.getVoxel(v->getX(), v->getY(), v->getZ(), voxel_c::VX_EMPTY));
    for (unsigned int z = 0; z < v->getZ(); z++)
      for (unsigned int y = 0; y < v->getY(); y++)
        for (unsigned int x = 0; x < v->getX(); x++)
          w->set(x, y, z, v->get(x, y, z));

    const symmetries_t gn = ns->calculateSymmetry(v.get());
    const symmetries_t gd = ds->calculateSymmetry(w.get());
    unsigned int cn = 0, cd = 0, un = 0, ud = 0;
    for (unsigned int t = 0; t < 48; t++) {
      cn += ns->symmetrieContainsTransformation(gn, t);
      cd += ds->symmetrieContainsTransformation(gd, t);
      un += ns->isTransformationUnique(gn, t);
      ud += ds->isTransformationUnique(gd, t);
    }
    INFO("shape " << s);
    CHECK(cn == cd);
    CHECK(un == ud);
    CHECK(ns->symmetryContainsMirror(gn) == ds->symmetryContainsMirror(gd));
  }
}

TEST_CASE("described grids: converted puzzles have the assemblies of the native ones", "[lattice][solver]") {
  struct P { const char * file; gridType_c::gridType to; };
  const P puzzles[] = {
    {"examples/CubeInCage.xmpuzzle", gridType_c::GT_DESCRIBED_BRICKS},
    {"examples/PelikanBurr.xmpuzzle", gridType_c::GT_DESCRIBED_BRICKS},
    {"examples/DemoMirrorParadox.xmpuzzle", gridType_c::GT_DESCRIBED_BRICKS},
    {"examples/HexSticks.xmpuzzle", gridType_c::GT_RHOMBIC_TETRAHEDRA},
    {"examples/AugmentedSecondStellation.xmpuzzle", gridType_c::GT_RHOMBIC_TETRAHEDRA},
    {"examples/Prisgon.xmpuzzle", gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM},
    {"examples/FourPieceTetrahedron.xmpuzzle", gridType_c::GT_OCTAHEDRAL_TETRAHEDRA},
  };

  auto count = [](problem_c & problem) {
    std::unique_ptr<assembler_c> assm = problem.getPuzzle().getGridType()->findAssembler(problem, bttest::settings());
    REQUIRE(assm);
    REQUIRE(assm->createMatrix(false, false) == assembler_c::ERR_NONE);
    unsigned long n = 0;
    assm->assemble([&n](std::unique_ptr<assembly_c>) { n++; return true; });
    return n;
  };

  for (const auto & pz : puzzles) {
    INFO(pz.file);
    auto p = puzzle_c::load(pz.file);
    REQUIRE(p);
    std::unique_ptr<puzzle_c> c(convertToDescribed(p.get()));
    REQUIRE(c);
    CHECK(c->getGridType()->getType() == pz.to);
    REQUIRE(c->getNumberOfProblems() == p->getNumberOfProblems());
    CHECK(count(*c->getProblem(0)) == count(*p->getProblem(0)));
  }
}

TEST_CASE("described grids: spheres and described grids have no described counterpart", "[lattice]") {
  CHECK(describedCounterpart(gridType_c::GT_SPHERES) == gridType_c::GT_NUM_GRIDS);
  for (gridType_c::gridType t : DESCRIBED)
    CHECK(describedCounterpart(t) == gridType_c::GT_NUM_GRIDS);
}

TEST_CASE("described grids: a puzzle saves and loads with its grid", "[lattice][xml]") {
  auto p = puzzle_c::load("examples/HexSticks.xmpuzzle");
  REQUIRE(p);
  std::unique_ptr<puzzle_c> c(convertToDescribed(p.get()));
  REQUIRE(c);

  std::ostringstream out;
  {
    xmlWriter_c xml(out);
    c->save(xml);
  }
  std::istringstream in(out.str());
  xmlParser_c pars(in);
  puzzle_c back(pars);
  CHECK(back.getGridType()->getType() == gridType_c::GT_RHOMBIC_TETRAHEDRA);
  REQUIRE(back.getNumberOfShapes() == c->getNumberOfShapes());
  for (unsigned int s = 0; s < c->getNumberOfShapes(); s++)
    CHECK(back.getShape(s)->identicalInBB(c->getShape(s)));
}

namespace {

/* assemblies and the ones that come apart, as burrTxt -d counts them */
std::pair<unsigned long, unsigned long> solve(problem_c & problem) {
  std::unique_ptr<assembler_c> assm = problem.getPuzzle().getGridType()->findAssembler(problem, bttest::settings());
  REQUIRE(assm);
  REQUIRE(assm->createMatrix(false, false) == assembler_c::ERR_NONE);
  std::vector<std::unique_ptr<assembly_c>> found;
  assm->assemble([&found](std::unique_ptr<assembly_c> a) { found.push_back(std::move(a)); return true; });
  disassembler_0_c d(problem, bttest::settings());
  unsigned long solutions = 0;
  for (const auto & a : found)
    if (d.disassemble(a.get())) solutions++;
  return { found.size(), solutions };
}

} // namespace

TEST_CASE("described grids: the grids with movement directions disassemble", "[lattice][disasm]") {
  for (gridType_c::gridType type : DESCRIBED) {
    INFO(name(type));
    gridType_c gt(type);
    const bool moves = type != gridType_c::GT_HEXAGONAL_PRISM && type != gridType_c::GT_CAIRO_PRISM;
    CHECK(((gt.getCapabilities() & gridType_c::CAP_DISASSEMBLE) != 0) == moves);
    CHECK(latticeGridInfo_c::get(type).canDisassemble() == moves);
  }
}

TEST_CASE("described grids: converted puzzles have the native solutions, and NG's on the tetrahedron grids",
          "[lattice][disasm][solver]") {
  struct P { const char * file; long assemblies, solutions; };
  /* bricks and prisms: compared with the native grid in the test; rhombic
   * and tetra-octa have no disassembler natively, the counts there are
   * BurrTools NG's (bench/ng-crosscheck, fdae7951) */
  const P puzzles[] = {
    {"examples/CubeInCage.xmpuzzle", -1, -1},
    {"examples/PelikanBurr.xmpuzzle", -1, -1},
    {"examples/DemoMirrorParadox.xmpuzzle", -1, -1},
    {"examples/Prisgon.xmpuzzle", -1, -1},
    {"examples/FourPieceTetrahedron.xmpuzzle", 1, 1},
    {"examples/HexSticks.xmpuzzle", 33, 2},
    {"examples/AugmentedSecondStellation.xmpuzzle", 2, 1},
  };

  for (const auto & pz : puzzles) {
    INFO(pz.file);
    auto p = puzzle_c::load(pz.file);
    REQUIRE(p);
    std::unique_ptr<puzzle_c> c(convertToDescribed(p.get()));
    REQUIRE(c);
    const auto got = solve(*c->getProblem(0));
    if (pz.assemblies < 0) {
      const auto native = solve(*p->getProblem(0));
      CHECK(got == native);
    } else {
      CHECK((long)got.first == pz.assemblies);
      CHECK((long)got.second == pz.solutions);
    }
  }
}

/* An independent check of the assembly counts on the grids nothing else
 * confirms (hexagonal and Cairo prisms; NG's importer does not read our
 * grid types): one layer of a grid tiled by two-cell pieces has as many
 * assemblies as the region's adjacency graph has perfect matchings, which a
 * short recursion counts. The region is grown from one voxel and chosen
 * without any symmetry, so no assembly is a turned or mirrored copy of
 * another; the pieces are every pair of neighbouring cells, one shape per
 * class of pairs that are the same up to rotation, each used 0 to n times.
 */
namespace {

unsigned long countMatchings(std::vector<std::vector<int>> & adj, std::vector<bool> & used) {
  int first = -1;
  for (size_t i = 0; i < used.size(); i++)
    if (!used[i]) { first = (int)i; break; }
  if (first < 0) return 1;
  used[first] = true;
  unsigned long n = 0;
  for (int j : adj[first])
    if (!used[j]) {
      used[j] = true;
      n += countMatchings(adj, used);
      used[j] = false;
    }
  used[first] = false;
  return n;
}

} // namespace

TEST_CASE("described grids: dominoes on one layer give the region's perfect matchings", "[lattice][solver]") {
  for (gridType_c::gridType type : { gridType_c::GT_HEXAGONAL_PRISM, gridType_c::GT_CAIRO_PRISM,
                                     gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM }) {
    INFO(name(type));
    auto puzzle = std::make_unique<puzzle_c>(std::make_unique<gridType_c>(type));
    const gridType_c & gt = *puzzle->getGridType();
    const symmetries_c * sym = gt.getSymmetries();

    const int S = 16;
    std::unique_ptr<voxel_c> space(gt.getVoxel(S, S, S, voxel_c::VX_EMPTY));

    /* a layer is a physical height (the prism axis is not one of NG's
     * voxel coordinates on every grid); the start voxels pick it */
    float layerZ = 0;
    auto height = [&](int x, int y, int z) {
      float a = (float)x, b = (float)y, c = (float)z;
      space->recalcSpaceCoordinates(&a, &b, &c);
      return c;
    };
    auto inLayer = [&](int x, int y, int z) {
      return x >= 0 && y >= 0 && z >= 0 && x < S && y < S && z < S && space->validCoordinate(x, y, z) &&
             std::fabs(height(x, y, z) - layerZ) < 1e-3f;
    };

    // grow regions of 10 cells from different start voxels until one has no symmetry
    std::vector<std::array<int, 3>> cells;
    for (int start = 0; start < 6 * 6 * 6 && cells.empty(); start++) {
      const int sx = 5 + start % 6, sy = 5 + (start / 6) % 6, sz = 5 + start / 36;
      if (!space->validCoordinate(sx, sy, sz)) continue;
      layerZ = height(sx, sy, sz);
      std::vector<std::array<int, 3>> region{{sx, sy, sz}};
      /* a lopsided growth: from the newest cell first, taking its
       * neighbours in a rotating order, so the blob is a crooked snake */
      for (int step = 0; step < 200 && region.size() < 10; step++) {
        bool grown = false;
        for (size_t back = 0; back < region.size() && !grown; back++) {
          const auto & from = region[region.size() - 1 - back];
          int nx, ny, nz;
          std::vector<std::array<int, 3>> nbs;
          for (unsigned int n = 0; space->getNeighbor(n, 0, from[0], from[1], from[2], &nx, &ny, &nz); n++)
            if (inLayer(nx, ny, nz)) nbs.push_back({nx, ny, nz});
          for (size_t q = 0; q < nbs.size() && !grown; q++) {
            const auto & c = nbs[(q + region.size() + start) % nbs.size()];
            bool seen = false;
            for (const auto & r : region) if (r == c) seen = true;
            if (!seen) { region.push_back(c); grown = true; }
          }
        }
        if (!grown) break;
      }
      if (region.size() != 10) continue;
      std::unique_ptr<voxel_c> r(gt.getVoxel(S, S, S, voxel_c::VX_EMPTY));
      for (const auto & c : region) r->setState(c[0], c[1], c[2], voxel_c::VX_FILLED);
      r->initHotspot();
      const symmetries_t g = sym->calculateSymmetry(r.get());
      /* a flat shape always has the mirror symmetry through its own
       * middle plane, which maps every flat piece onto itself and so never
       * makes two assemblies the same; anything beyond that one does */
      unsigned int symmetries = 0;
      for (unsigned int t = 0; t < sym->getNumTransformationsMirror(); t++)
        if (sym->symmetrieContainsTransformation(g, t)) symmetries++;
      if (symmetries == 2) cells = region;
    }
    REQUIRE(cells.size() == 10);

    // adjacency within the region
    std::vector<std::vector<int>> adj(cells.size());
    std::vector<std::pair<int, int>> pairs;
    for (size_t i = 0; i < cells.size(); i++) {
      int nx, ny, nz;
      for (unsigned int n = 0; space->getNeighbor(n, 0, cells[i][0], cells[i][1], cells[i][2], &nx, &ny, &nz); n++)
        for (size_t j = 0; j < cells.size(); j++)
          if (cells[j][0] == nx && cells[j][1] == ny && cells[j][2] == nz) {
            adj[i].push_back((int)j);
            if (i < j) pairs.push_back({(int)i, (int)j});
          }
    }
    std::vector<bool> used(cells.size(), false);
    const unsigned long matchings = countMatchings(adj, used);
    REQUIRE(matchings > 0);

    // the result, and one shape per class of neighbouring pairs
    std::unique_ptr<voxel_c> result(gt.getVoxel(S, S, S, voxel_c::VX_EMPTY));
    /* not minimised: the result sits away from the space's corner, as
     * results drawn in the GUIs do (transform(0) must leave it there) */
    for (const auto & c : cells) result->setState(c[0], c[1], c[2], voxel_c::VX_FILLED);
    result->initHotspot();
    const unsigned int resultId = puzzle->addShape(std::move(result));

    std::vector<std::unique_ptr<voxel_c>> classes;
    for (auto [i, j] : pairs) {
      std::unique_ptr<voxel_c> d(gt.getVoxel(S, S, S, voxel_c::VX_EMPTY));
      d->setState(cells[i][0], cells[i][1], cells[i][2], voxel_c::VX_FILLED);
      d->setState(cells[j][0], cells[j][1], cells[j][2], voxel_c::VX_FILLED);
      d->minimizePiece();
      d->initHotspot();
      bool known = false;
      for (const auto & c : classes)
        if (c->identicalWithRots(d.get(), false, false)) known = true;
      if (!known) classes.push_back(std::move(d));
    }
    problem_c * problem = puzzle->getProblem(puzzle->addProblem());
    problem->setResultId(resultId);
    for (auto & c : classes) {
      const unsigned int id = puzzle->addShape(std::move(c));
      problem->setShapeMinimum(id, 0);
      problem->setShapeMaximum(id, 5);
    }
    INFO(classes.size() << " kinds of pairs, " << matchings << " matchings");

    std::unique_ptr<assembler_c> assm = gt.findAssembler(*problem, bttest::settings());
    REQUIRE(assm);
    REQUIRE(assm->createMatrix(false, false) == assembler_c::ERR_NONE);
    unsigned long n = 0;
    assm->assemble([&n](std::unique_ptr<assembly_c>) { n++; return true; });
    CHECK(n == matchings);
  }
}
