/* Tests for btui::DocumentSession: the redesigned GUI's new / load / save /
 * undo, driven with no GUI. Example files are read relative to the project
 * root, the test working directory.
 */
#include <catch2/catch_test_macros.hpp>

#include "../src/uicore/documentsession.h"

#include "../src/lib/problem.h"
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

TEST_CASE("a stroke that changed something is unsaved and undoable before its button goes up", "[ui][document]") {
  // PR 181 review: the title's * waited for the release, and Ctrl+Z was
  // off during the first stroke after opening (no step before it)
  DocumentSession d;
  oneShape(d);
  TempDir t;
  REQUIRE(d.saveAs(t.path / "clean"));
  d.beginStroke(puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0);
  CHECK_FALSE(d.isModified());              // nothing painted yet
  CHECK_FALSE(d.strokeChanged());
  CHECK_FALSE(d.strokeEdit([](puzzle_c &) { return false; }));
  CHECK_FALSE(d.isModified());
  d.strokeEdit(fill(0));
  CHECK(d.strokeChanged());
  CHECK(d.isModified());
  CHECK(d.canUndo());
  d.undo();                                 // ends the stroke, a step, and undoes it
  CHECK_FALSE(d.inStroke());
  CHECK_FALSE(d.strokeChanged());
  CHECK(d.puzzle().getShape(0)->countState(voxel_c::VX_FILLED) == 0);
  CHECK_FALSE(d.isModified());
  CHECK(d.canRedo());
}

TEST_CASE("the first stroke on a fresh document can be undone while it goes on", "[ui][document]") {
  TempDir t;
  DocumentSession d;
  oneShape(d);
  REQUIRE(d.saveAs(t.path / "fresh"));
  REQUIRE(d.load(t.path / "fresh.xmpuzzle").ok);   // a shape, and no step before the stroke
  REQUIRE_FALSE(d.canUndo());
  d.beginStroke(puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0);
  CHECK_FALSE(d.canUndo());                 // a stroke that changed nothing is no step
  d.strokeEdit(fill(0));
  CHECK(d.canUndo());
  d.undo();
  CHECK(d.puzzle().getShape(0)->countState(voxel_c::VX_FILLED) == 0);
  CHECK_FALSE(d.canUndo());
}

TEST_CASE("a save during a stroke splits it: an undo back to the save reads clean", "[ui][document]") {
  TempDir t;
  DocumentSession d;
  oneShape(d);
  d.beginStroke(puzzleHistory_c::AK_ENTITIES_GRID_PAINT, 0);
  d.strokeEdit(fill(0));
  d.strokeEdit(fill(1));
  REQUIRE(d.saveAs(t.path / "mid"));
  CHECK(d.inStroke());
  CHECK_FALSE(d.isModified());
  d.strokeEdit(fill(2));
  CHECK(d.endStroke());
  CHECK(d.isModified());
  d.undo();                       // the rest of the drag
  CHECK(d.puzzle().getShape(0)->countState(voxel_c::VX_FILLED) == 2);
  CHECK_FALSE(d.isModified());    // the file holds just this
  d.undo();                       // the part before the save
  CHECK(d.puzzle().getShape(0)->countState(voxel_c::VX_FILLED) == 0);
  CHECK(d.isModified());
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

// --- a voxel edit and the solutions of the problems using that shape ----------
// (PR 181 review: legacy's changeShape() dropped them on every voxel edit, the
// redesigned GUI kept them, so a saved file held solutions that no longer fit
// its pieces. Now an edit drops them into its undo step, and undo puts them back.)

namespace {
  /* what a problem knows of its search, to compare before and after */
  struct Solved {
    solveState_e state = SS_UNSOLVED;
    unsigned long assemblies = 0, solutions = 0, time = 0;
    unsigned int kept = 0;
    bool operator==(const Solved &) const = default;
  };
  Solved solved(const DocumentSession & d, unsigned int problem) {
    const problem_c * pr = d.puzzle().getProblem(problem);
    Solved r;
    r.state = pr->getSolveState();
    if (pr->numAssembliesKnown()) {
      r.assemblies = pr->getNumAssemblies();
      r.solutions = pr->getNumSolutions();
      r.time = pr->getUsedTime();
    }
    r.kept = pr->getNumberOfSavedSolutions();
    return r;
  }
  bool unsolved(const DocumentSession & d, unsigned int problem) {
    return solved(d, problem) == Solved{};
  }

  /* flip voxel 0,0,0 of shape s */
  auto flip(unsigned int s) {
    return [s](puzzle_c & p) {
      voxel_c * v = p.getShape(s);
      v->setState(0, 0, 0, v->getState(0, 0, 0) == voxel_c::VX_EMPTY ? voxel_c::VX_FILLED : voxel_c::VX_EMPTY);
      return true;
    };
  }

  /* PelikanBurr: problem 0 is solved (12 assemblies, 1 solution kept) */
  struct Pelikan {
    DocumentSession d;
    unsigned int used = 0;        // a shape problem 0 uses
    Solved before;
    Pelikan(void) {
      REQUIRE(d.load("examples/PelikanBurr.xmpuzzle").ok);
      REQUIRE(d.puzzle().getNumberOfProblems() > 0);
      used = d.puzzle().getProblem(0)->getShapeIdOfPart(0);
      before = solved(d, 0);
      REQUIRE(before.kept > 0);
      REQUIRE(before.state == SS_SOLVED);
    }
    /* a shape no problem uses, added if there is none */
    unsigned int unusedShape(void) {
      const puzzle_c & p = d.puzzle();
      for (unsigned int s = 0; s < p.getNumberOfShapes(); s++) {
        bool uses = false;
        for (unsigned int k = 0; k < p.getNumberOfProblems(); k++)
          uses = uses || p.getProblem(k)->usesShape(s);
        if (!uses)
          return s;
      }
      d.edit(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0, [](puzzle_c & q) { q.addShape(3, 3, 3); return true; });
      return d.puzzle().getNumberOfShapes() - 1;
    }
  };
}

TEST_CASE("a voxel edit drops the solutions of the problems using that shape, and undo brings them back", "[ui][document]") {
  Pelikan f;
  DocumentSession & d = f.d;
  CHECK(d.editVoxels(puzzleHistory_c::AK_ENTITIES_CLICK_3D, f.used, { f.used }, flip(f.used)));
  CHECK(unsolved(d, 0));
  d.undo();
  CHECK(solved(d, 0) == f.before);
  d.redo();
  CHECK(unsolved(d, 0));
  d.undo();                                   // and again: the record goes back and forth whole
  CHECK(solved(d, 0) == f.before);
  CHECK(d.puzzle().getProblem(0)->getSavedSolution(0) != nullptr);
}

TEST_CASE("an edit that leaves the voxels as they were, or of a shape no problem uses, keeps the solutions", "[ui][document]") {
  Pelikan f;
  DocumentSession & d = f.d;
  // f says it changed something, but the voxels are the same
  d.editVoxels(puzzleHistory_c::AK_ENTITIES_CLICK_3D, f.used, { f.used }, [&](puzzle_c & p) {
    voxel_c * v = p.getShape(f.used);
    v->setState(0, 0, 0, v->getState(0, 0, 0));
    return true;
  });
  CHECK(solved(d, 0) == f.before);
  const unsigned int other = f.unusedShape();
  CHECK(d.editVoxels(puzzleHistory_c::AK_ENTITIES_CLICK_3D, other, { other }, flip(other)));
  CHECK(solved(d, 0) == f.before);
  // an edit that reports no change records no step, and keeps them
  CHECK_FALSE(d.editVoxels(puzzleHistory_c::AK_ENTITIES_CLICK_3D, f.used, { f.used }, [](puzzle_c &) { return false; }));
  CHECK(solved(d, 0) == f.before);
}

TEST_CASE("a stroke drops the solutions at its first change, and undo -- even mid-stroke -- brings them back", "[ui][document]") {
  Pelikan f;
  DocumentSession & d = f.d;
  d.beginStroke(puzzleHistory_c::AK_ENTITIES_GRID_PAINT, f.used);
  CHECK(solved(d, 0) == f.before);            // nothing painted yet
  CHECK(d.strokeEdit(flip(f.used)));
  CHECK(unsolved(d, 0));
  CHECK(d.strokeEdit(flip(f.used)));          // back to the voxels it began with: still a changed stroke
  CHECK(d.endStroke());
  CHECK(unsolved(d, 0));
  d.undo();
  CHECK(solved(d, 0) == f.before);

  d.beginStroke(puzzleHistory_c::AK_ENTITIES_GRID_PAINT, f.used);
  d.strokeEdit(flip(f.used));
  CHECK(unsolved(d, 0));
  d.undo();                                   // the button still down
  CHECK_FALSE(d.inStroke());
  CHECK(solved(d, 0) == f.before);
  d.redo();
  CHECK(unsolved(d, 0));
}

TEST_CASE("paint edits merged into one step keep the solutions from before the first of them", "[ui][document]") {
  Pelikan f;
  DocumentSession & d = f.d;
  // strokeEdit outside a stroke: grid-paint steps, merged while they follow quickly
  d.beginStroke(puzzleHistory_c::AK_ENTITIES_GRID_PAINT, f.used);
  CHECK_FALSE(d.endStroke());
  d.strokeEdit(flip(f.used));
  d.strokeEdit([&](puzzle_c & p) { p.getShape(f.used)->setState(1, 0, 0, voxel_c::VX_FILLED); return true; });
  CHECK(unsolved(d, 0));
  while (d.canUndo() && !(solved(d, 0) == f.before))
    d.undo();
  CHECK(solved(d, 0) == f.before);
}

TEST_CASE("a save after a voxel edit writes no stale solutions; after an undo it writes them again", "[ui][document]") {
  // the review's repro: edit a shape of a solved problem, save
  TempDir t;
  Pelikan f;
  DocumentSession & d = f.d;
  d.editVoxels(puzzleHistory_c::AK_ENTITIES_CLICK_3D, f.used, { f.used }, flip(f.used));
  REQUIRE(d.saveAs(t.path / "edited"));
  {
    DocumentSession again;
    REQUIRE(again.load(t.path / "edited.xmpuzzle").ok);
    CHECK(unsolved(again, 0));
  }
  d.undo();
  REQUIRE(d.saveAs(t.path / "undone"));
  DocumentSession again;
  REQUIRE(again.load(t.path / "undone.xmpuzzle").ok);
  CHECK(solved(again, 0) == f.before);
}

TEST_CASE("a step that drops solutions keeps them only while it is in the history", "[ui][document]") {
  // a new edit after the undo drops the redo step: the solutions stay put
  Pelikan f;
  DocumentSession & d = f.d;
  d.editVoxels(puzzleHistory_c::AK_ENTITIES_CLICK_3D, f.used, { f.used }, flip(f.used));
  d.undo();
  const unsigned int other = f.unusedShape();
  d.editVoxels(puzzleHistory_c::AK_ENTITIES_CLICK_3D, other, { other }, flip(other));
  CHECK_FALSE(d.canRedo());
  CHECK(solved(d, 0) == f.before);
  // a step pushed out by the depth limit takes what it held with it, safely
  d.setUndoDepth(1);
  d.editVoxels(puzzleHistory_c::AK_ENTITIES_CLICK_3D, f.used, { f.used }, flip(f.used));
  d.editVoxels(puzzleHistory_c::AK_ENTITIES_CLICK_3D, other, { other }, flip(other));
  d.undo();
  CHECK_FALSE(d.canUndo());
  CHECK(unsolved(d, 0));
}

TEST_CASE("an undo leaves the solutions of the problems the step did not change", "[ui][document]") {
  // every undo used to rebuild every problem through setters that drop
  // solutions: a rename's undo lost them all, in both GUIs
  Pelikan f;
  DocumentSession & d = f.d;
  d.edit(puzzleHistory_c::AK_ENTITIES_STRUCTURAL, 0, [](puzzle_c & p) { p.getShape(0)->setName("renamed"); return true; });
  d.undo();
  CHECK(solved(d, 0) == f.before);
  d.redo();
  CHECK(solved(d, 0) == f.before);
}

TEST_CASE("an undo that changes a shape drops the solutions no step kept for it", "[ui][document]") {
  // legacy's path: its edit drops them itself (changeShape), outside the
  // history; solutions found after an edit do not fit the shape before it
  Pelikan f;
  DocumentSession & d = f.d;
  d.edit(puzzleHistory_c::AK_ENTITIES_CLICK_3D, f.used, flip(f.used));   // edit(), not editVoxels()
  CHECK(solved(d, 0) == f.before);
  d.undo();
  CHECK(unsolved(d, 0));
}
