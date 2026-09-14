#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

#include "lib/gridtype.h"
#include "lib/problem.h"
#include "lib/puzzle.h"
#include "tools/gzstream.h"
#include "tools/xml.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

namespace {

/* a unique scratch directory, removed when the guard goes out of scope, so
   no test ever leaves a file behind in the source tree */
class TempDir {
public:
  TempDir() {
    path_ = std::filesystem::temp_directory_path() /
            ("burrtools-test-" + std::to_string(counter()++) + "-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(path_);
  }
  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  std::string file(const char * name) const { return (path_ / name).string(); }

private:
  static int & counter() { static int n = 0; return n; }
  std::filesystem::path path_;
};

} // namespace

TEST_CASE("gzstream: a plain text file is read back unchanged", "[gzstream]") {
  TempDir dir;
  const std::string name = dir.file("plain.txt");

  {
    std::ofstream out(name);
    out << "<puzzle/>";
  }

  std::unique_ptr<std::istream> in(openGzFile(name.c_str()));
  REQUIRE(in != nullptr);

  std::ostringstream got;
  got << in->rdbuf();
  REQUIRE(got.str() == "<puzzle/>");
}

TEST_CASE("gzstream: a gzipped file is transparently decompressed", "[gzstream]") {
  TempDir dir;
  const std::string name = dir.file("compressed.gz");

  {
    ogzstream out(name.c_str());
    REQUIRE(out.good());
    out << "<puzzle/>";
  }

  /* openGzFile must detect the gzip magic and decompress, so the caller sees
     the same bytes it would from a plain file */
  std::unique_ptr<std::istream> in(openGzFile(name.c_str()));
  REQUIRE(in != nullptr);

  std::ostringstream got;
  got << in->rdbuf();
  REQUIRE(got.str() == "<puzzle/>");
}

TEST_CASE("gzstream: compressing then reading back preserves a longer payload", "[gzstream]") {
  TempDir dir;
  const std::string name = dir.file("longer.gz");

  /* long enough and repetitive enough that compression genuinely engages,
     which is asserted directly below rather than just implied by the
     comment */
  std::string payload;
  for (int i = 0; i < 500; i++)
    payload += "<voxel>0123456789</voxel>";

  {
    ogzstream out(name.c_str());
    REQUIRE(out.good());
    out << payload;
  }

  /* proof that compression actually ran, not just that the roundtrip
     produced the right bytes: on disk this 12500-byte payload measures
     92 bytes, so require the file be far smaller than the payload rather
     than merely smaller */
  REQUIRE(std::filesystem::file_size(name) < payload.size() / 10);

  std::unique_ptr<std::istream> in(openGzFile(name.c_str()));
  REQUIRE(in != nullptr);

  std::ostringstream got;
  got << in->rdbuf();
  REQUIRE(got.str() == payload);
}

TEST_CASE("gzstream: a missing file is silently treated as an empty document (BUG)", "[gzstream]") {
  /* openGzFile(name) constructs `new igzstream(name)`. igzstream inherits from
     both gzstreambase and std::istream. gzstreambase's constructor correctly
     sets badbit when gzopen() fails to find the file, but igzstream's own
     constructor runs std::istream(&buf) *after* that, and that base
     constructor calls ios::init(&buf), which resets rdstate() to goodbit
     because the streambuf pointer is non-null. The badbit set moments
     earlier is silently wiped out.
     The net effect, confirmed here, is that openGzFile on a nonexistent
     path returns a non-null stream that reports good()/fail()/bad()/eof()
     as if nothing were wrong, both before and after an attempted read, and
     that read produces zero bytes. Every caller in the application that
     does `openGzFile(path)` then treats a *missing* puzzle file exactly
     like a *valid, empty* one -- there is no way for the caller to detect
     the file did not exist by checking stream state. This is a genuine,
     pre-existing BurrTools bug in the vendored gzstream wrapper's
     interaction with the igzstream multiple-inheritance layout, not a
     mistake in this test.

     Two things make it worse than a construction-order curiosity:

     1. The documented fallback for this situation never runs. openGzFile
        (gzstream.cpp:161-165) reads:
          igzstream * gz = new igzstream(name);
          if (!gz) { delete gz; return new std::ifstream(name); }
        `new` either returns a valid pointer or throws std::bad_alloc; it
        never returns null. So `!gz` is always false and the plain-
        std::ifstream fallback is dead code that can never execute -- the
        badbit-loses-to-goodbit path above is the only one ever taken.

     2. Real call sites have no guard and no recovery. src/burrTxt.cpp:203
        and src/burrTxt2.cpp:115 both call openGzFile(args[filenumber])
        directly, with no fileExists() check first (AGENTS.md notes
        src/tools/ already has file-existence helpers -- unused here) and
        no try/catch around the xmlParser_c/puzzle_c construction that
        follows. A mistyped filename does not report "missing"; it is read
        as an empty document, fails XML parsing instead, and that
        exception propagates out of main() uncaught, aborting the CLI
        tool. */
  TempDir dir;
  const std::string name = dir.file("does-not-exist.xmpuzzle");

  std::unique_ptr<std::istream> in(openGzFile(name.c_str()));
  REQUIRE(in != nullptr);

  // Documents the bug: the stream claims to be perfectly fine...
  REQUIRE(in->good());

  std::ostringstream got;
  got << in->rdbuf();

  // ...yet a read from the nonexistent file silently yields nothing, and the
  // stream still reports itself as good afterwards.
  REQUIRE(got.str().empty());
  REQUIRE(in->good());
}

TEST_CASE("puzzle: an in-memory puzzle survives a save and reload", "[roundtrip]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* build a puzzle with enough variety that a dropped field shows up: two
     shapes of different sizes, two colours (so the colour loop actually
     runs and can actually mismatch), a comment with its popup flag set
     (the popup attribute is only written when the comment is non-empty,
     puzzle.cpp:211-216), a non-zero per-voxel colour, and a problem */
  puzzle_c original(new gridType_c(gridType_c::GT_BRICKS));

  original.setComment("a test puzzle");
  original.setCommentPopup(true);
  original.addColor(10, 20, 30);
  original.addColor(40, 50, 60);

  std::unique_ptr<voxel_c> lShape = bttest::fromLayers(gt, {
    { "##",
      "#." },
  });
  lShape->setColor(0, 0, 0, 1);
  std::unique_ptr<voxel_c> bar = bttest::fromLayers(gt, {
    { "###" },
  });

  original.addShape(original.getGridType()->getVoxel(*lShape));
  original.addShape(original.getGridType()->getVoxel(*bar));

  const unsigned int prob = original.addProblem();
  original.getProblem(prob)->setName("a problem");

  /* save to a string, reload from that same string */
  std::ostringstream saved;
  {
    xmlWriter_c xml(saved);
    original.save(xml);
  }

  REQUIRE(saved.str().size() > 0);

  std::istringstream reloaded(saved.str());
  xmlParser_c pars(reloaded);
  puzzle_c restored(pars);

  REQUIRE(bttest::puzzlesRoundtripEqual(original, restored));
}

TEST_CASE("puzzle: saving a reloaded puzzle reproduces the same document", "[roundtrip]") {
  gridType_c gt(gridType_c::GT_BRICKS);

  /* this is deliberately the richest fixture in the file, not the
     thinnest: the byte-fixpoint check below compares the SERIALIZED
     document, so unlike puzzlesRoundtripEqual it covers every field the save
     format persists. Pairing it with a thin fixture would leave most of
     that extra strength unused, so this fixture exercises fields
     puzzlesRoundtripEqual does not look at (shape name, weight, hotspot,
     per-voxel colour, and a problem's result id, part min/max, maxHoles
     and a colour placement constraint) -- see the direct assertions after
     the fixpoint check, which confirm those fields specifically survive
     the roundtrip rather than merely happening not to break it. */
  puzzle_c original(new gridType_c(gridType_c::GT_BRICKS));
  original.setComment("fixpoint");
  original.setCommentPopup(true);
  original.addColor(10, 20, 30);
  original.addColor(40, 50, 60);

  std::unique_ptr<voxel_c> shape = bttest::fromLayers(gt, {
    { "#.",
      ".#" },
  });
  const unsigned int shapeIdx = original.addShape(original.getGridType()->getVoxel(*shape));

  /* gridType_c::getVoxel(const voxel_c &) above does not copy a shape's
     name. voxel_c has two copy constructors and BOTH omit `name` from
     their initializer list: the reference form (voxel.cpp:116-117) and
     the pointer form (voxel.cpp:141-142) -- both carry gt, sx, sy, sz,
     voxels, hx, hy, hz and weight, but not name. So a name set on the
     `shape` local before addShape would be silently lost before save/load
     ever runs. Set the mutable shape properties on the puzzle's own
     stored copy instead.

     This is reachable and user-visible today, not just an internal
     roundtrip footgun: cb_CopyShape() (src/gui/mainwindow.cpp:238) copies
     a shape through exactly this path when the user presses the GUI's
     "Copy" button, and PieceSelector::getText()
     (src/gui/BlockList.cpp:330-331) renders a shape's name in the piece
     list -- so naming a shape and clicking Copy produces a copy that
     shows up in the list with no name. No pinning test is added for this
     here; it belongs with voxel_c's own tests. */
  voxel_c * ownShape = original.getShape(shapeIdx);
  ownShape->setName("fixpoint shape");
  ownShape->setWeight(7);
  ownShape->setHotspot(1, 1, 0);
  ownShape->setColor(0, 0, 0, 1);

  const unsigned int prob = original.addProblem();
  original.getProblem(prob)->setName("fixpoint problem");
  original.getProblem(prob)->setResultId(shapeIdx);
  original.getProblem(prob)->setShapeMinimum(shapeIdx, 1);
  original.getProblem(prob)->setShapeMaximum(shapeIdx, 2);
  original.getProblem(prob)->setMaxHoles(3);
  original.getProblem(prob)->allowPlacement(1, 2);

  std::ostringstream first;
  {
    xmlWriter_c xml(first);
    original.save(xml);
  }

  std::istringstream in(first.str());
  xmlParser_c pars(in);
  puzzle_c restored(pars);

  std::ostringstream second;
  {
    xmlWriter_c xml(second);
    restored.save(xml);
  }

  /* save->load->save is a fixpoint: the second document must be byte-identical
     to the first, which catches fields that survive loading but are written
     back differently */
  REQUIRE(second.str() == first.str());

  /* puzzlesRoundtripEqual does not check any of the following (see its doc comment
     in test_helpers.h), so assert them directly to demonstrate they really
     do survive the roundtrip rather than merely riding along unexamined
     inside the byte-identical documents above. */
  const voxel_c * restoredShape = restored.getShape(shapeIdx);
  REQUIRE(restoredShape->getName() == "fixpoint shape");
  REQUIRE(restoredShape->getWeight() == 7);
  int hx = 0, hy = 0, hz = 0;
  REQUIRE(restoredShape->getHotspot(0, &hx, &hy, &hz));
  CHECK(hx == 1);
  CHECK(hy == 1);
  CHECK(hz == 0);
  REQUIRE(restoredShape->getColor(0, 0, 0) == 1);

  const problem_c * restoredProblem = restored.getProblem(0);
  REQUIRE(restoredProblem->resultValid());
  REQUIRE(restoredProblem->getResultId() == shapeIdx);
  REQUIRE(restoredProblem->getShapeMinimum(shapeIdx) == 1);
  REQUIRE(restoredProblem->getShapeMaximum(shapeIdx) == 2);
  REQUIRE(restoredProblem->maxHolesDefined());
  REQUIRE(restoredProblem->getMaxHoles() == 3);
  REQUIRE(restoredProblem->placementAllowed(1, 2));
}

TEST_CASE("puzzle: a roundtrip through a gzipped file preserves the puzzle", "[roundtrip][gzstream]") {
  TempDir dir;
  const std::string name = dir.file("roundtrip.xmpuzzle");

  gridType_c gt(gridType_c::GT_BRICKS);

  puzzle_c original(new gridType_c(gridType_c::GT_BRICKS));
  original.setComment("through a file");
  std::unique_ptr<voxel_c> shape = bttest::fromLayers(gt, {
    { "##",
      "##" },
  });
  original.addShape(original.getGridType()->getVoxel(*shape));

  {
    ogzstream out(name.c_str());
    REQUIRE(out.good());
    xmlWriter_c xml(out);
    original.save(xml);
  }

  std::unique_ptr<std::istream> in(openGzFile(name.c_str()));
  REQUIRE(in != nullptr);

  xmlParser_c pars(*in);
  puzzle_c restored(pars);

  REQUIRE(bttest::puzzlesRoundtripEqual(original, restored));
}

TEST_CASE("puzzle: the roundtrip holds on every grid", "[roundtrip]") {
  for (gridType_c::gridType t : bttest::ALL_GRIDS) {
    INFO("grid " << bttest::gridName(t));

    gridType_c gt(t);
    puzzle_c original(new gridType_c(t));

    /* filled cells at (0,0,0),(2,0,0),(1,1,0),(0,2,0),(2,2,0): all have an
       even x+y+z. Note this places filled cells at coordinates invalid for
       GT_RHOMBIC (voxel_3_c::validCoordinate wants one of x,y,z == 1 mod
       5, voxel_3.cpp:515-529); that is harmless for this byte roundtrip
       because nothing in the save/load path consults validCoordinate --
       setState does not filter, voxel_c::save writes all getXYZ() cells,
       and the loader reads all of them back unfiltered. */
    std::unique_ptr<voxel_c> shape = bttest::fromLayers(gt, {
      { "#.#",
        ".#.",
        "#.#" },
    });
    original.addShape(original.getGridType()->getVoxel(*shape));

    std::ostringstream saved;
    {
      xmlWriter_c xml(saved);
      original.save(xml);
    }

    std::istringstream in(saved.str());
    xmlParser_c pars(in);
    puzzle_c restored(pars);

    REQUIRE(bttest::puzzlesRoundtripEqual(original, restored));
  }
}

TEST_CASE("puzzlesRoundtripEqual detects a result that went from unset to set", "[roundtrip][helper]") {
  /* Every other case in this file calls puzzlesRoundtripEqual expecting
     true, so a comparator that degraded toward "always true" would still
     turn every one of them green. This case and the one below are the
     standing negative evidence for problem_c::resultValid() and
     getResultId(), the newest comparison in the helper and the only one
     with guard logic (getResultId() asserts resultValid()).

     This case alone only pins the resultValid() line -- both puzzles
     have a problem, and the only difference is whether a result shape
     was ever set, so `pa->resultValid() != pb->resultValid()` is what
     makes the comparison return false here; the mismatch never reaches
     the getResultId() equality check that follows it. See the next case
     for evidence that pins that line instead. */
  gridType_c gt(gridType_c::GT_BRICKS);

  auto build = [&](bool setResult) {
    auto p = std::make_unique<puzzle_c>(new gridType_c(gridType_c::GT_BRICKS));
    std::unique_ptr<voxel_c> shape = bttest::fromLayers(gt, {
      { "##" },
    });
    const unsigned int shapeIdx = p->addShape(p->getGridType()->getVoxel(*shape));
    const unsigned int prob = p->addProblem();
    p->getProblem(prob)->setName("result-diff problem");
    if (setResult)
      p->getProblem(prob)->setResultId(shapeIdx);
    return p;
  };

  std::unique_ptr<puzzle_c> withoutResult = build(false);
  std::unique_ptr<puzzle_c> withResult = build(true);

  REQUIRE_FALSE(withoutResult->getProblem(0)->resultValid());
  REQUIRE(withResult->getProblem(0)->resultValid());

  REQUIRE_FALSE(bttest::puzzlesRoundtripEqual(*withoutResult, *withResult));
}

TEST_CASE("puzzlesRoundtripEqual detects two different valid results", "[roundtrip][helper]") {
  /* Complements the case above: here resultValid() is true on BOTH sides,
     so that line of the comparison agrees and the only way the helper
     can return false is by actually evaluating getResultId() on the two
     puzzles and finding they differ. Each puzzle gets two shapes so the
     result id has more than one legal value to point at. */
  gridType_c gt(gridType_c::GT_BRICKS);

  auto build = [&](unsigned int resultShape) {
    auto p = std::make_unique<puzzle_c>(new gridType_c(gridType_c::GT_BRICKS));
    std::unique_ptr<voxel_c> shapeA = bttest::fromLayers(gt, {
      { "##" },
    });
    std::unique_ptr<voxel_c> shapeB = bttest::fromLayers(gt, {
      { "#" },
    });
    p->addShape(p->getGridType()->getVoxel(*shapeA));
    p->addShape(p->getGridType()->getVoxel(*shapeB));
    const unsigned int prob = p->addProblem();
    p->getProblem(prob)->setName("result-diff problem");
    p->getProblem(prob)->setResultId(resultShape);
    return p;
  };

  std::unique_ptr<puzzle_c> resultZero = build(0);
  std::unique_ptr<puzzle_c> resultOne = build(1);

  REQUIRE(resultZero->getProblem(0)->resultValid());
  REQUIRE(resultOne->getProblem(0)->resultValid());
  REQUIRE(resultZero->getProblem(0)->getResultId() != resultOne->getProblem(0)->getResultId());

  REQUIRE_FALSE(bttest::puzzlesRoundtripEqual(*resultZero, *resultOne));
}
