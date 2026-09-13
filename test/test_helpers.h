#ifndef BTTEST_TEST_HELPERS_H
#define BTTEST_TEST_HELPERS_H

#include "lib/gridtype.h"
#include "lib/voxel.h"

#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
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

} // namespace bttest

#endif
