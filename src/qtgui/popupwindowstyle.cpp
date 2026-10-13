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
#include "popupwindowstyle.h"
#include "theme.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>

namespace {

  // dwmapi.h of older SDKs lacks these (Windows 11, build 22000)
  constexpr DWORD kImmersiveDarkMode = 20;     // DWMWA_USE_IMMERSIVE_DARK_MODE
  constexpr DWORD kCornerPreference = 33;      // DWMWA_WINDOW_CORNER_PREFERENCE
  constexpr DWORD kBorderColor = 34;           // DWMWA_BORDER_COLOR
  constexpr int kRound = 2;                    // DWMWCP_ROUND: the 8 px of system menus

  DWORD windowsBuild(void) {
    using RtlGetVersionFn = LONG (WINAPI *)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    auto fn = ntdll ? reinterpret_cast<RtlGetVersionFn>(reinterpret_cast<void *>(GetProcAddress(ntdll, "RtlGetVersion"))) : nullptr;
    RTL_OSVERSIONINFOW v{};
    v.dwOSVersionInfoSize = sizeof v;
    return fn && fn(&v) == 0 ? v.dwBuildNumber : 0;
  }

  void frame(QWindow * w) {
    const HWND hwnd = reinterpret_cast<HWND>(w->winId());
    const Theme * t = Theme::instance();
    const BOOL dark = t && t->dark() ? TRUE : FALSE;
    const int corner = kRound;
    DwmSetWindowAttribute(hwnd, kImmersiveDarkMode, &dark, sizeof dark);
    DwmSetWindowAttribute(hwnd, kCornerPreference, &corner, sizeof corner);
    if (t) {
      const QColor c = t->line2();
      const COLORREF border = RGB(c.red(), c.green(), c.blue());
      DwmSetWindowAttribute(hwnd, kBorderColor, &border, sizeof border);
    }
  }
}
#endif

bool PopupWindowStyle::supported(void) {
#ifdef Q_OS_WIN
  static const bool ok = windowsBuild() >= 22000;
  return ok && QGuiApplication::platformName() == QLatin1String("windows");
#else
  return false;
#endif
}

PopupWindowStyle::PopupWindowStyle(QObject * parent) : QObject(parent), m_active(supported()) {
  if (m_active && QCoreApplication::instance())
    QCoreApplication::instance()->installEventFilter(this);
}

bool PopupWindowStyle::eventFilter(QObject * watched, QEvent * event) {
#ifdef Q_OS_WIN
  // each popup window as it is shown: the theme may have changed since the last
  if (event->type() == QEvent::Show
      || (event->type() == QEvent::PlatformSurface
          && static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated)) {
    if (auto * w = qobject_cast<QWindow *>(watched))
      if (w->type() == Qt::Popup)
        frame(w);
  }
#endif
  return QObject::eventFilter(watched, event);
}
