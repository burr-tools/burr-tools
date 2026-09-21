#include <catch2/catch_test_macros.hpp>

#include "tools/filepath.h"

/* directoryOfFile() exists to give the image and STL export dialogs a
   sensible default location: the directory the current puzzle was loaded
   from. Before it, the image dialog's path field started empty, which makes
   the exported name relative — and an app launched from a macOS .app bundle
   runs with a working directory of "/", which is read-only. The export then
   failed silently, because saveToPNG()'s return value was discarded.

   The function is deliberately pure and lives in src/tools rather than in
   the dialog, because src/tools is linked into test_burrtools while
   src/gui is not. Choosing the fallback when there is no directory is the
   caller's policy (the dialogs use homedir()), so it stays out of here and
   this stays testable. */

TEST_CASE("directoryOfFile splits a path at its last separator", "[filepath]")
{
  REQUIRE(directoryOfFile("/home/tom/puzzles/burr.xmpuzzle") == "/home/tom/puzzles");
  REQUIRE(directoryOfFile("/burr.xmpuzzle") == "/");
}

TEST_CASE("directoryOfFile returns empty when there is no directory part", "[filepath]")
{
  /* The caller substitutes its own fallback for these; the point is that it
     can tell the difference rather than being handed "." or the input. */
  REQUIRE(directoryOfFile("burr.xmpuzzle").empty());
  REQUIRE(directoryOfFile("").empty());
}

TEST_CASE("directoryOfFile keeps a trailing separator's directory", "[filepath]")
{
  /* A path that is already a directory should come back as that directory,
     not as its parent: the export dialogs may be handed either form. */
  REQUIRE(directoryOfFile("/home/tom/puzzles/") == "/home/tom/puzzles");
}

TEST_CASE("directoryOfFile does not mistake a dot for a directory", "[filepath]")
{
  /* "." is what the STL dialog used to default to. It is a relative
     directory, so it must survive as one rather than being read as an
     extension separator. */
  REQUIRE(directoryOfFile("./burr.xmpuzzle") == ".");
  REQUIRE(directoryOfFile("burr.tar.xmpuzzle").empty());
}

TEST_CASE("directoryOfFile handles Windows separators", "[filepath]")
{
  /* The project cross-compiles for Windows (cross-mingw64.txt), where
     Fl_Native_File_Chooser hands back backslash-separated paths. Treating
     only '/' as a separator would make every Windows puzzle path look like
     a bare filename, so the export dialogs would silently fall back to the
     home directory instead of the folder the puzzle came from. */
  REQUIRE(directoryOfFile("C:\\puzzles\\burr.xmpuzzle") == "C:\\puzzles");
  REQUIRE(directoryOfFile("C:\\puzzles\\") == "C:\\puzzles");

  /* Mixed separators turn up when a configured path meets a chosen one. */
  REQUIRE(directoryOfFile("C:/puzzles\\burr.xmpuzzle") == "C:/puzzles");
}
