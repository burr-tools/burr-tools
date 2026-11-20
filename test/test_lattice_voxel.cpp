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

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <memory>
#include <random>
#include <set>
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
    std::unique_ptr<assembler_c> assm = problem.getPuzzle().getGridType()->findAssembler(makeSolverSpec(problem), bttest::settings());
    REQUIRE(assm);
    REQUIRE(assm->createMatrix(false, false) == assembler_c::ERR_NONE);
    unsigned long n = 0;
    assm->assemble([&n](std::unique_ptr<assembly_c>) { n++; return true; });
    return n;
  };

  for (const auto & pz : puzzles) {
    INFO(pz.file);
    auto p = puzzle_c::load(pz.file, cutCubeGrids_e::native);
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
  auto p = puzzle_c::load("examples/HexSticks.xmpuzzle", cutCubeGrids_e::native);
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
  std::unique_ptr<assembler_c> assm = problem.getPuzzle().getGridType()->findAssembler(makeSolverSpec(problem), bttest::settings());
  REQUIRE(assm);
  REQUIRE(assm->createMatrix(false, false) == assembler_c::ERR_NONE);
  std::vector<std::unique_ptr<assembly_c>> found;
  assm->assemble([&found](std::unique_ptr<assembly_c> a) { found.push_back(std::move(a)); return true; });
  disassembler_0_c d(makeSolverSpec(problem), bttest::settings());
  unsigned long solutions = 0;
  for (const auto & a : found)
    if (d.disassemble(a.get())) solutions++;
  return { found.size(), solutions };
}

} // namespace

TEST_CASE("described grids: every described grid disassembles", "[lattice][disasm]") {
  /* hexagonal and Cairo prisms since step C6, which gave them movement
   * vectors; the others have NG's */
  for (gridType_c::gridType type : DESCRIBED) {
    INFO(name(type));
    gridType_c gt(type);
    CHECK((gt.getCapabilities() & gridType_c::CAP_DISASSEMBLE) != 0);
    CHECK(latticeGridInfo_c::get(type).canDisassemble());
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
    auto p = puzzle_c::load(pz.file, cutCubeGrids_e::native);
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

/* a region of `size` cells in one layer that has no symmetry beyond the
 * mirror through its own plane, grown inside a space of S^3 */
std::vector<std::array<int, 3>> asymmetricLayerRegion(const gridType_c & gt, int S, size_t size) {
  const symmetries_c * sym = gt.getSymmetries();
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

  // grow regions from different start voxels until one has no symmetry
  for (int start = 0; start < 6 * 6 * 6; start++) {
    const int sx = 5 + start % 6, sy = 5 + (start / 6) % 6, sz = 5 + start / 36;
    if (!space->validCoordinate(sx, sy, sz)) continue;
    layerZ = height(sx, sy, sz);
    std::vector<std::array<int, 3>> region{{sx, sy, sz}};
    /* a lopsided growth: from the newest cell first, taking its
     * neighbours in a rotating order, so the blob is a crooked snake */
    for (int step = 0; step < 200 && region.size() < size; step++) {
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
    if (region.size() != size) continue;
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
    if (symmetries == 2) return region;
  }
  return {};
}

/* The region of asymmetricLayerRegion, stacked `layers` high, as the
 * result, and as pieces one shape per class of neighbouring pairs within a
 * layer (the same up to rotation), each used 0 to all times; with the
 * perfect matchings of one layer of it. */
struct dominoes_s {
  std::unique_ptr<puzzle_c> puzzle;
  unsigned long matchings = 0;
  size_t kinds = 0;
};

dominoes_s dominoPuzzle(gridType_c::gridType type, size_t size, int layers) {
  dominoes_s out;
  out.puzzle = std::make_unique<puzzle_c>(std::make_unique<gridType_c>(type));
  puzzle_c & puzzle = *out.puzzle;
  const gridType_c & gt = *puzzle.getGridType();

  const int S = 16;
  const std::vector<std::array<int, 3>> cells = asymmetricLayerRegion(gt, S, size);
  REQUIRE(cells.size() == size);
  std::unique_ptr<voxel_c> space(gt.getVoxel(S, S, S, voxel_c::VX_EMPTY));

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
  out.matchings = countMatchings(adj, used);
  REQUIRE(out.matchings > 0);

  // the result, and one shape per class of neighbouring pairs
  int up[3] = {0, 0, 0};
  if (gt.isDescribed()) latticeGridInfo_c::get(type).latticeColumn(2, up[0], up[1], up[2]);
  else up[2] = 1;
  const int R = S + 2 * layers;
  std::unique_ptr<voxel_c> result(gt.getVoxel(R, R, R, voxel_c::VX_EMPTY));
  /* not minimised: the result sits away from the space's corner, as
   * results drawn in the GUIs do (transform(0) must leave it there) */
  for (int l = 0; l < layers; l++)
    for (const auto & c : cells)
      result->setState(c[0] + l * up[0], c[1] + l * up[1], c[2] + l * up[2], voxel_c::VX_FILLED);
  result->initHotspot();
  const unsigned int resultId = puzzle.addShape(std::move(result));

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
  out.kinds = classes.size();
  problem_c * problem = puzzle.getProblem(puzzle.addProblem());
  problem->setResultId(resultId);
  for (auto & c : classes) {
    const unsigned int id = puzzle.addShape(std::move(c));
    problem->setShapeMinimum(id, 0);
    problem->setShapeMaximum(id, (unsigned int)(size * layers / 2));
  }
  return out;
}

} // namespace

TEST_CASE("described grids: dominoes on one layer give the region's perfect matchings", "[lattice][solver]") {
  for (gridType_c::gridType type : { gridType_c::GT_HEXAGONAL_PRISM, gridType_c::GT_CAIRO_PRISM,
                                     gridType_c::GT_DESCRIBED_TRIANGULAR_PRISM }) {
    INFO(name(type));
    dominoes_s d = dominoPuzzle(type, 10, 1);
    INFO(d.kinds << " kinds of pairs, " << d.matchings << " matchings");
    problem_c & problem = *d.puzzle->getProblem(0);

    std::unique_ptr<assembler_c> assm = d.puzzle->getGridType()->findAssembler(makeSolverSpec(problem), bttest::settings());
    REQUIRE(assm);
    REQUIRE(assm->createMatrix(false, false) == assembler_c::ERR_NONE);
    unsigned long n = 0;
    assm->assemble([&n](std::unique_ptr<assembly_c>) { n++; return true; });
    CHECK(n == d.matchings);
  }
}

/* ---- step C6: movement on hexagonal and Cairo prisms ----
 *
 * The movement vectors in griddescriptions.cpp are ours (NG has none for
 * these grids); the check cells are NG's collision test run on them. They
 * are checked here against an independent sweep of the cell polygons, for
 * symmetry under the grid's transformations, and on small puzzles whose
 * assemblies and solutions follow by hand.
 */
namespace {

const gridType_c::gridType PRISM_GRIDS[] = { gridType_c::GT_HEXAGONAL_PRISM, gridType_c::GT_CAIRO_PRISM };

using Cell = std::array<int, 3>;
using CellSet = std::set<Cell>;

/* a voxel of each kind, away from negative coordinates */
std::vector<Cell> voxelPerKind(const latticeGridInfo_c & inf) {
  std::vector<Cell> out;
  for (int z = 10; z < 20; z++)
    for (int y = 10; y < 20; y++)
      for (int x = 10; x < 20; x++) {
        const int b = inf.base(x, y, z);
        if (b < 0) continue;
        if ((int)out.size() <= b) out.resize(b + 1, Cell{-1, -1, -1});
        if (out[b][0] < 0) out[b] = {x, y, z};
      }
  for (const auto & c : out) REQUIRE(c[0] >= 0);
  return out;
}

using Poly = std::vector<std::array<double, 2>>;

Poly hull(Poly p) {
  std::sort(p.begin(), p.end());
  p.erase(std::unique(p.begin(), p.end(), [](const auto & a, const auto & b) {
            return std::fabs(a[0] - b[0]) < 1e-6 && std::fabs(a[1] - b[1]) < 1e-6; }), p.end());
  if (p.size() < 3) return p;
  auto crossz = [](const auto & o, const auto & a, const auto & b) {
    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]); };
  Poly h(2 * p.size());
  size_t k = 0;
  for (size_t i = 0; i < p.size(); i++) {
    while (k >= 2 && crossz(h[k - 2], h[k - 1], p[i]) <= 1e-9) k--;
    h[k++] = p[i];
  }
  for (size_t i = p.size() - 1, t = k + 1; i-- > 0;) {
    while (k >= t && crossz(h[k - 2], h[k - 1], p[i]) <= 1e-9) k--;
    h[k++] = p[i];
  }
  h.resize(k - 1);
  return h;
}

/* a cell as an upright prism: its polygon in the plane and its height
 * range, from the corners of all its faces */
struct prism_s { Poly poly; double z0, z1; };

prism_s prismOf(const latticeGridInfo_c & inf, const Cell & c) {
  prism_s p{{}, 1e9, -1e9};
  std::vector<float> corners;
  const unsigned int nf = inf.faces(c[0], c[1], c[2]);
  REQUIRE(nf > 0);
  for (unsigned int f = 0; f < nf; f++) inf.faceCorners(c[0], c[1], c[2], f, 0, 0, corners);
  Poly pts;
  for (size_t i = 0; i + 2 < corners.size(); i += 3) {
    pts.push_back({corners[i], corners[i + 1]});
    p.z0 = std::min(p.z0, (double)corners[i + 2]);
    p.z1 = std::max(p.z1, (double)corners[i + 2]);
  }
  p.poly = hull(pts);
  return p;
}

/* whether two convex polygons overlap with positive area (separating axes;
 * touching edges do not count) */
bool overlap(const Poly & a, const Poly & b) {
  for (const Poly * p : { &a, &b })
    for (size_t i = 0; i < p->size(); i++) {
      const auto & u = (*p)[i];
      const auto & v = (*p)[(i + 1) % p->size()];
      const double nx = v[1] - u[1], ny = u[0] - v[0];
      double amin = 1e18, amax = -1e18, bmin = 1e18, bmax = -1e18;
      for (const auto & q : a) { const double d = nx * q[0] + ny * q[1]; amin = std::min(amin, d); amax = std::max(amax, d); }
      for (const auto & q : b) { const double d = nx * q[0] + ny * q[1]; bmin = std::min(bmin, d); bmax = std::max(bmax, d); }
      if (std::min(amax, bmax) - std::max(amin, bmin) <= 1e-4 * std::hypot(nx, ny)) return false;
    }
  return true;
}

/* the cells, relative to c, that the prism at c sweeps through on one step
 * along d: the prisms other than its own whose height range overlaps the
 * swept range and whose polygon overlaps the swept polygon */
CellSet sweptCells(const latticeGridInfo_c & inf, const Cell & c, const Cell & d) {
  const prism_s p = prismOf(inf, c);
  float vx = (float)d[0], vy = (float)d[1], vz = (float)d[2];
  inf.toPhysical(vx, vy, vz);
  Poly pts = p.poly;
  for (const auto & q : p.poly) pts.push_back({q[0] + vx, q[1] + vy});
  const Poly swept = hull(pts);
  const double z0 = p.z0 + std::min(0.0, (double)vz), z1 = p.z1 + std::max(0.0, (double)vz);

  CellSet out;
  const int R = 8;
  for (int dz = -R; dz <= R; dz++)
    for (int dy = -R; dy <= R; dy++)
      for (int dx = -R; dx <= R; dx++) {
        const Cell o{c[0] + dx, c[1] + dy, c[2] + dz};
        if ((dx == 0 && dy == 0 && dz == 0) || !inf.isVoxel(o[0], o[1], o[2])) continue;
        const prism_s q = prismOf(inf, o);
        if (std::min(z1, q.z1) - std::max(z0, q.z0) <= 1e-4) continue;
        if (overlap(swept, q.poly)) out.insert({dx, dy, dz});
      }
  return out;
}

CellSet checksOf(const latticeGridInfo_c & inf, int base, unsigned int d) {
  CellSet s;
  for (const auto & c : inf.moveChecks(base, d)) s.insert(c);
  return s;
}

} // namespace

TEST_CASE("described prisms: movement vectors are lattice vectors along the axis and to neighbours", "[lattice][disasm]") {
  for (gridType_c::gridType type : PRISM_GRIDS) {
    INFO(name(type));
    const latticeGridInfo_c & inf = latticeGridInfo_c::get(type);
    REQUIRE(inf.canDisassemble());
    const unsigned int dirs = inf.directions();
    CHECK(dirs == (type == gridType_c::GT_HEXAGONAL_PRISM ? 4u : 3u));

    // the first is the prism axis, of physical length 1, the others lie in the plane
    double inPlaneLength = -1;
    for (unsigned int d = 0; d < dirs; d++) {
      const Cell v = inf.direction(d);
      CHECK(inf.isLatticeVector(v[0], v[1], v[2]));
      float x = (float)v[0], y = (float)v[1], z = (float)v[2];
      inf.toPhysical(x, y, z);
      INFO("direction " << d << ": " << x << " " << y << " " << z);
      if (d == 0) {
        CHECK(std::fabs(x) < 1e-5f);
        CHECK(std::fabs(y) < 1e-5f);
        CHECK(std::fabs(z - 1) < 1e-5f);
      } else {
        CHECK(std::fabs(z) < 1e-5f);
        // all in-plane vectors equally long, and pairwise not parallel
        const double l = std::hypot(x, y);
        if (inPlaneLength < 0) inPlaneLength = l;
        CHECK(std::fabs(l - inPlaneLength) < 1e-5);
        for (unsigned int e = 1; e < d; e++) {
          const Cell w = inf.direction(e);
          float a = (float)w[0], b = (float)w[1], c = (float)w[2];
          inf.toPhysical(a, b, c);
          CHECK(std::fabs(x * b - y * a) > 1e-3f);
        }
      }
    }

    if (type == gridType_c::GT_HEXAGONAL_PRISM) {
      /* a hexagon's edge neighbours are sqrt 3 away; each in-plane vector
       * is the step from a cell to a face neighbour */
      CHECK(std::fabs(inPlaneLength - std::sqrt(3.0)) < 1e-5);
      const Cell c{10, 10, 10};
      for (unsigned int d = 1; d < dirs; d++) {
        const Cell v = inf.direction(d);
        bool neighbour = false;
        int nx, ny, nz;
        for (unsigned int f = 0; inf.faceNeighbour(c[0], c[1], c[2], f, nx, ny, nz); f++)
          if (nx - c[0] == v[0] && ny - c[1] == v[1] && nz - c[2] == v[2]) neighbour = true;
        CHECK(neighbour);
      }
    } else {
      // the shortest in-plane lattice vectors
      for (int x = -6; x <= 6; x++)
        for (int y = -6; y <= 6; y++)
          if ((x || y) && inf.isLatticeVector(x, y, 0)) {
            float a = (float)x, b = (float)y, c = 0;
            inf.toPhysical(a, b, c);
            CHECK(std::hypot(a, b) >= inPlaneLength - 1e-5);
          }
    }
  }
}

TEST_CASE("described prisms: the check cells are what the cell sweeps through", "[lattice][disasm]") {
  for (gridType_c::gridType type : PRISM_GRIDS) {
    INFO(name(type));
    const latticeGridInfo_c & inf = latticeGridInfo_c::get(type);
    const auto kinds = voxelPerKind(inf);
    for (int b = 0; b < (int)kinds.size(); b++)
      for (unsigned int d = 0; d < inf.directions(); d++) {
        INFO("kind " << b << " direction " << d);
        const CellSet ng = checksOf(inf, b, d);
        CHECK(ng == sweptCells(inf, kinds[b], inf.direction(d)));
        // along the axis: the single cell above
        if (d == 0) CHECK(ng == CellSet{inf.direction(0)});
      }
  }

  /* by hand for hexagons: a step towards one edge neighbour passes the two
   * neighbours beside it, at 60 degrees to either side */
  const latticeGridInfo_c & hex = latticeGridInfo_c::get(gridType_c::GT_HEXAGONAL_PRISM);
  CHECK(checksOf(hex, 0, 1) == CellSet{{1, -1, 0}, {1, 0, -1}, {0, -1, 1}});
}

TEST_CASE("described grids: the check cells are symmetric under the grid's transformations", "[lattice][disasm]") {
  for (gridType_c::gridType type : DESCRIBED) {
    INFO(name(type));
    const latticeGridInfo_c & inf = latticeGridInfo_c::get(type);
    const unsigned int dirs = inf.directions();
    const auto kinds = voxelPerKind(inf);
    const int R = inf.checkReach() + 1;

    /* the cells, relative to a voxel of each kind, that block it on one
     * step backwards along each direction: the voxels o for which it is a
     * check cell of o going forwards */
    std::vector<std::vector<CellSet>> back(kinds.size(), std::vector<CellSet>(dirs));
    for (int b = 0; b < (int)kinds.size(); b++)
      for (int z = -R; z <= R; z++)
        for (int y = -R; y <= R; y++)
          for (int x = -R; x <= R; x++) {
            const Cell & c = kinds[b];
            const int ob = inf.base(c[0] + x, c[1] + y, c[2] + z);
            if (ob < 0) continue;
            for (unsigned int d = 0; d < dirs; d++)
              for (const auto & k : inf.moveChecks(ob, d))
                if (k[0] == -x && k[1] == -y && k[2] == -z) back[b][d].insert({x, y, z});
          }

    // the cells a voxel at c must find empty for one step along d, forwards (sign 1) or backwards
    auto blocking = [&](const Cell & c, unsigned int d, int sign) {
      const int b = inf.base(c[0], c[1], c[2]);
      CellSet s;
      for (const auto & k : sign > 0 ? checksOf(inf, b, d) : back[b][d]) s.insert({c[0] + k[0], c[1] + k[1], c[2] + k[2]});
      return s;
    };

    for (unsigned int t = 0; t < inf.transformations(); t++)
      for (int b = 0; b < (int)kinds.size(); b++)
        for (unsigned int d = 0; d < dirs; d++) {
          INFO("transformation " << t << " kind " << b << " direction " << d);
          const Cell c = kinds[b];
          Cell tc = c;
          REQUIRE(inf.apply(t, tc[0], tc[1], tc[2]));
          // the transformed direction is a movement direction or its opposite
          Cell e{c[0] + inf.direction(d)[0], c[1] + inf.direction(d)[1], c[2] + inf.direction(d)[2]};
          REQUIRE(inf.apply(t, e[0], e[1], e[2]));
          const Cell td{e[0] - tc[0], e[1] - tc[1], e[2] - tc[2]};
          int d2 = -1, sign = 0;
          for (unsigned int k = 0; k < dirs; k++) {
            if (inf.direction(k) == td) { d2 = (int)k; sign = 1; }
            if (inf.direction(k) == Cell{-td[0], -td[1], -td[2]}) { d2 = (int)k; sign = -1; }
          }
          REQUIRE(d2 >= 0);
          for (int s : { 1, -1 }) {
            CellSet mapped;
            for (Cell o : blocking(c, d, s)) {
              REQUIRE(inf.apply(t, o[0], o[1], o[2]));
              mapped.insert(o);
            }
            CHECK(mapped == blocking(tc, (unsigned int)d2, s * sign));
          }
        }
  }
}

/* Small puzzles on both prism grids whose counts follow by hand. A patch
 * is a cell with its face neighbours in its layer (a hexagon with its
 * ring of 6, a pentagon with its 5); layers are stacked along the lattice
 * column of the prism axis. */
namespace {

struct handPuzzle_s {
  std::unique_ptr<puzzle_c> puzzle;
  std::vector<Cell> result;
  std::vector<std::vector<Cell>> pieces;
};

Cell up(const latticeGridInfo_c & inf, const Cell & c, int layers) {
  int x, y, z;
  inf.latticeColumn(2, x, y, z);
  return {c[0] + layers * x, c[1] + layers * y, c[2] + layers * z};
}

/* the patch around c, c first */
std::vector<Cell> patch(const latticeGridInfo_c & inf, const Cell & c) {
  std::vector<Cell> p{c};
  int nx, ny, nz;
  for (unsigned int f = 0; inf.faceNeighbour(c[0], c[1], c[2], f, nx, ny, nz); f++) {
    float a = (float)(nx - c[0]), b = (float)(ny - c[1]), h = (float)(nz - c[2]);
    inf.toPhysical(a, b, h);
    if (std::fabs(h) < 1e-4f) p.push_back({nx, ny, nz});
  }
  return p;
}

std::pair<unsigned long, unsigned long> solveHand(gridType_c::gridType type, const std::vector<Cell> & result,
                                                  const std::vector<std::vector<Cell>> & pieces) {
  puzzle_c puzzle(std::make_unique<gridType_c>(type));
  const gridType_c & gt = *puzzle.getGridType();
  const int S = 24;
  auto shape = [&](const std::vector<Cell> & cells, bool minimise) {
    std::unique_ptr<voxel_c> v(gt.getVoxel(S, S, S, voxel_c::VX_EMPTY));
    for (const auto & c : cells) {
      REQUIRE(v->validCoordinate(c[0], c[1], c[2]));
      v->setState(c[0], c[1], c[2], voxel_c::VX_FILLED);
    }
    if (minimise) v->minimizePiece();
    v->initHotspot();
    return puzzle.addShape(std::move(v));
  };
  const unsigned int resultId = shape(result, false);
  problem_c * problem = puzzle.getProblem(puzzle.addProblem());
  problem->setResultId(resultId);
  for (const auto & p : pieces) {
    const unsigned int id = shape(p, true);
    problem->setShapeMinimum(id, 1);
    problem->setShapeMaximum(id, 1);
  }
  return solve(*problem);
}

std::vector<Cell> layer(const latticeGridInfo_c & inf, const std::vector<Cell> & cells, int l) {
  std::vector<Cell> out;
  for (const auto & c : cells) out.push_back(up(inf, c, l));
  return out;
}

std::vector<Cell> join(std::initializer_list<std::vector<Cell>> parts) {
  std::vector<Cell> out;
  for (const auto & p : parts) out.insert(out.end(), p.begin(), p.end());
  return out;
}

std::vector<Cell> without(std::vector<Cell> cells, std::initializer_list<Cell> drop) {
  for (const auto & d : drop) cells.erase(std::remove(cells.begin(), cells.end(), d), cells.end());
  return cells;
}

} // namespace

TEST_CASE("described prisms: a cell in a frame leaves along the axis, a closed box keeps it", "[lattice][disasm][solver]") {
  for (gridType_c::gridType type : PRISM_GRIDS) {
    INFO(name(type));
    const latticeGridInfo_c & inf = latticeGridInfo_c::get(type);
    const auto kinds = voxelPerKind(inf);
    for (const Cell & c : kinds) {
      INFO("centre " << c[0] << " " << c[1] << " " << c[2]);
      const std::vector<Cell> p = patch(inf, c);
      CHECK(p.size() == (type == gridType_c::GT_HEXAGONAL_PRISM ? 7u : 6u));

      /* one layer: the frame and the centre; the frame fits one way up to
       * the patch's symmetry, the centre takes the hole and lifts out */
      CHECK(solveHand(type, p, {without(p, {c}), {c}}) == std::pair<unsigned long, unsigned long>(1, 1));

      /* two layers, the frame on a plate: an open box, the centre lifts out
       * of the top (turned over, the box is the same assembly) */
      CHECK(solveHand(type, join({p, layer(inf, p, 1)}), {join({p, without(layer(inf, p, 1), {up(inf, c, 1)})}), {up(inf, c, 1)}}) ==
            std::pair<unsigned long, unsigned long>(1, 1));

      // three layers, plate, frame, plate: the centre is shut in
      CHECK(solveHand(type, join({p, layer(inf, p, 1), layer(inf, p, 2)}),
                      {join({p, without(layer(inf, p, 1), {up(inf, c, 1)}), layer(inf, p, 2)}), {up(inf, c, 1)}}) ==
            std::pair<unsigned long, unsigned long>(1, 0));
    }
  }
}

TEST_CASE("described prisms: a layer between two plates slides out sideways", "[lattice][disasm][solver]") {
  /* An asymmetric region three layers high; one piece is the top and bottom
   * layer, the other the middle one. It fits one way and can only leave
   * in the plane, so the in-plane directions must work. */
  for (gridType_c::gridType type : PRISM_GRIDS) {
    INFO(name(type));
    const latticeGridInfo_c & inf = latticeGridInfo_c::get(type);
    puzzle_c dummy(std::make_unique<gridType_c>(type));
    std::vector<Cell> r;
    for (const auto & c : asymmetricLayerRegion(*dummy.getGridType(), 16, 10)) r.push_back(c);
    REQUIRE(r.size() == 10);
    CHECK(solveHand(type, join({r, layer(inf, r, 1), layer(inf, r, 2)}), {join({r, layer(inf, r, 2)}), layer(inf, r, 1)}) ==
          std::pair<unsigned long, unsigned long>(1, 1));
  }
}

TEST_CASE("hexagonal prisms: a piece leaves sideways only through a channel as wide as a hexagon", "[lattice][disasm][solver]") {
  /* Three layers of a hexagon with its ring; the middle layer holds the
   * second piece, the centre with some ring cells next to each other.
   * Sliding the centre towards a ring cell sweeps the two ring cells beside
   * that one too (a hexagon is wider across its corners than the gap
   * between two touching hexagons), so the centre with one ring cell is
   * stuck, and with three consecutive ring cells it slides out. Either
   * way the box piece fits one way up to the result's symmetry. */
  const latticeGridInfo_c & inf = latticeGridInfo_c::get(gridType_c::GT_HEXAGONAL_PRISM);
  const Cell c{10, 10, 10};
  const Cell m = up(inf, c, 1);
  const std::vector<Cell> p = patch(inf, c);
  const std::vector<Cell> result = join({p, layer(inf, p, 1), layer(inf, p, 2)});

  const Cell v = inf.direction(1);
  const Cell g{m[0] + v[0], m[1] + v[1], m[2] + v[2]};
  // the ring cells at 60 degrees to either side of v: the other two check cells of the step
  std::vector<Cell> beside;
  for (const auto & k : inf.moveChecks(0, 1))
    if (k != v) beside.push_back({m[0] + k[0], m[1] + k[1], m[2] + k[2]});
  REQUIRE(beside.size() == 2);

  const std::vector<Cell> narrow{m, g};
  CHECK(solveHand(gridType_c::GT_HEXAGONAL_PRISM, result,
                  {join({p, without(layer(inf, p, 1), {m, g}), layer(inf, p, 2)}), narrow}) ==
        std::pair<unsigned long, unsigned long>(1, 0));

  const std::vector<Cell> wide{m, g, beside[0], beside[1]};
  CHECK(solveHand(gridType_c::GT_HEXAGONAL_PRISM, result,
                  {join({p, without(layer(inf, p, 1), {m, g, beside[0], beside[1]}), layer(inf, p, 2)}), wide}) ==
        std::pair<unsigned long, unsigned long>(1, 1));
}

TEST_CASE("described prisms: flat dominoes come apart, in one layer and stacked two high", "[lattice][disasm][solver]") {
  /* Every tiling of a layer by flat pieces comes apart (each piece lifts
   * out), so solutions equal assemblies equal the perfect matchings m of
   * the region's neighbour graph. Two layers high, a tiling is a pair of
   * layer tilings, and the mirror through the plane between the layers
   * (the result's only symmetry; flat pieces are their own mirror images)
   * swaps the two: m (m + 1) / 2 assemblies, all coming apart top layer
   * first. */
  for (gridType_c::gridType type : PRISM_GRIDS) {
    INFO(name(type));
    {
      dominoes_s d = dominoPuzzle(type, 10, 1);
      INFO(d.matchings << " matchings");
      const auto got = solve(*d.puzzle->getProblem(0));
      CHECK(got.first == d.matchings);
      CHECK(got.second == d.matchings);
    }
    {
      dominoes_s d = dominoPuzzle(type, 10, 2);
      INFO(d.matchings << " matchings of a layer");
      const auto got = solve(*d.puzzle->getProblem(0));
      CHECK(got.first == d.matchings * (d.matchings + 1) / 2);
      CHECK(got.second == got.first);
    }
  }
}
