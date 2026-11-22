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
#include "nativedialogs.h"

#include <QGuiApplication>
#include <QPointer>
#include <QTimer>
#include <QWindow>

#include <iterator>
#include <string>

#ifdef Q_OS_WIN
#include <windows.h>
#include <commctrl.h>

namespace {

  using TaskDialogIndirectFn = HRESULT (WINAPI *)(const TASKDIALOGCONFIG *, int *, int *, BOOL *);

  /* comctl32's TaskDialogIndirect, looked up rather than linked: only
   * version 6 has it, and a program without the manifest naming 6 (the
   * test programs, say) gets version 5.82, where it would fail to start */
  TaskDialogIndirectFn taskDialog(void) {
    static const TaskDialogIndirectFn fn = [] {
      HMODULE comctl = LoadLibraryW(L"comctl32.dll");
      return comctl ? reinterpret_cast<TaskDialogIndirectFn>(
                        reinterpret_cast<void *>(GetProcAddress(comctl, "TaskDialogIndirect")))
                    : nullptr;
    }();
    return fn;
  }

  constexpr int kSave = 100;
  constexpr int kDontSave = 101;
}

bool NativeDialogs::systemHasIt(void) {
  return taskDialog() != nullptr;
}

std::optional<NativeDialogs::Choice> NativeDialogs::systemAskToSave(QWindow * window, const QString & title,
                                                                    const QString & text,
                                                                    const QString & informativeText) {
  const TaskDialogIndirectFn fn = taskDialog();
  if (!fn)
    return std::nullopt;
  // the strings live until the dialog is gone
  const std::wstring wTitle = title.toStdWString();
  const std::wstring wText = text.toStdWString();
  const std::wstring wInfo = informativeText.toStdWString();
  const std::wstring wSave = tr("&Save").toStdWString();
  const std::wstring wDontSave = tr("Do&n't Save").toStdWString();
  const TASKDIALOG_BUTTON buttons[] = {
    { kSave, wSave.c_str() },
    { kDontSave, wDontSave.c_str() },
  };

  TASKDIALOGCONFIG c{};
  c.cbSize = sizeof c;
  c.hwndParent = window ? reinterpret_cast<HWND>(window->winId()) : nullptr;
  // Esc and the close box cancel; centred on the window
  c.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
  c.dwCommonButtons = TDCBF_CANCEL_BUTTON;
  c.pszWindowTitle = wTitle.c_str();
  c.pszMainInstruction = wText.c_str();
  c.pszContent = wInfo.empty() ? nullptr : wInfo.c_str();
  c.cButtons = UINT(std::size(buttons));
  c.pButtons = buttons;
  c.nDefaultButton = kSave;

  int pressed = 0;
  if (FAILED(fn(&c, &pressed, nullptr, nullptr)))
    return std::nullopt;
  switch (pressed) {
    case kSave:     return Save;
    case kDontSave: return Discard;
    default:        return Cancel;
  }
}

#else

bool NativeDialogs::systemHasIt(void) {
  return false;
}

std::optional<NativeDialogs::Choice> NativeDialogs::systemAskToSave(QWindow *, const QString &, const QString &,
                                                                    const QString &) {
  return std::nullopt;
}

#endif

NativeDialogs::NativeDialogs(QObject * parent) : QObject(parent) {
  // not offscreen or minimal (the tests): a dialog there would wait for
  // a click nobody can give
  m_available = systemHasIt() && QGuiApplication::platformName() == QLatin1String("windows");
}

void NativeDialogs::askToSave(QWindow * window, const QString & text, const QString & informativeText) {
  if (!m_available)
    return;
  QTimer::singleShot(0, this, [this, w = QPointer<QWindow>(window), text, informativeText] {
    const std::optional<Choice> c =
      systemAskToSave(w, QGuiApplication::applicationDisplayName(), text, informativeText);
    if (c) {
      emit answered(*c);
    } else {
      qWarning("NativeDialogs: the system's dialog failed; the shell asks instead");
      emit failed(text, informativeText);
    }
  });
}
