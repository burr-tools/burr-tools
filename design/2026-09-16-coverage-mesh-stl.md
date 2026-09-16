# Mesh and STL Coverage — Design

**Date:** 2026-09-16
**Status:** Planned
**Parent:** [`2026-09-14-coverage-next-phase.md`](2026-09-14-coverage-next-phase.md), package P2
**Predecessors delivered:** P4 (error and write paths), P1 (grid parametrization)

The parent plan lists P2 as the one package that "deserves its own design
document rather than being started directly from this one". This is that
document. It exists because the package is 2,062 uncovered lines across
eleven files with no shared vocabulary for asserting anything about them,
and because the obvious way to test mesh code is the wrong way.

## Why this needs a design and the other packages did not

P4 and P1 were both *retrofits*: the code had a testable API and the job was
to call it. Here the difficulty is not reaching the code, it is knowing what
to assert once you have.

A mesh is a large, floating-point, order-sensitive artefact. The tempting
test is to record one and compare. The parent plan is explicit about why
not, and it is worth restating because the pressure to do it will be real
the first time a case is hard to write:

> Mesh output is verbose and sensitive to floating-point detail; a
> recorded-output test will either be brittle or will be "fixed" by
> re-recording, which silently ratifies whatever regression prompted it.

So the whole package rests on finding properties that are *checkable
without knowing the answer*. Most of this document is about what those are.

## The target

Measured on master at `5f07969b`, macOS/Apple Clang. Re-measure before
quoting; the parent plan explains why these numbers are toolchain-specific.

| Lines | File | Covered |
|---|---|---|
| 606 | `halfedge/modifiers.cpp` | 1% |
| 475 | `voxel_2_mesh.cpp` | 0% |
| 231 | `halfedge/polyhedron.cpp` | 44% |
| 211 | `halfedge/vector3.cpp` | 23% |
| 109 | `halfedge/vertex.cpp` | 20% |
| 97 | `halfedge/face.cpp` | 14% |
| 72 | `stl_2.cpp` | 0% |
| 70 | `stl.cpp` | 0% |
| 55 | `triangulate.h` | 74% |
| 40 | `stl_0.cpp` | 46% |
| 28 | `halfedge/volume.cpp` | 0% |

## What already exists to build on

Not nothing, and the existing work settles some vocabulary.

`test_cubemesh.cpp` tests the cube lookup mesher and already has two
helpers worth generalising: a `watertight()` check that counts directed
edges and requires each to appear exactly once with its reverse present,
and a signed `volume()` by the divergence theorem. Both operate on
`cubeMesh::mesh_s` — a flat vertex/index buffer — not on `Polyhedron`.

`test_minkmesh.cpp` covers the Minkowski chamfer mesher, including a seeded
`[stress]` case over random shapes.

`test_manifold_smoke.cpp` is three assertions confirming the vendored
Manifold library links and runs.

The gap is that none of them touch the **half-edge** library, which is
where 1,254 of the uncovered lines are.

## The central asset: `Polyhedron::check()` already exists

`polyhedron.cpp` ships a `check(bool holesFilled)` that walks every vertex,
half-edge and face and asserts the structure's invariants: indices agree
with positions in their arrays, every half-edge's face back-pointer is
consistent, face loops close, no two adjacent faces are both holes.
`Vertex::check()` and `HalfEdge::check()` do the same locally.

This is a large part of the package's oracle, already written, currently
almost never called. It has one property that shapes how it must be used:
**it is built from `bt_assert`, so it throws `assert_exception` on failure
and compiles to nothing under `NDEBUG`.** Every case that relies on it
therefore needs the `#ifndef NDEBUG` guard the parent plan's conventions
section describes, or it will pass vacuously in a release build.

That is a trap worth stating plainly, because a test that calls `check()`
and asserts nothing else is exactly the kind that looks thorough and, under
`NDEBUG`, tests nothing at all.

## Approach, file by file

### `vector3.cpp` — 211 lines, the cheapest real coverage in the repository

Pure arithmetic on a template class: dot and cross products, normalisation,
spherical conversion, rotation about each axis, projection onto a plane or
line, distance in several metrics. Small, total functions with no state.

These get tested directly and exhaustively, against identities rather than
computed constants wherever one exists:

- `a ^ b` is perpendicular to both `a` and `b`, and `b ^ a == -(a ^ b)`.
- `a * (b ^ c)` is the signed volume and is invariant under cyclic
  permutation of the three.
- `normalize()` preserves direction and yields module 1 — and the zero
  vector is the case to check explicitly, because it is the one that
  divides by zero.
- `setSpheric` followed by `getSpheric` returns the angles put in, modulo
  the wrapping the implementation chooses. Which wrapping it chooses is a
  decision to *discover and pin*, not to assume.
- `rotateX/Y/Z` by an angle then by its negation is the identity;
  rotating four times by 90° is the identity; rotation preserves module.
- `project(n)` lands in the plane and `v - project(v)` is parallel to `n`.
- `closestPointInLine` is on the line, and the vector to it is
  perpendicular to the line's direction.

Floating point means comparisons need a tolerance. Catch2's `Approx` is
already in use in `test_disassembly.cpp`; the same convention carries over.
Choose the tolerance once, in a shared helper, and say what it is relative
to — an absolute epsilon on values of wildly different magnitude is a
source of tests that pass or fail depending on the fixture's scale.

**Expected yield:** 150–200 lines. **Cost:** low. This is the package's
warm-up and should be done first: it needs no mesh fixtures at all.

### `polyhedron.cpp`, `vertex.cpp`, `face.cpp`, `halfedge.cpp` — the data structure

668 lines between them. The structure's invariants are testable without
golden files, which is the whole reason this package is tractable:

- every half-edge has a twin, and twins are mutual (`he->twin()->twin() == he`);
- `he->next()` walked around a face returns to `he` in exactly the face's
  edge count;
- `he->src() == he->twin()->dst()`, by construction, on every edge;
- the vertex circulator visits each incident edge once and returns;
- for a closed mesh, V - E/2 + F equals 2 for a topological sphere, and
  `numHoles()` is 0;
- `addFace` then `edge(from, to)` finds the half-edge just created, and
  `edge(to, from)` finds its twin;
- `erase(Face*)` leaves the structure consistent, which is precisely what
  `check()` is for.

The fixtures should be built by hand and be small enough to reason about: a
tetrahedron (4 vertices, 4 faces, 12 half-edges), a cube as 12 triangles, an
open square with a boundary, and one deliberately non-manifold construction
to pin what the library does with it rather than assuming it rejects it.

Build them once in a shared header, the way `test_helpers.h` serves the
voxel cases. The half-edge fixtures are the single most reusable thing this
package produces and every later file depends on them.

**Expected yield:** 300–400 lines. **Cost:** moderate, mostly in the
fixtures.

### `modifiers.cpp` — 606 lines at 1%, the largest single gap

Three operations: `scalePolyhedron`, `fillPolyhedronHoles`, and
`mergeCoplanarFaces`.

Each is checked against what it preserves or establishes, never against
recorded output:

- **`scalePolyhedron`** — scaling by `s` multiplies volume by `s³` and
  leaves the face and vertex counts untouched; scaling by `s` then by `1/s`
  returns the original coordinates within tolerance; the non-uniform
  overload scales each axis independently, which a uniform-only
  implementation would fail on an asymmetric fixture.
- **`fillPolyhedronHoles`** — afterwards `numHoles()` is 0 and
  `check(true)` passes. The fixture needs a mesh that genuinely has a hole,
  and the case must assert that it does *before* filling, or it will pass
  against a fixture that never had one. That is the specific "passes for
  the wrong reason" failure the parent plan warns about, and it is easy to
  walk into here.
- **`mergeCoplanarFaces`** — the header states the contract: fewer, larger
  triangles; all boundary vertices kept, so no t-junctions; faces marked as
  holes dropped; groups where retriangulation fails keep their original
  faces. So: volume is unchanged, face count does not increase, the result
  passes `check()`, and on a fixture with a known coplanar group (a cube's
  face split into four triangles) the count strictly decreases. The
  fall-back branch — retriangulation failing — needs a fixture that
  triggers it, and finding one may be the hardest single thing in this
  package. If it cannot be triggered, say so in place rather than leaving
  it looking covered.

**Expected yield:** 350–450 lines. **Cost:** high.

### `volume.cpp` — 28 lines at 0%

One function. It is also the measuring instrument several cases above rely
on, so it gets tested first and independently: a unit cube has volume 1, a
tetrahedron has the volume its determinant says, scaling by `s` multiplies
it by `s³`, and a mesh wound inside-out yields the negative. Test it
against hand-computed values, because here the constant genuinely is known.

**Expected yield:** 25 lines. **Cost:** trivial. Do it before `modifiers`.

### `stl.cpp`, `stl_0.cpp`, `stl_2.cpp` — 182 lines

The exporters. The parent plan is specific: check them by **parsing back
what they emit**, in both ASCII and binary form, rather than by byte
comparison.

So the package needs a small STL reader in the test support code — perhaps
60 lines, handling both encodings. That reader is itself the thing to be
careful about: it must be written from the format, not from the writer, or
the two will agree on a shared misunderstanding. Binary STL's 80-byte
header, little-endian triangle count and 50-byte records are worth reading
from the specification while writing it.

Then: triangle count matches the mesh, every triangle's vertices are finite,
the parsed-back normals agree with the winding, and ASCII and binary of the
same mesh parse to the same triangle set. `stlException_c` has error paths
for unwritable destinations, which a temp directory can reach.

**Expected yield:** 130–170 lines. **Cost:** moderate, concentrated in the
reader.

### `voxel_2_mesh.cpp` — 475 lines at 0%

The sphere grid's mesher. Left for last deliberately: it is the largest
single file, it is grid-specific, and P1 established that sphere-grid
fixtures must be built from coordinates the grid accepts rather than from
ASCII art. Reuse that.

Once a legal sphere-grid shape exists, the assertions are the mesh
invariants already established above — watertight, positive volume,
`check()` passes — applied to its output. Its own geometry does not need a
separate oracle.

**Expected yield:** 200–300 lines. **Cost:** moderate, given everything
before it is in place.

## Order

1. `vector3` — no fixtures needed, warms up the tolerance convention.
2. `volume` — 28 lines, and it is the instrument the later cases measure with.
3. The half-edge fixtures and the data structure cases — everything after
   this depends on them.
4. `modifiers`, which is where the mass is.
5. The STL reader and the three exporters.
6. `voxel_2_mesh`.

Each step is usable on its own, so the package can stop after any of them
and still have delivered something coherent.

## Risks specific to this package

**`check()` under `NDEBUG`.** Stated above and worth repeating: it is
`bt_assert`-based, so every case depending on it needs `#ifndef NDEBUG` or
it silently passes in a release build. A case whose *only* assertion is
`check()` is worth avoiding entirely for this reason — pair it with
something that holds in both build types.

**Tolerance chosen per case.** If each case picks its own epsilon, the
suite accumulates a set of magic numbers nobody can justify later. Choose
once, relative rather than absolute, in the shared helper.

**Fixtures that cannot fail.** The `fillPolyhedronHoles` case is the
obvious one — a fixture with no hole passes whatever the function does —
but it generalises. Every case here should be able to say what would have
to break for it to fail, and the cheap ones should be checked by actually
breaking the production code and watching them go red, as P4 and P1 did.

**Scope creep into Manifold.** `voxel_2_mesh` and the Minkowski mesher sit
next to the vendored Manifold library. Testing Manifold is not this
package's job; testing what BurrTools does with it is.

## What this package deliberately does not do

It does not touch the GUI-facing mesh consumers (`voxeldrawer`, the 3D
view), which sit outside the coverage filter entirely. It does not attempt
`triangulate.h`'s remaining 14 uncovered lines as a target in their own
right — they will move incidentally with `modifiers`. And it does not
convert the existing `test_cubemesh.cpp` and `test_minkmesh.cpp` cases to
the new fixtures; they work, and rewriting working tests to share a helper
is a refactor, not coverage.
