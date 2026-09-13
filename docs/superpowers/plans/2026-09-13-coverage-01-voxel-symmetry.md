# Voxel and Symmetry Coverage (PR 1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add unit-level tests for the voxel space, symmetry, bitfield, and grouping units, and remove the three orphaned Boost.Test files that never compiled.

**Architecture:** Four new Catch2 files in `test/`, sharing a `test/test_helpers.h` fixture header that builds voxel spaces from ASCII layer art and iterates the five grid types. Tests are hybrid: algebraic properties for transformations and symmetry groups, worked examples for boundaries and error paths.

**Tech Stack:** C++20, Catch2 v3, Meson, just.

**Spec:** `docs/superpowers/specs/2026-09-13-test-coverage-stack-design.md`

## Global Constraints

- Never edit files under `subprojects/` or `src/lua/` (AGENTS.md rule 5).
- **No production code changes.** If a test fails against current behaviour, report it; do not fix `src/lib`. Production fixes belong in a separate PR.
- No test may assert an exact solver iteration count (AGENTS.md rule 4).
- Every new test file must be added to the `test_burrtools` source list at `meson.build:376`.
- House style, copied from `test/test_cubemesh.cpp`: helpers in an anonymous namespace at the top of the file, `TEST_CASE("description", "[tag]")`, includes prefixed `lib/` / `tools/` / `halfedge/`.
- `voxel_c` objects returned by `gridType_c::getVoxel` are owned by the caller. Always wrap them in `std::unique_ptr<voxel_c>`.
- Branch: `coverage/voxel-symmetry`, based on `coverage/tooling`.
- Verification for every task: `just build && just test`. Before the PR: also `just check`.

---

### Task 1: Port the bitfield test to Catch2

The existing `src/lib/bitfield_test.cpp` is a working Boost.Test file absent from the build. Port its assertions, then delete it.

**Files:**
- Create: `test/test_bitfield.cpp`
- Modify: `meson.build:376` (add the new file to the source list)
- Delete: `src/lib/bitfield_test.cpp`

**Interfaces:**
- Consumes: nothing
- Produces: nothing later tasks depend on

- [ ] **Step 1: Write the test file**

Create `test/test_bitfield.cpp`. The first three cases port the old Boost assertions; the fourth and fifth are new, covering the hex-string constructor and the 64-bit word boundary that the old test never touched.

```cpp
#include <catch2/catch_test_macros.hpp>

#include "lib/bitfield.h"

TEST_CASE("bitfield: a fresh field is all zero", "[bitfield]") {
  bitfield_c<240> bits;

  for (int i = 0; i < 240; i++)
    REQUIRE_FALSE(bits.get(i));

  REQUIRE_FALSE(bits.notNull());
}

TEST_CASE("bitfield: set and reset touch exactly one bit", "[bitfield]") {
  bitfield_c<240> bits;

  bits.set(1);
  bits.set(2);
  bits.set(6);

  for (int i = 0; i < 240; i++)
    REQUIRE(bits.get(i) == (i == 1 || i == 2 || i == 6));

  bits.clear();
  for (int i = 0; i < 240; i++)
    REQUIRE_FALSE(bits.get(i));

  for (int i = 0; i < 240; i++)
    bits.set(i);

  bits.reset(1);
  bits.reset(5);

  for (int i = 0; i < 240; i++)
    REQUIRE(bits.get(i) == !(i == 1 || i == 5));
}

TEST_CASE("bitfield: notNull sees a bit in any word", "[bitfield]") {
  bitfield_c<240> bits;

  REQUIRE_FALSE(bits.notNull());

  bits.set(100);
  REQUIRE(bits.notNull());

  bits.reset(100);
  REQUIRE_FALSE(bits.notNull());
}

TEST_CASE("bitfield: bits survive the 64-bit word boundaries", "[bitfield]") {
  bitfield_c<240> bits;

  /* the field is stored as four uint64_t words; check every seam */
  const int seams[] = { 63, 64, 127, 128, 191, 192, 239 };

  for (int pos : seams) {
    bits.clear();
    bits.set(pos);

    REQUIRE(bits.get(pos));
    REQUIRE(bits.notNull());

    for (int i = 0; i < 240; i++)
      REQUIRE(bits.get(i) == (i == pos));
  }
}

TEST_CASE("bitfield: the hex string constructor fills from the low bits up", "[bitfield]") {
  /* "1" sets only bit 0 */
  bitfield_c<240> one("1");
  REQUIRE(one.get(0));
  for (int i = 1; i < 240; i++)
    REQUIRE_FALSE(one.get(i));

  /* "f" sets bits 0..3 */
  bitfield_c<240> nibble("f");
  for (int i = 0; i < 240; i++)
    REQUIRE(nibble.get(i) == (i < 4));

  /* upper and lower case agree */
  bitfield_c<240> lower("abc");
  bitfield_c<240> upper("ABC");
  for (int i = 0; i < 240; i++)
    REQUIRE(lower.get(i) == upper.get(i));

  /* each hex digit is four bits, so "10" is bit 4 only */
  bitfield_c<240> shifted("10");
  for (int i = 0; i < 240; i++)
    REQUIRE(shifted.get(i) == (i == 4));
}

TEST_CASE("bitfield: copy construction preserves every bit", "[bitfield]") {
  bitfield_c<240> src;
  src.set(0);
  src.set(64);
  src.set(239);

  bitfield_c<240> copy(src);

  for (int i = 0; i < 240; i++)
    REQUIRE(copy.get(i) == src.get(i));

  /* the copy is independent */
  copy.set(100);
  REQUIRE(copy.get(100));
  REQUIRE_FALSE(src.get(100));
}
```

- [ ] **Step 2: Add the file to the build**

At `meson.build:376`, add `'test/test_bitfield.cpp'` to the source list so it reads:

```meson
  tools_src + halfedge_src + libburr_src + ['test/test_solver.cpp', 'test/test_cubemesh.cpp', 'test/test_manifold_smoke.cpp', 'test/test_minkmesh.cpp', 'test/test_bitfield.cpp'],
```

- [ ] **Step 3: Run the new tests**

```bash
just build && ./build/test_burrtools "[bitfield]"
```

Expected: all bitfield cases pass. If the hex-string case fails, **do not change `bitfield.h`** — record the failure and report it; the constraint above forbids production fixes in this PR.

- [ ] **Step 4: Delete the orphaned Boost file**

```bash
git rm src/lib/bitfield_test.cpp
```

- [ ] **Step 5: Verify the full suite still passes**

```bash
just build && just test
```

Expected: all tests pass, with the bitfield cases added to the count.

- [ ] **Step 6: Commit**

```bash
git add test/test_bitfield.cpp meson.build
git commit -m "test: port the bitfield test to Catch2 and extend it

src/lib/bitfield_test.cpp was written against Boost.Test and was never
added to meson.build, so it has not compiled or run for as long as the
Meson build has existed. Port its assertions to Catch2 and delete it.

Adds cases the original lacked: the 64-bit word seams, the hex-string
constructor, and copy independence.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01VGbetP1s9EhUwpC96TKiht"
```

---

### Task 2: Add the shared fixture header and port the voxel_0 test

**Files:**
- Create: `test/test_helpers.h`
- Create: `test/test_voxel.cpp`
- Modify: `meson.build:376`
- Delete: `src/lib/voxel_0_test.cpp`, `src/lib/main_test.cpp`

**Interfaces:**
- Consumes: nothing
- Produces: `test/test_helpers.h`, used by Tasks 3–6 and by PRs 2–4. Exact interface:
  - `namespace bttest`
  - `const gridType_c::gridType ALL_GRIDS[5]` — every grid type
  - `const char * gridName(gridType_c::gridType t)` — human-readable name for `INFO()` output
  - `std::unique_ptr<voxel_c> makeVoxel(const gridType_c & gt, unsigned x, unsigned y, unsigned z)` — an empty space
  - `std::unique_ptr<voxel_c> fromLayers(const gridType_c & gt, std::initializer_list<std::vector<std::string>> layers)` — build a space from ASCII art, `#` filled, `+` variable, `.` or space empty; one `vector<string>` per z-layer, each string a y-row

- [ ] **Step 1: Write the fixture header**

Create `test/test_helpers.h`:

```cpp
#ifndef BTTEST_TEST_HELPERS_H
#define BTTEST_TEST_HELPERS_H

#include "lib/gridtype.h"
#include "lib/voxel.h"

#include <initializer_list>
#include <memory>
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

} // namespace bttest

#endif
```

- [ ] **Step 2: Write the first voxel test file**

Create `test/test_voxel.cpp`. The first case ports the old Boost `voxel_0_test`; the rest cover construction and state accessors across every grid.

```cpp
#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "lib/gridtype.h"
#include "lib/voxel.h"

#include <memory>

using namespace bttest;

TEST_CASE("voxel: a new space has the requested size and is empty", "[voxel]") {
  /* ported from the old Boost voxel_0_test */
  gridType_c gt(gridType_c::GT_BRICKS);
  std::unique_ptr<voxel_c> v = makeVoxel(gt, 1, 2, 3);

  REQUIRE(v->getX() == 1);
  REQUIRE(v->getY() == 2);
  REQUIRE(v->getZ() == 3);
  REQUIRE(v->getXYZ() == 6);

  for (unsigned int i = 0; i < v->getXYZ(); i++)
    REQUIRE(v->getState(i) == voxel_c::VX_EMPTY);
}

TEST_CASE("voxel: every grid builds an empty space of the right size", "[voxel]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    std::unique_ptr<voxel_c> v = makeVoxel(gt, 2, 3, 4);

    REQUIRE(v->getX() == 2);
    REQUIRE(v->getY() == 3);
    REQUIRE(v->getZ() == 4);
    REQUIRE(v->getXYZ() == 24);
    REQUIRE(v->countState(voxel_c::VX_EMPTY) == 24);
    REQUIRE(v->countState(voxel_c::VX_FILLED) == 0);
    REQUIRE(v->getGridType()->getType() == t);
  }
}

TEST_CASE("voxel: setState and getState agree on all three states", "[voxel]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    std::unique_ptr<voxel_c> v = makeVoxel(gt, 3, 1, 1);

    v->setState(0, 0, 0, voxel_c::VX_FILLED);
    v->setState(1, 0, 0, voxel_c::VX_VARIABLE);
    /* (2,0,0) stays empty */

    REQUIRE(v->isFilled(0, 0, 0));
    REQUIRE(v->isVariable(1, 0, 0));
    REQUIRE(v->isEmpty(2, 0, 0));

    REQUIRE(v->countState(voxel_c::VX_FILLED) == 1);
    REQUIRE(v->countState(voxel_c::VX_VARIABLE) == 1);
    REQUIRE(v->countState(voxel_c::VX_EMPTY) == 1);
  }
}

TEST_CASE("voxel: colour is independent of state", "[voxel]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  std::unique_ptr<voxel_c> v = makeVoxel(gt, 2, 1, 1);

  v->setState(0, 0, 0, voxel_c::VX_FILLED);
  v->setColor(0, 0, 0, 5);

  REQUIRE(v->isFilled(0, 0, 0));
  REQUIRE(v->getColor(0, 0, 0) == 5);

  /* changing state must not disturb the colour */
  v->setState(0, 0, 0, voxel_c::VX_VARIABLE);
  REQUIRE(v->isVariable(0, 0, 0));
  REQUIRE(v->getColor(0, 0, 0) == 5);

  /* and changing colour must not disturb the state */
  v->setColor(0, 0, 0, 63);
  REQUIRE(v->isVariable(0, 0, 0));
  REQUIRE(v->getColor(0, 0, 0) == 63);
}

TEST_CASE("voxel: get2 returns empty outside the space", "[voxel]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  std::unique_ptr<voxel_c> v = makeVoxel(gt, 2, 2, 2);
  v->setAll(voxel_c::VX_FILLED);

  /* inside is filled */
  REQUIRE(v->getState2(0, 0, 0) == voxel_c::VX_FILLED);
  REQUIRE(v->getState2(1, 1, 1) == voxel_c::VX_FILLED);

  /* every direction out of bounds reads as empty, never as a crash */
  REQUIRE(v->getState2(-1, 0, 0) == voxel_c::VX_EMPTY);
  REQUIRE(v->getState2(0, -1, 0) == voxel_c::VX_EMPTY);
  REQUIRE(v->getState2(0, 0, -1) == voxel_c::VX_EMPTY);
  REQUIRE(v->getState2(2, 0, 0) == voxel_c::VX_EMPTY);
  REQUIRE(v->getState2(0, 2, 0) == voxel_c::VX_EMPTY);
  REQUIRE(v->getState2(0, 0, 2) == voxel_c::VX_EMPTY);

  REQUIRE(v->isEmpty2(-1, -1, -1));
  REQUIRE(v->isFilled2(1, 1, 1));
}

TEST_CASE("voxel: the ASCII fixture helper places voxels where it says", "[voxel]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* an L in the z=0 layer, one lone voxel in z=1 */
  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "##",
      "#." },
    { "#.",
      ".." },
  });

  REQUIRE(v->getX() == 2);
  REQUIRE(v->getY() == 2);
  REQUIRE(v->getZ() == 2);

  REQUIRE(v->isFilled(0, 0, 0));
  REQUIRE(v->isFilled(1, 0, 0));
  REQUIRE(v->isFilled(0, 1, 0));
  REQUIRE(v->isEmpty(1, 1, 0));

  REQUIRE(v->isFilled(0, 0, 1));
  REQUIRE(v->isEmpty(1, 0, 1));

  REQUIRE(v->countState(voxel_c::VX_FILLED) == 4);
}
```

- [ ] **Step 3: Add both files to the build**

At `meson.build:376`, add `'test/test_voxel.cpp'` to the list. `test_helpers.h` is a header and needs no entry, but the test target must find it — it is in the same `test/` directory as the `.cpp` that includes it, so `#include "test_helpers.h"` resolves without an include-path change.

- [ ] **Step 4: Run the new tests**

```bash
just build && ./build/test_burrtools "[voxel]"
```

Expected: all voxel cases pass.

If `fromLayers` produces a space whose axes are transposed relative to the assertions, fix `test_helpers.h` — not `src/lib`. The helper is new code and is allowed to change.

- [ ] **Step 5: Delete the remaining orphaned Boost files**

```bash
git rm src/lib/voxel_0_test.cpp src/lib/main_test.cpp
```

`main_test.cpp` contains only `#define BOOST_TEST_MAIN` and an include — it is the Boost runner entry point, made redundant by Catch2's `catch2-with-main` dependency at `meson.build:371`.

- [ ] **Step 6: Verify the full suite**

```bash
just build && just test
```

Expected: all tests pass.

- [ ] **Step 7: Commit**

```bash
git add test/test_helpers.h test/test_voxel.cpp meson.build
git commit -m "test: add voxel construction and state tests, port voxel_0_test

Introduces test/test_helpers.h, a fixture header that builds voxel spaces
from ASCII layer art and enumerates the five grid types, so the tests that
follow can be parametrized over every grid instead of only bricks.

Ports src/lib/voxel_0_test.cpp from Boost.Test and deletes it along with
src/lib/main_test.cpp, the Boost runner entry point that Catch2's
catch2-with-main replaced.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01VGbetP1s9EhUwpC96TKiht"
```

---

### Task 3: Index arithmetic and bounding box tests

**Files:**
- Modify: `test/test_voxel.cpp` (append cases)

**Interfaces:**
- Consumes: `bttest::ALL_GRIDS`, `gridName`, `makeVoxel`, `fromLayers` from Task 2
- Produces: nothing later tasks depend on

- [ ] **Step 1: Append the index arithmetic cases**

Add to `test/test_voxel.cpp`:

```cpp
TEST_CASE("voxel: getIndex and indexToXYZ are inverses over the whole space", "[voxel][index]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    std::unique_ptr<voxel_c> v = makeVoxel(gt, 3, 4, 5);

    for (unsigned int z = 0; z < v->getZ(); z++)
      for (unsigned int y = 0; y < v->getY(); y++)
        for (unsigned int x = 0; x < v->getX(); x++) {
          int idx = v->getIndex(x, y, z);
          REQUIRE(idx >= 0);
          REQUIRE(static_cast<unsigned int>(idx) < v->getXYZ());

          unsigned int rx = 0, ry = 0, rz = 0;
          REQUIRE(v->indexToXYZ(idx, &rx, &ry, &rz));
          REQUIRE(rx == x);
          REQUIRE(ry == y);
          REQUIRE(rz == z);
        }
  }
}

TEST_CASE("voxel: every index maps to a distinct coordinate", "[voxel][index]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  std::unique_ptr<voxel_c> v = makeVoxel(gt, 3, 4, 5);

  std::vector<bool> seen(v->getXYZ(), false);

  for (unsigned int i = 0; i < v->getXYZ(); i++) {
    unsigned int x = 0, y = 0, z = 0;
    REQUIRE(v->indexToXYZ(i, &x, &y, &z));

    int back = v->getIndex(x, y, z);
    REQUIRE(static_cast<unsigned int>(back) == i);

    REQUIRE_FALSE(seen[i]);
    seen[i] = true;
  }

  for (bool s : seen)
    REQUIRE(s);
}
```

Add `#include <vector>` to the file's include block if it is not already there.

- [ ] **Step 2: Run them**

```bash
just build && ./build/test_burrtools "[index]"
```

Expected: pass.

- [ ] **Step 3: Append the bounding box cases**

```cpp
TEST_CASE("voxel: the bounding box tracks the filled voxels", "[voxel][bbox]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* a single voxel at (1,1,0) in a 3x3x1 space */
  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "...",
      ".#.",
      "..." },
  });

  REQUIRE(v->boundX1() == 1);
  REQUIRE(v->boundX2() == 1);
  REQUIRE(v->boundY1() == 1);
  REQUIRE(v->boundY2() == 1);
  REQUIRE(v->boundZ1() == 0);
  REQUIRE(v->boundZ2() == 0);
}

TEST_CASE("voxel: minimizePiece crops to the bounding box", "[voxel][bbox]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "....",
      ".##.",
      "....",
      "...." },
  });

  const unsigned int before = v->countState(voxel_c::VX_FILLED);
  REQUIRE(before == 2);

  v->minimizePiece();

  /* the shape survives, the padding does not */
  REQUIRE(v->countState(voxel_c::VX_FILLED) == before);
  REQUIRE(v->getX() == 2);
  REQUIRE(v->getY() == 1);
  REQUIRE(v->getZ() == 1);
  REQUIRE(v->isFilled(0, 0, 0));
  REQUIRE(v->isFilled(1, 0, 0));
}

TEST_CASE("voxel: translate moves the shape and fills the vacated cells", "[voxel][bbox]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "#..",
      "...",
      "..." },
  });

  REQUIRE(v->isFilled(0, 0, 0));

  v->translate(1, 1, 0, voxel_c::VX_EMPTY);

  REQUIRE(v->isEmpty(0, 0, 0));
  REQUIRE(v->isFilled(1, 1, 0));
  REQUIRE(v->countState(voxel_c::VX_FILLED) == 1);
}

TEST_CASE("voxel: translating out and back is the identity", "[voxel][bbox]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  std::unique_ptr<voxel_c> original = fromLayers(gt, {
    { ".#.",
      "###",
      ".#." },
  });

  std::unique_ptr<voxel_c> moved(gt.getVoxel(original.get()));

  moved->translate(1, 0, 0, voxel_c::VX_EMPTY);
  moved->translate(-1, 0, 0, voxel_c::VX_EMPTY);

  /* the rightmost column was shifted off the edge and cannot come back,
     so compare only what stayed inside: a 2-wide translate round trip on
     a shape that fits clear of the boundary is lossless */
  std::unique_ptr<voxel_c> safe = fromLayers(gt, {
    { ".#..",
      ".#..",
      "...." },
  });
  std::unique_ptr<voxel_c> safeMoved(gt.getVoxel(safe.get()));

  safeMoved->translate(1, 0, 0, voxel_c::VX_EMPTY);
  safeMoved->translate(-1, 0, 0, voxel_c::VX_EMPTY);

  REQUIRE(*safeMoved == *safe);
}

TEST_CASE("voxel: resize keeps the overlapping region", "[voxel][bbox]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "##",
      "##" },
  });

  REQUIRE(v->countState(voxel_c::VX_FILLED) == 4);

  /* growing keeps every voxel and pads with the filler */
  v->resize(4, 4, 1, voxel_c::VX_EMPTY);
  REQUIRE(v->getX() == 4);
  REQUIRE(v->getY() == 4);
  REQUIRE(v->countState(voxel_c::VX_FILLED) == 4);
  REQUIRE(v->isFilled(0, 0, 0));
  REQUIRE(v->isFilled(1, 1, 0));
  REQUIRE(v->isEmpty(3, 3, 0));

  /* shrinking discards what falls outside */
  v->resize(1, 1, 1, voxel_c::VX_EMPTY);
  REQUIRE(v->getX() == 1);
  REQUIRE(v->countState(voxel_c::VX_FILLED) == 1);
}
```

- [ ] **Step 4: Run the bounding box cases**

```bash
just build && ./build/test_burrtools "[bbox]"
```

Expected: pass. If `minimizePiece` or `resize` behaves differently from the assertions, **record it and report** — the constraint forbids changing `src/lib` in this PR. Adjust the test only if the test's expectation was wrong, not to paper over a real defect.

- [ ] **Step 5: Run the whole suite and commit**

```bash
just build && just test
git add test/test_voxel.cpp
git commit -m "test: cover voxel index arithmetic and bounding box operations

getIndex/indexToXYZ are checked as exact inverses over a whole space and
across all five grids, and as a bijection. translate, resize and
minimizePiece are checked for the voxels they keep, move and drop.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01VGbetP1s9EhUwpC96TKiht"
```

---

### Task 4: Connectivity and hole filling

**Files:**
- Create: `test/test_voxel_connect.cpp`
- Modify: `meson.build:376`

A separate file rather than more appended cases: `test_voxel.cpp` is already carrying construction, index, and bounding-box concerns, and connectivity is a distinct unit with its own fixtures.

**Interfaces:**
- Consumes: `bttest::fromLayers`, `makeVoxel`, `ALL_GRIDS`, `gridName` from Task 2
- Produces: nothing later tasks depend on

- [ ] **Step 1: Write the file**

```cpp
#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "lib/gridtype.h"
#include "lib/voxel.h"

#include <memory>

using namespace bttest;

TEST_CASE("voxel: a solid block is face connected", "[voxel][connect]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "##",
      "##" },
  });

  /* type 0 is face connectivity; check the filled voxels hang together */
  REQUIRE(v->connected(0, false, voxel_c::VX_EMPTY, false));
}

TEST_CASE("voxel: two separated voxels are not connected", "[voxel][connect]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* a gap between the two filled cells */
  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "#.#" },
  });

  REQUIRE_FALSE(v->connected(0, false, voxel_c::VX_EMPTY, false));
}

TEST_CASE("voxel: diagonal voxels are not face connected", "[voxel][connect]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* corner-to-corner touch only */
  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "#.",
      ".#" },
  });

  REQUIRE_FALSE(v->connected(0, false, voxel_c::VX_EMPTY, false));
}

TEST_CASE("voxel: an L shape is connected", "[voxel][connect]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "#..",
      "#..",
      "###" },
  });

  REQUIRE(v->connected(0, false, voxel_c::VX_EMPTY, false));
}

TEST_CASE("voxel: connectivity spans layers", "[voxel][connect]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* one voxel per layer, stacked in z */
  std::unique_ptr<voxel_c> stacked = fromLayers(gt, {
    { "#" },
    { "#" },
  });
  REQUIRE(stacked->connected(0, false, voxel_c::VX_EMPTY, false));

  /* one voxel per layer, offset so they only touch at an edge */
  std::unique_ptr<voxel_c> offset = fromLayers(gt, {
    { "#." },
    { ".#" },
  });
  REQUIRE_FALSE(offset->connected(0, false, voxel_c::VX_EMPTY, false));
}

TEST_CASE("voxel: fillHoles closes an enclosed cavity", "[voxel][connect]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* a ring in a single layer: the centre is enclosed in x and y but open
     in z, so a 3D hole fill must not close it */
  std::unique_ptr<voxel_c> ring = fromLayers(gt, {
    { "###",
      "#.#",
      "###" },
  });

  const unsigned int before = ring->countState(voxel_c::VX_FILLED);
  ring->fillHoles(0);

  /* open in z, so nothing is enclosed and nothing is filled */
  REQUIRE(ring->countState(voxel_c::VX_FILLED) == before);

  /* a fully enclosed 3x3x3 shell has one trapped cell at the centre */
  std::unique_ptr<voxel_c> shell = fromLayers(gt, {
    { "###",
      "###",
      "###" },
    { "###",
      "#.#",
      "###" },
    { "###",
      "###",
      "###" },
  });

  REQUIRE(shell->isEmpty(1, 1, 1));
  REQUIRE(shell->countState(voxel_c::VX_FILLED) == 26);

  shell->fillHoles(0);

  REQUIRE(shell->countState(voxel_c::VX_FILLED) == 27);
  REQUIRE_FALSE(shell->isEmpty(1, 1, 1));
}

TEST_CASE("voxel: counting states adds up to the whole space", "[voxel][connect]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    std::unique_ptr<voxel_c> v = makeVoxel(gt, 2, 2, 2);

    v->setState(0, 0, 0, voxel_c::VX_FILLED);
    v->setState(1, 0, 0, voxel_c::VX_FILLED);
    v->setState(0, 1, 0, voxel_c::VX_VARIABLE);

    const unsigned int total = v->countState(voxel_c::VX_EMPTY)
                             + v->countState(voxel_c::VX_FILLED)
                             + v->countState(voxel_c::VX_VARIABLE);

    REQUIRE(total == v->getXYZ());
    REQUIRE(v->countState(voxel_c::VX_FILLED) == 2);
    REQUIRE(v->countState(voxel_c::VX_VARIABLE) == 1);
  }
}
```

- [ ] **Step 2: Add to the build**

Add `'test/test_voxel_connect.cpp'` to the list at `meson.build:376`.

- [ ] **Step 3: Run**

```bash
just build && ./build/test_burrtools "[connect]"
```

Expected: pass.

The `connected` signature is `connected(char type, bool inverse, voxel_type value, bool outsideZ = true)`. If the `type` or `outsideZ` argument behaves differently from the assumption above, read `src/lib/voxel.cpp:530` and adjust the **test arguments** to express the intended property. Do not change `voxel.cpp`.

- [ ] **Step 4: Run the whole suite and commit**

```bash
just build && just test
git add test/test_voxel_connect.cpp meson.build
git commit -m "test: cover voxel connectivity and hole filling

Face connectivity is checked on solid, L-shaped, gapped, diagonal and
multi-layer fixtures, so both the connected and disconnected verdicts are
exercised. fillHoles is checked against a shape that is open in z, which
must stay untouched, and a fully enclosed shell, whose trapped centre must
be filled.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01VGbetP1s9EhUwpC96TKiht"
```

---

### Task 5: Transformation and symmetry properties

**Files:**
- Create: `test/test_symmetries.cpp`
- Modify: `meson.build:376`

**Interfaces:**
- Consumes: `bttest::ALL_GRIDS`, `gridName`, `makeVoxel`, `fromLayers` from Task 2
- Produces: nothing later tasks depend on

- [ ] **Step 1: Write the file**

```cpp
#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "lib/gridtype.h"
#include "lib/symmetries.h"
#include "lib/voxel.h"

#include <memory>

using namespace bttest;

TEST_CASE("symmetries: every grid reports a transformation count", "[symmetry]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    const symmetries_c * sym = gt.getSymmetries();
    REQUIRE(sym != nullptr);

    const unsigned int plain  = sym->getNumTransformations();
    const unsigned int mirror = sym->getNumTransformationsMirror();

    REQUIRE(plain > 0);
    /* the mirrored count includes the plain ones, so it is never smaller */
    REQUIRE(mirror >= plain);
  }
}

TEST_CASE("symmetries: transformation 0 is the identity", "[symmetry]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    std::unique_ptr<voxel_c> v = makeVoxel(gt, 2, 2, 2);
    v->setState(0, 0, 0, voxel_c::VX_FILLED);
    v->setState(1, 0, 0, voxel_c::VX_FILLED);

    std::unique_ptr<voxel_c> copy(gt.getVoxel(v.get()));

    REQUIRE(copy->transform(0));
    REQUIRE(*copy == *v);
  }
}

TEST_CASE("symmetries: transAdd composes transformations", "[symmetry]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    const symmetries_c * sym = gt.getSymmetries();

    const unsigned int n = sym->getNumTransformations();

    /* applying t1 then t2 must equal applying transAdd(t1, t2) in one go,
       whenever the composed transformation exists */
    for (unsigned int t1 = 0; t1 < n; t1++)
      for (unsigned int t2 = 0; t2 < n; t2++) {
        const unsigned char combined = sym->transAdd(t1, t2);
        if (combined == TND) continue;

        INFO("t1 " << t1 << " t2 " << t2 << " combined " << (int)combined);

        std::unique_ptr<voxel_c> stepwise = fromLayers(gt, {
          { "##.",
            "#..",
            "..." },
        });
        std::unique_ptr<voxel_c> direct(gt.getVoxel(stepwise.get()));

        if (!stepwise->transform(t1)) continue;
        if (!stepwise->transform(t2)) continue;
        if (!direct->transform(combined)) continue;

        REQUIRE(stepwise->identicalInBB(direct.get()));
      }
  }
}

TEST_CASE("symmetries: a shape is identical to its own rotations", "[symmetry]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);
    const symmetries_c * sym = gt.getSymmetries();

    std::unique_ptr<voxel_c> original = fromLayers(gt, {
      { "##.",
        "#..",
        "..." },
    });

    for (unsigned int tr = 0; tr < sym->getNumTransformations(); tr++) {
      INFO("transformation " << tr);

      std::unique_ptr<voxel_c> rotated(gt.getVoxel(original.get()));
      if (!rotated->transform(tr)) continue;

      /* a rotation never adds or removes material */
      REQUIRE(rotated->countState(voxel_c::VX_FILLED)
              == original->countState(voxel_c::VX_FILLED));

      /* and the shape is still recognisably the same one */
      REQUIRE(original->identicalWithRots(rotated.get(), false, false));
    }
  }
}

TEST_CASE("symmetries: a cube is symmetric under every transformation", "[symmetry]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  const symmetries_c * sym = gt.getSymmetries();

  std::unique_ptr<voxel_c> cube = makeVoxel(gt, 2, 2, 2);
  cube->setAll(voxel_c::VX_FILLED);

  const symmetries_t s = cube->selfSymmetries();

  /* a solid cube maps onto itself under every rotation the grid has */
  for (unsigned int tr = 0; tr < sym->getNumTransformations(); tr++) {
    INFO("transformation " << tr);
    REQUIRE(sym->symmetrieContainsTransformation(s, tr));
  }
}

TEST_CASE("symmetries: an asymmetric shape has the trivial symmetry group", "[symmetry]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* a deliberately lopsided shape */
  std::unique_ptr<voxel_c> lopsided = fromLayers(gt, {
    { "##.",
      "#..",
      "..." },
    { "#..",
      "...",
      "..." },
  });

  const symmetries_t s = lopsided->selfSymmetries();

  /* transformation 0 always maps a shape onto itself */
  REQUIRE(gt.getSymmetries()->symmetrieContainsTransformation(s, 0));
  REQUIRE(unSymmetric(s));
}

TEST_CASE("symmetries: normalizeTransformation returns a usable transformation", "[symmetry]") {
  for (gridType_c::gridType t : ALL_GRIDS) {
    INFO("grid " << gridName(t));
    gridType_c gt(t);

    std::unique_ptr<voxel_c> v = fromLayers(gt, {
      { "##",
        "#." },
    });

    const unsigned int n = gt.getSymmetries()->getNumTransformationsMirror();

    for (unsigned int tr = 0; tr < n; tr++) {
      const unsigned char norm = v->normalizeTransformation(tr);
      INFO("transformation " << tr << " normalized to " << (int)norm);
      REQUIRE(norm < n);
    }
  }
}
```

- [ ] **Step 2: Add to the build**

Add `'test/test_symmetries.cpp'` to `meson.build:376`.

- [ ] **Step 3: Run**

```bash
just build && ./build/test_burrtools "[symmetry]"
```

Expected: pass. The `transAdd` case is the slowest — it is O(n²) transformations per grid, and the sphere grid has the most. If it runs longer than a few seconds, tag that single case `[symmetry][slow]` and exclude it from the default run with a `~[slow]` filter rather than deleting coverage.

Note the `TND` constant comes from `symmetries.h` and means "this composition does not exist" — those pairs are skipped, not failed.

- [ ] **Step 4: Commit**

```bash
just build && just test
git add test/test_symmetries.cpp meson.build
git commit -m "test: cover transformation composition and symmetry groups

transAdd is checked as a genuine composition law: applying two
transformations in sequence must equal applying their composition
directly, across every grid. Rotations are checked to preserve material
and shape identity, a solid cube to be symmetric under every
transformation, and a lopsided shape to have the trivial symmetry group.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01VGbetP1s9EhUwpC96TKiht"
```

---

### Task 6: Grouping tests

**Files:**
- Create: `test/test_grouping.cpp`
- Modify: `meson.build:376`

**Interfaces:**
- Consumes: nothing from earlier tasks
- Produces: nothing later tasks depend on

- [ ] **Step 1: Read the grouping API before writing**

```bash
sed -n '40,125p' src/lib/grouping.h
sed -n '1,150p' src/lib/grouping.cpp
```

`grouping_c` has `addPieces(unsigned pc, unsigned group, unsigned count)`, `reSet()`, `newSet()`, and `addPieceToSet(unsigned pc)` returning `bool`. Confirm the semantics of `newSet` versus `reSet` from the implementation before asserting on them — the names are close and the header comments are terse.

- [ ] **Step 2: Write the file**

```cpp
#include <catch2/catch_test_macros.hpp>

#include "lib/grouping.h"

TEST_CASE("grouping: a piece in no group is always accepted", "[grouping]") {
  grouping_c g;

  g.newSet();
  /* a piece that was never registered belongs to no group and so has no
     capacity limit to violate */
  REQUIRE(g.addPieceToSet(0));
  REQUIRE(g.addPieceToSet(0));
}

TEST_CASE("grouping: a group accepts pieces up to its count", "[grouping]") {
  grouping_c g;

  /* piece 0 may appear twice within group 1 */
  g.addPieces(0, 1, 2);

  g.newSet();
  REQUIRE(g.addPieceToSet(0));
  REQUIRE(g.addPieceToSet(0));
}

TEST_CASE("grouping: a new set starts the counting over", "[grouping]") {
  grouping_c g;

  g.addPieces(0, 1, 1);

  g.newSet();
  REQUIRE(g.addPieceToSet(0));

  /* a fresh set must not inherit the previous set's usage */
  g.newSet();
  REQUIRE(g.addPieceToSet(0));
}

TEST_CASE("grouping: reSet clears everything back to the start", "[grouping]") {
  grouping_c g;

  g.addPieces(0, 1, 1);

  g.newSet();
  REQUIRE(g.addPieceToSet(0));

  g.reSet();
  g.newSet();
  REQUIRE(g.addPieceToSet(0));
}

TEST_CASE("grouping: a piece can belong to more than one group", "[grouping]") {
  grouping_c g;

  g.addPieces(0, 1, 1);
  g.addPieces(0, 2, 1);

  g.newSet();
  /* with capacity in two groups the piece can be placed twice */
  REQUIRE(g.addPieceToSet(0));
  REQUIRE(g.addPieceToSet(0));
}
```

- [ ] **Step 3: Add to the build and run**

Add `'test/test_grouping.cpp'` to `meson.build:376`, then:

```bash
just build && ./build/test_burrtools "[grouping]"
```

Expected: pass. `grouping_c` is a constraint solver over piece-to-group assignment, so if a case fails, re-read `grouping.cpp` and correct the **test's** expectation — the assertions above encode an intent that the implementation is the authority on. Report any case where the implementation looks genuinely wrong rather than merely different.

- [ ] **Step 4: Commit**

```bash
just build && just test
git add test/test_grouping.cpp meson.build
git commit -m "test: cover grouping capacity and set lifecycle

grouping_c had no direct tests; it was exercised only through full solver
runs on puzzles that happen to use piece groups.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01VGbetP1s9EhUwpC96TKiht"
```

---

### Task 7: Measure, verify, and open the PR

**Files:**
- Modify: none

**Interfaces:**
- Consumes: everything above, plus `just coverage` from PR 0
- Produces: the merged-ready `coverage/voxel-symmetry` branch

- [ ] **Step 1: Confirm the orphaned files are gone**

```bash
ls src/lib/*_test.cpp src/lib/main_test.cpp 2>&1
```

Expected: "No such file or directory". All three Boost files must be deleted by now.

- [ ] **Step 2: Run the full verification suite**

```bash
just build && just test && just check
```

Expected: build clean, all tests pass, cppcheck silent. All three are required by AGENTS.md rule 6.

- [ ] **Step 3: Measure coverage**

```bash
just coverage
```

Record the TOTAL. Compare against the PR 0 baseline; the delta is what goes in the PR body.

- [ ] **Step 4: Confirm no production code changed**

```bash
git diff master --stat -- src/
```

Expected: only deletions of the three `*_test.cpp` files. Any other `src/` change violates the no-production-changes constraint and must be moved to its own PR.

- [ ] **Step 5: Push and open the PR**

Confirm with the user before pushing.

```bash
git push -u origin coverage/voxel-symmetry
gh pr create --base coverage/tooling --head coverage/voxel-symmetry \
  --title "Unit tests for voxel spaces, symmetries, bitfield and grouping" \
  --body "$(cat <<'EOF'
## What

Adds unit-level tests for four units that previously had none of their own:
voxel spaces, transformations and symmetry groups, `bitfield_c`, and
`grouping_c`. Until now these were exercised only indirectly, through full
puzzle solves.

Also deletes `src/lib/bitfield_test.cpp`, `src/lib/voxel_0_test.cpp` and
`src/lib/main_test.cpp` — Boost.Test files that were never added to
`meson.build` and so have not compiled or run in years. Their assertions
are ported to Catch2 first.

## Coverage

- Before: **<PR 0 baseline>%**
- After: **<measured in step 3>%**

## Highlights

- `transAdd` is tested as a real composition law: applying two
  transformations in sequence must equal applying their composition
  directly, checked exhaustively across all five grids.
- `getIndex`/`indexToXYZ` are tested as exact inverses over whole spaces
  and as a bijection.
- Connectivity is checked on solid, L-shaped, gapped, diagonal and
  multi-layer fixtures, so both verdicts are exercised.
- `fillHoles` is checked against a shape open in z (must not fill) and a
  sealed shell (must fill the trapped centre).

## Notes

No production code changes. Stacked on #<PR 0 number>.

Design: `docs/superpowers/specs/2026-09-13-test-coverage-stack-design.md`

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_01VGbetP1s9EhUwpC96TKiht
EOF
)"
```

Replace the three placeholders (both coverage numbers and the PR 0 number) with real values before running.

---

## Self-Review

**Spec coverage.** The spec's PR 1 section lists: porting and deleting the three Boost files (Tasks 1, 2), `test_helpers.h` (Task 2), transformation composition and `normalizeTransformation` (Task 5), `identicalWithRots` and `getMirrorTransform` (Task 5 covers `identicalWithRots`; `getMirrorTransform` is **not** covered — see gap below), symmetry group membership (Task 5), `getIndex`/`indexToXYZ` roundtrip (Task 3), `get2` out-of-range (Task 2), bounding box after translate/resize/minimizePiece (Task 3), connectivity (Task 4), `fillHoles` (Task 4), `countState`/`count` (Tasks 2, 4), hotspot (**gap**), `scale`/`scaleDown` (**gap**), `operator==` and `identicalInBB` (Tasks 3, 5), `grouping_c` (Task 6).

Three spec items have no task: `getMirrorTransform`, hotspot defaults and `setHotspot`, and the `scale`/`scaleDown` roundtrip. Rather than pad the plan with speculative code for APIs I have not read closely, these are folded into Task 3 as an explicit addendum below.

**Placeholder scan.** The PR body in Task 7 carries three intentional placeholders that cannot be known until the steps run; Step 5 flags them. No other placeholders.

**Type consistency.** `bttest::makeVoxel`, `fromLayers`, `ALL_GRIDS`, and `gridName` are defined once in Task 2 and used with identical signatures in Tasks 3, 4, and 5. `voxel_c::VX_EMPTY` / `VX_FILLED` / `VX_VARIABLE` are used consistently. `gt.getVoxel(ptr)` is the copy-construction form throughout.

### Task 3 addendum: hotspot, scale, and mirror transform

Append to `test/test_voxel.cpp` during Task 3, after reading `src/lib/voxel.cpp` for `setHotspot`, `scale`, `scaleDown`, and `getMirrorTransform`:

```cpp
TEST_CASE("voxel: the hotspot defaults to the origin and moves when set", "[voxel][hotspot]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  std::unique_ptr<voxel_c> v = makeVoxel(gt, 3, 3, 3);

  REQUIRE(v->getHx() == 0);
  REQUIRE(v->getHy() == 0);
  REQUIRE(v->getHz() == 0);

  v->setHotspot(1, 2, 0);

  REQUIRE(v->getHx() == 1);
  REQUIRE(v->getHy() == 2);
  REQUIRE(v->getHz() == 0);
}

TEST_CASE("voxel: scaling up multiplies the filled voxel count", "[voxel][scale]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "#." },
  });

  const unsigned int before = v->countState(voxel_c::VX_FILLED);
  REQUIRE(before == 1);

  v->scale(2, false);

  /* each voxel becomes a 2x2x2 block */
  REQUIRE(v->countState(voxel_c::VX_FILLED) == before * 8);
}

TEST_CASE("voxel: scaling up then down is the identity", "[voxel][scale]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  std::unique_ptr<voxel_c> original = fromLayers(gt, {
    { "##",
      "#." },
  });
  std::unique_ptr<voxel_c> roundtrip(gt.getVoxel(original.get()));

  roundtrip->scale(2, false);
  REQUIRE(roundtrip->scaleDown(2, true));

  REQUIRE(roundtrip->countState(voxel_c::VX_FILLED)
          == original->countState(voxel_c::VX_FILLED));
  REQUIRE(roundtrip->identicalInBB(original.get()));
}

TEST_CASE("voxel: scaleDown refuses a shape that does not divide evenly", "[voxel][scale]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* a single voxel cannot be halved */
  std::unique_ptr<voxel_c> v = fromLayers(gt, {
    { "#" },
  });

  REQUIRE_FALSE(v->scaleDown(2, false));
}

TEST_CASE("voxel: getMirrorTransform finds the mirror of a chiral shape", "[voxel][mirror]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* an S/Z tetromino pair: chiral in the plane, so one is the other's mirror */
  std::unique_ptr<voxel_c> s = fromLayers(gt, {
    { ".##",
      "##." },
  });
  std::unique_ptr<voxel_c> z = fromLayers(gt, {
    { "##.",
      ".##" },
  });

  const unsigned char tr = s->getMirrorTransform(z.get());

  /* 0 means "no mirror transformation relates these two" */
  INFO("mirror transform " << (int)tr);
  REQUIRE(tr != 0);

  /* a shape is not its own mirror unless it is achiral */
  std::unique_ptr<voxel_c> square = fromLayers(gt, {
    { "##",
      "##" },
  });
  REQUIRE(square->getMirrorTransform(square.get()) != 0);
}
```

Run with `./build/test_burrtools "[hotspot],[scale],[mirror]"`. If `scale`'s `grid` parameter or `getMirrorTransform`'s zero convention differs from the assumptions here, read the implementation and correct the **test**, not `src/lib`.
