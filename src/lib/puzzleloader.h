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
#ifndef __PUZZLELOADER_H__
#define __PUZZLELOADER_H__

#include <atomic>
#include <cstddef>
#include <functional>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

class puzzle_c;

/* A saved puzzle read in two steps, for a front end that shows a large one
 * before its solutions are all in. The solutions are nearly all of such a
 * file (a 42,873-solution tiling: 578 MB of XML, 448 bytes of it the
 * shapes and the problem), and each problem's come after its definition:
 *
 *   readDefinitions() reads the file once, quickly, without parsing the
 *   content of the problems' <solutions> elements, and builds the puzzle
 *   from the rest: every problem as loading it whole makes it, its
 *   solution list empty.
 *
 *   solutionReader_c then reads the file again on a thread of its own and
 *   puts each problem's solutions behind its list, in the file's order, a
 *   batch at a time under the problem's solutions lock.
 *
 * The result is the puzzle loading it whole gives (puzzle_c(xmlParser_c&)),
 * and so is an error: what the staged read is not sure of (a problem's
 * solutions before its shapes, a document type declaration, markup out of
 * place) it leaves to the whole load. A solution that does not read is
 * left out, the others read on, and the reader reads the file whole at the
 * end: when that reads, the batches were cut in the wrong place (an end tag
 * in a comment) and its solutions are the file's; when it does not, the
 * solutions left out are counted, with what the whole read says of them.
 */
namespace puzzleLoader {

  /* a fresh stream over the file from its start; null when it does not open */
  using opener = std::function<std::unique_ptr<std::istream>(void)>;

  struct definitions_t {
    /* the puzzle: without its solutions when staged, whole otherwise */
    std::unique_ptr<puzzle_c> puzzle;
    /* its solutions are for a solutionReader_c to read */
    bool staged = false;
    /* the document's length uncompressed, and how much of it is solutions */
    size_t bytes = 0;
    size_t solutionBytes = 0;
  };

  /* Read the puzzle from `in`: staged when the file allows it and its
   * solutions run to `threshold` bytes or more, whole otherwise (then from
   * a stream `reopen` makes). Throws what loading it whole throws. */
  definitions_t readDefinitions(std::istream & in, const opener & reopen, size_t threshold);
}

/* Reads a staged puzzle's solutions (puzzleLoader above) on a thread of its
 * own, from the moment it is made. The puzzle must outlive it and stay as
 * readDefinitions() left it but for its solution lists, which the reader
 * adds to under each problem's lock: a front end keeps the puzzle from
 * being edited meanwhile, and reads the lists under the lock, as while a
 * solver runs. */
class solutionReader_c {

public:

  /* bytes: definitions_t::bytes, for the progress. The file is read in
   * batches of a solution or more, from about batchBytes of XML, each put
   * behind its list in the file's order; `threads` read them (one: the
   * reader's own thread does) */
  solutionReader_c(puzzle_c & puzzle, puzzleLoader::opener reopen, size_t bytes, unsigned threads = 1,
                   size_t batchBytes = 512 * 1024);
  /* stops the reading and waits for it */
  ~solutionReader_c(void);

  /* stop and wait: the lists keep what was read */
  void stop(void);

  /* all read, or given up: then error() says which */
  bool done(void) const { return finished.load(std::memory_order_acquire); }
  /* how far into the file the reading is, 0 to 1 */
  double progress(void) const;
  /* after done(): empty when the file was read through, else what reading
   * the file whole says is wrong with it (the lists then hold what was read) */
  std::string error(void) const;
  /* after done(): the solutions that did not read and were left out, the
   * file read through otherwise, and what reading it whole says of them */
  unsigned skipped(void) const;
  std::string skipError(void) const;

  solutionReader_c(const solutionReader_c &) = delete;
  solutionReader_c & operator=(const solutionReader_c &) = delete;

private:

  void run(std::stop_token stop);
  /* the solutions as loading the file whole reads them, for a reading that
   * went wrong; false, with why, when that goes wrong too */
  bool readWhole(std::string & why);

  puzzle_c & puzzle;
  puzzleLoader::opener reopen;
  size_t bytes;
  unsigned threads;
  size_t batchBytes;

  std::atomic<size_t> scanned{0};
  std::atomic<bool> finished{false};
  mutable std::mutex errorMutex;
  std::string errorText;
  unsigned skippedCount = 0;
  std::string skipText;

  std::jthread worker;
};

#endif
