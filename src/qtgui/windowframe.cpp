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
#include "windowframe.h"

#include "theme.h"

#include <QWindow>

// Linux and the BSDs; elsewhere Theme's colour scheme reaches the frame
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#define BTQT_FRAME_FOLLOWS 1

#include <QGuiApplication>
#include <QPointer>
#include <QStandardPaths>
#include <QVariant>
#endif

#if defined(BTQT_FRAME_FOLLOWS) && defined(BURRTOOLS_HAVE_XCB)
#include <QtGui/qguiapplication_platform.h>
#include <xcb/xcb.h>

#include <cstdlib>
#include <cstring>
#endif

namespace windowframe {

#if defined(BTQT_FRAME_FOLLOWS)
  namespace {

#if defined(BURRTOOLS_HAVE_XCB)
    xcb_atom_t atom(xcb_connection_t * c, const char * name) {
      xcb_intern_atom_reply_t * r = xcb_intern_atom_reply(c, xcb_intern_atom(c, 0, uint16_t(std::strlen(name)), name), nullptr);
      const xcb_atom_t a = r ? r->atom : xcb_atom_t(XCB_ATOM_NONE);
      std::free(r);
      return a;
    }

    void x11Variant(QWindow * w, bool dark) {
      auto * x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
      if (!x11 || !x11->connection())
        return;                                       // not X11: Wayland
      xcb_connection_t * c = x11->connection();
      const xcb_atom_t variant = atom(c, "_GTK_THEME_VARIANT");
      if (variant == XCB_ATOM_NONE)
        return;
      const xcb_window_t id = xcb_window_t(w->winId());
      if (dark) {
        static const char kDark[] = "dark";
        xcb_change_property(c, XCB_PROP_MODE_REPLACE, id, variant, atom(c, "UTF8_STRING"), 8,
                            uint32_t(std::strlen(kDark)), kDark);
      } else {
        xcb_delete_property(c, id, variant);
      }
      xcb_flush(c);
    }
#endif

    // KDE's platform theme reads it, on X11 and Wayland alike
    void kdeScheme(bool dark) {
      if (!qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains(QLatin1String("KDE"), Qt::CaseInsensitive))
        return;
      static const QString path =
          QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("color-schemes/BreezeDark.colors"));
      if (path.isEmpty())
        return;
      const QVariant now = qGuiApp->property("KDE_COLOR_SCHEME_PATH");
      if (dark && now.toString() != path)
        qGuiApp->setProperty("KDE_COLOR_SCHEME_PATH", path);
      else if (!dark && now.isValid())
        qGuiApp->setProperty("KDE_COLOR_SCHEME_PATH", QVariant());
    }
  }

  void follow(QWindow * window) {
    Theme * theme = Theme::instance();
    if (!window || !theme)
      return;
    QPointer<QWindow> w(window);
    // Theme::changed comes for much else (density, motion): only a new dark
    auto apply = [w, theme, last = -1]() mutable {
      if (!w || int(theme->dark()) == last)
        return;
      const bool dark = theme->dark();
      last = int(dark);
#if defined(BURRTOOLS_HAVE_XCB)
      x11Variant(w, dark);
#endif
      kdeScheme(dark);
    };
    QObject::connect(theme, &Theme::changed, window, apply);
    apply();
  }
#else
  void follow(QWindow *) {}
#endif
}
