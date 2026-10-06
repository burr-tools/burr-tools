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
#ifndef BTQT_POPUPWINDOWSTYLE_H
#define BTQT_POPUPWINDOWSTYLE_H

#include <QObject>
#include <QtQml/qqmlregistration.h>

/* Windows 11's frame for the menus' popup windows (Popup.Window): the
 * system's rounded corners and shadow, a border in the theme's line colour
 * and the theme's light or dark mode -- what its own menus have.
 *
 * Not its translucent backdrop: the desktop window manager draws that as a
 * flat fill on windows that are not active, and menu windows never are
 * (focus stays in the main window), so the menus keep the theme's opaque
 * fill. Where the system has no rounded frames (before Windows 11, other
 * platforms) roundsCorners is false and the menu draws its own.
 */
class PopupWindowStyle : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("owned by App")

  Q_PROPERTY(bool roundsCorners READ roundsCorners CONSTANT)

public:

  explicit PopupWindowStyle(QObject * parent = nullptr);

  bool roundsCorners(void) const { return m_active; }

  /* this system frames popups (Windows 11) */
  static bool supported(void);

protected:

  bool eventFilter(QObject * watched, QEvent * event) override;

private:

  bool m_active = false;
};

#endif
