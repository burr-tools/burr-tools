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
#include <stdexcept>
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

  // a folder is there but is no file: refused, nothing changed
  auto folder = d.load("examples");
  CHECK_FALSE(folder.ok);
  CHECK_FALSE(folder.error.empty());
  CHECK(d.puzzle().getNumberOfShapes() == shapes);
  CHECK(d.fileName() == std::filesystem::path("examples/PelikanBurr.xmpuzzle"));
}

TEST_CASE("saveAs appends the extension, adopts the name and round-trips", "[ui][document]") {
  TempDir t;
  DocumentSession d;
  REQUIRE(d.load("examples/PelikanBurr.xmpuzzle").ok);
  d.edit(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0, [](puzzle_c & p) { p.getShape(0)->setName("renamed"); return true; });
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
  d.edit(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0, [](puzzle_c & p) { p.addShape(2, 2, 2); return true; });
  CHECK_FALSE(d.save());
  CHECK(d.isModified());

  // a directory that does not exist cannot be written
  CHECK_FALSE(d.saveAs("no/such/dir/at/all/x.xmpuzzle"));
  CHECK(d.isModified());
  CHECK(d.fileName().empty());
}

TEST_CASE("a save replaces the file whole, and a failed one leaves the old file as it was", "[ui][document]") {
  TempDir t;
  DocumentSession d;
  REQUIRE(d.load("examples/PelikanBurr.xmpuzzle").ok);
  const std::filesystem::path file = t.path / "keep.xmpuzzle";
  REQUIRE(d.saveAs(file));
  const auto size = std::filesystem::file_size(file);
  d.edit(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0, [](puzzle_c & p) { p.getShape(0)->setName("changed"); return true; });
  REQUIRE(d.save());                                   // over the existing file
  CHECK_FALSE(std::filesystem::exists(t.path / "keep.xmpuzzle.tmp"));

  // the file cannot be written: the save fails, and the file on disk is
  // still the last good one
  std::filesystem::permissions(file, std::filesystem::perms::owner_write | std::filesystem::perms::group_write |
                               std::filesystem::perms::others_write, std::filesystem::perm_options::remove);
  if (std::ofstream(file, std::ios::app).is_open()) {
    std::filesystem::permissions(file, std::filesystem::perms::owner_write, std::filesystem::perm_options::add);
    SKIP("a read-only file is still writable here (running as root)");
  }
  d.edit(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0, [](puzzle_c & p) { p.getShape(0)->setName("lost"); return true; });
  CHECK_FALSE(d.save());
  CHECK(d.isModified());
  CHECK_FALSE(std::filesystem::exists(t.path / "keep.xmpuzzle.tmp"));
  DocumentSession e;
  REQUIRE(e.load(file).ok);
  CHECK(e.puzzle().getShape(0)->getName() == "changed");
  CHECK(std::filesystem::file_size(file) > size / 2);
  // so that the folder can be removed (Windows)
  std::filesystem::permissions(file, std::filesystem::perms::owner_write, std::filesystem::perm_options::add);
}

TEST_CASE("a save where no copy fits beside the file goes by the temporary folder", "[ui][document]") {
  TempDir t;
  DocumentSession d;
  REQUIRE(d.load("examples/PelikanBurr.xmpuzzle").ok);
  const std::filesystem::path file = t.path / "keep.xmpuzzle";
  REQUIRE(d.saveAs(file));

  // a directory where the copy would go, as good as a folder that cannot
  // be written to, on every system and for every user
  std::filesystem::create_directory(t.path / "keep.xmpuzzle.tmp");
  d.edit(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0, [](puzzle_c & p) { p.getShape(0)->setName("changed"); return true; });
  REQUIRE(d.save());
  CHECK_FALSE(d.isModified());
  CHECK(std::filesystem::is_directory(t.path / "keep.xmpuzzle.tmp"));    // left alone
  DocumentSession e;
  REQUIRE(e.load(file).ok);
  CHECK(e.puzzle().getShape(0)->getName() == "changed");
}

#ifndef _WIN32
TEST_CASE("a save writes into the file: a link to it and its permissions stay", "[ui][document]") {
  TempDir t;
  DocumentSession d;
  REQUIRE(d.load("examples/PelikanBurr.xmpuzzle").ok);
  std::filesystem::create_directory(t.path / "real");
  const std::filesystem::path real = t.path / "real" / "p.xmpuzzle";
  const std::filesystem::path link = t.path / "link.xmpuzzle";
  REQUIRE(d.saveAs(real));
  std::filesystem::create_symlink(real, link);
  const auto mode = std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
                    std::filesystem::perms::group_read;                  // 0640
  std::filesystem::permissions(real, mode);

  REQUIRE(d.load(link).ok);
  d.setComment("through the link");
  REQUIRE(d.save());
  CHECK(std::filesystem::is_symlink(link));
  CHECK((std::filesystem::status(real).permissions() & std::filesystem::perms::all) == mode);
  DocumentSession e;
  REQUIRE(e.load(real).ok);
  CHECK(e.puzzle().getComment() == "through the link");
}
#endif

TEST_CASE("withPuzzleExtension leaves a correct name alone", "[ui][document]") {
  CHECK(DocumentSession::withPuzzleExtension("a/b.xmpuzzle") == std::filesystem::path("a/b.xmpuzzle"));
  CHECK(DocumentSession::withPuzzleExtension("a/b") == std::filesystem::path("a/b.xmpuzzle"));
  CHECK(DocumentSession::withPuzzleExtension("a/b.txt") == std::filesystem::path("a/b.txt.xmpuzzle"));
}

TEST_CASE("undo and redo restore the recorded states", "[ui][document]") {
  DocumentSession d;
  d.edit(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0, [](puzzle_c & p) { p.addShape(3, 3, 3); return true; });
  REQUIRE(d.canUndo());

  auto r = d.undo();
  CHECK(d.puzzle().getNumberOfShapes() == 0);
  CHECK(r.tab == puzzleHistory_c::TAB_ENTITIES);
  CHECK(d.canRedo());

  d.redo();
  CHECK(d.puzzle().getNumberOfShapes() == 1);
}

TEST_CASE("an edit that changes nothing leaves no undo step; one that does leaves one", "[ui][document]") {
  DocumentSession d;
  CHECK_FALSE(d.edit(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0, [](puzzle_c &) { return false; }));
  CHECK_FALSE(d.canUndo());
  CHECK_FALSE(d.isModified());

  CHECK(d.edit(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0, [](puzzle_c & p) { p.addShape(2, 2, 2); return true; }));
  CHECK(d.canUndo());
  CHECK(d.isModified());
}

TEST_CASE("an edit that throws half way is still an undo step back to the state before it", "[ui][document]") {
  // the change may be half done: without the step, undo could not reach
  // the puzzle as it was, and the next step would swallow the half change
  DocumentSession d;
  CHECK_THROWS_AS(d.edit(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0, [](puzzle_c & p) -> bool {
                    p.addShape(2, 2, 2);
                    throw std::runtime_error("half done");
                  }),
                  std::runtime_error);
  CHECK(d.puzzle().getNumberOfShapes() == 1);
  REQUIRE(d.canUndo());
  d.undo();
  CHECK(d.puzzle().getNumberOfShapes() == 0);
}

TEST_CASE("editWithoutUndo marks the document unsaved but adds no undo step", "[ui][document]") {
  DocumentSession d;
  d.editWithoutUndo([](puzzle_c & p) { p.addShape(2, 2, 2); });
  CHECK(d.puzzle().getNumberOfShapes() == 1);
  CHECK(d.isModified());
  CHECK_FALSE(d.canUndo());
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

namespace {
  /* fill voxel x of shape 0 */
  auto fill(unsigned x) {
    return [x](puzzle_c & p) {
      if (p.getShape(0)->getState(x, 0, 0) == voxel_c::VX_FILLED)
        return false;
      p.getShape(0)->setState(x, 0, 0, voxel_c::VX_FILLED);
      return true;
    };
  }

  void oneShape(DocumentSession & d) {
    d.edit(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0, [](puzzle_c & p) { p.addShape(5, 1, 1); return true; });
  }
}

TEST_CASE("a stroke is one undo step however many cells it paints", "[ui][document]") {
  // AC-C08-05
  DocumentSession d;
  oneShape(d);
  d.beginStroke(puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0);
  CHECK(d.inStroke());
  for (unsigned x = 0; x < 5; x++)
    CHECK(d.strokeEdit(fill(x)));
  CHECK_FALSE(d.strokeEdit(fill(2)));
  CHECK(d.endStroke());
  CHECK_FALSE(d.inStroke());
  CHECK(d.puzzle().getShape(0)->countState(voxel_c::VX_FILLED) == 5);
  auto r = d.undo();
  CHECK(r.selectedShape == 0);
  CHECK(d.puzzle().getShape(0)->countState(voxel_c::VX_FILLED) == 0);
  d.redo();
  CHECK(d.puzzle().getShape(0)->countState(voxel_c::VX_FILLED) == 5);
}

TEST_CASE("a stroke that changes nothing leaves no step, and two strokes are two steps", "[ui][document]") {
  DocumentSession d;
  oneShape(d);
  d.beginStroke(puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0);
  CHECK_FALSE(d.endStroke());
  d.beginStroke(puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0);
  d.strokeEdit(fill(0));
  d.endStroke();
  d.beginStroke(puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0);   // at once: not merged
  d.strokeEdit(fill(1));
  d.endStroke();
  d.undo();
  CHECK(d.puzzle().getShape(0)->countState(voxel_c::VX_FILLED) == 1);
}

TEST_CASE("an undo, an edit or another document during a stroke ends it first", "[ui][document]") {
  DocumentSession d;
  oneShape(d);
  d.beginStroke(puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0);
  d.strokeEdit(fill(0));
  d.undo();                       // the stroke so far was a step: undoing it
  CHECK_FALSE(d.inStroke());
  CHECK(d.puzzle().getShape(0)->countState(voxel_c::VX_FILLED) == 0);
  d.redo();
  CHECK(d.puzzle().getShape(0)->countState(voxel_c::VX_FILLED) == 1);

  d.beginStroke(puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0);
  d.strokeEdit(fill(1));
  d.edit(puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0, fill(2));
  CHECK_FALSE(d.inStroke());
  d.undo();
  CHECK(d.puzzle().getShape(0)->countState(voxel_c::VX_FILLED) == 2);

  d.beginStroke(puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0);
  d.newDocument(gridType_c::GT_BRICKS);
  CHECK_FALSE(d.inStroke());
  CHECK_FALSE(d.endStroke());
  CHECK_FALSE(d.canUndo());
}

TEST_CASE("strokeEdit outside a stroke is an edit of its own", "[ui][document]") {
  DocumentSession d;
  oneShape(d);
  CHECK(d.strokeEdit(fill(0)));
  CHECK(d.isModified());
  d.undo();
  CHECK(d.puzzle().getShape(0)->countState(voxel_c::VX_FILLED) == 0);
}

TEST_CASE("a stroke that throws is still recorded, so undo reaches the state before it", "[ui][document]") {
  DocumentSession d;
  oneShape(d);
  d.beginStroke(puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0);
  CHECK_THROWS(d.strokeEdit([](puzzle_c & p) -> bool {
    p.getShape(0)->setState(0, 0, 0, voxel_c::VX_FILLED);
    throw std::runtime_error("half way");
  }));
  CHECK(d.endStroke());
  d.undo();
  CHECK(d.puzzle().getShape(0)->countState(voxel_c::VX_FILLED) == 0);
}
