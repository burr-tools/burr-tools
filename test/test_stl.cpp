#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "stl_reader.h"
#include "test_helpers.h"

#include "lib/gridtype.h"
#include "lib/stl.h"
#include "lib/stl_0.h"
#include "lib/voxel.h"
#include "halfedge/polyhedron.h"
#include "halfedge/face.h"
#include "halfedge/volume.h"
#include "halfedge/modifiers.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <random>
#include <memory>
#include <set>
#include <string>

using Catch::Approx;
using namespace bttest;

/* The STL exporters: stl.cpp (70 lines, 0%), stl_0.cpp (40, 46%) and
   stl_2.cpp (72, 0%).

   Step five of the mesh/STL design doc, which is specific about the
   method: check the writers by PARSING BACK what they emit, in both ASCII
   and binary form, rather than by byte comparison against a fixture. The
   reader lives in stl_reader.h and is written from the format rather than
   from the writer, so the two cannot agree on a shared misunderstanding. */

namespace {

/* a scratch directory that cleans up after itself, following the pattern
   test_roundtrip.cpp established */
class TempDir {
public:
  TempDir() {
  /* Process-unique, and created exclusively rather than cleared.

     A process-local counter alone collides: two test binaries running at
     the same time both reach "bttest_stl_1", and the constructor used to
     remove_all() that path first -- so the second process would delete the
     first one's exports out from under it. Mixing in a per-process random
     token makes the name unique to this run, and create_directory's
     "did I create it, or was it already there?" answer drives a retry
     instead of destroying whatever it finds. */
    /* Kept short on purpose. The binary STL header is a fixed 80-byte
       field, and until the basename fix lands the exporter writes the whole
       path into it, so a temp path plus a long directory name stops fitting.
       Three-character tag plus eight hex digits is the same length as the
       "bttest_stl_" it replaces. */
    const std::string base = "bts" + processToken() + "_";

    for (int attempt = 0; attempt < 1000; attempt++) {
      std::filesystem::path candidate =
          std::filesystem::temp_directory_path() / (base + std::to_string(++counter()));

      std::error_code ec;
      if (std::filesystem::create_directory(candidate, ec)) {
        path_ = candidate;
        return;
      }
      if (ec)
        throw std::runtime_error("TempDir: " + ec.message());
      /* it already existed -- someone else owns it, so take the next name */
    }

    throw std::runtime_error("TempDir: no unique directory name was available");
  }
  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  std::string file(const char * name) const { return (path_ / name).string(); }

private:
  static int & counter() { static int n = 0; return n; }

  /* random rather than the pid, so this stays portable -- the suite is
     built on Windows too. Drawn once per process. */
  static const std::string & processToken() {
    static const std::string t = [] {
      std::random_device rd;
      char buf[16];
      std::snprintf(buf, sizeof buf, "%08x", static_cast<unsigned>(rd()));
      return std::string(buf);
    }();
    return t;
  }
  std::filesystem::path path_;
};

/** a single filled cube cell on the given grid */
std::unique_ptr<voxel_c> unitCell(const gridType_c & gt) {
  std::unique_ptr<voxel_c> v = makeVoxel(gt, 1, 1, 1);
  v->setState(0, 0, 0, voxel_c::VX_FILLED);
  return v;
}

/** a straight domino: two touching cube cells */
std::unique_ptr<voxel_c> domino(const gridType_c & gt) {
  std::unique_ptr<voxel_c> v = makeVoxel(gt, 2, 1, 1);
  v->setState(0, 0, 0, voxel_c::VX_FILLED);
  v->setState(1, 0, 0, voxel_c::VX_FILLED);
  return v;
}

/* a rounded comparison for coordinates, so a vertex written through
   "%9.4e" and read back can be matched to the binary float of the same
   vertex. The ASCII writer emits four decimal places of mantissa, which is
   the coarser of the two representations and therefore sets the tolerance. */
bool sameVertex(const float a[3], const float b[3]) {
  for (int i = 0; i < 3; i++)
    if (a[i] != Approx(b[i]).epsilon(1e-3).margin(1e-3)) return false;
  return true;
}

} // namespace

/* ------------------------------------------------------------------ */
/* the reader itself                                                   */
/* ------------------------------------------------------------------ */

TEST_CASE("stl reader: a hand-written ascii document parses to the triangle it describes",
          "[stl][reader]") {
  /* The reader is test support code, so it gets its own case rather than
     being trusted because the exporter cases pass. Written by hand from
     the format, with values chosen so a transposed coordinate would show. */
  const std::string doc =
    "solid demo\n"
    "  facet normal 0 0 1\n"
    "    outer loop\n"
    "      vertex 1 2 3\n"
    "      vertex 4 5 6\n"
    "      vertex 7 8 9\n"
    "    endloop\n"
    "  endfacet\n"
    "endsolid\n";

  StlMesh m = readAsciiStl(doc);

  REQUIRE(m.name == "demo");
  REQUIRE(m.triangles.size() == 1);

  REQUIRE(m.triangles[0].normal[2] == Approx(1.0));
  REQUIRE(m.triangles[0].vertex[0][0] == Approx(1.0));
  REQUIRE(m.triangles[0].vertex[0][1] == Approx(2.0));
  REQUIRE(m.triangles[0].vertex[2][2] == Approx(9.0));
}

TEST_CASE("stl reader: a truncated ascii document is rejected rather than yielding fewer triangles",
          "[stl][reader]") {
  /* A lenient reader would return zero triangles here, and every exporter
     case below compares triangle counts -- so leniency would turn a real
     failure into a passing test. */
  const std::string doc =
    "solid demo\n"
    "  facet normal 0 0 1\n"
    "    outer loop\n"
    "      vertex 1 2 3\n";

  REQUIRE_THROWS_AS(readAsciiStl(doc), std::runtime_error);
}

TEST_CASE("stl reader: a binary document shorter than its own triangle count is rejected",
          "[stl][reader]") {
  std::string doc(84, '\0');
  const uint32_t count = 5;
  std::memcpy(&doc[80], &count, 4);   // claims five triangles, carries none

  REQUIRE_THROWS_AS(readBinaryStl(doc), std::runtime_error);

  /* and a genuinely empty one, claiming zero, is fine */
  std::string empty(84, '\0');
  REQUIRE(readBinaryStl(empty).triangles.empty());
}

/* ------------------------------------------------------------------ */
/* the cube exporter                                                   */
/* ------------------------------------------------------------------ */

TEST_CASE("stl export: an ascii file parses back to a non-empty triangle soup", "[stl]") {
  TempDir dir;
  const std::string path = dir.file("cell.stl");

  gridType_c gt(gridType_c::GT_BRICKS);
  std::unique_ptr<voxel_c> v = unitCell(gt);

  stlExporter_0_c exp;
  exp.setBinaryMode(false);
  exp.write(path.c_str(), *v);

  StlMesh m = readAsciiStl(slurp(path));

  /* the premise every later assertion rests on: something was written */
  REQUIRE(m.triangles.size() > 0);

  /* Every coordinate is a real number, and no triangle collapses to fewer
     than three distinct corners. NaN coordinates are what a mesher with a
     degenerate face produces and they survive both encodings silently.

     Both scanned to one assertion each rather than asserted per component
     and per triangle: the meshes here run to dozens of triangles and the
     per-element form buys no coverage, only assertion count. The INFO
     still names the offending triangle. */
  bool allFinite = true, allDistinct = true;
  size_t firstNonFinite = 0, firstDegenerate = 0;

  for (size_t t = 0; t < m.triangles.size(); t++) {
    const StlTriangle & tri = m.triangles[t];

    if (allFinite)
      for (int c = 0; c < 3; c++)
        for (int i = 0; i < 3; i++)
          if (!std::isfinite(tri.vertex[c][i])) { allFinite = false; firstNonFinite = t; }

    if (allDistinct &&
        (sameVertex(tri.vertex[0], tri.vertex[1]) ||
         sameVertex(tri.vertex[1], tri.vertex[2]) ||
         sameVertex(tri.vertex[0], tri.vertex[2]))) {
      allDistinct = false;
      firstDegenerate = t;
    }
  }

  INFO("first non-finite coordinate in triangle " << firstNonFinite);
  REQUIRE(allFinite);

  INFO("first degenerate triangle at " << firstDegenerate);
  REQUIRE(allDistinct);
}

TEST_CASE("stl export: the binary file carries the same triangles as the ascii one", "[stl]") {
  TempDir dir;
  const std::string ascii = dir.file("cell_ascii.stl");
  const std::string binary = dir.file("cell_binary.stl");

  gridType_c gt(gridType_c::GT_BRICKS);
  std::unique_ptr<voxel_c> v = unitCell(gt);

  {
    stlExporter_0_c exp;
    exp.setBinaryMode(false);
    exp.write(ascii.c_str(), *v);
  }
  {
    stlExporter_0_c exp;
    exp.setBinaryMode(true);
    exp.write(binary.c_str(), *v);
  }

  StlMesh a = readAsciiStl(slurp(ascii));
  StlMesh b = readBinaryStl(slurp(binary));

  REQUIRE(a.triangles.size() > 0);

  /* The two encodings are separate code paths in stlExporter_c::write --
     fprintf against fwrite -- fed from the same mesh. They must describe
     the same solid. Comparing the triangle COUNT and the total volume
     rather than the triangles pairwise, because the ASCII path writes four
     decimal places and the binary path writes full floats, so an exact
     pairwise match is not available and forcing one would mean recording
     output. */
  REQUIRE(b.triangles.size() == a.triangles.size());

  const double va = stlVolume(a);
  const double vb = stlVolume(b);

  /* a solid with real extent, so the comparison is not two zeroes */
  REQUIRE(std::fabs(vb) > 1.0);
  REQUIRE(va == Approx(vb).epsilon(1e-3));
}

TEST_CASE("stl export: the solid's volume is positive -- the triangles are wound outwards",
          "[stl]") {
  TempDir dir;
  const std::string path = dir.file("cell.stl");

  gridType_c gt(gridType_c::GT_BRICKS);
  std::unique_ptr<voxel_c> v = unitCell(gt);

  stlExporter_0_c exp;
  exp.setBinaryMode(true);
  exp.write(path.c_str(), *v);

  StlMesh m = readBinaryStl(slurp(path));

  /* Winding is the thing an STL consumer cares about and the thing a
     writer can invert without any other symptom -- the file still parses,
     the triangle count is right, and the model prints inside-out. The
     signed volume is the cheapest way to catch it. */
  REQUIRE(stlVolume(m) > 0.0);
}

TEST_CASE("stl export: a two-cell shape encloses more volume than a one-cell shape", "[stl]") {
  TempDir dir;

  gridType_c gt(gridType_c::GT_BRICKS);

  auto volumeOf = [&](const voxel_c & shape, const char * name) {
    const std::string path = dir.file(name);
    stlExporter_0_c exp;
    exp.setBinaryMode(true);
    exp.write(path.c_str(), shape);
    return stlVolume(readBinaryStl(slurp(path)));
  };

  const double one = volumeOf(*unitCell(gt), "one.stl");
  const double two = volumeOf(*domino(gt), "two.stl");

  /* The exporter must actually depend on the shape it is given. Without
     this every case above would hold against an exporter that emitted the
     same fixed solid whatever it was asked for.

     Not asserted as exactly double: the mesher bevels edges and merges
     coplanar faces, so two joined cells are not two separate ones. Only
     the direction is a property of the operation. */
  REQUIRE(two > one * 1.5);
}

TEST_CASE("stl export: the solid name is derived from the path -- and on this platform the "
          "derivation is wrong", "[stl][defect]") {
  TempDir dir;
  const std::string path = dir.file("named.stl");

  gridType_c gt(gridType_c::GT_BRICKS);
  std::unique_ptr<voxel_c> v = unitCell(gt);

  stlExporter_0_c exp;
  exp.setBinaryMode(false);
  exp.write(path.c_str(), *v);

  StlMesh m = readAsciiStl(slurp(path));

  /* True whichever basename is in play, and the part a consumer of the
     file actually relies on. Asserted first so the case still says
     something if the platform split below is ever removed. */
  REQUIRE(m.name.size() >= 9);
  REQUIRE(m.name.compare(m.name.size() - 9, 9, "named.stl") == 0);

#if defined(_WIN32) || defined(__APPLE__) || defined(EMSCRIPTEN)
  /* A defect, pinned rather than fixed.

     stl.cpp supplies its own basename() on Windows, macOS and Emscripten,
     because those platforms lack the POSIX one. It reads:

         const char * res1 = strchr(name, '/');
         const char * res2 = strchr(name, 0x5C);
         const char * res = res1>res2 ? res1 : res2;

     strchr finds the FIRST separator; a basename needs the LAST, which is
     strrchr. So for an absolute path the function returns everything after
     the leading slash -- the whole path bar one character -- rather than
     the file's name.

     The consequence is not entirely cosmetic: the name goes into the
     exported STL, so every file a macOS or Windows user exports carries
     most of their directory structure in its header. Binary STL's header
     is 80 bytes, so a deep path is also silently truncated part-way.

     It has gone unnoticed because CI runs on Linux, where the POSIX
     basename is used instead and is correct -- which is also why this case
     splits by platform rather than simply asserting the broken value.

     (The pointer comparison res1 > res2, on two pointers into different
     objects, is separately undefined, though it happens to give the right
     answer in the NULL cases. Noted for whoever fixes the strchr.)

     The expected value is computed from the first separator rather than
     written as path.substr(1). Those coincide only on a POSIX absolute
     path, where the separator IS character zero; on Windows the path
     begins "C:\\..." and the first separator is at index 2, so the
     hardcoded form asserted the wrong string and failed the Windows
     cross-build while passing on macOS. */
  const size_t firstSep = path.find_first_of("/\\");
  REQUIRE(firstSep != std::string::npos);
  REQUIRE(m.name == path.substr(firstSep + 1));
#else
  /* POSIX basename: correct */
  REQUIRE(m.name == "named.stl");
#endif
}

TEST_CASE("stl export: the binary header carries the name and the true triangle count", "[stl]") {
  TempDir dir;
  const std::string path = dir.file("counted.stl");

  gridType_c gt(gridType_c::GT_BRICKS);
  std::unique_ptr<voxel_c> v = unitCell(gt);

  stlExporter_0_c exp;
  exp.setBinaryMode(true);
  exp.write(path.c_str(), *v);

  const std::string raw = slurp(path);
  StlMesh m = readBinaryStl(raw);

  /* The same basename defect as above reaches the binary header too, so
     only the tail is portable here.

     That also makes this sensitive to how long the system temp path is:
     the binary header is a fixed 80-byte field, and the whole path is
     written into it, so a sufficiently long TMPDIR truncates the basename
     away. Say so directly rather than failing as a confusing string
     mismatch -- and note the sensitivity disappears once the exporter
     writes a basename rather than a path. */
  REQUIRE(path.size() <= 80);
  REQUIRE(m.name.size() >= 11);
  REQUIRE(m.name.compare(m.name.size() - 11, 11, "counted.stl") == 0);

  /* The count is written LAST, by seeking back to offset 80 once every
     triangle has been emitted. That seek-and-patch is the part that can go
     wrong -- a count left at zero, or written before the triangles -- and
     the file size is the independent check on it: 84 bytes of header plus
     50 per triangle, exactly. */
  REQUIRE(raw.size() == 84 + m.triangles.size() * 50);
}

TEST_CASE("stl export: writing to a path that cannot be opened raises rather than failing silently",
          "[stl]") {
  gridType_c gt(gridType_c::GT_BRICKS);
  std::unique_ptr<voxel_c> v = unitCell(gt);

  /* a directory that does not exist. #59 fixed a stream failure that
     crashed the command-line tools on a mistyped filename, so this path
     has a history. */
  const std::string bad = "/nonexistent-directory-for-bttest/out.stl";

  {
    stlExporter_0_c exp;
    exp.setBinaryMode(false);
    REQUIRE_THROWS_AS(exp.write(bad.c_str(), *v), stlException_c);
  }
  {
    /* the binary path opens the file separately, so it is a distinct
       branch and gets its own assertion */
    stlExporter_0_c exp;
    exp.setBinaryMode(true);
    REQUIRE_THROWS_AS(exp.write(bad.c_str(), *v), stlException_c);
  }
}

/* ------------------------------------------------------------------ */
/* the exporter parameter surface                                      */
/* ------------------------------------------------------------------ */

TEST_CASE("stl export: every parameter round-trips through its setter and reader", "[stl][params]") {
  stlExporter_0_c exp;

  REQUIRE(exp.numParameters() == 6);

  for (unsigned int i = 0; i < exp.numParameters(); i++) {
    INFO("parameter " << i << " (" << exp.getParameterName(i) << ")");

    /* every parameter is named and tooltipped -- an out-of-range index in
       either would show up as a null pointer here */
    REQUIRE(exp.getParameterName(i) != nullptr);
    REQUIRE(exp.getParameterTooltip(i) != nullptr);

    const double before = exp.getParameter(i);

    /* A value each parameter can plausibly take. Integer and boolean
       parameters are stored as doubles, so 1 is legal for all of them,
       and it differs from at least some of the defaults. */
    exp.setParameter(i, 1.0);
    REQUIRE(exp.getParameter(i) == Approx(1.0));

    exp.setParameter(i, before);
    REQUIRE(exp.getParameter(i) == Approx(before));
  }
}

TEST_CASE("stl export: changing the cube scale changes the size of the solid written",
          "[stl][params]") {
  TempDir dir;

  gridType_c gt(gridType_c::GT_BRICKS);
  std::unique_ptr<voxel_c> v = unitCell(gt);

  auto volumeAtScale = [&](double scale, const char * name) {
    stlExporter_0_c exp;
    exp.setBinaryMode(true);

    /* find the cube-size parameter by name rather than by a hardcoded
       index, so reordering the parameter list does not silently make this
       case test something else */
    bool found = false;
    for (unsigned int i = 0; i < exp.numParameters(); i++) {
      if (std::string(exp.getParameterName(i)) == "Unit Size") {
        exp.setParameter(i, scale);
        found = true;
        break;
      }
    }

    /* found by name rather than by a hardcoded index, so reordering the
       parameter list makes this case fail loudly instead of quietly
       testing a different parameter */
    REQUIRE(found);

    const std::string path = dir.file(name);
    exp.write(path.c_str(), *v);
    return stlVolume(readBinaryStl(slurp(path)));
  };

  const double small = volumeAtScale(10.0, "small.stl");
  const double big = volumeAtScale(20.0, "big.stl");

  /* doubling the unit size should multiply the volume by roughly eight.
     The bevel does not scale identically, so the assertion is a band
     rather than a point -- but a band narrow enough to exclude "the
     parameter did nothing" (ratio 1) and "it scaled linearly" (ratio 2). */
  REQUIRE(big > small * 6.0);
  REQUIRE(big < small * 10.0);
}

TEST_CASE("stl export: the coplanar merge the writer runs changes nothing for these meshers",
          "[stl]") {
  /* stlExporter_c::write runs mergeCoplanarFaces over the mesher's output
     before writing. This case exists to record that, for every shape and
     grid reachable from here, that step changes nothing at all.

     Measured over a single cell, an L-tromino and a plus pentomino, on the
     cube and triangular-prism grids: the merged mesh has exactly as many
     faces, and exactly as many triangles, as the mesher produced. Not
     "roughly" -- identically, in all six combinations.

     The function is not broken; test_modifiers.cpp shows it collapsing a
     four-triangle coplanar fan to two on a hand-built fixture. What these
     meshers emit simply has no group it can improve: every coplanar group
     is already at its minimal triangulation, and retriangulating a group of
     two triangles gives two back.

     Two consequences worth stating rather than leaving implicit.

     First, the merge costs a full polyhedron rebuild on every STL export
     and currently buys nothing. Whether the meshers changed underneath it
     or it was always redundant here is a question for a maintainer, not
     for a coverage change.

     Second, and the reason this case is written as an equality: it means
     deleting the merge call from stl.cpp does not fail any test in this
     file, and cannot be made to without a mesher that produces a mergeable
     group. Asserting the equality at least makes the situation visible --
     if a mesher ever starts emitting mergeable geometry, this goes red and
     someone finds out. */
  gridType_c cube(gridType_c::GT_BRICKS);
  gridType_c prism(gridType_c::GT_TRIANGULAR_PRISM);

  struct Shape { const char * name; int cells; };
  const Shape shapes[] = {
    { "single cell", 1 },
    { "L-tromino",   2 },
    { "plus",        3 },
  };

  for (const gridType_c * gt : { &cube, &prism })
    for (const Shape & sh : shapes) {
      INFO("grid " << gridName(gt->getType()) << ", shape " << sh.name);

      std::unique_ptr<voxel_c> v = makeVoxel(*gt, 3, 3, 1);
      v->setState(0, 0, 0, voxel_c::VX_FILLED);
      if (sh.cells >= 2) {
        v->setState(1, 0, 0, voxel_c::VX_FILLED);
        v->setState(0, 1, 0, voxel_c::VX_FILLED);
      }
      if (sh.cells >= 3) {
        v->setState(2, 0, 0, voxel_c::VX_FILLED);
        v->setState(0, 2, 0, voxel_c::VX_FILLED);
        v->setState(1, 1, 0, voxel_c::VX_FILLED);
      }

      stlExporter_0_c exp;
      std::unique_ptr<Polyhedron> raw(exp.getMesh(*v));
      REQUIRE(raw != nullptr);

      auto countTriangles = [](const Polyhedron & p) {
        int n = 0;
        for (Polyhedron::const_face_iterator it = p.fBegin(); it != p.fEnd(); ++it)
          if (!(*it)->hole()) n += (*it)->size() - 2;
        return n;
      };

      const int before = countTriangles(*raw);
      REQUIRE(before > 0);

      std::unique_ptr<Polyhedron> merged(mergeCoplanarFaces(*raw));
      REQUIRE(merged != nullptr);

      REQUIRE(countTriangles(*merged) == before);

      /* and the solid is unchanged, which is the merge's actual
         obligation whether or not it reduces anything */
      REQUIRE(volume(*merged) == Approx(volume(*raw)).epsilon(1e-3));
    }
}
