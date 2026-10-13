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
#include <QWindow>

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

void KeyboardCues::setArmed(bool on) {
  if (on == m_armed)
    return;
  m_armed = on;
  emit menuArmedChanged();
}

QString KeyboardCues::accessKeyText(const QString & label, bool show) {
  QString plain;
  int at = -1;
  for (qsizetype i = 0; i < label.size(); i++) {
    if (label.at(i) == u'&' && i + 1 < label.size()) {
      i++;
      if (label.at(i) != u'&' && at < 0)
        at = int(plain.size());
    }
    plain += label.at(i);
  }
  if (!show || at < 0)
    return plain;
  return plain.left(at) + QStringLiteral("<u>") + plain.at(at) + QStringLiteral("</u>") + plain.mid(at + 1);
}

bool KeyboardCues::eventFilter(QObject * watched, QEvent * event) {
  switch (event->type()) {
    // a key a shortcut takes (Esc: the layout's) arrives only as the
    // ShortcutOverride sent ahead of it, never as a KeyPress
    case QEvent::ShortcutOverride:
    case QEvent::KeyPress: {
      auto * k = static_cast<QKeyEvent *>(event);
      if (k->key() == Qt::Key_Alt) {
        if (!k->isAutoRepeat())
          m_altAlone = true;
        setKeyboardMode(true);
        break;
      }
      m_altAlone = false;
      if (k->key() == Qt::Key_Escape) {
        // Esc only disarms the bar, before the layout's Esc ladder sees it
        const bool wasArmed = m_armed;
        setArmed(false);
        setKeyboardMode(false);
        if (wasArmed) {
          emit menusDismissed();
          event->accept();
          return true;
        }
        break;
      }
      // a dialog is up: its keys are its own, no menu opens over it
      if (blocked()) {
        setArmed(false);
        break;
      }
      const qsizetype menu = (k->key() >= Qt::Key_A && k->key() <= Qt::Key_Z)
                             ? m_menuKeys.indexOf(QChar(char16_t(k->key()))) : -1;
      const Qt::KeyboardModifiers mods = k->modifiers() & ~Qt::KeypadModifier;
      if (m_armed && mods == Qt::NoModifier && k->key() >= Qt::Key_A && k->key() <= Qt::Key_Z) {
        // armed, a letter is for the menus and no one else (not the O / P
        // shortcuts): ahead of a shortcut, claim the key so it comes as a press
        event->accept();
        if (event->type() == QEvent::KeyPress && menu >= 0) {
          emit menuKeyPressed(int(menu));     // while still armed: a submenu opens, not a dismissal
          setArmed(false);
        }
        return true;
      }
      if (m_armed)
        setArmed(false);               // any other key ends it
      // Alt+letter that no shortcut took (a visible menu bar takes its own)
      if (event->type() == QEvent::KeyPress && mods == Qt::AltModifier && menu >= 0) {
        emit menuKeyPressed(int(menu));
        return true;                   // once: it is passed on to each parent item
      }
      break;
    }
    case QEvent::KeyRelease: {
      const auto * k = static_cast<QKeyEvent *>(event);
      if (k->key() == Qt::Key_Alt && !k->isAutoRepeat() && m_altAlone) {
        m_altAlone = false;
        if (!m_menuKeys.isEmpty() && !blocked()) {
          setArmed(!m_armed);          // a second tap disarms
          if (!m_armed)
            emit menusDismissed();
          setKeyboardMode(m_armed);
        }
      }
      break;
    }
    case QEvent::MouseButtonPress:
      if (const auto * w = qobject_cast<QWindow *>(watched); w && w->type() != Qt::Popup && w->type() != Qt::ToolTip)
        emit windowPressed();
      [[fallthrough]];
    case QEvent::ApplicationDeactivate:
      m_altAlone = false;
      setArmed(false);
      setKeyboardMode(false);
      break;
    default:
      break;
  }
  return QObject::eventFilter(watched, event);
}
