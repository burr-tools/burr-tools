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
#include "keyboardcues.h"

#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

KeyboardCues::KeyboardCues(QObject * parent) : QObject(parent), m_always(alwaysShown()) {
  if (QCoreApplication::instance())
    QCoreApplication::instance()->installEventFilter(this);
}

bool KeyboardCues::alwaysShown(void) {
#ifdef Q_OS_WIN
  BOOL on = FALSE;
  if (SystemParametersInfoW(SPI_GETKEYBOARDCUES, 0, &on, 0))
    return on != FALSE;
#endif
  return false;
}

void KeyboardCues::setKeyboardMode(bool on) {
  if (on == m_keyboardMode)
    return;
  m_keyboardMode = on;
  emit changed();
}

bool KeyboardCues::eventFilter(QObject * watched, QEvent * event) {
  switch (event->type()) {
    // a key a shortcut takes (Esc: the layout's) arrives only as the
    // ShortcutOverride sent ahead of it, never as a KeyPress
    case QEvent::ShortcutOverride:
    case QEvent::KeyPress: {
      const auto * k = static_cast<QKeyEvent *>(event);
      if (k->key() == Qt::Key_Alt)
        setKeyboardMode(true);
      else if (k->key() == Qt::Key_Escape)
        setKeyboardMode(false);
      break;
    }
    case QEvent::MouseButtonPress:
    case QEvent::ApplicationDeactivate:
      setKeyboardMode(false);
      break;
    default:
      break;
  }
  return QObject::eventFilter(watched, event);
}
