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
#include "documentsession.h"

#include "../lib/converter.h"
#include "../lib/problem.h"
#include "../lib/ps3dloader.h"
#include "../lib/puzzle.h"
#include "../lib/puzzleloader.h"
#include "../lib/voxel.h"

#include "../tools/fileexists.h"
#include "../tools/gzstream.h"
#include "../tools/xml.h"

#include <algorithm>
#include <atomic>
#include <thread>
#include <fstream>
#include <random>
#include <string>

namespace btui {

  /* the reading of beginLoad(): its thread, and what it read */
  struct DocumentSession::Opening {
    std::filesystem::path file;
    FileRead read;
    std::atomic<bool> finished{false};
    std::jthread worker;        // last: it goes first, and the rest outlives it
  };

  DocumentSession::DocumentSession(void) :
    puz(std::make_unique<puzzle_c>(std::make_unique<gridType_c>())),
    hist(std::make_unique<puzzleHistory_c>())
  {
    hist->reset(puz.get());
  }

  DocumentSession::~DocumentSession(void) {
    stopOpening();
    stopLoading();
  }

  void DocumentSession::adopt(std::unique_ptr<puzzle_c> p) {
    stopOpening();                    // a file being opened would replace this one
    stopLoading();                    // it reads into the puzzle that goes
    stroking = strokeDirty = false;   // the history starts afresh: the stroke goes with the old puzzle
    strokeLost.clear();
    puz = std::move(p);
    hist->reset(puz.get());
  }

  void DocumentSession::newDocument(gridType_c::gridType type) {
    adopt(std::make_unique<puzzle_c>(std::make_unique<gridType_c>(type)));
    fname.clear();
  }

  DocumentSession::FileRead DocumentSession::readFile(const std::filesystem::path & file, size_t stageAbove) {

    FileRead f;
    LoadResult & r = f.result;

    if (file.empty() || !fileExists(file)) {
      r.error = "The file does not exist.";
      return f;
    }

    // openGzFile() can still fail after fileExists() passed: the file may
    // have gone in between, or gzopen() may fail to allocate
    auto str = openGzFile(file);
    if (!str) {
      r.error = "The file could not be opened.";
      return f;
    }

    const puzzleLoader::opener reopen = [file] { return openGzFile(file); };

    try {
      if (stageAbove == SIZE_MAX) {
        xmlParser_c pars(*str);
        f.puzzle = std::make_unique<puzzle_c>(pars);
      } else {
        puzzleLoader::definitions_t d = puzzleLoader::readDefinitions(*str, reopen, stageAbove);
        f.puzzle = std::move(d.puzzle);
        f.staged = d.staged;
        f.bytes = d.bytes;
      }
    }
    catch (xmlParserException_c & e) {
      r.error = std::string("load error: ") + e.what();
      return f;
    }

    for (unsigned int p = 0; p < f.puzzle->getNumberOfProblems(); p++)
      if (f.puzzle->getProblem(p)->getSolveState() == SS_SOLVING)
        r.containsStartedSearch = true;

    r.showComment = f.puzzle->getCommentPopup();
    r.ok = true;
    return f;
  }

  DocumentSession::LoadResult DocumentSession::adoptRead(FileRead && f, const std::filesystem::path & file) {
    LoadResult r = f.result;
    if (!r.ok)
      return r;
    adopt(std::move(f.puzzle));
    fname = file;
    if (f.staged) {
      reader = std::make_unique<solutionReader_c>(*puz, [file] { return openGzFile(file); }, f.bytes, loadThreads);
      r.solutionsPending = true;
    }
    return r;
  }

  DocumentSession::LoadResult DocumentSession::load(const std::filesystem::path & file) {
    stopOpening();
    return adoptRead(readFile(file, stageAbove), file);
  }

  DocumentSession::LoadResult DocumentSession::beginLoad(const std::filesystem::path & file, std::function<void()> done) {
    stopOpening();
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(file, ec);
    if (ec || size < openAsyncAbove)
      return load(file);        // a missing file says so at once, too
    opener = std::make_unique<Opening>();
    opener->file = file;
    Opening * o = opener.get();
    const size_t stage = stageAbove;
    o->worker = std::jthread([o, stage, done = std::move(done)] {
      try {
        o->read = readFile(o->file, stage);
      } catch (std::exception & e) {
        o->read = FileRead();
        o->read.result.error = e.what();
      }
      o->finished.store(true, std::memory_order_release);
      if (done)
        done();
    });
    LoadResult r;
    r.ok = true;
    r.opening = true;
    return r;
  }

  bool DocumentSession::openingDone(void) const {
    return opener && opener->finished.load(std::memory_order_acquire);
  }

  DocumentSession::LoadResult DocumentSession::finishOpening(void) {
    if (!opener) {
      LoadResult r;
      r.error = "Nothing is being opened.";
      return r;
    }
    opener->worker.join();
    std::unique_ptr<Opening> o = std::move(opener);
    return adoptRead(std::move(o->read), o->file);
  }

  void DocumentSession::stopOpening(void) {
    opener.reset();             // waits for the reading, which it drops
  }

  double DocumentSession::loadProgress(void) const {
    return reader ? reader->progress() : 1.0;
  }

  DocumentSession::LoadEnd DocumentSession::finishLoading(void) {
    LoadEnd e;
    if (!reader || !reader->done())
      return e;
    e.done = true;
    const std::string why = reader->error();
    e.skipped = reader->skipped();
    if (e.skipped)
      e.skipError = "load error: " + reader->skipError();
    reader.reset();
    if (!why.empty()) {
      e.error = "load error: " + why;
      newDocument(puz->getGridType()->getType());
    }
    return e;
  }

  void DocumentSession::stopLoading(void) {
    reader.reset();          // stops it and waits
  }

  DocumentSession::LoadResult DocumentSession::importPuzzleSolver3D(const std::filesystem::path & file) {

    LoadResult r;

    std::ifstream in(file);
    if (!in) {
      r.error = "The file could not be opened.";
      return r;
    }

    auto np = loadPuzzlerSolver3D(&in);
    if (!np) {
      r.error = "Could not load puzzle, sorry!";
      return r;
    }

    // not under the .puz name: save() would write xmpuzzle over the source
    // file without asking; like convert(), the result counts as unsaved
    adopt(std::move(np));
    fname.clear();
    hist->markModified();
    r.ok = true;
    return r;
  }

  bool DocumentSession::convert(gridType_c::gridType to) {
    if (loadingSolutions())
      return false;
    puzzle_c * p = doConvert(puz.get(), to);
    if (!p)
      return false;
    adopt(std::unique_ptr<puzzle_c>(p));
    hist->markModified();
    return true;
  }

  std::filesystem::path DocumentSession::withPuzzleExtension(const std::filesystem::path & file) {
    if (file.extension() == ".xmpuzzle")
      return file;
    std::filesystem::path out = file;
    out += ".xmpuzzle";
    return out;
  }

  /* Write the puzzle the way legacy cb_Save does. ogzstream takes a narrow
   * name, so on Windows this has the legacy limitation for paths outside the
   * active code page.
   *
   * Whether the file opened is asked of the stream buffer, not the stream:
   * ogzstream's std::ostream base runs init() after gzstreambase has set
   * badbit for a failed open and resets it to good (the trap openGzFile()
   * documents for reading). `if (!ostr)` therefore never catches an
   * unwritable path -- which is why legacy cb_Save reports such a save as a
   * success.
   *
   * Written to a copy first, beside the file or, in a folder that cannot be
   * written to, in the system's temporary folder, and then copied over the
   * file: a save that fails while writing (a full disk, an error thrown)
   * leaves the previous file as it was, where writing in place had already
   * truncated it. Copied, not renamed over it, so the file stays the same
   * file: a symbolic link to it still points to it, and its permissions,
   * owner, ACLs and other hard links are kept. The copy is removed, unless
   * copying it over failed part way: then it is the only complete one.
   */
  static bool writePuzzle(const puzzle_c & p, const std::filesystem::path & file) {
    // the copy goes on every way out but a copy over the file that failed
    class Discard {
    public:
      explicit Discard(const std::filesystem::path & f) : file(f) {}
      Discard(const Discard &) = delete;
      Discard & operator=(const Discard &) = delete;
      ~Discard() {
        std::error_code ec;
        if (!keep)
          std::filesystem::remove(file, ec);
      }
      bool keep = false;
    private:
      const std::filesystem::path & file;
    };
    // named once it is open, so that a file in the way is not removed; and
    // the stream, declared after, is closed before the copy is removed
    std::filesystem::path tmp;
    Discard discard(tmp);
    std::filesystem::path beside = file;
    beside += ".tmp";
    auto ostr = std::make_unique<ogzstream>(beside.string().c_str());
    if (ostr->rdbuf()->is_open()) {
      tmp = beside;
    } else {
      std::error_code ec;
      std::random_device rd;
      const std::filesystem::path other = std::filesystem::temp_directory_path(ec) /
          (file.filename().string() + "." + std::to_string(rd()) + ".tmp");
      if (ec)
        return false;
      ostr = std::make_unique<ogzstream>(other.string().c_str());
      if (!ostr->rdbuf()->is_open())
        return false;
      tmp = other;
    }
    {
      xmlWriter_c xml(*ostr);
      p.save(xml);
      ostr->close();
      if (!*ostr)
        return false;
      ostr.reset();
    }
    std::ifstream in(tmp, std::ios::binary);
    if (!in)
      return false;
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out)
      return false;                                // the file is untouched
    out << in.rdbuf();
    out.close();
    if (!out || in.bad()) {
      discard.keep = true;
      return false;
    }
    return true;
  }

  bool DocumentSession::save(void) {
    // the file would lose the solutions not read yet
    if (fname.empty() || loadingSolutions())
      return false;
    splitStroke();
    if (!writePuzzle(*puz, fname))
      return false;
    hist->markSaved();
    return true;
  }

  bool DocumentSession::saveAs(const std::filesystem::path & file) {
    if (loadingSolutions())
      return false;
    std::filesystem::path target = withPuzzleExtension(file);
    splitStroke();
    if (!writePuzzle(*puz, target))
      return false;
    fname = target;
    hist->markSaved();
    return true;
  }

  bool DocumentSession::isModified(void) const { return hist->isModifiedFromSave() || strokeChanged(); }

  void DocumentSession::markModified(void) { hist->markModified(); }

  /* Legacy changed the comment without marking the puzzle modified, so an
   * edited comment could be lost without a prompt. The comment is part of
   * the file, so here it does mark it -- still without an undo step, as the
   * comment is not puzzle structure.
   */
  void DocumentSession::setComment(const std::string & comment) {
    if (comment == puz->getComment())
      return;
    puz->setComment(comment);
    hist->markModified();
  }

  bool DocumentSession::canUndo(void) const { return hist->canUndo() || strokeChanged(); }
  bool DocumentSession::canRedo(void) const { return hist->canRedo(); }

  puzzleHistory_c::undoResult_t DocumentSession::undo(void) {
    endStroke();
    const long held = solutionsHeld();
    const puzzleHistory_c::undoResult_t r = hist->undo(puz.get());
    noteSolutionsLost(held);
    return r;
  }

  puzzleHistory_c::undoResult_t DocumentSession::redo(void) {
    endStroke();
    const long held = solutionsHeld();
    const puzzleHistory_c::undoResult_t r = hist->redo(puz.get());
    noteSolutionsLost(held);
    return r;
  }

  void DocumentSession::beginStep(const puzzleHistory_c::selection_t & selection, unsigned int coalesceKey) {
    hist->beginStep(selection, coalesceKey);
  }

  void DocumentSession::endStep(const puzzleHistory_c::selection_t & selection) {
    hist->endStep(selection);
  }

  long DocumentSession::solutionsHeld(void) const {
    long held = 0;
    for (unsigned int i = 0; i < puz->getNumberOfProblems(); i++) {
      const problem_c * pr = puz->getProblem(i);
      held += long(pr->getNumberOfSavedSolutions()) + (pr->getSolveState() == SS_UNSOLVED ? 0 : 1);
    }
    return held + hist->solutionsKept() + puzzleHistory_c::solutionsIn(strokeLost);
  }

  void DocumentSession::noteSolutionsLost(long heldBefore) {
    if (solutionsHeld() < heldBefore)
      hist->markModified();
  }

  void DocumentSession::beginStroke(puzzleHistory_c::actionKind_e kind, unsigned int selectedShape) {
    endStroke();
    stroking = true;
    strokeDirty = false;
    strokeKind = kind;
    strokeShape = selectedShape;
    hist->beginStroke(kind);
  }

  bool DocumentSession::endStroke(void) {
    if (!stroking)
      return false;
    stroking = false;
    strokeDirty = false;
    const bool took = hist->endStroke(puz.get(), strokeShape);
    keepDiscarded(std::move(strokeLost), took);
    strokeLost.clear();
    return took;
  }

  void DocumentSession::markStrokeChanged(void) {
    strokeDirty = true;
    hist->markStrokeDirty();
    if (strokeShape != puzzleHistory_c::NO_SHAPE)
      for (auto & l : discardUsing({ strokeShape }))
        strokeLost.push_back(std::move(l));
  }

  DocumentSession::ShapeCopies::ShapeCopies(void) = default;
  DocumentSession::ShapeCopies::~ShapeCopies(void) = default;
  DocumentSession::ShapeCopies::ShapeCopies(ShapeCopies &&) noexcept = default;

  DocumentSession::ShapeCopies DocumentSession::copyShapes(const std::vector<unsigned int> & shapes) const {
    ShapeCopies copies;
    copies.shapes.reserve(shapes.size());
    for (unsigned int s : shapes)
      if (s < puz->getNumberOfShapes())
        copies.shapes.emplace_back(s, std::unique_ptr<voxel_c>(puz->getGridType()->getVoxel(*puz->getShape(s))));
    return copies;
  }

  puzzleHistory_c::discarded_t DocumentSession::discardChanged(const ShapeCopies & before) {
    std::vector<unsigned int> changed;
    for (const auto & [s, copy] : before.shapes)
      if (s >= puz->getNumberOfShapes() || !(*puz->getShape(s) == *copy))
        changed.push_back(s);
    return discardUsing(changed);
  }

  puzzleHistory_c::discarded_t DocumentSession::discardUsing(const std::vector<unsigned int> & shapes) {
    puzzleHistory_c::discarded_t lost;
    if (shapes.empty())
      return lost;
    for (unsigned int k = 0; k < puz->getNumberOfProblems(); k++) {
      problem_c * pr = puz->getProblem(k);
      bool uses = false;
      for (unsigned int s : shapes)
        uses = uses || pr->usesShape(s);
      if (!uses)
        continue;
      problem_c::solveRecord_t r;
      pr->swapSolveRecord(r);
      if (!r.empty())
        lost.emplace_back(k, std::move(r));
    }
    return lost;
  }

  puzzleHistory_c::discarded_t DocumentSession::takeSolutions(const std::vector<unsigned int> & problems) {
    puzzleHistory_c::discarded_t taken;
    for (unsigned int k : problems) {
      if (k >= puz->getNumberOfProblems())
        continue;
      problem_c::solveRecord_t r;
      puz->getProblem(k)->swapSolveRecord(r);
      if (!r.empty())
        taken.emplace_back(k, std::move(r));
    }
    return taken;
  }

  void DocumentSession::keepDiscarded(puzzleHistory_c::discarded_t && lost, bool recorded) {
    if (lost.empty())
      return;
    if (recorded)
      switch (hist->keepDiscarded(std::move(lost))) {
        case puzzleHistory_c::KEPT:
          return;
        case puzzleHistory_c::NO_ROOM:
          // too big for the history: gone, and the file still has them
          lost.clear();
          hist->markModified();
          return;
        case puzzleHistory_c::NO_STEP:
          break;
      }
    // no step to keep them in: the edit changed nothing after all
    for (auto & [k, r] : lost)
      if (k < puz->getNumberOfProblems())
        puz->getProblem(k)->swapSolveRecord(r);
  }

  bool DocumentSession::keepsSolutionsOf(const std::vector<unsigned int> & shapes) const {
    size_t bytes = puzzleHistory_c::bytesIn(strokeLost);
    for (unsigned int k = 0; k < puz->getNumberOfProblems(); k++) {
      const problem_c * pr = puz->getProblem(k);
      if (std::any_of(shapes.begin(), shapes.end(), [pr](unsigned int s) { return pr->usesShape(s); }))
        bytes += sizeof(puzzleHistory_c::discarded_t::value_type) + pr->solveRecordBytes();
    }
    return puzzleHistory_c::roomForSolutions(bytes);
  }

  bool DocumentSession::keepsSolutionsOfProblems(const std::vector<unsigned int> & problems) const {
    size_t bytes = 0;
    for (unsigned int k : problems)
      if (k < puz->getNumberOfProblems())
        bytes += sizeof(puzzleHistory_c::discarded_t::value_type) + puz->getProblem(k)->solveRecordBytes();
    return puzzleHistory_c::roomForSolutions(bytes);
  }

  void DocumentSession::splitStroke(void) {
    if (!stroking)
      return;
    const puzzleHistory_c::actionKind_e kind = strokeKind;
    const unsigned int shape = strokeShape;
    endStroke();
    beginStroke(kind, shape);
  }

  void DocumentSession::record(puzzleHistory_c::actionKind_e kind, unsigned int selectedShape) {
    hist->record(puz.get(), kind, selectedShape);
  }

  void DocumentSession::setUndoDepth(unsigned int depth) { hist->setMaxUndo(depth); }
}
