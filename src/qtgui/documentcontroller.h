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
#ifndef BTQT_DOCUMENTCONTROLLER_H
#define BTQT_DOCUMENTCONTROLLER_H

#include "../uicore/documentsession.h"

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

/* The document as the GUI sees it: file name, modified state, undo/redo,
 * and the New / Open / Import / Save / Save as / Quit flows.
 *
 * Every question for the user is asked by QML with an OS-native dialog. The
 * controller asks for one by emitting a ...Requested() signal and carries on
 * when QML calls back -- resolveDiscard(), openFile(), saveAsFile(),
 * newDocument() -- so a flow such as "New with unsaved changes -> Save ->
 * pick a name -> choose the voxel type" is a short state machine here, and
 * tests drive it by calling the same continuation methods.
 */
class DocumentController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  Q_PROPERTY(QString fileName READ fileName NOTIFY fileChanged FINAL)
  Q_PROPERTY(QString filePath READ filePath NOTIFY fileChanged FINAL)
  Q_PROPERTY(QString windowTitle READ windowTitle NOTIFY stateChanged FINAL)
  Q_PROPERTY(QUrl folder READ folder NOTIFY fileChanged FINAL)
  Q_PROPERTY(bool modified READ modified NOTIFY stateChanged FINAL)
  Q_PROPERTY(bool canUndo READ canUndo NOTIFY stateChanged FINAL)
  Q_PROPERTY(bool canRedo READ canRedo NOTIFY stateChanged FINAL)
  Q_PROPERTY(int gridType READ gridType NOTIFY documentReplaced FINAL)
  Q_PROPERTY(QString gridTypeName READ gridTypeName NOTIFY documentReplaced FINAL)
  Q_PROPERTY(QString comment READ comment NOTIFY stateChanged FINAL)
  Q_PROPERTY(bool flowPending READ flowPending NOTIFY stateChanged FINAL)

public:

  explicit DocumentController(QObject * parent = nullptr);

  btui::DocumentSession & session(void) { return m_session; }
  const btui::DocumentSession & session(void) const { return m_session; }

  QString fileName(void) const;
  QString filePath(void) const;
  QString windowTitle(void) const;
  QUrl folder(void) const;
  bool modified(void) const { return m_session.isModified(); }
  bool canUndo(void) const { return m_session.canUndo(); }
  bool canRedo(void) const { return m_session.canRedo(); }
  int gridType(void) const;
  QString gridTypeName(void) const;
  QString comment(void) const;
  bool flowPending(void) const { return m_pending != Pending::None; }

  /* The official voxel type names in the File ▸ New order (spec C14). The
   * legacy dialog's "Tetrahedra-Octahera" is corrected.
   */
  Q_INVOKABLE static QString gridTypeDisplayName(int type);

  // --- starting a flow (menu commands) -----------------------------------

  Q_INVOKABLE void requestNew(void);
  Q_INVOKABLE void requestOpen(void);
  Q_INVOKABLE void requestImport(void);
  Q_INVOKABLE void requestQuit(void);
  /* open a file named by the OS or the command line; still asks about
   * unsaved changes first */
  Q_INVOKABLE void requestOpenPath(const QString & path);

  Q_INVOKABLE void save(void);
  Q_INVOKABLE void requestSaveAs(void);

  Q_INVOKABLE void undo(void);
  Q_INVOKABLE void redo(void);

  Q_INVOKABLE void setComment(const QString & text);

  /* Convert to another grid type (Convert dialog). The converted puzzle
   * replaces this one; false, with a message, when the conversion fails.
   */
  Q_INVOKABLE bool convertTo(int gridType);

  // --- continuations (QML reports the user's answer) ---------------------

  enum DiscardChoice { Cancel = 0, Save = 1, Discard = 2 };
  Q_ENUM(DiscardChoice)

  Q_INVOKABLE void resolveDiscard(int choice);
  Q_INVOKABLE void newDocument(int gridType);
  Q_INVOKABLE void openFile(const QUrl & file);
  Q_INVOKABLE void importFile(const QUrl & file);
  Q_INVOKABLE void saveAsFile(const QUrl & file);
  /* the user closed a file or type dialog without choosing */
  Q_INVOKABLE void cancelFlow(void);

  /* Load directly, without the unsaved-changes question: the command line
   * at start-up. Returns whether it loaded.
   */
  bool loadPath(const QString & path);

  /* The puzzle files named on the command line, opened by
   * loadStartupFiles(): Main.qml calls it after the window's first frame,
   * before it makes the workspace. Like legacy, the first that loads wins;
   * the others are not tried. */
  void setStartupFiles(const QStringList & files);
  Q_INVOKABLE void loadStartupFiles(void);
  bool startupFilesPending(void) const { return !m_startupFiles.isEmpty(); }
  bool startupFileLoaded(void) const { return m_startupLoaded; }

  /* Called after anything changes the puzzle outside this class, so the
   * modified and undo states repaint.
   */
  void notifyEdited(void);

  /* A dialog changed the puzzle's shapes or problems in place: record the
   * undo step (legacy AK_ENTITIES_STRUCTURAL) and tell the views. -1 keeps
   * the current shape selection where possible.
   */
  void recordStructuralEdit(int selectedShape = -1);

signals:

  void fileChanged(void);
  void stateChanged(void);
  /* a different puzzle object: everything showing the old one must reset */
  void documentReplaced(void);
  /* undo/redo changed the puzzle; tab and shape say where (puzzleHistory_c) */
  void historyApplied(int tab, int selectedShape);
  /* shapes or problems changed in place (not undo/redo); -1 = no preference */
  void puzzleEdited(int selectedShape);

  void confirmDiscardRequested(const QString & action);
  void newFileTypeRequested(void);
  void openFileRequested(void);
  void importFileRequested(void);
  void saveAsRequested(void);
  void quitApproved(void);
  void messageRequested(const QString & title, const QString & text);
  /* loadStartupFiles() ran (whether a file loaded or not) */
  void startupFilesLoaded(void);

private:

  enum class Pending { None, New, Open, Import, Quit, OpenPath };

  void start(Pending p, const char * action);
  void proceed(void);
  void finishLoad(const btui::DocumentSession::LoadResult & r, const QString & path);
  void setPending(Pending p);

  btui::DocumentSession m_session;
  Pending m_pending = Pending::None;
  QString m_pendingPath;
  bool m_continueAfterSaveAs = false;
  QStringList m_startupFiles;
  bool m_startupLoaded = false;
};

#endif
