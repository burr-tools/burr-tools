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
#include <QtQml/qqmlregistration.h>

/* Whether menu labels underline their access keys now.
 *
 * Windows (and the Linux desktops, which copied it) show the underlines
 * only once the keyboard is being used for the menus: from an Alt press
 * until the mouse is clicked, Esc is pressed or the window loses focus --
 * unless the user asked Windows to always underline access keys
 * (SPI_GETKEYBOARDCUES). macOS has no access keys at all.
 */
class KeyboardCues : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  Q_PROPERTY(bool showAccessKeys READ showAccessKeys NOTIFY changed FINAL)

public:

  explicit KeyboardCues(QObject * parent = nullptr);

  bool showAccessKeys(void) const { return m_always || m_keyboardMode; }

  /* the user's "always underline access keys" choice, read from the system */
  static bool alwaysShown(void);

  /* a menu command ran: keyboard use of the menus is over */
  Q_INVOKABLE void reset(void) { setKeyboardMode(false); }

signals:

  void changed(void);

protected:

  bool eventFilter(QObject * watched, QEvent * event) override;

private:

  void setKeyboardMode(bool on);

  bool m_always = false;
  bool m_keyboardMode = false;
};

#endif
