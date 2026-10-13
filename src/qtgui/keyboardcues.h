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
#ifndef BTQT_KEYBOARDCUES_H
#define BTQT_KEYBOARDCUES_H

#include <QObject>

#include <functional>
#include <QtQml/qqmlregistration.h>

/* Whether menu labels underline their access keys now.
 *
 * Windows (and the Linux desktops, which copied it) show the underlines
 * only once the keyboard is being used for the menus: from an Alt press
 * until the mouse is clicked, Esc is pressed or the window loses focus --
 * unless the user asked Windows to always underline access keys
 * (SPI_GETKEYBOARDCUES). macOS has no access keys at all.
 *
 * Alt also reaches the menus by keyboard, as on Windows: a tap of Alt
 * alone (pressed and released with no key or click in between) arms the
 * menus -- the menu bar, or the rail's menu button where View ▸ Show menu
 * bar hid it -- and a menu's access key then opens that menu; Alt with the
 * key at once does the same.
 * Alt held while clicking, dragging or pressing another key (the Voxel
 * editor's quick tool, Alt+↑/↓ in the shapes list) is not a tap and arms
 * nothing.
 */
class KeyboardCues : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  Q_PROPERTY(bool showAccessKeys READ showAccessKeys NOTIFY changed FINAL)
  Q_PROPERTY(bool menuArmed READ menuArmed NOTIFY menuArmedChanged FINAL)

public:

  explicit KeyboardCues(QObject * parent = nullptr);

  bool showAccessKeys(void) const { return m_always || m_keyboardMode; }

  /* an Alt tap armed the menu bar: it shows, and a letter opens a menu */
  bool menuArmed(void) const { return m_armed; }

  /* the top-level menus' access keys in their order ("FEVXH"); empty, as on
   * macOS, and Alt reaches no menus */
  void setMenuKeys(const QString & keys) { m_menuKeys = keys.toUpper(); }

  /* asked before Alt arms or opens a menu: true while a dialog is up, which
   * the menus must not open over (nor take the letters typed into it) */
  void setBlockedBy(std::function<bool()> blocked) { m_blocked = std::move(blocked); }

  /* "&File" -> "File", or "<u>F</u>ile" while the access keys show */
  Q_INVOKABLE static QString accessKeyText(const QString & label, bool show);

  /* the user's "always underline access keys" choice, read from the system */
  static bool alwaysShown(void);

  /* a menu command ran: keyboard use of the menus is over */
  Q_INVOKABLE void reset(void) { setArmed(false); setKeyboardMode(false); }

signals:

  void changed(void);

  /* the access key of top-level menu `index` was pressed: after an Alt tap,
   * or with Alt held (only when no visible menu bar took it) */
  void menuKeyPressed(int index);
  void menuArmedChanged(void);
  /* the keyboard put the armed menus away (Esc, a second Alt tap) -- not a
   * click, which may be on a menu */
  void menusDismissed(void);
  /* a mouse press in one of the application's ordinary windows -- not in a
   * popup's window (a menu): menus in windows of their own close on it */
  void windowPressed(void);

protected:

  bool eventFilter(QObject * watched, QEvent * event) override;

private:

  void setKeyboardMode(bool on);
  void setArmed(bool on);

  bool m_always = false;
  bool m_keyboardMode = false;
  bool m_altAlone = false;     // Alt is down, nothing else since
  bool m_armed = false;
  QString m_menuKeys;
  std::function<bool()> m_blocked;
  bool blocked(void) const { return m_blocked && m_blocked(); }
};

#endif
