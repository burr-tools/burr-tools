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
#include "documentcontroller.h"

#include "../lib/puzzle.h"

#include <QDir>
#include <QFileInfo>

namespace {

  std::filesystem::path toPath(const QString & s) {
    return std::filesystem::path(s.toStdU16String());
  }

  QString fromPath(const std::filesystem::path & p) {
    return QString::fromStdU16String(p.u16string());
  }
}

DocumentController::DocumentController(QObject * parent) : QObject(parent) {}

QString DocumentController::fileName(void) const {
  return QFileInfo(fromPath(m_session.fileName())).fileName();
}

QString DocumentController::filePath(void) const {
  return QDir::toNativeSeparators(fromPath(m_session.fileName()));
}

/* Each platform's convention for a document window: the document's name
 * without its extension, then the application. Windows: "*Name - BurrTools"
 * (the star while unsaved, as Notepad does); Linux desktops: "*Name –
 * BurrTools"; macOS: the name alone -- the menu bar names the application --
 * and "Name — Edited" while unsaved.
 */
QString DocumentController::windowTitle(void) const {
  const QString name = m_session.fileName().empty() ? tr("Untitled")
                                                    : QFileInfo(fileName()).completeBaseName();
  const bool dirty = m_session.isModified();
#if defined(Q_OS_MACOS)
  return dirty ? tr("%1 — Edited").arg(name) : name;
#elif defined(Q_OS_WIN)
  return (dirty ? QStringLiteral("*") : QString()) + name + QStringLiteral(" - BurrTools");
#else
  return (dirty ? QStringLiteral("*") : QString()) + name + QStringLiteral(" – BurrTools");
#endif
}

QUrl DocumentController::folder(void) const {
  if (m_session.fileName().empty())
    return QUrl::fromLocalFile(QDir::currentPath());
  return QUrl::fromLocalFile(QFileInfo(fromPath(m_session.fileName())).absolutePath());
}

int DocumentController::gridType(void) const {
  return int(m_session.puzzle().getGridType()->getType());
}

QString DocumentController::gridTypeName(void) const {
  return gridTypeDisplayName(gridType());
}

QString DocumentController::gridTypeDisplayName(int type) {
  switch (type) {
    case gridType_c::GT_BRICKS:           return QStringLiteral("Brick");
    case gridType_c::GT_TRIANGULAR_PRISM: return QStringLiteral("Triangular Prism");
    case gridType_c::GT_SPHERES:          return QStringLiteral("Spheres");
    case gridType_c::GT_RHOMBIC:          return QStringLiteral("Rhombic Tetrahedra");
    case gridType_c::GT_TETRA_OCTA:       return QStringLiteral("Tetrahedra-Octahedra");
    default:                              return QString();
  }
}

QString DocumentController::comment(void) const {
  return QString::fromStdString(m_session.puzzle().getComment());
}

void DocumentController::setPending(Pending p) {
  m_pending = p;
  if (p == Pending::None) {
    m_pendingPath.clear();
    m_continueAfterSaveAs = false;
  }
  emit stateChanged();
}

/* Begin a flow that would replace or close the document: ask about unsaved
 * changes first, the way legacy confirmDiscard() does. A flow already
 * waiting for an answer is not stacked on -- the first one wins, as legacy
 * openFromSystem() does for repeated open requests.
 */
void DocumentController::start(Pending p, const char * action) {
  if (m_pending != Pending::None)
    return;
  setPending(p);
  if (m_session.isModified())
    emit confirmDiscardRequested(QString::fromLatin1(action));
  else
    proceed();
}

void DocumentController::proceed(void) {
  switch (m_pending) {
    case Pending::New:
      emit newFileTypeRequested();
      break;
    case Pending::Open:
      emit openFileRequested();
      break;
    case Pending::Import:
      emit importFileRequested();
      break;
    case Pending::Quit:
      setPending(Pending::None);
      emit quitApproved();
      break;
    case Pending::OpenPath: {
      // loadPath() reports a failure itself
      QString path = m_pendingPath;
      setPending(Pending::None);
      loadPath(path);
      break;
    }
    case Pending::None:
      break;
  }
}

void DocumentController::requestNew(void) { start(Pending::New, "create a new puzzle"); }
void DocumentController::requestOpen(void) { start(Pending::Open, "open another puzzle"); }
void DocumentController::requestImport(void) { start(Pending::Import, "import another puzzle"); }
void DocumentController::requestQuit(void) { start(Pending::Quit, "quit"); }

void DocumentController::requestOpenPath(const QString & path) {
  if (m_pending != Pending::None)
    return;
  m_pendingPath = path;
  start(Pending::OpenPath, "open that puzzle");
}

void DocumentController::resolveDiscard(int choice) {
  if (m_pending == Pending::None)
    return;

  switch (choice) {
    case Discard:
      proceed();
      break;

    case Save:
      if (m_session.fileName().empty()) {
        m_continueAfterSaveAs = true;
        emit saveAsRequested();
      } else if (m_session.save()) {
        emit stateChanged();
        proceed();
      } else {
        // the work must not be thrown away after a failed save
        setPending(Pending::None);
        emit messageRequested(tr("Save"), tr("The puzzle could not be saved."));
      }
      break;

    default:
      setPending(Pending::None);
      break;
  }
}

void DocumentController::newDocument(int type) {
  if (type < 0 || type >= gridType_c::GT_NUM_GRIDS)
    type = gridType_c::GT_BRICKS;
  m_session.newDocument(gridType_c::gridType(type));
  if (m_pending == Pending::New)
    setPending(Pending::None);
  emit documentReplaced();
  emit fileChanged();
  emit stateChanged();
}

void DocumentController::finishLoad(const btui::DocumentSession::LoadResult & r, const QString & path) {
  if (!r.ok) {
    emit messageRequested(tr("Open"), r.error.empty() ? tr("Could not open %1").arg(path)
                                                      : QString::fromStdString(r.error));
    return;
  }

  emit documentReplaced();
  emit fileChanged();
  emit stateChanged();

  if (r.containsStartedSearch)
    emit messageRequested(tr("Open"), tr("This puzzle file contains started but not finished search for solutions."));
  if (r.showComment)
    emit messageRequested(tr("Comment"), comment());
}

bool DocumentController::loadPath(const QString & path) {
  btui::DocumentSession::LoadResult r;
  try {
    r = m_session.load(toPath(path));
  } catch (const std::exception & e) {
    r.ok = false;
    r.error = e.what();
  }
  finishLoad(r, path);
  return r.ok;
}

void DocumentController::openFile(const QUrl & file) {
  if (m_pending == Pending::Open)
    setPending(Pending::None);
  loadPath(file.isLocalFile() ? file.toLocalFile() : file.toString());
}

void DocumentController::importFile(const QUrl & file) {
  if (m_pending == Pending::Import)
    setPending(Pending::None);
  const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
  btui::DocumentSession::LoadResult r;
  try {
    r = m_session.importPuzzleSolver3D(toPath(path));
  } catch (const std::exception & e) {
    r.ok = false;
    r.error = e.what();
  }
  if (!r.ok) {
    emit messageRequested(tr("Import"), QString::fromStdString(r.error));
    return;
  }
  emit documentReplaced();
  emit fileChanged();
  emit stateChanged();
}

void DocumentController::save(void) {
  if (m_session.fileName().empty()) {
    emit saveAsRequested();
    return;
  }
  bool ok = false;
  try {
    ok = m_session.save();
  } catch (const std::exception &) {
    ok = false;
  }
  if (!ok)
    emit messageRequested(tr("Save"), tr("The puzzle was NOT saved."));
  emit stateChanged();
}

void DocumentController::requestSaveAs(void) {
  emit saveAsRequested();
}

void DocumentController::saveAsFile(const QUrl & file) {
  const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
  bool ok = false;
  try {
    ok = m_session.saveAs(toPath(path));
  } catch (const std::exception &) {
    ok = false;
  }

  if (!ok) {
    if (m_continueAfterSaveAs)
      setPending(Pending::None);
    emit messageRequested(tr("Save"), tr("The puzzle was NOT saved."));
    emit stateChanged();
    return;
  }

  emit fileChanged();
  emit stateChanged();

  if (m_continueAfterSaveAs) {
    m_continueAfterSaveAs = false;
    proceed();
  }
}

void DocumentController::cancelFlow(void) {
  if (m_pending != Pending::None)
    setPending(Pending::None);
}

void DocumentController::undo(void) {
  if (!m_session.canUndo())
    return;
  auto r = m_session.undo();
  emit historyApplied(int(r.tab), r.selectedShape == puzzleHistory_c::NO_SHAPE ? -1 : int(r.selectedShape));
  emit stateChanged();
}

void DocumentController::redo(void) {
  if (!m_session.canRedo())
    return;
  auto r = m_session.redo();
  emit historyApplied(int(r.tab), r.selectedShape == puzzleHistory_c::NO_SHAPE ? -1 : int(r.selectedShape));
  emit stateChanged();
}

void DocumentController::setComment(const QString & text) {
  m_session.setComment(text.toStdString());
  emit stateChanged();
}

void DocumentController::notifyEdited(void) {
  emit stateChanged();
}

bool DocumentController::convertTo(int type) {
  if (type < 0 || type >= gridType_c::GT_NUM_GRIDS || !m_session.convert(gridType_c::gridType(type))) {
    emit messageRequested(tr("Convert"), tr("The puzzle could not be converted."));
    return false;
  }
  emit documentReplaced();
  emit stateChanged();
  return true;
}

void DocumentController::recordStructuralEdit(int selectedShape) {
  m_session.record(puzzleHistory_c::AK_ENTITIES_STRUCTURAL,
                   selectedShape < 0 ? puzzleHistory_c::NO_SHAPE : unsigned(selectedShape));
  emit puzzleEdited(selectedShape);
  emit stateChanged();
}
