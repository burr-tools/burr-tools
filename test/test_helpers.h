#ifndef BTTEST_TEST_HELPERS_H
#define BTTEST_TEST_HELPERS_H

#include "lib/gridtype.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/voxel.h"

#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace bttest {

/** every grid type BurrTools supports, for parametrized cases */
inline const gridType_c::gridType ALL_GRIDS[] = {
  gridType_c::GT_BRICKS,
  gridType_c::GT_TRIANGULAR_PRISM,
  gridType_c::GT_SPHERES,
  gridType_c::GT_RHOMBIC,
  gridType_c::GT_TETRA_OCTA,
};

/** a readable name, so a parametrized failure says which grid broke */
inline const char * gridName(gridType_c::gridType t) {
  switch (t) {
    case gridType_c::GT_BRICKS:           return "GT_BRICKS";
    case gridType_c::GT_TRIANGULAR_PRISM: return "GT_TRIANGULAR_PRISM";
    case gridType_c::GT_SPHERES:          return "GT_SPHERES";
    case gridType_c::GT_RHOMBIC:          return "GT_RHOMBIC";
    case gridType_c::GT_TETRA_OCTA:       return "GT_TETRA_OCTA";
    default:                              return "unknown";
  }
}

/** an empty voxel space of the given grid; the caller owns it */
inline std::unique_ptr<voxel_c> makeVoxel(const gridType_c & gt,
                                          unsigned int x, unsigned int y, unsigned int z) {
  return std::unique_ptr<voxel_c>(gt.getVoxel(x, y, z, voxel_c::VX_EMPTY));
}

/* voxel_c stores its grid as a raw `const gridType_c *` (voxel.h:58), not a
   shared/owned reference. A `const gridType_c &` parameter happily binds to
   a temporary (e.g. makeVoxel(gridType_c(gridType_c::GT_BRICKS), 2,2,2)),
   which is destroyed at the end of the full expression, leaving the
   returned voxel_c's transform()/selfSymmetries()/normalizeTransformation()
   reading freed memory. Deleting the rvalue overload turns that mistake
   into a compile error instead of a use-after-free. */
std::unique_ptr<voxel_c> makeVoxel(gridType_c &&, unsigned int, unsigned int, unsigned int) = delete;

/** a copy of an existing voxel space, built in the given grid; the caller owns it */
inline std::unique_ptr<voxel_c> copyVoxel(const gridType_c & gt, const voxel_c & orig) {
  return std::unique_ptr<voxel_c>(gt.getVoxel(orig));
}

/* same dangling-grid footgun as makeVoxel above. */
std::unique_ptr<voxel_c> copyVoxel(gridType_c &&, const voxel_c &) = delete;

/**
 * The coordinates the grid actually accepts, within a size x size x size box.
 *
 * GT_SPHERES, GT_RHOMBIC and GT_TETRA_OCTA accept only a sparse subset of
 * coordinates; a cell written outside it is not a differently-shaped piece,
 * it is not a piece at all. Anything that wants a fixture which is legal on
 * every grid has to ask the grid rather than address coordinates directly.
 */
inline std::vector<std::tuple<int, int, int>> validCoordinates(const voxel_c & v, int size) {
  std::vector<std::tuple<int, int, int>> out;

  for (int z = 0; z < size; z++)
    for (int y = 0; y < size; y++)
      for (int x = 0; x < size; x++)
        if (v.validCoordinate(x, y, z))
          out.emplace_back(x, y, z);

  return out;
}

/** how many of the shape's filled cells sit on coordinates the grid accepts */
inline unsigned int countValidFilled(const voxel_c & v) {
  unsigned int n = 0;

  for (unsigned int x = 0; x < v.getX(); x++)
    for (unsigned int y = 0; y < v.getY(); y++)
      for (unsigned int z = 0; z < v.getZ(); z++)
        if (v.validCoordinate(x, y, z) && v.getState(x, y, z) == voxel_c::VX_FILLED)
          n++;

  return n;
}

/**
 * A shape made of the first `cells` coordinates the grid accepts inside a
 * box of the given size, so it is a legal piece on every grid.
 *
 * Returns nullptr when the grid does not offer that many coordinates in a
 * box that size, so a caller can say so rather than silently testing a
 * smaller shape than it asked for.
 */
inline std::unique_ptr<voxel_c> legalShape(const gridType_c & gt, int size, unsigned int cells) {
  std::unique_ptr<voxel_c> v = makeVoxel(gt, size, size, size);

  std::vector<std::tuple<int, int, int>> coords = validCoordinates(*v, size);
  if (coords.size() < cells) return nullptr;

  for (unsigned int i = 0; i < cells; i++) {
    auto [x, y, z] = coords[i];
    v->setState(x, y, z, voxel_c::VX_FILLED);
  }

  return v;
}

/* a box big enough that every grid offers a workable number of valid
   coordinates inside it -- the tetra-octa grid is the sparsest, so it sets
   the floor */
constexpr int LEGAL_SHAPE_BOX = 6;


/**
 * Build a voxel space from ASCII layer art.
 *
 * One vector<string> per z-layer, one string per y-row, one character per x.
 * '#' is filled, '+' is variable, everything else is empty. Rows are padded
 * to the longest row found, so ragged art is legal.
 */
inline std::unique_ptr<voxel_c> fromLayers(const gridType_c & gt,
                                           std::initializer_list<std::vector<std::string>> layers) {
  unsigned int sz = static_cast<unsigned int>(layers.size());
  unsigned int sy = 0, sx = 0;

  for (const auto & layer : layers) {
    if (layer.size() > sy) sy = static_cast<unsigned int>(layer.size());
    for (const auto & row : layer)
      if (row.size() > sx) sx = static_cast<unsigned int>(row.size());
  }

  /* an empty layer list (or all-empty rows) yields sx == 0 || sy == 0 ||
     sz == 0. voxel_c's constructor computes bx1 = x-1 on that unsigned
     zero, which wraps to UINT_MAX and produces a wild bounding box instead
     of an obviously-broken shape. Guard here so a fixture typo like
     fromLayers(gt, {}) fails loudly at the call site instead of silently
     handing a corrupt voxel_c to the test body. */
  if (sx == 0 || sy == 0 || sz == 0)
    throw std::invalid_argument("fromLayers: layer art is empty, which would build a degenerate 0-sized voxel space");

  std::unique_ptr<voxel_c> v = makeVoxel(gt, sx, sy, sz);

  unsigned int z = 0;
  for (const auto & layer : layers) {
    for (unsigned int y = 0; y < layer.size(); y++) {
      const std::string & row = layer[y];
      for (unsigned int x = 0; x < row.size(); x++) {
        if (row[x] == '#')      v->setState(x, y, z, voxel_c::VX_FILLED);
        else if (row[x] == '+') v->setState(x, y, z, voxel_c::VX_VARIABLE);
      }
    }
    z++;
  }

  return v;
}

/* same dangling-grid footgun as makeVoxel above. */
std::unique_ptr<voxel_c> fromLayers(gridType_c &&, std::initializer_list<std::vector<std::string>>) = delete;

/**
 * Deep-compare two puzzles for the properties a save/load roundtrip must
 * preserve: grid type, comment, comment-popup flag, colours, shapes
 * (including per-voxel state and colour), and problems with their names
 * and result shapes.
 *
 * This is NOT a complete comparison of everything the save format
 * persists. It deliberately does not look at: a shape's name, weight or
 * hotspot; a problem's part list (per-shape minimum/maximum), piece
 * groups, colour placement constraints, maxHoles, solveState, assembler
 * state, solutions, or a problem's numAssemblies/numSolutions/usedTime
 * (src/lib/problem.cpp:121-123 persists all three). A field missing from
 * this list can still be covered directly in a test body with its own
 * assertions (see the byte-fixpoint case in test_roundtrip.cpp, which
 * does cover the format in full because it compares the serialized
 * document rather than the in-memory object).
 *
 * Name it for what it checks, not for what it sounds like it checks: this
 * is a narrow "did the roundtrip preserve the fields we track" probe, not
 * a general-purpose "are these puzzles the same" comparison -- notably, a
 * shape's name is the single most conspicuous thing it ignores. Do not
 * reach for this as an "unchanged" check elsewhere without first checking
 * the omissions list above.
 *
 * A note for whoever extends this: three problem_c accessors assert
 * before use and are unsafe to call unconditionally --
 * `getResultId()` asserts `resultValid()`, `getMaxHoles()` asserts
 * `maxHolesDefined()`, and `getNumAssemblies()`/`getNumSolutions()`/
 * `getUsedTime()` assert `solveState != SS_UNSOLVED`. Guard each before
 * reading it, the way the result-id comparison below guards on
 * `resultValid()`.
 *
 * Returns true when they match. Deliberately returns a plain bool rather
 * than asserting, so a caller can use it inside REQUIRE and get the whole
 * comparison reported as one assertion.
 */
inline bool puzzlesRoundtripEqual(const puzzle_c & a, const puzzle_c & b) {
  if (a.getGridType()->getType() != b.getGridType()->getType()) return false;
  if (a.getComment() != b.getComment()) return false;
  if (a.getCommentPopup() != b.getCommentPopup()) return false;

  if (a.colorNumber() != b.colorNumber()) return false;
  for (unsigned int i = 0; i < a.colorNumber(); i++) {
    unsigned char ar, ag, ab, br, bg, bb;
    a.getColor(i, &ar, &ag, &ab);
    b.getColor(i, &br, &bg, &bb);
    if (ar != br || ag != bg || ab != bb) return false;
  }

  if (a.getNumberOfShapes() != b.getNumberOfShapes()) return false;
  for (unsigned int s = 0; s < a.getNumberOfShapes(); s++) {
    const voxel_c * va = a.getShape(s);
    const voxel_c * vb = b.getShape(s);
    if (va->getX() != vb->getX()) return false;
    if (va->getY() != vb->getY()) return false;
    if (va->getZ() != vb->getZ()) return false;
    for (unsigned int i = 0; i < va->getXYZ(); i++) {
      if (va->getState(i) != vb->getState(i)) return false;
      if (va->getColor(i) != vb->getColor(i)) return false;
    }
  }

  if (a.getNumberOfProblems() != b.getNumberOfProblems()) return false;
  for (unsigned int p = 0; p < a.getNumberOfProblems(); p++) {
    const problem_c * pa = a.getProblem(p);
    const problem_c * pb = b.getProblem(p);
    if (pa->getName() != pb->getName()) return false;

    if (pa->resultValid() != pb->resultValid()) return false;
    if (pa->resultValid() && pa->getResultId() != pb->getResultId()) return false;
  }

  return true;
}

} // namespace bttest

#endif
