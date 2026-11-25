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
#include "systempointer.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <QString>

#include <vector>
#elif !defined(Q_OS_MACOS)
#include "../uicore/xcursor.h"

#include <QFile>
#include <QString>

#include <cstdlib>
#endif

// macOS: systempointer_mac.mm
#if !defined(Q_OS_MACOS)

namespace systempointer {

#if defined(Q_OS_WIN)

  namespace {
    constexpr const wchar_t * kCursors = L"Control Panel\\Cursors";
    constexpr int kDefaultSize = 32;               // Windows' pointer at size 1, 100 %

    /* a cursor drawn by the system on a plain background, 32-bit BGRX */
    std::vector<uint32_t> drawnOn(HCURSOR cursor, int w, int h, uint32_t background) {
      std::vector<uint32_t> out;
      BITMAPINFO bi{};
      bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bi.bmiHeader.biWidth = w;
      bi.bmiHeader.biHeight = -h;                  // top down
      bi.bmiHeader.biPlanes = 1;
      bi.bmiHeader.biBitCount = 32;
      bi.bmiHeader.biCompression = BI_RGB;
      HDC dc = CreateCompatibleDC(nullptr);
      void * bits = nullptr;
      HBITMAP bmp = dc ? CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
      if (bmp && bits) {
        HGDIOBJ old = SelectObject(dc, bmp);
        std::fill_n(static_cast<uint32_t *>(bits), size_t(w) * size_t(h), background);
        if (DrawIconEx(dc, 0, 0, cursor, w, h, 0, nullptr, DI_NORMAL)) {
          GdiFlush();
          out.assign(static_cast<uint32_t *>(bits), static_cast<uint32_t *>(bits) + size_t(w) * size_t(h));
        }
        SelectObject(dc, old);
      }
      if (bmp)
        DeleteObject(bmp);
      if (dc)
        DeleteDC(dc);
      return out;
    }

    /* The cursor's pixels: drawn on black and on white, the difference is
     * its transparency and the black one its premultiplied colour -- for
     * every kind of cursor, the old monochrome ones too (QImage::fromHICON
     * gives nothing for a cursor). */
    QImage pixelsOf(HCURSOR cursor, int w, int h) {
      const std::vector<uint32_t> black = drawnOn(cursor, w, h, 0x000000), white = drawnOn(cursor, w, h, 0xffffff);
      if (black.empty() || white.empty())
        return {};
      QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
      for (int y = 0; y < h; y++) {
        QRgb * row = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < w; x++) {
          const uint32_t b = black[size_t(y) * size_t(w) + size_t(x)], wh = white[size_t(y) * size_t(w) + size_t(x)];
          // white over a pixel minus black over it is 255 - alpha, per channel
          int alpha = 0;
          for (int shift : { 0, 8, 16 })
            alpha = std::max(alpha, 255 - (int((wh >> shift) & 0xff) - int((b >> shift) & 0xff)));
          alpha = std::clamp(alpha, 0, 255);
          const int r = std::min(alpha, int((b >> 16) & 0xff)), g = std::min(alpha, int((b >> 8) & 0xff)),
                    bl = std::min(alpha, int(b & 0xff));
          row[x] = qRgba(r, g, bl, alpha);
        }
      }
      return img;
    }
  }

  double sizeFactor(void) {
    DWORD v = 0, n = sizeof v;
    if (RegGetValueW(HKEY_CURRENT_USER, kCursors, L"CursorBaseSize", RRF_RT_REG_DWORD, nullptr, &v, &n) == ERROR_SUCCESS &&
        v >= 16 && v <= 512)
      return double(v) / kDefaultSize;
    return 1.0;
  }

  Arrow arrow(double scale) {
    scale = std::max(1.0, scale);
    const int px = int(std::lround(kDefaultSize * sizeFactor() * scale));
    // the scheme's arrow file (REG_EXPAND_SZ, expanded by RegGetValue), else the system's
    wchar_t path[MAX_PATH] = {};
    DWORD n = sizeof path;
    if (RegGetValueW(HKEY_CURRENT_USER, kCursors, L"Arrow", RRF_RT_REG_SZ, nullptr, path, &n) != ERROR_SUCCESS)
      path[0] = 0;
    // loaded again only when the scheme, the size or the screen changed
    static QString lastKey;
    static Arrow last;
    const QString key = QString::fromWCharArray(path) + QLatin1Char('|') + QString::number(px);
    if (key == lastKey)
      return last;
    HCURSOR h = nullptr;
    if (path[0])
      h = static_cast<HCURSOR>(LoadImageW(nullptr, path, IMAGE_CURSOR, px, px, LR_LOADFROMFILE));
    if (!h)
      h = static_cast<HCURSOR>(LoadImageW(nullptr, MAKEINTRESOURCEW(32512) /* OCR_NORMAL */, IMAGE_CURSOR, px, px, 0));
    lastKey = key;
    last = {};
    if (!h)
      return {};
    Arrow & a = last;
    ICONINFO info{};
    if (GetIconInfo(h, &info)) {
      // its size: the colour bitmap's, or half the mask's (AND over XOR)
      BITMAP bm{};
      int w = 0, ht = 0;
      if (info.hbmColor && GetObject(info.hbmColor, sizeof bm, &bm)) {
        w = bm.bmWidth;
        ht = bm.bmHeight;
      } else if (info.hbmMask && GetObject(info.hbmMask, sizeof bm, &bm)) {
        w = bm.bmWidth;
        ht = bm.bmHeight / 2;
      }
      if (info.hbmColor)
        DeleteObject(info.hbmColor);
      if (info.hbmMask)
        DeleteObject(info.hbmMask);
      if (w > 0 && ht > 0 && w <= 1024 && ht <= 1024) {
        a.image = pixelsOf(h, w, ht);
        a.image.setDevicePixelRatio(scale);
        a.hotspot = QPoint(int(std::lround(info.xHotspot / scale)), int(std::lround(info.yHotspot / scale)));
      }
    }
    DestroyCursor(h);
    return a;
  }

#else

  namespace {
    const char * env(const char * name) { return std::getenv(name); }
    constexpr int kDefaultSize = 24;               // Xcursor's default
  }

  double sizeFactor(void) {
    return double(btui::xcursor::settings(env).size) / kDefaultSize;
  }

  Arrow arrow(double scale) {
    scale = std::max(1.0, scale);
    const btui::xcursor::Settings s = btui::xcursor::settings(env);
    // read again only when the theme, the size or the screen changed
    static QString lastKey;
    static Arrow last;
    const QString key = QString::fromStdString(s.theme) + QLatin1Char('|') + QString::number(s.size) +
                        QLatin1Char('|') + QString::number(scale);
    if (key == lastKey)
      return last;
    lastKey = key;
    last = {};
    const std::filesystem::path file = btui::xcursor::find(s.theme, { "default", "left_ptr", "arrow" },
                                                           btui::xcursor::searchPath(env));
    if (file.empty())
      return {};
    QFile f(QString::fromStdString(file.string()));
    if (!f.open(QIODevice::ReadOnly) || f.size() > (64 << 20))
      return {};
    const QByteArray bytes = f.readAll();
    const int px = int(std::lround(s.size * scale));
    const auto img = btui::xcursor::parse(std::string_view(bytes.constData(), size_t(bytes.size())), px);
    if (!img)
      return {};
    QImage pixels(img->width, img->height, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < img->height; y++)
      std::copy_n(img->argb.data() + size_t(y) * size_t(img->width), img->width,
                  reinterpret_cast<uint32_t *>(pixels.scanLine(y)));
    // a theme without that size: scaled to it, as the desktop does
    const double k = double(px) / std::max(1, img->nominal > 0 ? img->nominal : img->height);
    Arrow & a = last;
    a.image = std::abs(k - 1.0) < 1e-3 ? pixels
              : pixels.scaled(int(std::lround(img->width * k)), int(std::lround(img->height * k)),
                              Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    a.image.setDevicePixelRatio(scale);
    a.hotspot = QPoint(int(std::lround(img->xhot * k / scale)), int(std::lround(img->yhot * k / scale)));
    return a;
  }

#endif
}

#endif
