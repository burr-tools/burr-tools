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

#include "../tools/fileexists.h"
#include "../tools/gzstream.h"
#include "../tools/xml.h"

#include <fstream>

namespace btui {

  DocumentSession::DocumentSession(void) :
    puz(std::make_unique<puzzle_c>(std::make_unique<gridType_c>())),
    hist(std::make_unique<puzzleHistory_c>())
  {
    hist->reset(puz.get());
  }

  DocumentSession::~DocumentSession(void) = default;

  void DocumentSession::adopt(std::unique_ptr<puzzle_c> p) {
    puz = std::move(p);
    hist->reset(puz.get());
  }

  void DocumentSession::newDocument(gridType_c::gridType type) {
    adopt(std::make_unique<puzzle_c>(std::make_unique<gridType_c>(type)));
    fname.clear();
  }

  DocumentSession::LoadResult DocumentSession::load(const std::filesystem::path & file) {

    LoadResult r;

    if (file.empty() || !fileExists(file)) {
      r.error = "The file does not exist.";
      return r;
    }

    // openGzFile() can still fail after fileExists() passed: the file may
    // have gone in between, or gzopen() may fail to allocate
    auto str = openGzFile(file);
    if (!str) {
      r.error = "The file could not be opened.";
      return r;
    }

    std::unique_ptr<puzzle_c> np;

    try {
      xmlParser_c pars(*str);
      np = std::make_unique<puzzle_c>(pars);
    }
    catch (xmlParserException_c & e) {
      r.error = std::string("load error: ") + e.what();
      return r;
    }

    for (unsigned int p = 0; p < np->getNumberOfProblems(); p++)
      if (np->getProblem(p)->getSolveState() == SS_SOLVING)
        r.containsStartedSearch = true;

    r.showComment = np->getCommentPopup();

    adopt(std::move(np));
    fname = file;
    r.ok = true;
    return r;
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

    adopt(std::move(np));
    fname = file;
    r.ok = true;
    return r;
  }

  bool DocumentSession::convert(gridType_c::gridType to) {
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
   */
  static bool writePuzzle(const puzzle_c & p, const std::filesystem::path & file) {
    ogzstream ostr(file.string().c_str());
    if (!ostr.rdbuf()->is_open())
      return false;
    xmlWriter_c xml(ostr);
    p.save(xml);
    ostr.close();
    return static_cast<bool>(ostr);
  }

  bool DocumentSession::save(void) {
    if (fname.empty())
      return false;
    if (!writePuzzle(*puz, fname))
      return false;
    hist->markSaved();
    return true;
  }

  bool DocumentSession::saveAs(const std::filesystem::path & file) {
    std::filesystem::path target = withPuzzleExtension(file);
    if (!writePuzzle(*puz, target))
      return false;
    fname = target;
    hist->markSaved();
    return true;
  }

  bool DocumentSession::isModified(void) const { return hist->isModifiedFromSave(); }

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

  bool DocumentSession::canUndo(void) const { return hist->canUndo(); }
  bool DocumentSession::canRedo(void) const { return hist->canRedo(); }

  puzzleHistory_c::undoResult_t DocumentSession::undo(void) { return hist->undo(puz.get()); }
  puzzleHistory_c::undoResult_t DocumentSession::redo(void) { return hist->redo(puz.get()); }

  void DocumentSession::record(puzzleHistory_c::actionKind_e kind, unsigned int selectedShape) {
    hist->record(puz.get(), kind, selectedShape);
  }

  void DocumentSession::setUndoDepth(unsigned int depth) { hist->setMaxUndo(depth); }
}
