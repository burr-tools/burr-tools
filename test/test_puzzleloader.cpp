#include <catch2/catch_test_macros.hpp>

#include "lib/problem.h"
#include "lib/puzzle.h"
#include "lib/puzzleloader.h"
#include "lib/solution.h"
#include "tools/gzstream.h"
#include "tools/xml.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

/* The staged read (puzzleloader.h): the definitions first, the solutions
 * after on a thread of their own, the puzzle at the end the one loading the
 * file whole gives. */

namespace {

  std::string gunzip(const std::string & file) {
    auto in = openGzFile(file.c_str());
    REQUIRE(in);
    return std::string(std::istreambuf_iterator<char>(*in), {});
  }

  puzzleLoader::opener fromText(const std::string & text) {
    return [text] { return std::unique_ptr<std::istream>(std::make_unique<std::istringstream>(text)); };
  }

  std::string saved(const puzzle_c & p) {
    std::ostringstream out;
    {
      xmlWriter_c xml(out);
      p.save(xml);
    }
    return out.str();
  }

  std::unique_ptr<puzzle_c> whole(const std::string & text) {
    std::istringstream in(text);
    xmlParser_c pars(in);
    return std::make_unique<puzzle_c>(pars);
  }

  void waitFor(const solutionReader_c & r) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (!r.done() && std::chrono::steady_clock::now() < until)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    REQUIRE(r.done());
  }

  size_t savedSolutions(const puzzle_c & p) {
    size_t n = 0;
    for (unsigned k = 0; k < p.getNumberOfProblems(); k++)
      n += p.getProblem(k)->getNumberOfSavedSolutions();
    return n;
  }

  /* the example with the most solutions, as text */
  std::string sixPieceBurrs() { return gunzip("examples/SolidSixPieceBurrs.xmpuzzle"); }

  /* the same with its solutions sixteen times over: several of the
   * reader's reads (1 MiB each) */
  std::string manySolutions() {
    std::string t = sixPieceBurrs();
    const size_t b = t.find("<solutions>") + 11;
    const size_t e = t.find("</solutions>");
    const std::string body = t.substr(b, e - b);
    std::string more;
    for (int i = 0; i < 16; i++)
      more += body;
    t.replace(b, e - b, more);
    return t;
  }

}

TEST_CASE("puzzle loader: a staged read ends with the puzzle the whole read gives", "[puzzleloader][parallel]") {
  int staged = 0;
  for (const auto & e : std::filesystem::directory_iterator("examples")) {
    if (e.path().extension() != ".xmpuzzle")
      continue;
    const std::string text = gunzip(e.path().string());
    const std::string want = saved(*whole(text));
    INFO(e.path().string());
    for (auto [threads, batch] : { std::pair<unsigned, size_t>(1, 1), std::pair<unsigned, size_t>(1, 512 * 1024),
                                   std::pair<unsigned, size_t>(4, 1), std::pair<unsigned, size_t>(3, 4096) }) {
      std::istringstream in(text);
      auto d = puzzleLoader::readDefinitions(in, fromText(text), 0);
      REQUIRE(d.puzzle);
      CHECK(d.bytes == text.size());
      if (d.staged) {
        staged++;
        solutionReader_c r(*d.puzzle, fromText(text), d.bytes, threads, batch);
        waitFor(r);
        CHECK(r.error().empty());
        CHECK(r.progress() == 1.0);
      }
      CHECK(saved(*d.puzzle) == want);
    }
  }
  CHECK(staged > 0);
}

TEST_CASE("puzzle loader: the definitions come without the solutions", "[puzzleloader][parallel]") {
  const std::string text = sixPieceBurrs();
  std::istringstream in(text);
  auto d = puzzleLoader::readDefinitions(in, fromText(text), 0);
  REQUIRE(d.staged);
  CHECK(savedSolutions(*d.puzzle) == 0);
  CHECK(d.solutionBytes > d.bytes / 2);
  // the rest as it is: the problems, their counts and states
  auto w = whole(text);
  REQUIRE(d.puzzle->getNumberOfProblems() == w->getNumberOfProblems());
  for (unsigned k = 0; k < w->getNumberOfProblems(); k++) {
    CHECK(d.puzzle->getProblem(k)->getName() == w->getProblem(k)->getName());
    CHECK(d.puzzle->getProblem(k)->getSolveState() == w->getProblem(k)->getSolveState());
    CHECK(d.puzzle->getProblem(k)->getNumberOfPieces() == w->getProblem(k)->getNumberOfPieces());
  }
}

TEST_CASE("puzzle loader: solutions below the threshold are read with the rest", "[puzzleloader][parallel]") {
  const std::string text = sixPieceBurrs();
  std::istringstream in(text);
  auto d = puzzleLoader::readDefinitions(in, fromText(text), text.size());
  CHECK_FALSE(d.staged);
  CHECK(saved(*d.puzzle) == saved(*whole(text)));
}

TEST_CASE("puzzle loader: what a staged read cannot be sure of is read whole", "[puzzleloader][parallel]") {
  const std::string text = sixPieceBurrs();
  const size_t at = text.find("<solution ");
  REQUIRE(at != std::string::npos);

  SECTION("a comment among the solutions") {
    std::string t = text;
    t.insert(at, "<!-- </solutions> -->");
    std::istringstream in(t);
    auto d = puzzleLoader::readDefinitions(in, fromText(t), 0);
    CHECK_FALSE(d.staged);
    CHECK(saved(*d.puzzle) == saved(*whole(t)));
  }
  SECTION("an end tag in a comment among the solutions: cut wrongly, then read whole") {
    std::string t = text;
    t.insert(at, "<!-- </solution> -->");
    std::istringstream in(t);
    auto d = puzzleLoader::readDefinitions(in, fromText(t), 0);
    REQUIRE(d.staged);
    solutionReader_c r(*d.puzzle, fromText(t), d.bytes, 1, 1);
    waitFor(r);
    CHECK(r.error().empty());
    CHECK(saved(*d.puzzle) == saved(*whole(t)));
  }
  SECTION("a comment in the definitions that looks like solutions is skipped over") {
    std::string t = text;
    t.insert(t.find("<problems>"), "<!-- <problem><solutions> -->");
    std::istringstream in(t);
    auto d = puzzleLoader::readDefinitions(in, fromText(t), 0);
    REQUIRE(d.staged);
    solutionReader_c r(*d.puzzle, fromText(t), d.bytes);
    waitFor(r);
    CHECK(r.error().empty());
    CHECK(saved(*d.puzzle) == saved(*whole(t)));
  }
  SECTION("a document type declaration") {
    const std::string t = "<!DOCTYPE puzzle>" + text.substr(text.find("<puzzle"));
    std::istringstream in(t);
    auto d = puzzleLoader::readDefinitions(in, fromText(t), 0);
    CHECK_FALSE(d.staged);
    CHECK(savedSolutions(*d.puzzle) == savedSolutions(*whole(t)));
  }
}

TEST_CASE("puzzle loader: a problem's solutions before its shapes are read whole", "[puzzleloader][parallel]") {
  const std::string t =
    "<?xml version=\"1.0\"?>\n<puzzle version=\"2\"><gridType type=\"0\"/><colors/>"
    "<shapes><voxel x=\"1\" y=\"1\" z=\"1\" type=\"0\">#</voxel></shapes>"
    "<problems><problem name=\"p\" state=\"0\"><solutions>\n</solutions>"
    "<shapes><shape id=\"0\" count=\"1\"/></shapes><result id=\"0\"/></problem></problems>"
    "<comment/></puzzle>\n";
  std::istringstream in(t);
  auto d = puzzleLoader::readDefinitions(in, fromText(t), 0);
  CHECK_FALSE(d.staged);
  REQUIRE(d.puzzle->getNumberOfProblems() == 1);
}

TEST_CASE("puzzle loader: a broken file is reported as the whole read reports it", "[puzzleloader][parallel]") {
  const std::string text = sixPieceBurrs();

  SECTION("broken solutions are left out, the others read, and the whole read says what is wrong") {
    const std::string good = text;
    // break the first and the last solution's assembly: a letter is no number
    std::string t = good, without = good;
    auto breakAt = [&](size_t at) {
      REQUIRE(at != std::string::npos);
      t.insert(at + 10, "q");
    };
    auto drop = [&](size_t at) {
      const size_t b = without.rfind("<solution", at);
      const size_t e = without.find("</solution>", at) + 11;
      without.erase(b, e - b);
    };
    drop(without.rfind("<assembly>"));
    drop(without.find("<assembly>"));
    breakAt(t.rfind("<assembly>"));
    breakAt(t.find("<assembly>"));
    std::string want;
    try {
      whole(t);
    } catch (xmlParserException_c & e) {
      want = e.what();
    }
    REQUIRE_FALSE(want.empty());
    const std::string expected = saved(*whole(without));

    for (auto [threads, threshold] : { std::pair<unsigned, size_t>(1, 0), std::pair<unsigned, size_t>(4, 0),
                                       std::pair<unsigned, size_t>(1, SIZE_MAX) }) {
      // also below the threshold: a whole read that fails there for a solution stages
      std::istringstream in(t);
      auto d = puzzleLoader::readDefinitions(in, fromText(t), threshold);
      REQUIRE(d.staged);
      solutionReader_c r(*d.puzzle, fromText(t), d.bytes, threads, 1);
      waitFor(r);
      CHECK(r.error().empty());
      CHECK(r.skipped() == 2);
      CHECK(r.skipError() == want);
      CHECK(saved(*d.puzzle) == expected);
    }
  }
  SECTION("broken definitions: readDefinitions throws what the whole read throws") {
    std::string t = text;
    t.replace(t.find("<problems>"), 10, "<problems><problem name=\"x\" maxHoles=\"-1\"/>");
    std::string want;
    try {
      whole(t);
    } catch (xmlParserException_c & e) {
      want = e.what();
    }
    REQUIRE_FALSE(want.empty());
    std::istringstream in(t);
    std::string got;
    try {
      puzzleLoader::readDefinitions(in, fromText(t), 0);
    } catch (xmlParserException_c & e) {
      got = e.what();
    }
    CHECK(got == want);
  }
}

TEST_CASE("puzzle loader: a file that changed between the two reads is an error", "[puzzleloader][parallel]") {
  // cut short after the definitions were read: the reader cannot get through it
  const std::string text = sixPieceBurrs();
  const std::string shorter = text.substr(0, text.size() / 2);
  std::istringstream in(text);
  auto d = puzzleLoader::readDefinitions(in, fromText(text), 0);
  REQUIRE(d.staged);
  solutionReader_c r(*d.puzzle, fromText(shorter), d.bytes, 2, 1);
  waitFor(r);
  CHECK_FALSE(r.error().empty());
  CHECK(r.skipped() == 0);
}

TEST_CASE("puzzle loader: stopping keeps the solutions read, in their order", "[puzzleloader][parallel]") {
  const std::string text = sixPieceBurrs();
  auto w = whole(text);
  std::istringstream in(text);
  auto d = puzzleLoader::readDefinitions(in, fromText(text), 0);
  REQUIRE(d.staged);
  {
    solutionReader_c r(*d.puzzle, fromText(text), d.bytes, 4, 1);
    r.stop();
    CHECK(r.done());
  }
  for (unsigned k = 0; k < w->getNumberOfProblems(); k++) {
    const problem_c * got = d.puzzle->getProblem(k);
    const problem_c * all = w->getProblem(k);
    REQUIRE(got->getNumberOfSavedSolutions() <= all->getNumberOfSavedSolutions());
    for (unsigned i = 0; i < got->getNumberOfSavedSolutions(); i++)
      CHECK(got->getSavedSolution(i)->getAssemblyNumber() == all->getSavedSolution(i)->getAssemblyNumber());
  }
}

TEST_CASE("puzzle loader: the solutions arrive in batches while the reader runs", "[puzzleloader][parallel]") {
  /* a stream that hands out its bytes slowly: the list is seen growing */
  struct slowBuf : std::streambuf {
    std::string text;
    size_t at = 0;
    explicit slowBuf(std::string t) : text(std::move(t)) {}
    int_type underflow() override {
      if (at >= text.size())
        return traits_type::eof();
      std::this_thread::sleep_for(std::chrono::microseconds(200));
      const size_t n = std::min<size_t>(4096, text.size() - at);
      setg(text.data() + at, text.data() + at, text.data() + at + n);
      at += n;
      return traits_type::to_int_type(*gptr());
    }
  };
  struct slowStream : std::istream {
    slowBuf buf;
    explicit slowStream(std::string t) : std::istream(nullptr), buf(std::move(t)) { rdbuf(&buf); }
  };

  const std::string text = manySolutions();
  std::istringstream in(text);
  auto d = puzzleLoader::readDefinitions(in, fromText(text), 0);
  REQUIRE(d.staged);
  solutionReader_c r(*d.puzzle, [text] { return std::unique_ptr<std::istream>(std::make_unique<slowStream>(text)); },
                     d.bytes, 4, 1);
  size_t seen = 0, sizes = 0;
  double progress = 0;
  bool progressRose = true;
  while (!r.done()) {
    size_t now = 0;
    for (unsigned k = 0; k < d.puzzle->getNumberOfProblems(); k++) {
      auto lock = d.puzzle->getProblem(k)->lockSolutions();
      now += d.puzzle->getProblem(k)->getNumberOfSavedSolutions();
    }
    if (now != seen) {
      sizes++;
      seen = now;
    }
    const double p = r.progress();
    progressRose = progressRose && p >= progress;
    progress = p;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  CHECK(r.error().empty());
  CHECK(sizes > 1);
  CHECK(progressRose);
  CHECK(saved(*d.puzzle) == saved(*whole(text)));
}
