/* Tests for btui::DocumentSession: the redesigned GUI's new / load / save /
 * undo, driven with no GUI. Example files are read relative to the project
 * root, the test working directory.
 */
#include <catch2/catch_test_macros.hpp>

#include "../src/uicore/documentsession.h"

#include "../src/lib/puzzle.h"
#include "../src/lib/voxel.h"

#include <filesystem>
#include <fstream>
#include <random>
#include <string>

using btui::DocumentSession;

namespace {

  struct TempDir {
    std::filesystem::path path;
    TempDir() {
      std::random_device rd;
      path = std::filesystem::temp_directory_path() / ("bt_doc_" + std::to_string(rd()) + std::to_string(rd()));
      std::filesystem::create_directories(path);
    }
    ~TempDir() { std::error_code ec; std::filesystem::remove_all(path, ec); }
  };
}

TEST_CASE("a fresh session is an unmodified, unnamed brick puzzle", "[ui][document]") {
  DocumentSession d;
  CHECK(d.fileName().empty());
  CHECK_FALSE(d.isModified());
  CHECK_FALSE(d.canUndo());
  CHECK(d.puzzle().getGridType()->getType() == gridType_c::GT_BRICKS);
  CHECK(d.puzzle().getNumberOfShapes() == 0);
}

TEST_CASE("newDocument takes the chosen grid type and drops the name", "[ui][document]") {
  DocumentSession d;
  REQUIRE(d.load("examples/PelikanBurr.xmpuzzle").ok);
  d.newDocument(gridType_c::GT_SPHERES);
  CHECK(d.fileName().empty());
  CHECK(d.puzzle().getGridType()->getType() == gridType_c::GT_SPHERES);
  CHECK(d.puzzle().getNumberOfShapes() == 0);
  CHECK_FALSE(d.isModified());
}

TEST_CASE("loading an example adopts it and its name", "[ui][document]") {
  DocumentSession d;
  auto r = d.load("examples/PelikanBurr.xmpuzzle");
  REQUIRE(r.ok);
  CHECK(r.error.empty());
  CHECK(d.fileName() == std::filesystem::path("examples/PelikanBurr.xmpuzzle"));
  CHECK(d.puzzle().getNumberOfShapes() > 0);
  CHECK_FALSE(d.isModified());
  CHECK_FALSE(d.canUndo());
}

TEST_CASE("a failed load leaves the current document alone", "[ui][document]") {
  DocumentSession d;
  REQUIRE(d.load("examples/PelikanBurr.xmpuzzle").ok);
  unsigned int shapes = d.puzzle().getNumberOfShapes();

  auto r = d.load("examples/does-not-exist.xmpuzzle");
  CHECK_FALSE(r.ok);
  CHECK_FALSE(r.error.empty());
  CHECK(d.puzzle().getNumberOfShapes() == shapes);
  CHECK(d.fileName() == std::filesystem::path("examples/PelikanBurr.xmpuzzle"));

  // a malformed file reports the parser's message and changes nothing
  auto bad = d.load("test/malformed/oversized_dimensions.xmpuzzle");
  CHECK_FALSE(bad.ok);
  CHECK(bad.error.starts_with("load error: "));
  CHECK(d.puzzle().getNumberOfShapes() == shapes);
}

TEST_CASE("saveAs appends the extension, adopts the name and round-trips", "[ui][document]") {
  TempDir t;
  DocumentSession d;
  REQUIRE(d.load("examples/PelikanBurr.xmpuzzle").ok);
  d.puzzle().getShape(0)->setName("renamed");
  d.record(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);
  REQUIRE(d.isModified());

  REQUIRE(d.saveAs(t.path / "copy"));
  CHECK(d.fileName() == t.path / "copy.xmpuzzle");
  CHECK_FALSE(d.isModified());

  DocumentSession e;
  REQUIRE(e.load(t.path / "copy.xmpuzzle").ok);
  CHECK(e.puzzle().getShape(0)->getName() == "renamed");
  CHECK(e.puzzle().getNumberOfShapes() == d.puzzle().getNumberOfShapes());
}

TEST_CASE("save needs a name, and a failed save keeps the document modified", "[ui][document]") {
  DocumentSession d;
  d.puzzle().addShape(2, 2, 2);
  d.record(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);
  CHECK_FALSE(d.save());
  CHECK(d.isModified());

  // a directory that does not exist cannot be written
  CHECK_FALSE(d.saveAs("no/such/dir/at/all/x.xmpuzzle"));
  CHECK(d.isModified());
  CHECK(d.fileName().empty());
}

TEST_CASE("withPuzzleExtension leaves a correct name alone", "[ui][document]") {
  CHECK(DocumentSession::withPuzzleExtension("a/b.xmpuzzle") == std::filesystem::path("a/b.xmpuzzle"));
  CHECK(DocumentSession::withPuzzleExtension("a/b") == std::filesystem::path("a/b.xmpuzzle"));
  CHECK(DocumentSession::withPuzzleExtension("a/b.txt") == std::filesystem::path("a/b.txt.xmpuzzle"));
}

TEST_CASE("undo and redo restore the recorded states", "[ui][document]") {
  DocumentSession d;
  d.puzzle().addShape(3, 3, 3);
  d.record(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0);
  REQUIRE(d.canUndo());

  auto r = d.undo();
  CHECK(d.puzzle().getNumberOfShapes() == 0);
  CHECK(r.tab == puzzleHistory_c::TAB_ENTITIES);
  CHECK(d.canRedo());

  d.redo();
  CHECK(d.puzzle().getNumberOfShapes() == 1);
}

TEST_CASE("changing the comment marks the document modified without an undo step", "[ui][document]") {
  DocumentSession d;
  d.setComment("hello");
  CHECK(d.puzzle().getComment() == "hello");
  CHECK(d.isModified());
  CHECK_FALSE(d.canUndo());

  DocumentSession e;
  e.setComment("");  // unchanged: nothing to mark
  CHECK_FALSE(e.isModified());
}

TEST_CASE("importing a missing PuzzleSolver3D file fails cleanly", "[ui][document]") {
  DocumentSession d;
  auto r = d.importPuzzleSolver3D("no-such-file.puz");
  CHECK_FALSE(r.ok);
  CHECK_FALSE(r.error.empty());
  CHECK(d.fileName().empty());
}

TEST_CASE("an imported PuzzleSolver3D file is unsaved and has no file name", "[ui][document]") {
  TempDir t;
  const auto src = t.path / "cube.puz";
  {
    std::ofstream out(src);
    out << "PIECE 1,1,1\nX\nRESULT 1,1,1\nX\n";
  }
  DocumentSession d;
  auto r = d.importPuzzleSolver3D(src);
  REQUIRE(r.ok);
  CHECK(d.isModified());
  // Save must ask where: it may not write xmpuzzle over the source file
  CHECK(d.fileName().empty());
  CHECK_FALSE(d.save());
}
