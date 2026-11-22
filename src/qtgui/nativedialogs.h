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
#ifndef BTQT_NATIVEDIALOGS_H
#define BTQT_NATIVEDIALOGS_H

#include <QObject>
#include <QString>
#include <QWindow>                   // a Q_INVOKABLE parameter: moc needs it complete
#include <QtQml/qqmlregistration.h>

#include <optional>

/* The system's own "save changes?" question where Qt Quick Dialogs has
 * none.
 *
 * QtQuick.Dialogs' MessageDialog is the system's on macOS (and iOS,
 * Android) but Qt Quick's own drawing on Windows and Linux. On Windows this
 * asks with a task dialog -- Save, Don't Save, Cancel, as Notepad and
 * Office ask; it needs version 6 of the common controls, which
 * burrtools-qt's manifest names (burrtools-qt.manifest). Linux has no
 * system message box to ask with (GTK's is GTK's, the desktop portal has
 * none), so there, and on macOS, `available` is false and the shell keeps
 * its MessageDialog.
 */
class NativeDialogs : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  Q_PROPERTY(bool available READ available CONSTANT FINAL)

public:

  enum Choice { Cancel = 0, Save = 1, Discard = 2 };
  Q_ENUM(Choice)

  explicit NativeDialogs(QObject * parent = nullptr);

  /* askToSave() shows the system's dialog: Windows, with the common
   * controls' task dialog, on a real screen (not offscreen, as the tests are) */
  bool available(void) const { return m_available; }

  /* Ask whether to save the unsaved changes, modal to `window`, with the
   * system's dialog: answered() then, or failed() if the system's dialog
   * could not be shown. Returns at once -- the dialog comes up from the
   * event loop and waits for the answer in a loop of its own, so not inside
   * the signal that asked. Nothing where it is not `available`. */
  Q_INVOKABLE void askToSave(QWindow * window, const QString & text, const QString & informativeText);

  /* the system's dialog itself, no check of the platform plugin; none where
   * the system has none (or it failed). Tests call this directly. */
  static std::optional<Choice> systemAskToSave(QWindow * window, const QString & title,
                                               const QString & text, const QString & informativeText);

  /* the system has the dialog (Windows with common controls 6) */
  static bool systemHasIt(void);

signals:

  /* a Choice; App passes it to DocumentController::resolveDiscard */
  void answered(int choice);
  /* the shell asks with its own dialog instead */
  void failed(const QString & text, const QString & informativeText);

private:

  bool m_available = false;
};

#endif
