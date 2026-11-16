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
#include "puzzleloader.h"

#include "problem.h"
#include "puzzle.h"
#include "solution.h"
#include "gridtype.h"

#include "../tools/xml.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <cstring>
#include <functional>
#include <istream>
#include <streambuf>
#include <vector>

namespace {

  /* Walks a saved puzzle's bytes and tells a sink which of them are a
   * problem's solutions: the content of a <solutions> element in a
   * <problem> in <problems>, which the k-th problem's loader reads. Outside
   * that content every '<' starts markup (an attribute value and a text
   * hold none), so tags are told from comments, processing instructions
   * and CDATA sections; what the scanner cannot be sure of clears `staged`
   * and ends the walk. Inside, only the ends of solutions are looked for. */
  class solutionsScanner {

  public:

    struct sink {
      virtual ~sink() = default;
      /* bytes outside the solutions, the <solutions> tags among them */
      virtual void outside(const char *, size_t) {}
      /* bytes of the problem's solutions */
      virtual void inside(unsigned, const char *, size_t) {}
      /* a solution ended (its "</solution>" told by inside() already);
       * false stops the walk */
      virtual bool cut(unsigned) { return true; }
      /* the problem's solutions ended */
      virtual bool end(unsigned) { return true; }
      /* bytes read so far */
      virtual void read(size_t) {}
    };

    explicit solutionsScanner(std::istream & in) : in(in), buf(1 << 20) {}

    /* false: the sink stopped it, or it could not be sure (staged) */
    bool run(sink & s);

    bool staged = true;
    unsigned problems = 0;         // <problem> elements in <problems>
    size_t bytes = 0;              // read
    size_t insideBytes = 0;

  private:

    // a tag test reads this far past its '<'
    static constexpr size_t kLook = 16;
    // what a solution's end tag and the solutions' start with
    static constexpr const char * kEndTag = "</solution";
    static constexpr size_t kEndTagLength = 10;

    enum mode_e { Outside, Inside };

    /* the pending span [mark, p) to the sink, the rest of the buffer to its
     * front, and read on behind it: at least `need` bytes from p unless the
     * input ends */
    void refill(sink & s, size_t need);
    void flush(sink & s);
    /* the name at p + skip, a whole name: the byte after it ends it */
    bool nameIs(size_t at, const char * name) const;
    /* p moved past `term`, false when the input ends first */
    bool skipPast(sink & s, const char * term);
    /* the start tag at p read to its '>', quotes respected: q its end */
    bool tagEnd(sink & s, size_t & q, bool & empty);

    std::istream & in;
    std::vector<char> buf;
    size_t p = 0, end = 0, mark = 0;
    bool eof = false;
    mode_e mode = Outside;
    unsigned problem = 0;          // the solutions' problem, inside
  };

  void solutionsScanner::flush(sink & s) {
    if (p > mark) {
      if (mode == Outside)
        s.outside(buf.data() + mark, p - mark);
      else {
        s.inside(problem, buf.data() + mark, p - mark);
        insideBytes += p - mark;
      }
    }
    mark = p;
  }

  void solutionsScanner::refill(sink & s, size_t need) {
    flush(s);
    const size_t keep = end - p;
    memmove(buf.data(), buf.data() + p, keep);
    p = mark = 0;
    end = keep;
    if (need > buf.size())
      buf.resize(need);
    while (end < need && !eof) {
      in.read(buf.data() + end, std::streamsize(buf.size() - end));
      const size_t got = size_t(in.gcount());
      if (got == 0) {
        eof = true;
        break;
      }
      end += got;
      bytes += got;
      s.read(bytes);
    }
  }

  bool solutionsScanner::nameIs(size_t at, const char * name) const {
    const size_t n = strlen(name);
    if (at + n >= end || memcmp(buf.data() + at, name, n) != 0)
      return false;
    const char c = buf[at + n];
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '>' || c == '/';
  }

  bool solutionsScanner::skipPast(sink & s, const char * term) {
    const size_t n = strlen(term);
    while (true) {
      const char * b = buf.data() + p;
      const char * e = buf.data() + end;
      const char * f = std::search(b, e, term, term + n);
      if (f != e) {
        p = size_t(f - buf.data()) + n;
        return true;
      }
      if (eof)
        return false;
      // keep what could be the terminator's start, read on
      p = end - std::min(end - p, n - 1);
      refill(s, n + 4096);
    }
  }

  bool solutionsScanner::tagEnd(sink & s, size_t & q, bool & empty) {
    while (true) {
      char quote = 0;
      for (q = p + 1; q < end; q++) {
        const char c = buf[q];
        if (quote) {
          if (c == quote)
            quote = 0;
        } else if (c == '"' || c == '\'')
          quote = c;
        else if (c == '>') {
          empty = buf[q - 1] == '/';
          return true;
        }
      }
      // a start tag longer than the buffer is not one this file writes
      if (eof || end - p >= buf.size())
        return false;
      refill(s, (end - p) + 4096);
    }
  }

  bool solutionsScanner::run(sink & s) {

    bool inProblems = false, inProblem = false, shapesRead = false;

    while (true) {
      if (end - p < kLook && !eof)
        refill(s, buf.size());
      if (p >= end)
        break;

      if (mode == Inside) {
        /* Only where a solution, or the solutions, end: a search that skips
         * through the bytes. An end tag in a comment or a CDATA section
         * among the solutions cuts in the wrong place; reading on from
         * there fails, and the whole read is then made (readDefinitions,
         * solutionReader_c::readWhole) */
        static const std::boyer_moore_horspool_searcher<const char *> endTag(kEndTag, kEndTag + kEndTagLength);
        const char * f = std::search(buf.data() + p, buf.data() + end, endTag);
        if (f == buf.data() + end) {
          // keep what could be the start of one, read on
          p = end - std::min(end - p, kEndTagLength - 1);
          if (eof) {
            p = end;
            break;
          }
          refill(s, buf.size());
          continue;
        }
        p = size_t(f - buf.data());
        if (end - p < kLook && !eof) {
          refill(s, buf.size());
          continue;
        }
        const char c = p + kEndTagLength < end ? buf[p + kEndTagLength] : 0;
        if (c == '>') {
          p += kEndTagLength + 1;
          flush(s);
          if (!s.cut(problem))
            return false;
          continue;
        }
        if (c == 's' && nameIs(p + 2, "solutions")) {
          flush(s);                // the content, the end tag outside
          mode = Outside;
          if (!s.end(problem))
            return false;
          p += kEndTagLength + 1;
          continue;
        }
        p += kEndTagLength;
        continue;
      }

      const char * b = buf.data() + p;
      const char * lt = static_cast<const char *>(memchr(b, '<', end - p));
      if (!lt) {
        p = end;
        continue;
      }
      p = size_t(lt - buf.data());
      if (end - p < kLook && !eof) {
        refill(s, buf.size());
        continue;
      }
      const char c1 = p + 1 < end ? buf[p + 1] : 0;

      // outside
      if (c1 == '!') {
        if (end - p >= 4 && memcmp(buf.data() + p, "<!--", 4) == 0) {
          if (!skipPast(s, "-->")) { staged = false; return false; }
        } else if (end - p >= 9 && memcmp(buf.data() + p, "<![CDATA[", 9) == 0) {
          if (!skipPast(s, "]]>")) { staged = false; return false; }
        } else {
          // a document type declaration can hold markup of its own
          staged = false;
          return false;
        }
        continue;
      }
      if (c1 == '?') {
        if (!skipPast(s, "?>")) { staged = false; return false; }
        continue;
      }
      if (c1 == '/') {
        if (nameIs(p + 2, "problems"))
          inProblems = false;
        else if (nameIs(p + 2, "problem"))
          inProblem = false;
        p += 2;
        continue;
      }

      if (nameIs(p + 1, "problems") || nameIs(p + 1, "problem") || nameIs(p + 1, "solutions")) {
        size_t q;
        bool empty = false;
        if (!tagEnd(s, q, empty)) { staged = false; return false; }
        if (nameIs(p + 1, "problems")) {
          inProblems = !empty;
        } else if (nameIs(p + 1, "problem")) {
          if (!inProblems || inProblem) { staged = false; return false; }
          problem = problems++;
          inProblem = !empty;
          shapesRead = false;
        } else if (!empty) {
          // a problem reads its solutions against the shapes before them
          if (!inProblem || !shapesRead) { staged = false; return false; }
          p = q + 1;
          flush(s);                // the start tag outside, the content inside
          mode = Inside;
          continue;
        } else if (!inProblem) {
          staged = false;
          return false;
        }
        p = q + 1;
        continue;
      }
      if (inProblem && nameIs(p + 1, "shapes"))
        shapesRead = true;
      p++;
    }

    p = end;
    flush(s);
    if (mode == Inside)
      staged = false;              // the input ended inside the solutions
    return staged;
  }

  /* a stream over bytes in memory, for xmlParser_c */
  class memoryBuf : public std::streambuf {
  public:
    memoryBuf(const char * b, size_t n) {
      char * p = const_cast<char *>(b);
      setg(p, p, p + n);
    }
  };

  /* the solutions in "<solutions>" + content + "</solutions>", as
   * problem_c's loader reads them */
  void readSolutions(const std::string & doc, unsigned pieces, const gridType_c * gt,
                     std::vector<std::unique_ptr<solution_c>> & out) {
    memoryBuf mb(doc.data(), doc.size());
    std::istream is(&mb);
    xmlParser_c pars(is);
    pars.nextTag();
    pars.require(xmlParser_c::START_TAG, "solutions");
    do {
      int state = pars.nextTag();
      if (state == xmlParser_c::END_TAG)
        break;
      pars.require(xmlParser_c::START_TAG, "");
      if (pars.getName() == "solution")
        out.push_back(std::make_unique<solution_c>(pars, pieces, gt));
      else
        pars.skipSubTree();
      pars.require(xmlParser_c::END_TAG, "solution");
    } while (true);
    pars.require(xmlParser_c::END_TAG, "solutions");
  }

  const std::string kOpen = "<solutions>";
  const std::string kClose = "</solutions>";

}

namespace puzzleLoader {

  definitions_t readDefinitions(std::istream & in, const opener & reopen, size_t threshold) {

    struct skeleton : solutionsScanner::sink {
      std::string text;
      void outside(const char * p, size_t n) override { text.append(p, n); }
    } sk;

    solutionsScanner scan(in);
    scan.run(sk);

    definitions_t d;
    d.bytes = scan.bytes;
    d.solutionBytes = scan.insideBytes;

    auto whole = [&] {
      std::unique_ptr<std::istream> again = reopen();
      if (!again)
        throw xmlParserException_c("the file could not be opened again");
      xmlParser_c pars(*again);
      d.puzzle = std::make_unique<puzzle_c>(pars);
      d.staged = false;
      return std::move(d);
    };

    // the puzzle from everything but the solutions; false when that does not read
    auto skeletonReads = [&] {
      try {
        memoryBuf mb(sk.text.data(), sk.text.size());
        std::istream is(&mb);
        xmlParser_c pars(is);
        d.puzzle = std::make_unique<puzzle_c>(pars);
      } catch (...) {
        d.puzzle.reset();
        return false;
      }
      return d.puzzle->getNumberOfProblems() == scan.problems;
    };

    if (!scan.staged)
      return whole();

    if (scan.insideBytes < threshold) {
      try {
        return whole();
      } catch (xmlParserException_c &) {
        /* a solution that does not read, when the rest does: the reader
         * reads the others and leaves it out (solutionReader_c::skipped) */
        if (!skeletonReads())
          throw;
        d.staged = true;
        return d;
      }
    }

    // what is wrong with it otherwise, as loading it whole says, with its position
    if (!skeletonReads())
      return whole();
    d.staged = true;
    return d;
  }

}

solutionReader_c::solutionReader_c(puzzle_c & p, puzzleLoader::opener r, size_t b, unsigned t, size_t batch) :
  puzzle(p), reopen(std::move(r)), bytes(b), threads(std::max(1u, t)), batchBytes(batch)
{
  // made on first use and not guarded: made here, the reader only reads it
  if (puzzle.getGridType())
    puzzle.getGridType()->getSymmetries();
  worker = std::jthread([this](std::stop_token stop) { run(stop); });
}

solutionReader_c::~solutionReader_c(void) {
  stop();
}

void solutionReader_c::stop(void) {
  if (worker.joinable()) {
    worker.request_stop();
    worker.join();
  }
}

double solutionReader_c::progress(void) const {
  if (done())
    return 1.0;
  return bytes ? std::min(1.0, double(scanned.load(std::memory_order_relaxed)) / double(bytes)) : 0.0;
}

std::string solutionReader_c::error(void) const {
  std::lock_guard<std::mutex> g(errorMutex);
  return errorText;
}

void solutionReader_c::run(std::stop_token stop) {

  /* A batch of a problem's solutions: its XML, then what it reads as. The
   * batches are made and put behind the lists in the file's order, read in
   * any, on `threads` threads of their own (none for one: read where made). */
  struct batch_t {
    problem_c * problem;
    unsigned pieces;
    std::string doc;
    std::vector<std::unique_ptr<solution_c>> solutions;
    unsigned skipped = 0;          // solutions that did not read, left out
    std::string why;               // what was wrong with the first
    bool done = false;
  };

  struct reader : solutionsScanner::sink {
    puzzle_c & puzzle;
    const gridType_c * gt;
    std::stop_token stop;
    std::atomic<size_t> & scanned;
    size_t batchBytes;
    unsigned threads;
    std::string doc = kOpen;
    bool failed = false;
    unsigned skipped = 0;
    std::string why;

    std::mutex m;
    std::condition_variable cv;                    // a batch read, or one to read
    std::deque<batch_t *> toRead;                  // not taken by a thread yet
    std::deque<std::unique_ptr<batch_t>> inOrder;  // made, not behind its list yet
    bool closing = false;
    std::vector<std::thread> parsers;

    reader(puzzle_c & p, std::stop_token s, std::atomic<size_t> & n, size_t b, unsigned t) :
      puzzle(p), gt(p.getGridType()), stop(std::move(s)), scanned(n), batchBytes(b), threads(t)
    {
      for (unsigned i = 0; threads > 1 && i < threads; i++)
        parsers.emplace_back([this] { parse(); });
    }

    ~reader() override {
      {
        std::lock_guard<std::mutex> g(m);
        closing = true;
        toRead.clear();
      }
      cv.notify_all();
      for (std::thread & t : parsers)
        t.join();
    }

    void readOne(batch_t & b) {
      try {
        readSolutions(b.doc, b.pieces, gt, b.solutions);
      } catch (...) {
        // one that does not read: the batch again, a solution at a time
        b.solutions.clear();
        readEach(b);
      }
      std::string().swap(b.doc);                   // its XML no longer needed
    }

    /* the batch's solutions one by one, from one solution's end to the
     * next: those that do not read are left out, and counted */
    void readEach(batch_t & b) {
      static const std::string kEnd = "</solution>";
      const size_t to = b.doc.size() - kClose.size();
      for (size_t at = kOpen.size(); at < to;) {
        size_t e = b.doc.find(kEnd, at);
        e = e == std::string::npos || e + kEnd.size() > to ? to : e + kEnd.size();
        std::string one = kOpen;
        one.append(b.doc, at, e - at);
        one += kClose;
        std::vector<std::unique_ptr<solution_c>> got;
        std::string wrong;
        try {
          readSolutions(one, b.pieces, gt, got);
        } catch (xmlParserException_c & x) {
          wrong = x.what();
        } catch (std::exception & x) {
          wrong = x.what();
        }
        if (wrong.empty())
          for (auto & sol : got)
            b.solutions.push_back(std::move(sol));
        else {
          b.skipped++;
          if (b.why.empty())
            b.why = wrong;
        }
        at = e;
      }
    }

    void parse(void) {
      std::unique_lock<std::mutex> l(m);
      while (true) {
        cv.wait(l, [&] { return closing || !toRead.empty(); });
        if (toRead.empty())
          return;
        batch_t * b = toRead.front();
        toRead.pop_front();
        l.unlock();
        readOne(*b);
        l.lock();
        b->done = true;
        cv.notify_all();
      }
    }

    /* the batches read, from the first, behind their lists; all: wait for
     * every one made. False when one did not read */
    bool settle(bool all) {
      while (true) {
        std::unique_ptr<batch_t> b;
        {
          std::unique_lock<std::mutex> l(m);
          if (all)
            cv.wait(l, [&] { return inOrder.empty() || inOrder.front()->done; });
          if (inOrder.empty() || !inOrder.front()->done)
            return true;
          b = std::move(inOrder.front());
          inOrder.pop_front();
        }
        if (b->skipped && skipped == 0)
          why = b->why;
        skipped += b->skipped;
        b->problem->appendLoadedSolutions(b->solutions);
      }
    }

    bool submit(unsigned k) {
      if (k >= puzzle.getNumberOfProblems()) {
        failed = true;
        return false;
      }
      auto b = std::make_unique<batch_t>();
      b->problem = puzzle.getProblem(k);
      b->pieces = b->problem->getNumberOfPieces();
      doc += kClose;
      b->doc.swap(doc);
      doc.assign(kOpen);
      if (parsers.empty()) {
        readOne(*b);
        b->done = true;
        inOrder.push_back(std::move(b));
      } else {
        {
          std::lock_guard<std::mutex> g(m);
          toRead.push_back(b.get());
          inOrder.push_back(std::move(b));
        }
        cv.notify_all();
        // no more in hand than this: their XML and solutions are held meanwhile
        std::unique_lock<std::mutex> l(m);
        cv.wait(l, [&] { return inOrder.size() < 2 * parsers.size() || inOrder.front()->done; });
      }
      return settle(false) && !stop.stop_requested();
    }

    void inside(unsigned, const char * p, size_t n) override { doc.append(p, n); }
    bool cut(unsigned k) override {
      if (stop.stop_requested())
        return false;
      return doc.size() < batchBytes || submit(k);
    }
    bool end(unsigned k) override { return submit(k); }
    void read(size_t n) override { scanned.store(n, std::memory_order_relaxed); }
  };

  bool ok = false;
  unsigned skipped = 0;
  std::string skippedWhy;
  try {
    std::unique_ptr<std::istream> in = reopen();
    if (in) {
      reader r(puzzle, stop, scanned, batchBytes, threads);
      solutionsScanner scan(*in);
      const bool walked = scan.run(r) && r.settle(true);
      if (stop.stop_requested()) {
        finished.store(true, std::memory_order_release);
        return;
      }
      // the same file as readDefinitions() read, every solution in it read or left out
      ok = walked && !r.failed && scan.staged && scan.problems == puzzle.getNumberOfProblems() && scan.bytes == bytes;
      skipped = r.skipped;
      skippedWhy = r.why;
    }
  } catch (...) {
    ok = false;
  }

  if (!stop.stop_requested() && (!ok || skipped > 0)) {
    /* Read whole, the file says what is wrong with it, and where. When that
     * reads, the batches were cut in the wrong place (an end tag in a
     * comment): its solutions are the file's. When it does not, the ones
     * left out stay out (skipped) -- or, when the batches did not even get
     * through the file, the reading failed (error) */
    std::string why;
    if (!readWhole(why)) {
      std::lock_guard<std::mutex> g(errorMutex);
      if (ok) {
        skippedCount = skipped;
        skipText = why.empty() ? skippedWhy : why;
      } else
        errorText = why;
    }
  }
  finished.store(true, std::memory_order_release);
}

unsigned solutionReader_c::skipped(void) const {
  std::lock_guard<std::mutex> g(errorMutex);
  return skippedCount;
}

std::string solutionReader_c::skipError(void) const {
  std::lock_guard<std::mutex> g(errorMutex);
  return skipText;
}

bool solutionReader_c::readWhole(std::string & why) {
  try {
    std::unique_ptr<std::istream> in = reopen();
    if (!in)
      why = "the file could not be opened again";
    else {
      xmlParser_c pars(*in);
      puzzle_c whole(pars);
      if (whole.getNumberOfProblems() != puzzle.getNumberOfProblems())
        why = "the file changed while it was read";
      else {
        for (unsigned k = 0; k < puzzle.getNumberOfProblems(); k++)
          puzzle.getProblem(k)->replaceLoadedSolutions(whole.getProblem(k)->takeSolutions());
        return true;
      }
    }
  } catch (xmlParserException_c & e) {
    why = e.what();
  } catch (std::exception & e) {
    why = e.what();
  }
  return false;
}
