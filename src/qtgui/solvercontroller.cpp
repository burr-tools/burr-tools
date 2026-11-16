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
#include "solvercontroller.h"

#include "guarded.h"
#include "layoutcontroller.h"
#include "puzzlecontroller.h"
#include "settingscontroller.h"
#include "theme.h"

#include "../uicore/palette.h"
#include "../uicore/problemlist.h"

#include "../lib/assembler.h"
#include "../lib/assembly.h"
#include "../lib/disassembler_0.h"
#include "../lib/disassembly.h"
#include "../lib/disasmtomoves.h"
#include "../lib/gridtype.h"
#include "../lib/problem.h"
#include "../lib/puzzle.h"
#include "../lib/solution.h"
#include "../lib/solvethread.h"
#include "../lib/voxel.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <set>
#include <utility>

namespace {

  constexpr int kPollMs = 100;       // the counters and the bar, ten times a second (legacy: once a second)
  constexpr qint64 kListMs = 200;    // the list during a solve: five times a second,
  constexpr qint64 kListSlowMs = 500;   // twice past this many solutions
  constexpr size_t kManySolutions = 2000;
  constexpr int kFrameMs = 16;       // the player: at most once a frame

  QColor toQColor(btui::Rgb c) {
    return QColor::fromRgbF(c.r, c.g, c.b);
  }

  QString timeWords(double seconds) {
    const btui::TimeText t = btui::timeText(seconds);
    using U = btui::TimeText::Unit;
    switch (t.unit) {
      case U::Seconds:   return int(t.value) == 1 ? SolverController::tr("1 second")
                                                  : SolverController::tr("%1 seconds").arg(int(t.value));
      case U::Minutes:   return SolverController::tr("%1 minutes").arg(t.value, 0, 'f', 1);
      case U::Hours:     return SolverController::tr("%1 hours").arg(t.value, 0, 'f', 1);
      case U::Days:      return SolverController::tr("%1 days").arg(t.value, 0, 'f', 1);
      case U::Months:    return SolverController::tr("%1 months").arg(t.value, 0, 'f', 1);
      case U::Years:     return SolverController::tr("%1 years").arg(t.value, 0, 'f', 1);
      case U::Millennia: return SolverController::tr("%1 millennia").arg(t.value, 0, 'f', 1);
      case U::Ages:      return SolverController::tr("ages");
    }
    return QString();
  }

  btui::SolverKey solverKeyOf(int key, bool & known) {
    known = true;
    switch (key) {
      case Qt::Key_Space:     return btui::SolverKey::Space;
      case Qt::Key_Return:
      case Qt::Key_Enter:     return btui::SolverKey::Enter;
      case Qt::Key_Left:      return btui::SolverKey::Left;
      case Qt::Key_Right:     return btui::SolverKey::Right;
      case Qt::Key_Up:        return btui::SolverKey::Up;
      case Qt::Key_Down:      return btui::SolverKey::Down;
      case Qt::Key_Home:      return btui::SolverKey::Home;
      case Qt::Key_End:       return btui::SolverKey::End;
      case Qt::Key_PageUp:    return btui::SolverKey::PageUp;
      case Qt::Key_PageDown:  return btui::SolverKey::PageDown;
      case Qt::Key_Delete:    return btui::SolverKey::Delete;
      case Qt::Key_Backspace: return btui::SolverKey::Backspace;
      case Qt::Key_Escape:    return btui::SolverKey::Escape;
      default:
        known = false;
        return btui::SolverKey::Escape;
    }
  }
}

/* Manage ▸ Analyse: the assemblies copied, disassembled one after another on
 * a thread of its own; the results are handed over under a mutex and stored
 * on the GUI thread, under the problem's solution lock. The document is busy
 * meanwhile, so the problem the disassembler reads does not change. */
struct SolverController::Analysis {
  int problem = -1;
  int total = 0;
  std::atomic<int> done{ 0 };
  std::mutex mutex;
  struct Result {
    unsigned int assembly;
    std::unique_ptr<separation_c> tree;     ///< null: cannot be taken apart
  };
  std::vector<Result> results;              ///< under mutex
  std::atomic<bool> failed{ false };
  std::jthread worker;
};

SolverController::SolverController(SettingsController * settings, DocumentController * doc, LayoutController * layout,
                                   PuzzleController * puzzle, QObject * parent) :
  QObject(parent), m_settings(settings), m_doc(doc), m_layout(layout), m_puzzle(puzzle),
  m_rows(new RowsModel({ "number", "level", "moves", "movesFraction", "pieces", "da", "willDelete" }, this)),
  m_pieces(new RowsModel({ "pid", "chipColor", "pieceState", "tip" }, this))
{
  m_placements = new RowsModel({ "pid", "chipColor", "count", "placed" }, this);
  m_poll.setInterval(kPollMs);
  connect(&m_poll, &QTimer::timeout, this, &SolverController::poll);
  m_play.setInterval(kFrameMs);
  connect(&m_play, &QTimer::timeout, this, &SolverController::tick);
  m_sceneTimer.setSingleShot(true);
  m_sceneTimer.setInterval(kFrameMs);
  connect(&m_sceneTimer, &QTimer::timeout, this, [this] {
    buildScene();
    emit selectionChanged();
  });

  connect(m_doc, &DocumentController::documentReplaced, this, [this] {
    forgetProblemState(Forget::Rebuilt);
    m_problem = -1;
    documentChanged();
  });
  // an edit may renumber or rebuild problems: what was kept per position goes
  connect(m_doc, &DocumentController::puzzleEdited, this, [this](int, DocumentController::Change c) {
    forgetProblemState(c == DocumentController::Change::Structure ? Forget::Structure : Forget::Edited);
    if (shown())
      documentChanged();
    else
      m_stale = true;
  });
  connect(m_doc, &DocumentController::historyApplied, this, [this] {
    forgetProblemState(Forget::Rebuilt);
    if (shown())
      documentChanged();
    else
      m_stale = true;
  });
  // results changed elsewhere (or here: refreshing twice costs little); they
  // can change without the count changing (Analyse), so read them all
  // a large file's solutions coming in: the list follows as during a solve
  connect(m_doc, &DocumentController::loadChanged, this, [this] {
    if (m_doc->loadingSolutions() && !m_poll.isActive())
      m_poll.start();
    refreshRun();
  });
  connect(m_doc, &DocumentController::resultsChanged, this, [this] {
    if (!shown()) {
      m_stale = true;
      return;
    }
    refreshRun();
    refreshSolutions(true);
  });
  connect(m_layout, &LayoutController::changed, this, [this] {
    if (!shown())
      return;
    if (std::exchange(m_stale, false))
      documentChanged();
    else if (m_listStale)
      refreshSolutions(false);     // what a solve added meanwhile
    adoptPuzzleProblem();
  });
  // the voxel style and the shape colours make the scene anew; other settings do not
  auto restyled = [this] {
    if (!m_sceneBuilt || (m_settings->voxelStyleValue() == m_builtStyle && Theme::shapeColours() == m_builtColours))
      return;
    if (shown()) {
      buildScene();
      emit selectionChanged();
    } else
      m_stale = true;
  };
  connect(m_settings, &SettingsController::changed, this, restyled);
  if (const Theme * t = Theme::instance())
    connect(t, &Theme::shapeColoursChanged, this, restyled);

  documentChanged();
}

SolverController::~SolverController() {
  // App stops them before the document goes (stopAndWait); this is the last guard
  if (m_analysis) {
    m_analysis->worker.request_stop();
    if (m_analysis->worker.joinable())
      m_analysis->worker.join();
  }
  m_analysis.reset();
  m_thread.reset();
}

void SolverController::stopAndWait(void) {
  guarded([&] {
    if (m_analysis) {
      m_analysis->worker.request_stop();
      if (m_analysis->worker.joinable())
        m_analysis->worker.join();
      finishAnalysis();               // stores what was done; the rest stays unchecked
      m_analysis.reset();
    }
    if (m_thread) {
      m_thread->stop();
      m_thread->joinThread();
      retireThread(true);
    }
    m_poll.stop();
    updateBusy();
    refreshRun();
  });
}

void SolverController::forgetProblemState(Forget how) {
  // the problems may be others at the same addresses now
  m_list.clear();
  m_prepared.clear();
  m_cannot.clear();
  m_floor.clear();
  // the assembler it showed is gone with the edit, or belongs to another puzzle
  if (m_assemblerState) {
    m_assemblerState.reset();
    m_sceneBuilt = false;
  }
  closeExplorer();
  if (how == Forget::Edited)
    return;
  if (how == Forget::Rebuilt)
    m_optionsOf.clear();
  else {
    // the problems that went: their options with them (a new problem may reuse the address)
    const puzzle_c & p = m_doc->session().puzzle();
    std::set<const problem_c *> live;
    for (unsigned i = 0; i < p.getNumberOfProblems(); i++)
      live.insert(p.getProblem(i));
    std::erase_if(m_optionsOf, [&](const auto & e) { return !live.contains(e.first); });
  }
  m_hasSelection = false;
  if (m_problem >= 0) {
    m_options = optionsFor(m_problem);
    emit optionsChanged();
  }
}

const btui::SolutionFacts * SolverController::selectedFacts(void) const {
  if (m_row < 0 || size_t(m_row) >= m_list.shown().size())
    return nullptr;
  const size_t i = size_t(m_list.shown()[size_t(m_row)]);
  return i < m_list.all().size() ? &m_list.all()[i] : nullptr;
}

bool SolverController::shown(void) const {
  return m_layout->workspace() == LayoutController::Solver;
}

/* A problem's options this session; one not seen yet checks the disassembly
 * when its stored results were found that way, so the list shows them as
 * they were kept */
btui::SolverOptions SolverController::optionsFor(int problem) const {
  const puzzle_c & p = m_doc->session().puzzle();
  if (problem >= 0 && unsigned(problem) < p.getNumberOfProblems())
    if (auto it = m_optionsOf.find(p.getProblem(unsigned(problem))); it != m_optionsOf.end())
      return it->second;
  btui::SolverOptions o;
  if (problem >= 0 && unsigned(problem) < p.getNumberOfProblems() && canDisassemble()) {
    const problem_c * pr = p.getProblem(unsigned(problem));
    auto lock = pr->lockSolutions();
    for (unsigned i = 0; i < pr->getNumberOfSavedSolutions(); i++)
      if (pr->getSavedSolution(i)->getDisassemblyInfo()) {
        o.disassemble = true;
        break;
      }
  }
  return o;
}

problem_c * SolverController::currentProblem(void) const {
  puzzle_c & p = const_cast<puzzle_c &>(m_doc->session().puzzle());
  if (m_problem < 0 || unsigned(m_problem) >= p.getNumberOfProblems())
    return nullptr;
  return p.getProblem(unsigned(m_problem));
}

void SolverController::documentChanged(void) {
  const int n = int(m_doc->session().puzzle().getNumberOfProblems());
  if (m_problem >= n || (m_problem < 0 && n > 0)) {
    m_problem = n > 0 ? std::clamp(m_puzzle->problem(), 0, n - 1) : -1;
    m_options = optionsFor(m_problem);
    m_hasSelection = false;
    emit optionsChanged();
  }
  emit problemChanged();
  refreshRun();
  refreshSolutions(true);
}

void SolverController::adoptPuzzleProblem(void) {
  // the Solver opens on the problem the Puzzle workspace shows, unless a solve runs
  if (m_thread || m_analysis)
    return;
  const int p = m_puzzle->problem();
  if (p >= 0 && p != m_problem)
    selectProblem(p);
}

// --- the problem ------------------------------------------------------------------

void SolverController::selectProblem(int row) {
  guarded([&] {
    const int n = int(m_doc->session().puzzle().getNumberOfProblems());
    const int want = n == 0 ? -1 : std::clamp(row, 0, n - 1);
    if (want == m_problem)
      return;
    stopPlaying();
    if (const problem_c * pr = currentProblem())
      m_optionsOf[pr] = m_options;
    m_problem = want;
    m_options = optionsFor(want);
    m_hasSelection = false;
    m_assemblerState.reset();
    m_preview = -1;
    emit problemChanged();
    emit optionsChanged();
    refreshRun();
    refreshSolutions(true);
    keepExplorer();
    refreshPlacements();
  });
}

QString SolverController::problemIdText(void) const {
  return m_problem < 0 ? QString() : QString::fromStdString(btui::problemId(m_problem));
}

QString SolverController::problemLabel(void) const {
  const problem_c * pr = currentProblem();
  return pr ? QString::fromStdString(pr->getName()) : QString();
}

QString SolverController::problemDetail(void) const {
  const problem_c * pr = currentProblem();
  if (!pr)
    return tr("No problem to solve");
  const int shapes = int(pr->getNumberOfParts());
  if (!pr->resultValid())
    return shapes == 1 ? tr("No result · 1 piece shape") : tr("No result · %1 piece shapes").arg(shapes);
  const int result = int(pr->getResultId()) + 1;
  return shapes == 1 ? tr("Result S%1 · 1 piece shape").arg(result)
                     : tr("Result S%1 · %2 piece shapes").arg(result).arg(shapes);
}

bool SolverController::canDisassemble(void) const {
  return m_doc->session().puzzle().getGridType()->getCapabilities() & gridType_c::CAP_DISASSEMBLE;
}

// --- the options ----------------------------------------------------------------------

void SolverController::setOptions(const btui::SolverOptions & o) {
  const btui::SolverOptions n = btui::normalised(o);
  if (n == m_options)
    return;
  const bool filter = btui::checksDisassembly(n) != btui::checksDisassembly(m_options);
  m_options = n;
  if (const problem_c * pr = currentProblem())
    m_optionsOf[pr] = m_options;
  emit optionsChanged();
  if (filter)
    refreshSolutions(true);     // the list shows only what comes apart, or all
  refreshRun();
}

void SolverController::setDisassemble(bool v) {
  btui::SolverOptions o = m_options;
  o.disassemble = v;
  if (!v)
    o.dropDisassemblies = false;   // the sub-switch belongs to the disassembly card
  setOptions(o);
}

void SolverController::setDropDisassemblies(bool v) {
  btui::SolverOptions o = m_options;
  o.dropDisassemblies = v;
  setOptions(o);
}

void SolverController::setJustCount(bool v) { btui::SolverOptions o = m_options; o.justCount = v; setOptions(o); }
void SolverController::setKeepMirror(bool v) { btui::SolverOptions o = m_options; o.keepMirror = v; setOptions(o); }
void SolverController::setKeepRotated(bool v) { btui::SolverOptions o = m_options; o.keepRotated = v; setOptions(o); }
void SolverController::setThoroughRotation(bool v) {
  btui::SolverOptions o = m_options;
  o.thoroughRotation = v;
  setOptions(o);
}

void SolverController::setSearchSort(int v) {
  if (v < 0 || v > int(btui::SearchSort::Moves))
    return;
  btui::SolverOptions o = m_options;
  o.sort = btui::SearchSort(v);
  setOptions(o);
}

void SolverController::setKeepEvery(int v) { btui::SolverOptions o = m_options; o.keepEvery = v; setOptions(o); }
void SolverController::setLimit(int v) { btui::SolverOptions o = m_options; o.limit = v; setOptions(o); }

void SolverController::setOptionsOpen(bool v) {
  if (v == m_optionsOpen)
    return;
  m_optionsOpen = v;
  emit optionsChanged();
}

bool SolverController::searchSortAvailable(int sort) const {
  return sort >= 0 && sort <= int(btui::SearchSort::Moves) && btui::searchSortAvailable(m_options, btui::SearchSort(sort));
}

void SolverController::resetOptions(void) {
  btui::SolverOptions o;
  o.disassemble = m_options.disassemble;
  o.dropDisassemblies = m_options.dropDisassemblies;
  setOptions(o);
}

// --- the run ------------------------------------------------------------------------------

void SolverController::run(bool fresh, bool prepareOnly) {
  guarded([&] {
    problem_c * pr = currentProblem();
    if (!pr || m_thread || m_analysis)
      return;
    const gridType_c * gt = m_doc->session().puzzle().getGridType();
    // legacy cb_BtnCont's checks, in its words
    if (!(gt->getCapabilities() & gridType_c::CAP_ASSEMBLE)) {
      emit messageRequested(tr("Solver"), tr("Sorry, this space grid doesn't have an assembler (yet)!"));
      return;
    }
    if (btui::checksDisassembly(m_options) && !(gt->getCapabilities() & gridType_c::CAP_DISASSEMBLE)) {
      emit messageRequested(tr("Solver"), tr("Sorry, this space grid doesn't have a disassembler (yet)!\n"
                                             "Choose “Assemblies” to search without it."));
      return;
    }
    if (!pr->resultValid()) {
      emit messageRequested(tr("Solver"), tr("A result shape must be defined"));
      return;
    }

    stopPlaying();
    m_assemblerState.reset();
    if (fresh) {
      // legacy cb_BtnStart: the earlier results go, then the run starts afresh
      m_doc->editResults([&](puzzle_c & p) {
        problem_c * q = p.getProblem(unsigned(m_problem));
        q->removeAllSolutions();
        for (unsigned i = 0; i < p.getNumberOfShapes(); i++)
          p.getShape(i)->initHotspot();
      });
      m_cannot.erase(m_problem);
      m_hasSelection = false;
      pr = currentProblem();
    }
    const btui::SolverParameters par = btui::solverParameters(m_options);
    auto t = std::make_unique<solveThread_c>(*pr, par.flags, unsigned(m_settings->workerThreads()));
    t->setSortMethod(par.sortMethod);
    t->setSolutionLimits(par.limit, par.drop);
    t->setLiveSort(par.liveSort);
    if (!t->start(prepareOnly)) {
      emit messageRequested(tr("Solver"), tr("Could not start the solving process, the thread creation failed, sorry."));
      return;
    }
    m_thread = std::move(t);
    m_threadProblem = m_problem;
    // after the start: a failed one leaves Ready or Paused as they were
    m_prepared[m_problem] = prepareOnly;
    if (fresh)
      m_floor.erase(m_problem);
    updateBusy();
    m_poll.start();
    emit announceRequested(prepareOnly ? tr("Preparing the search") : tr("Search started"));
    refreshRun();
    refreshSolutions(true);
  });
}

void SolverController::prepare(void) { run(true, true); }
void SolverController::start(void) { run(true, false); }
void SolverController::resume(void) { run(false, false); }

void SolverController::stop(void) {
  if (m_thread)
    m_thread->stop();
  refreshRun();
}

void SolverController::step(void) {
  guarded([&] {
    problem_c * pr = currentProblem();
    if (!pr || !m_buttons.step)
      return;
    assembler_c * assm = pr->getAssembler();
    if (!assm)
      return;
    // legacy cb_BtnAssemblerStep: one step, on this thread; no solve runs
    assm->debug_step(1);
    if (assm->getFinished() >= 1)
      m_doc->editResults([&](puzzle_c & p) { p.getProblem(unsigned(m_problem))->finishedSolving(); });
    m_assemblerState = assm->getAssembly();
    if (m_assemblerState) {
      int placed = 0;
      for (unsigned i = 0; i < m_assemblerState->placementCount(); i++)
        placed += m_assemblerState->isPlaced(i) ? 1 : 0;
      m_assemblerCaption = tr("Assembler state — %1 of %2 pieces placed")
                             .arg(placed).arg(m_assemblerState->placementCount());
    }
    stopPlaying();
    buildScene();
    emit selectionChanged();
    refreshRun();
    refreshSolutions(false);
    refreshPlacements();
  });
}

void SolverController::poll(void) {
  guarded([&] {
    if (m_thread) {
      const unsigned a = m_thread->currentAction();
      if (a == solveThread_c::ACT_ASSERT) {
        const QString what = QString::fromLocal8Bit(m_thread->getAssertException().what());
        retireThread(true);
        emit messageRequested(tr("Solver"), tr("Because of an internal error the current puzzle\ncan not be solved\n\n%1").arg(what));
      } else if (a == solveThread_c::ACT_PAUSING || a == solveThread_c::ACT_FINISHED) {
        // read before the thread retires: retiring forgets its problem
        const auto it = m_prepared.find(m_threadProblem);
        const bool prepared = it != m_prepared.end() && it->second;
        retireThread(true);
        emit announceRequested(a == solveThread_c::ACT_FINISHED ? tr("Search finished")
                               : prepared ? tr("Search prepared") : tr("Search stopped"));
      } else if (a == solveThread_c::ACT_ERROR) {
        const assembler_c::errState err = m_thread->getErrorState();
        const int param = m_thread->getErrorParam();
        // LIFECYCLE-4: an error leaves the problem as it was, and the file saved
        retireThread(false);
        QString text;
        int shape = -1;
        switch (err) {
          case assembler_c::ERR_TOO_MANY_UNITS:
            text = tr("Pieces contain %1 units too many").arg(param);
            break;
          case assembler_c::ERR_TOO_FEW_UNITS:
            text = tr("Pieces contain %1 units less than required").arg(param);
            break;
          case assembler_c::ERR_CAN_NOT_PLACE:
            text = tr("Piece %1 can be placed nowhere within the result").arg(param + 1);
            shape = param;
            break;
          case assembler_c::ERR_CAN_NOT_RESTORE_VERSION:
            text = tr("Impossible to restore the saved state because the internal format changed.\n"
                      "You either have to start from the beginning or finish with the old version of BurrTools, sorry");
            break;
          case assembler_c::ERR_CAN_NOT_RESTORE_SYNTAX:
            text = tr("Impossible to restore the saved state because something with the data is wrong.\n"
                      "You have to start from the beginning, sorry");
            break;
          case assembler_c::ERR_CAN_NOT_RESTORE_INTERRUPTED:
            text = tr("The saved search was stopped before it had any search state to save.\n"
                      "It can not be continued, so it has been reset -- please start it again.");
            break;
          case assembler_c::ERR_CAN_NOT_START:
            text = param == SS_SOLVED ? tr("The problem is already solved.\nUse Start, not Continue, to solve it again from the beginning.")
                                      : tr("The problem can not be continued in its current state.\nUse Start to solve it from the beginning.");
            break;
          case assembler_c::ERR_PUZZLE_UNHANDABLE:
            text = tr("Something went wrong the program can not solve your puzzle definitions.\n"
                      "You should send the puzzle file to the programmer!");
            break;
          case assembler_c::ERR_NONE:
            break;
        }
        if (!text.isEmpty())
          emit messageRequested(tr("Solver"), text);
        if (shape >= 0)
          emit shapeRequested(shape);
      }
    }
    if (m_analysis)
      finishAnalysis();
    refreshRun();
    if (listDue())
      refreshSolutions(false);
    if (!m_thread && !m_analysis && !m_doc->loadingSolutions())
      m_poll.stop();
  });
}

void SolverController::retireThread(bool results) {
  m_thread.reset();
  m_threadProblem = -1;
  if (results)
    m_doc->editResults([](puzzle_c &) {});
  updateBusy();
  // the same solutions can read otherwise now: "Searching" ends, even with none kept
  refreshRun();
  refreshSolutions(true);
}

void SolverController::updateBusy(void) {
  m_doc->setBusy(m_thread != nullptr || m_analysis != nullptr);
}

QString SolverController::progressText(void) const {
  // one decimal (C19 §4), and never "100.0" before the end
  const double pc = std::floor(m_progress * 1000.0) / 10.0;
  return tr("%1 %").arg(pc, 0, 'f', 1);
}

void SolverController::refreshRun(void) {
  problem_c * pr = currentProblem();
  const gridType_c * gt = m_doc->session().puzzle().getGridType();
  btui::RunFacts f;
  f.problem = pr != nullptr;
  f.threadHere = m_thread && m_threadProblem == m_problem;
  f.threadElsewhere = m_thread && m_threadProblem != m_problem;
  f.busy = m_analysis != nullptr || m_doc->loadingSolutions();
  // the state once: the thread may change it between two reads
  const solveState_e st = pr ? pr->getSolveState() : SS_UNSOLVED;
  if (pr) {
    f.solvable = pr->getNumberOfPieces() > 0 && pr->resultValid() && (gt->getCapabilities() & gridType_c::CAP_ASSEMBLE);
    f.action = f.threadHere ? int(m_thread->currentAction()) : -1;
    f.solveState = int(st);
    // the assembler's fields are the thread's while it runs here
    f.canContinue = !f.threadHere && st == SS_SOLVING && pr->canStartSolving();
    f.hasAssembler = !f.threadHere && pr->getAssembler() != nullptr;
    auto it = m_prepared.find(m_problem);
    f.prepared = it != m_prepared.end() && it->second;
  }
  m_facts = f;
  m_buttons = btui::runButtons(f);

  double progress = 0;
  QString assemblies, solutions, used, left;
  if (pr) {
    if (f.threadHere && f.action != solveThread_c::ACT_FINISHED) {
      // never back while this problem's run goes on; another problem's bar is not its floor
      double & floor = m_floor[m_problem];
      floor = std::max(floor, double(m_thread->getProgress()));
      progress = floor;
    } else if (st == SS_SOLVED)
      progress = 1;
    else if (st == SS_SOLVING && pr->getAssembler() && !f.threadHere)
      progress = std::max(double(pr->getAssembler()->getFinished()), m_floor.count(m_problem) ? m_floor[m_problem] : 0.0);
    progress = std::clamp(progress, 0.0, 1.0);

    // the counters: known unless unsolved -- read as the state above said, and
    // a reset by the thread in between (a refused restore) reads as unknown
    try {
      if (st != SS_UNSOLVED) {
        assemblies = QString::number(pr->getNumAssemblies());
        // Solutions reads "—" when the disassembly is not checked (C19 §4)
        if (btui::checksDisassembly(m_options))
          solutions = QString::number(pr->getNumSolutions());
      }
    } catch (const assert_exception &) {
      assemblies.clear();
    }
    if (solutions.isEmpty())
      solutions = QStringLiteral("\u2014");
    double usedSeconds = st != SS_UNSOLVED ? double(pr->getUsedTime()) : 0;
    if (f.threadHere)
      usedSeconds += double(m_thread->getTime());
    if (st != SS_UNSOLVED || f.threadHere)
      used = timeWords(usedSeconds);
    if (f.threadHere) {
      const auto l = btui::timeLeft(usedSeconds, progress);
      left = l ? timeWords(*l) : tr("unknown");
    }
  }

  QString act;
  switch (btui::activity(f, m_options.justCount)) {
    case btui::Activity::None:          act = QString(); break;
    case btui::Activity::Preparing:     act = tr("preparing"); break;
    case btui::Activity::Optimising:    act = tr("optimising"); break;
    case btui::Activity::Assembling:    act = tr("assembling"); break;
    case btui::Activity::Disassembling: act = tr("disassembling"); break;
    case btui::Activity::Counting:      act = tr("counting"); break;
    case btui::Activity::Stopping:      act = tr("please wait"); break;
    case btui::Activity::Paused:        act = tr("stopped"); break;
    case btui::Activity::Ready:         act = tr("ready (placements prepared)"); break;
    case btui::Activity::Finished:      act = tr("finished"); break;
    case btui::Activity::Partial:       act = tr("partial"); break;
    case btui::Activity::Error:         act = tr("error"); break;
  }

  m_progress = progress;
  m_activityText = act;
  m_assembliesText = assemblies;
  m_solutionsText = solutions;
  m_timeUsedText = used;
  m_timeLeftText = left;
  m_lastState = int(btui::runState(f));
  // always: the buttons depend on more than the texts
  emit runChanged();
  keepExplorer();
}

// --- the solutions ------------------------------------------------------------------------

void SolverController::refreshSolutions(bool force) {
  problem_c * pr = currentProblem();
  const bool here = m_thread && m_threadProblem == m_problem;
  m_listClock.start();
  m_listStale = false;
  // a poll that would read the same: the counts and the list's revisions say
  // so without walking the list
  Signature sig;
  if (pr) {
    {
      auto lock = pr->lockSolutions();
      sig.saved = pr->getNumberOfSavedSolutions();
      sig.revision = pr->solutionsRevision();
    }
    sig.changes = solution_c::changeCount();
    sig.state = int(pr->getSolveState());
    sig.assemblies = pr->numAssembliesKnown() ? pr->getNumAssemblies() : 0;
    sig.solutions = pr->numSolutionsKnown() ? pr->getNumSolutions() : 0;
    auto c = m_cannot.find(m_problem);
    sig.cannot = c == m_cannot.end() ? 0 : c->second.size();
  }
  sig.threadHere = here;
  if (!force && sig == m_signature)
    return;
  m_signature = sig;

  // the new solutions' facts only, and where their rows go
  btui::SolutionList::Change change;
  if (pr) {
    auto lock = pr->lockSolutions();
    auto it = m_cannot.find(m_problem);
    change = m_list.read(*pr, it == m_cannot.end() ? std::vector<unsigned>{} : it->second,
                         btui::checksDisassembly(m_options), m_sort);
  } else
    change = m_list.clear();
  if (change.none() && !force)
    return;
  // until refreshSelection() finds it again, the row stays inside the list
  if (m_row >= int(m_list.shown().size()))
    m_row = int(m_list.shown().size()) - 1;
  int maxMoves = 0;
  for (int r : m_list.shown())
    maxMoves = std::max(maxMoves, m_list.all()[size_t(r)].moves);
  // the rows stand as the list did before this read, unless it was cleared
  const bool inStep = m_rows->count() == int(m_list.shown().size() + change.removed.size() - change.inserted.size());
  if (change.reset || !inStep || maxMoves != m_maxMoves || m_preview >= 0)
    rebuildRows();      // every row's bar follows the longest, and a preview tints rows
  else {
    m_rows->removeRowsAt(change.removed);
    std::vector<RowsModel::Row> fresh;
    fresh.reserve(change.inserted.size());
    for (int r : change.inserted)
      fresh.push_back(rowOf(m_list.all()[size_t(m_list.shown()[size_t(r)])], false));
    m_rows->insertRowsAt(change.inserted, std::move(fresh));
  }

  // what the area says without rows
  EmptyState e = NotEmpty;
  if (!pr)
    e = NoProblem;
  else if (m_list.shown().empty()) {
    const solveState_e st = pr->getSolveState();
    const unsigned long assemblies = sig.assemblies;
    if (here)
      e = Searching;
    else if (m_doc->loadingSolutions())
      e = Reading;               // what shows so far says nothing yet
    else if (!m_list.all().empty())
      e = NoneDisassemblable;
    else if (st == SS_UNSOLVED || st == SS_UNKNOWN)
      e = NeverRun;
    else if (st == SS_SOLVING && assemblies == 0)
      e = NotFinished;             // prepared, or stopped before the first assembly
    else if (assemblies == 0)
      e = NoAssemblies;
    else if (m_options.justCount)
      e = JustCounted;
    else if (btui::checksDisassembly(m_options) && sig.solutions == 0)
      e = NoneDisassemblable;
    else
      e = NoneKept;
  }
  const bool messageChanged = e != m_empty;
  m_empty = e;
  emit solutionsChanged();
  // Manage and Analyse depend on the list too
  emit runChanged();
  refreshSelection();
  keepExplorer();
  // the 3D view's message follows the empty state
  if (messageChanged)
    emit sceneChanged();
}

bool SolverController::listDue(void) {
  if (!shown()) {
    m_listStale = true;            // read when the Solver shows
    return false;
  }
  const qint64 every = m_list.all().size() > kManySolutions ? kListSlowMs : kListMs;
  return !m_listClock.isValid() || m_listClock.elapsed() >= every;
}

RowsModel::Row SolverController::rowOf(const btui::SolutionFacts & f, bool doomed) const {
  return { int(f.assembly) + 1, QString::fromStdString(f.levelText), f.moves,
           m_maxMoves > 0 && f.moves > 0 ? double(f.moves) / m_maxMoves : 0.0, f.pieces, int(f.da), doomed };
}

void SolverController::rebuildRows(void) {
  // the preview of a Remove item: the rows it would take
  std::vector<int> doomed;
  if (m_preview >= 0)
    doomed = btui::toRemove(m_list.all(), m_list.shown(), m_row, btui::RemoveKind(m_preview));
  m_maxMoves = 0;
  for (int r : m_list.shown())
    m_maxMoves = std::max(m_maxMoves, m_list.all()[size_t(r)].moves);
  std::vector<RowsModel::Row> rows;
  rows.reserve(m_list.shown().size());
  for (int r : m_list.shown()) {
    const btui::SolutionFacts & f = m_list.all()[size_t(r)];
    rows.push_back(rowOf(f, std::binary_search(doomed.begin(), doomed.end(), f.index)));
  }
  m_rows->setRows(std::move(rows));
}

QString SolverController::emptyTitle(void) const {
  switch (m_empty) {
    case NotEmpty:           return QString();
    case Searching:          return tr("Searching…");
    case NoAssemblies:       return tr("No assemblies found");
    case NoneDisassemblable: return tr("No solutions to show");
    case NeverRun:           return tr("No results yet");
    case JustCounted:        return tr("Only counted");
    case NoProblem:          return tr("No problem to solve");
    case NotFinished:        return tr("Search not finished");
    case NoneKept:           return tr("No solutions kept");
    case Reading:            return tr("Reading the solutions…");
  }
  return QString();
}

QString SolverController::emptyText(void) const {
  const problem_c * pr = currentProblem();
  switch (m_empty) {
    case NotEmpty:     return QString();
    case Searching:    return tr("Solutions appear here as they are found.");
    case Reading:      return tr("The puzzle is open; its solutions appear here as they are read from the file.");
    case NoAssemblies: return tr("The pieces cannot fill the result with the current ranges and colour rules. Review the Puzzle tab.");
    case NoneDisassemblable: {
      const int n = m_list.all().empty() && pr && pr->numAssembliesKnown() ? int(pr->getNumAssemblies()) : int(m_list.all().size());
      const bool unchecked = std::any_of(m_list.all().begin(), m_list.all().end(),
                                         [](const btui::SolutionFacts & f) { return f.da == btui::DaState::NotChecked; });
      const QString found = n == 1 ? tr("1 assembly was found") : tr("%1 assemblies were found").arg(n);
      if (!btui::checksDisassembly(m_options))
        return tr("%1, but none is kept.").arg(found);
      return unchecked ? tr("%1, but none has been checked for disassembly. Switch to “Assemblies” to list them, or analyse them.").arg(found)
                       : tr("%1, but none can be disassembled. Switch to “Assemblies” to list them anyway.").arg(found);
    }
    case NeverRun:     return tr("Press Start to search for assemblies.");
    case JustCounted:  return tr("Just count was on: the solutions were counted, not kept.");
    case NoProblem:    return tr("Make a problem in the Puzzle workspace first.");
    case NotFinished:  return tr("No assembly found so far. Continue the search, or Start it again.");
    case NoneKept: {
      const int n = pr && pr->numAssembliesKnown() ? int(pr->getNumAssemblies()) : 0;
      return n == 1 ? tr("1 assembly was found; it was removed or not kept.")
                    : tr("%1 assemblies were found; they were removed or not kept (Keep every).").arg(n);
    }
  }
  return QString();
}

void SolverController::refreshSelection(void) {
  int row = -1;
  if (!m_list.shown().empty()) {
    if (m_hasSelection)
      for (size_t r = 0; r < m_list.shown().size(); r++)
        if (m_list.all()[size_t(m_list.shown()[r])].assembly == m_selectedAssembly) {
          row = int(r);
          break;
        }
    if (row < 0)
      row = m_hasSelection ? std::clamp(m_row, 0, int(m_list.shown().size()) - 1) : 0;
  }
  const bool rowChanged = row != m_row;
  m_row = row;
  if (row < 0) {
    const bool had = m_hasSelection || !m_scene.empty();
    m_hasSelection = false;
    if (had || rowChanged) {
      stopPlaying();
      buildScene();
      emit selectionChanged();
    }
    keepExplorer();
    return;
  }
  const btui::SolutionFacts & f = m_list.all()[size_t(m_list.shown()[size_t(row)])];
  const bool solutionChanged = !m_hasSelection || f.assembly != m_selectedAssembly;
  m_selectedAssembly = f.assembly;
  m_hasSelection = true;
  if (solutionChanged) {
    // a new solution starts assembled and still (C20)
    stopPlaying();
    m_position = 0;
    m_assemblerState.reset();
    buildScene();
  } else {
    // its disassembly may have come or gone (Analyse, Delete): the player follows
    const bool stored = f.da == btui::DaState::Stored;
    if (stored != bool(m_moves))
      buildScene();
  }
  emit selectionChanged();
  keepExplorer();
  refreshPlacements();
}

void SolverController::selectRow(int row) {
  selectRowNow(row, false);
}

void SolverController::scrubTo(int row) {
  guarded([&] { selectRowNow(row, true); });
}

void SolverController::selectRowNow(int row, bool deferScene) {
  if (m_list.shown().empty())
    return;
  row = std::clamp(row, 0, int(m_list.shown().size()) - 1);
  const btui::SolutionFacts & f = m_list.all()[size_t(m_list.shown()[size_t(row)])];
  if (m_hasSelection && f.assembly == m_selectedAssembly && !m_assemblerState)
    return;
  m_selectedAssembly = f.assembly;
  m_hasSelection = true;
  m_row = row;
  stopPlaying();
  m_position = 0;
  m_assemblerState.reset();
  if (m_preview >= 0)
    rebuildRows();
  if (deferScene) {
    // the title and tiles now, the scene once a frame
    if (!m_sceneTimer.isActive())
      m_sceneTimer.start();
  } else {
    m_sceneTimer.stop();
    buildScene();
  }
  emit positionChanged();
  emit selectionChanged();
  keepExplorer();
  refreshPlacements();
}

void SolverController::clickSort(int key) {
  if (key < 0 || key > int(btui::ListKey::Pieces))
    return;
  if (!btui::listKeyAvailable(m_list.all(), btui::ListKey(key)))
    return;
  m_sort = btui::clickSort(m_sort, btui::ListKey(key));
  refreshSolutions(true);
}

void SolverController::showAssemblies(void) {
  setDisassemble(false);
}

void SolverController::setView(const QString & v) {
  const QString n = v == QLatin1String("slider") ? v : QStringLiteral("list");
  if (n == m_view)
    return;
  m_view = n;
  emit viewChanged();
}

int SolverController::selectedNumber(void) const {
  const btui::SolutionFacts * f = selectedFacts();
  return f ? int(f->assembly) + 1 : 0;
}

QString SolverController::selectedLevel(void) const {
  const btui::SolutionFacts * f = selectedFacts();
  if (!f)
    return QString();
  return f->levelText.empty() ? QStringLiteral("—") : QString::fromStdString(f->levelText);
}

QString SolverController::selectedMoves(void) const {
  const btui::SolutionFacts * f = selectedFacts();
  if (!f)
    return QString();
  return f->moves < 0 ? QStringLiteral("—") : QString::number(f->moves);
}

int SolverController::selectedPieces(void) const {
  const btui::SolutionFacts * f = selectedFacts();
  return f ? f->pieces : 0;
}

int SolverController::selectedDa(void) const {
  const btui::SolutionFacts * f = selectedFacts();
  return f ? int(f->da) : int(btui::DaState::NotChecked);
}

bool SolverController::canMovements(void) const {
  return selectedFacts() && !m_thread && !m_assemblerState && selectedDa() == int(btui::DaState::Stored);
}

QString SolverController::tagText(void) const {
  if (m_assemblerState)
    return m_assemblerCaption;
  if (!selectedFacts())
    return QString();
  return tr("Solution %1 of %2").arg(m_row + 1).arg(m_list.shown().size());
}

void SolverController::toggleExplorer(const QString & kind) {
  const bool ok = (kind == QLatin1String("placements") && canPlacements())
               || (kind == QLatin1String("movements") && canMovements());
  if (!ok) {
    // a refusal is no close: an open drawer stays as it is, and says why
    emit flashRequested(kind == QLatin1String("movements") ? tr("Movements needs a solution with its disassembly stored")
                                                          : tr("Placements needs a prepared or stopped search"));
    return;
  }
  m_explorer = m_explorer == kind ? QString() : kind;
  refreshPlacements();
  emit explorerChanged();
}

void SolverController::closeExplorer(void) {
  if (m_explorer.isEmpty())
    return;
  m_explorer.clear();
  emit explorerChanged();
}

/* legacy placementBrowser_c's list: each piece and the assembler's
 * placements for it (getPiecePlacementCount), and whether the assembler's
 * state places it now */
void SolverController::refreshPlacements(void) {
  std::vector<RowsModel::Row> rows;
  problem_c * pr = currentProblem();
  if (pr && m_explorer == QLatin1String("placements") && !m_thread)
    if (const assembler_c * assm = pr->getAssembler())
      for (const btui::PieceChip & c : btui::pieceChips(*pr)) {
        const bool placed = m_assemblerState && unsigned(c.piece) < m_assemblerState->placementCount()
                         && m_assemblerState->isPlaced(unsigned(c.piece));
        const int count = assm->getPiecePlacementSupported() ? int(assm->getPiecePlacementCount(unsigned(c.piece))) : -1;
        rows.push_back({ QString::fromStdString(c.id), toQColor(Theme::shapeColours().of(c.shape, c.copy)), count, placed });
      }
  m_placements->setRows(std::move(rows));
}

/* each drawer shuts itself when what it browses goes (AC-C19-05) */
void SolverController::keepExplorer(void) {
  if ((m_explorer == QLatin1String("movements") && !canMovements())
      || (m_explorer == QLatin1String("placements") && !canPlacements()))
    closeExplorer();
}

// --- the player ----------------------------------------------------------------------------

int SolverController::playerState(void) const {
  if (!selectedFacts())
    return NothingToPlay;
  switch (btui::DaState(selectedDa())) {
    case btui::DaState::Stored:     return m_moves && m_total > 0 ? Playable : NoneStored;
    case btui::DaState::LevelOnly:  return LevelOnly;
    case btui::DaState::Cannot:     return CannotDisassemble;
    case btui::DaState::NotChecked: return canDisassemble() ? NotChecked : NoDisassembler;
  }
  return NothingToPlay;
}

QString SolverController::moveText(void) const {
  if (playerState() != Playable)
    return QString();
  return tr("Move %1 / %2").arg(m_position, 0, 'f', 1).arg(int(std::lround(m_total)));
}

void SolverController::setSpeed(double v) {
  double best = btui::playSpeeds[1];
  for (double s : btui::playSpeeds)
    if (std::abs(s - v) < std::abs(best - v))
      best = s;
  if (best == m_speed)
    return;
  m_speed = best;
  emit viewChanged();
}

void SolverController::stopPlaying(void) {
  if (!m_playing)
    return;
  m_playing = false;
  m_play.stop();
  emit positionChanged();
}

void SolverController::togglePlay(void) {
  if (m_playing) {
    stopPlaying();
    return;
  }
  if (playerState() != Playable) {
    emit flashRequested(m_row < 0 ? tr("Nothing to play yet") : tr("No disassembly is stored for this solution"));
    return;
  }
  if (m_position >= m_total)
    setPosition(0);
  m_playing = true;
  m_playClock.start();
  m_play.start();
  emit positionChanged();
}

void SolverController::tick(void) {
  const double dt = double(m_playClock.nsecsElapsed()) / 1e9;
  m_playClock.restart();
  const double to = btui::playedTo(m_position, m_total, dt, m_speed);
  setPosition(to);
  if (to >= m_total)
    stopPlaying();
}

void SolverController::setPosition(double pos) {
  pos = std::clamp(pos, 0.0, m_total);
  if (pos == m_position)
    return;
  m_position = pos;
  composeScene();
  emit positionChanged();
}

void SolverController::seek(double pos) {
  // dragging the slider pauses (C20)
  stopPlaying();
  setPosition(pos);
}

void SolverController::transport(const QString & which) {
  if (which == QLatin1String("play")) {
    togglePlay();
    return;
  }
  stopPlaying();
  if (which == QLatin1String("start"))
    setPosition(0);
  else if (which == QLatin1String("back"))
    setPosition(btui::stepMove(m_position, m_total, -1));
  else if (which == QLatin1String("fwd"))
    setPosition(btui::stepMove(m_position, m_total, 1));
  else if (which == QLatin1String("end"))
    setPosition(m_total);
}

bool SolverController::key(const QString & context, int key, bool shift) {
  bool known = false;
  const btui::SolverKey k = solverKeyOf(key, known);
  if (!known)
    return false;
  const btui::KeyContext c = context == QLatin1String("seekbar") ? btui::KeyContext::Seekbar
                           : context == QLatin1String("slider") ? btui::KeyContext::Slider : btui::KeyContext::List;
  const btui::SolverAction a = btui::solverKey(c, k, shift);
  const int before = m_row;
  switch (a) {
    case btui::SolverAction::None:           return false;
    case btui::SolverAction::TogglePlay:     togglePlay(); break;
    case btui::SolverAction::MoveBack:       transport(QStringLiteral("back")); break;
    case btui::SolverAction::MoveForward:    transport(QStringLiteral("fwd")); break;
    case btui::SolverAction::ToAssembled:    transport(QStringLiteral("start")); break;
    case btui::SolverAction::ToDisassembled: transport(QStringLiteral("end")); break;
    case btui::SolverAction::Pause:
      // Esc while nothing plays is the window's (its Esc ladder)
      if (!m_playing)
        return false;
      stopPlaying();
      break;
    case btui::SolverAction::Remove:
      // the list's Delete: Manage ▸ At, which the card confirms (it handles the key)
      return false;
    case btui::SolverAction::Previous:
    case btui::SolverAction::Next:
    case btui::SolverAction::First:
    case btui::SolverAction::Last:
    case btui::SolverAction::PageBack:
    case btui::SolverAction::PageForward:
      selectRow(btui::rowAfter(a, m_row, int(m_list.shown().size())));
      // the list keeps the focus: a screen reader hears the new row through this
      if (m_row != before && m_row >= 0)
        emit announceRequested(rowName(m_row));
      break;
  }
  return true;
}

QString SolverController::rowName(int row) const {
  if (row < 0 || row >= int(m_list.shown().size()))
    return QString();
  const btui::SolutionFacts & f = m_list.all()[size_t(m_list.shown()[size_t(row)])];
  const QString level = f.levelText.empty() ? tr("level unknown") : tr("level %1").arg(QString::fromStdString(f.levelText));
  return tr("Solution %1 of %2, assembly %3, %4").arg(row + 1).arg(m_list.shown().size()).arg(f.assembly + 1).arg(level);
}

// --- the pieces and the scene ----------------------------------------------------------------

void SolverController::buildScene(void) {
  guarded([&] {
    problem_c * pr = currentProblem();
    m_scene = btui::SolutionScene();
    m_moves.reset();
    m_total = 0;
    m_reach = 0;
    m_chips.clear();
    m_builtStyle = m_settings->voxelStyleValue();
    m_builtColours = Theme::shapeColours();
    m_sceneBuilt = true;
    if (pr) {
      btui::MeshOptions opt;
      opt.style = m_builtStyle;
      opt.shapeColours = m_builtColours;
      const puzzle_c & p = m_doc->session().puzzle();
      for (unsigned i = 0; i < p.colorNumber(); i++) {
        unsigned char r, g, b;
        p.getColor(i, &r, &g, &b);
        opt.palette.push_back({ r / 255.0f, g / 255.0f, b / 255.0f });
      }
      if (m_assemblerState) {
        m_scene = btui::SolutionScene::build(*pr, m_assemblerState.get(), opt);
        m_chips = btui::pieceChips(*pr, m_assemblerState.get());
      } else if (m_hasSelection) {
        // copies of what the thread may change, read under its lock; found by
        // its assembly number, as a live-sorted search moves it between polls
        auto lock = pr->lockSolutions();
        for (unsigned i = 0; i < pr->getNumberOfSavedSolutions(); i++) {
          const solution_c * s = pr->getSavedSolution(i);
          if (s->getAssemblyNumber() != m_selectedAssembly)
            continue;
          m_scene = btui::SolutionScene::build(*pr, s->getAssembly(), opt);
          m_chips = btui::pieceChips(*pr, s->getAssembly());
          if (const separation_c * d = s->getDisassembly()) {
            const unsigned reach = 2 * getResultShape(*pr)->getBiggestDimension();
            m_moves = std::make_unique<disasmToMoves_c>(d, reach, pr->getNumberOfPieces());
            m_total = d->sumMoves();
            m_reach = float(reach);
          }
          break;
        }
      }
    }
    m_position = std::clamp(m_position, 0.0, m_total);
    refreshChips();
    m_framing++;
    composeScene();
    // the player's counter and seekbar read the moves just built
    emit positionChanged();
  });
}

void SolverController::refreshChips(void) {
  std::vector<RowsModel::Row> rows;
  for (const btui::PieceChip & c : m_chips) {
    const auto it = m_states.find(c.id);
    const btui::PieceState st = it == m_states.end() ? btui::PieceState::Solid : it->second;
    const QString id = QString::fromStdString(c.id);
    const QString word = st == btui::PieceState::Solid ? tr("solid") : st == btui::PieceState::Wireframe ? tr("wireframe") : tr("hidden");
    rows.push_back({ id, toQColor(Theme::shapeColours().of(c.shape, c.copy)), int(st),
                     tr("%1 — %2. Click to cycle: solid → wireframe → hidden").arg(id, word) });
  }
  m_pieces->setRows(std::move(rows));
}

void SolverController::composeScene(void) {
  std::vector<btui::PieceState> states;
  for (const btui::PieceChip & c : m_chips) {
    if (size_t(c.piece) >= states.size())
      states.resize(size_t(c.piece) + 1, btui::PieceState::Solid);
    const auto it = m_states.find(c.id);
    states[size_t(c.piece)] = it == m_states.end() ? btui::PieceState::Solid : it->second;
  }
  if (m_moves)
    m_moves->setStep(float(m_position), true, true);
  auto mesh = std::make_shared<btui::ShapeMesh>();
  m_scene.compose(*mesh, m_wire, m_wireColours, m_moves.get(), states);
  m_sceneMesh = std::move(mesh);
  m_sceneRevision++;
  emit sceneChanged();
}

void SolverController::cyclePiece(int row) {
  if (row < 0 || row >= int(m_chips.size()))
    return;
  const std::string & id = m_chips[size_t(row)].id;
  const auto it = m_states.find(id);
  const btui::PieceState next = btui::nextPieceState(it == m_states.end() ? btui::PieceState::Solid : it->second);
  if (next == btui::PieceState::Solid)
    m_states.erase(id);
  else
    m_states[id] = next;
  // what is drawn changes, not the meshes, the camera or the player
  refreshChips();
  composeScene();
  const QString word = next == btui::PieceState::Solid ? tr("solid") : next == btui::PieceState::Wireframe ? tr("wireframe") : tr("hidden");
  emit announceRequested(tr("%1 %2").arg(QString::fromStdString(id), word));
}

void SolverController::showAllPieces(void) {
  if (m_states.empty())
    return;
  m_states.clear();
  refreshChips();
  composeScene();
}

void SolverController::setXray(bool v) {
  if (v == m_xray)
    return;
  m_xray = v;
  emit sceneChanged();
}

QString SolverController::sceneMessage(void) const {
  if (!m_scene.empty())
    return QString();
  if (m_thread && m_threadProblem == m_problem)
    return tr("Searching…");
  switch (m_empty) {
    case NoneDisassemblable: return btui::checksDisassembly(m_options) ? tr("No assembly can be taken apart — nothing to show")
                                                                       : tr("No solution kept — nothing to show");
    case NoneKept:           return tr("No solution kept — nothing to show");
    case NoAssemblies:       return tr("No assemblies found — nothing to show");
    case NotFinished:        return tr("The search is not finished — Continue it to find assemblies");
    case JustCounted:        return tr("The solutions were counted, not kept");
    case NoProblem:          return tr("No problem to solve");
    case Reading:            return tr("Reading the solutions…");
    case NotEmpty:
    case Searching:
    case NeverRun:           break;
  }
  return tr("Run the solver to see solutions here");
}

QString SolverController::statusText(void) const {
  const problem_c * pr = currentProblem();
  if (!pr)
    return QString();
  QString found;
  if (pr->numAssembliesKnown()) {
    const int n = int(pr->getNumAssemblies());
    found = n == 1 ? tr("1 assembly found") : tr("%1 assemblies found").arg(n);
    if (btui::checksDisassembly(m_options) && pr->numSolutionsKnown())
      found = tr("%1, %2 can be disassembled").arg(found).arg(pr->getNumSolutions());
  } else
    found = tr("not solved yet");
  const QString act = m_activityText.isEmpty() ? QString() : tr(" · Activity: %1").arg(m_activityText);
  return tr("Problem <b>P%1</b> — %2%3").arg(m_problem + 1).arg(found, act);
}

// --- Manage ----------------------------------------------------------------------------------

bool SolverController::canManage(void) const {
  return currentProblem() && !m_thread && !m_analysis && !m_doc->loadingSolutions() && !m_list.all().empty();
}

bool SolverController::canAnalyse(void) const {
  return canManage() && canDisassemble();
}

QString SolverController::analysingText(void) const {
  if (!m_analysis)
    return QString();
  return m_analysis->total == 1 ? tr("Analysing 1 assembly…") : tr("Analysing %1 assemblies…").arg(m_analysis->total);
}

void SolverController::setPreviewRemove(int kind) {
  if (kind < -1 || kind > int(btui::RemoveKind::All))
    kind = -1;
  if (kind == m_preview)
    return;
  m_preview = kind;
  // only the rows' preview column changes: nothing to read again
  rebuildRows();
  emit solutionsChanged();
}

int SolverController::removeCount(int kind) const {
  if (kind < 0 || kind > int(btui::RemoveKind::All))
    return 0;
  return int(btui::toRemove(m_list.all(), m_list.shown(), m_row, btui::RemoveKind(kind)).size());
}

void SolverController::removeSolutions(int kind) {
  guarded([&] {
    if (!canManage() || kind < 0 || kind > int(btui::RemoveKind::All))
      return;
    const std::vector<int> gone = btui::toRemove(m_list.all(), m_list.shown(), m_row, btui::RemoveKind(kind));
    if (gone.empty())
      return;
    int above = 0;
    for (int r = 0; r < m_row && r < int(m_list.shown().size()); r++)
      above += std::binary_search(gone.begin(), gone.end(), m_list.all()[size_t(m_list.shown()[size_t(r)])].index) ? 1 : 0;
    const btui::SolutionFacts * sel = selectedFacts();
    const bool selectedGone = sel && std::binary_search(gone.begin(), gone.end(), sel->index);
    const int row = m_row;
    m_doc->editResults([&](puzzle_c & p) {
      problem_c * pr = p.getProblem(unsigned(m_problem));
      auto lock = pr->lockSolutions();
      for (auto it = gone.rbegin(); it != gone.rend(); ++it)
        if (unsigned(*it) < pr->getNumberOfSavedSolutions())
          pr->removeSolution(unsigned(*it));
    });
    m_preview = -1;
    // keep the selected solution, or select the one that took its place
    if (selectedGone)
      m_hasSelection = false;
    m_row = row - above;
    refreshSolutions(true);
    if (selectedGone && !m_list.shown().empty())
      selectRow(btui::rowAfterRemoval(row, above, int(m_list.shown().size())));
    emit flashRequested(gone.size() == 1 ? tr("1 solution removed") : tr("%1 solutions removed").arg(gone.size()));
  });
}

void SolverController::deleteDisassembly(bool all) {
  guarded([&] {
    if (!canManage())
      return;
    const btui::SolutionFacts * sel = selectedFacts();
    const int index = sel ? sel->index : -1;
    if (!all && index < 0)
      return;
    m_doc->editResults([&](puzzle_c & p) {
      problem_c * pr = p.getProblem(unsigned(m_problem));
      auto lock = pr->lockSolutions();
      for (unsigned i = 0; i < pr->getNumberOfSavedSolutions(); i++)
        if (all || int(i) == index)
          pr->getSavedSolution(i)->removeDisassembly();
    });
    refreshSolutions(true);
    emit flashRequested(all ? tr("Every disassembly deleted") : tr("Disassembly deleted"));
  });
}

void SolverController::analyse(const QString & which) {
  guarded([&] {
    problem_c * pr = currentProblem();
    if (!pr || !canAnalyse())
      return;
    const btui::SolutionFacts * sel = selectedFacts();
    const int selected = sel ? sel->index : -1;
    // the ones an analysis already found cannot come apart: their search is
    // the slowest, and it would say the same
    const auto c = m_cannot.find(m_problem);
    auto knownCannot = [&](unsigned assembly) {
      return c != m_cannot.end() && std::find(c->second.begin(), c->second.end(), assembly) != c->second.end();
    };
    std::vector<std::pair<unsigned, std::unique_ptr<assembly_c>>> work;
    {
      auto lock = pr->lockSolutions();
      for (unsigned i = 0; i < pr->getNumberOfSavedSolutions(); i++) {
        const solution_c * s = pr->getSavedSolution(i);
        const bool want = which == QLatin1String("all") ? true
                        : which == QLatin1String("missing") ? s->getDisassembly() == nullptr && !knownCannot(s->getAssemblyNumber())
                        : int(i) == selected;
        if (want && s->getAssembly())
          work.emplace_back(s->getAssemblyNumber(), std::make_unique<assembly_c>(s->getAssembly()));
      }
    }
    if (work.empty()) {
      emit flashRequested(tr("Nothing to analyse"));
      return;
    }
    auto a = std::make_unique<Analysis>();
    a->problem = m_problem;
    a->total = int(work.size());
    Analysis * raw = a.get();
    const problem_c * prob = pr;
    a->worker = std::jthread([raw, prob, work = std::move(work)](std::stop_token stop) mutable {
      try {
        disassembler_0_c dis(*prob);
        for (auto & [number, assembly] : work) {
          if (stop.stop_requested())
            return;
          auto tree = dis.disassemble(assembly.get());
          std::lock_guard<std::mutex> g(raw->mutex);
          raw->results.push_back({ number, std::move(tree) });
          raw->done++;
        }
      } catch (...) {
        raw->failed = true;
      }
    });
    m_analysis = std::move(a);
    updateBusy();
    m_poll.start();
    emit announceRequested(analysingText());
    refreshRun();
  });
}

QList<int> SolverController::selectedShapes(void) const {
  QList<int> out;
  const problem_c * pr = currentProblem();
  const btui::SolutionFacts * sel = selectedFacts();
  if (!pr || !sel)
    return out;
  auto lock = pr->lockSolutions();
  if (unsigned(sel->index) < pr->getNumberOfSavedSolutions())
    for (int s : btui::placedShapes(*pr, pr->getSavedSolution(unsigned(sel->index))->getAssembly()))
      out << s;
  return out;
}

void SolverController::finishAnalysis(void) {
  Analysis & a = *m_analysis;
  std::vector<Analysis::Result> results;
  bool over = false;
  {
    // the results and whether that was all, together: a result handed over
    // between the two reads would otherwise be dropped with the analysis
    std::lock_guard<std::mutex> g(a.mutex);
    results.swap(a.results);
    over = a.done.load() >= a.total || a.failed || a.worker.get_stop_token().stop_requested();
  }
  if (!results.empty())
    m_doc->editResults([&](puzzle_c & p) {
      if (unsigned(a.problem) >= p.getNumberOfProblems())
        return;
      problem_c * pr = p.getProblem(unsigned(a.problem));
      auto lock = pr->lockSolutions();
      std::vector<unsigned> & cannot = m_cannot[a.problem];
      for (auto & r : results) {
        for (unsigned i = 0; i < pr->getNumberOfSavedSolutions(); i++)
          if (pr->getSavedSolution(i)->getAssemblyNumber() == r.assembly) {
            cannot.erase(std::remove(cannot.begin(), cannot.end(), r.assembly), cannot.end());
            if (r.tree)
              pr->getSavedSolution(i)->setDisassembly(std::move(r.tree));
            else {
              pr->getSavedSolution(i)->removeDisassembly();
              cannot.push_back(r.assembly);
            }
            break;
          }
      }
    });
  if (over && m_analysis) {
    const bool failed = a.failed;
    const bool stopped = a.worker.get_stop_token().stop_requested();
    const int total = a.total, done = a.done.load();
    if (a.worker.joinable())
      a.worker.join();
    m_analysis.reset();
    updateBusy();
    if (failed)
      emit messageRequested(tr("Solver"), tr("The disassembly could not be analysed."));
    else if (stopped)
      emit flashRequested(tr("%1 of %2 assemblies analysed").arg(done).arg(total));
    else
      emit flashRequested(total == 1 ? tr("1 assembly analysed") : tr("%1 assemblies analysed").arg(total));
  }
}
