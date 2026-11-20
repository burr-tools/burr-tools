/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/* Reading a puzzle through a descriptor, the way standard input is read: the
 * tests feed a pipe, which like a real stdin can not seek. The programs built
 * on this are driven through their real stdin by test/cli_stdin.py.
 */

#include "../src/lib/puzzle.h"
#include "../src/tools/cliargs.h"
#include "../src/tools/gzstream.h"
#include "../src/tools/xml.h"
#include "fdcompat.h"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <thread>

#ifndef _WIN32
#include <csignal>
#include <pthread.h>
#endif

namespace {

  const char * const example = "examples/PelikanBurr.xmpuzzle";

  std::string bytesOf(const char * file) {
    std::ifstream in(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }

  std::string contentOf(std::istream & in) {
    std::ostringstream all;
    all << in.rdbuf();
    return all.str();
  }

  /* A pipe holding the given data and then at its end. A thread writes it, as
   * the data can be larger than what a pipe buffers. A reader may stop early:
   * the writer then fails to write, and stops too. */
  class Feed {

    public:

      explicit Feed(const std::string & data) {
        int fds[2];
#ifdef _WIN32
        // text mode, as a real stdin starts out on Windows
        REQUIRE(_pipe(fds, 4096, _O_TEXT) == 0);
        _setmode(fds[1], _O_BINARY);
#else
        REQUIRE(pipe(fds) == 0);
#endif
        reader = fds[0];
        writer = std::thread([fd = fds[1], data] {
#ifndef _WIN32
          /* a write to a pipe nobody reads any more raises SIGPIPE, which
           * would kill the test binary. That signal goes to the thread that
           * wrote, so blocking it here is enough: write() fails with EPIPE.
           */
          sigset_t pipeSignal;
          sigemptyset(&pipeSignal);
          sigaddset(&pipeSignal, SIGPIPE);
          pthread_sigmask(SIG_BLOCK, &pipeSignal, nullptr);
#endif
          size_t done = 0;
          while (done < data.size()) {
            const long n = btfd::write(fd, data.data() + done, data.size() - done);
            if (n <= 0)
              break;
            done += static_cast<size_t>(n);
          }
          btfd::close(fd);
        });
      }

      ~Feed() {
        // a reader that stopped early must not leave the writer blocked
        if (reader >= 0)
          btfd::close(reader);
        writer.join();
      }

      Feed(const Feed &) = delete;
      Feed & operator=(const Feed &) = delete;

      int fd(void) const { return reader; }

      /* Closes the read end; false if that was already done. */
      bool closeReader(void) {
        const bool ok = btfd::close(reader) == 0;
        reader = -1;
        return ok;
      }

    private:

      int reader = -1;
      std::thread writer;
  };
}

TEST_CASE("a descriptor reads plain data unchanged", "[stdin]") {
  const std::string data = "<puzzle>plain</puzzle>\n";
  Feed feed(data);
  auto in = openGzDescriptor(feed.fd());
  REQUIRE(in);
  CHECK(contentOf(*in) == data);
}

TEST_CASE("a descriptor reads compressed data decompressed", "[stdin]") {
  auto file = openGzFile(example);
  REQUIRE(file);
  const std::string expected = contentOf(*file);

  Feed feed(bytesOf(example));
  auto in = openGzDescriptor(feed.fd());
  REQUIRE(in);
  CHECK(contentOf(*in) == expected);
}

/* What a Windows text-mode descriptor would mangle: it turns CR LF into LF and
 * stops at ^Z. Everywhere else this has to hold just the same. */
TEST_CASE("a descriptor reads every byte as it is", "[stdin]") {
  const std::string data = "first\r\nsecond\x1athird\r\n";
  Feed feed(data);
  auto in = openGzDescriptor(feed.fd());
  REQUIRE(in);
  CHECK(contentOf(*in) == data);
}

/* More than any pipe buffers, of which one byte is read: the writer is still
 * blocked when the reader goes, and must come out of that with an error. */
TEST_CASE("a reader may stop before the data ends", "[stdin]") {
  Feed feed(std::string(4 << 20, 'x'));
  auto in = openGzDescriptor(feed.fd());
  REQUIRE(in);
  CHECK(in->get() == 'x');
}

TEST_CASE("the stream leaves the descriptor open", "[stdin]") {
  Feed feed("data");
  {
    auto in = openGzDescriptor(feed.fd());
    REQUIRE(in);
    CHECK(in->get() == 'd');
  }
  // gzclose() would have closed it, had the stream not read through a duplicate
  CHECK(feed.closeReader());
}

#ifdef _WIN32
TEST_CASE("the stream leaves the descriptor's mode alone", "[stdin]") {
  Feed feed("data");
  auto in = openGzDescriptor(feed.fd());
  REQUIRE(in);
  CHECK(_setmode(feed.fd(), _O_TEXT) == _O_TEXT);
}
#endif

TEST_CASE("a descriptor that is none gives no stream", "[stdin]") {
  CHECK_FALSE(openGzDescriptor(-1));
  CHECK_FALSE(openGzDescriptor(-2));         // Windows' stdin of a GUI program without one
}

TEST_CASE("a puzzle comes through whole", "[stdin]") {
  Feed feed(bytesOf(example));
  std::string error;
  auto in = btcli::openPuzzleInput(feed.fd(), error);
  REQUIRE(in);
  CHECK(error.empty());

  xmlParser_c pars(*in);
  puzzle_c p(pars);
  CHECK(p.getNumberOfShapes() > 0);
  CHECK(p.getNumberOfProblems() > 0);
}

TEST_CASE("an empty input says there is no puzzle", "[stdin]") {
  Feed feed("");
  std::string error;
  bool damaged = true;
  CHECK_FALSE(btcli::openPuzzleInput(feed.fd(), error, damaged));
  CHECK(error == "no puzzle on standard input");
  CHECK_FALSE(damaged);
}

TEST_CASE("compressed data damaged from the start says it is damaged", "[stdin]") {
  // the gzip signature, then a compression method that does not exist
  Feed feed(std::string("\x1f\x8b\x7f\x00\x00\x00\x00\x00\x00\x00 and no more", 22));
  std::string error;
  bool damaged = false;
  CHECK_FALSE(btcli::openPuzzleInput(feed.fd(), error, damaged));
  CHECK(error == "damaged compressed data on standard input");
  CHECK(damaged);
}

TEST_CASE("a stream tells damaged data from its end", "[stdin]") {
  SECTION("plain data read to its end") {
    Feed feed("plain");
    auto in = openGzDescriptor(feed.fd());
    REQUIRE(in);
    CHECK(contentOf(*in) == "plain");
    CHECK_FALSE(in->rdbuf()->readFailed());
  }
  SECTION("compressed data read to its end") {
    Feed feed(bytesOf(example));
    auto in = openGzDescriptor(feed.fd());
    REQUIRE(in);
    contentOf(*in);
    CHECK_FALSE(in->rdbuf()->readFailed());
  }
  SECTION("compressed data that does not match its checksum") {
    // a gzip member ends in the CRC-32 of its data, then the data's length
    std::string data = bytesOf(example);
    REQUIRE(data.size() > 8);
    data[data.size() - 8] ^= 0x55;
    Feed feed(data);
    auto in = openGzDescriptor(feed.fd());
    REQUIRE(in);
    contentOf(*in);
    CHECK(in->rdbuf()->readFailed());
  }
}

TEST_CASE("an input that can not be read says so", "[stdin]") {
  std::string error;
  bool damaged = true;
  CHECK_FALSE(btcli::openPuzzleInput(-1, error, damaged));
  CHECK(error == "could not read standard input");
  CHECK_FALSE(damaged);
}

TEST_CASE("no descriptor is no terminal", "[stdin]") {
  // never handed to _isatty(), which fails fast on Windows
  CHECK_FALSE(btcli::isTerminal(-1));
  CHECK_FALSE(btcli::isTerminal(-2));         // Windows' stdin of a GUI program without one
}

TEST_CASE("the file name - stands for standard input", "[stdin]") {
  CHECK(btcli::namesStdin("-"));
  CHECK_FALSE(btcli::namesStdin("--"));
  CHECK_FALSE(btcli::namesStdin("./-"));
  CHECK_FALSE(btcli::namesStdin(""));
  CHECK_FALSE(btcli::namesStdin("puzzle.xmpuzzle"));
  CHECK_FALSE(btcli::namesStdin(nullptr));
}

TEST_CASE("a solver given a file name reads that, given - standard input", "[stdin]") {
  CHECK(btcli::solverReadsStdin("-"));
  CHECK_FALSE(btcli::solverReadsStdin("puzzle.xmpuzzle"));
}
