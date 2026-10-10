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
#ifndef BTQT_EARLYDEVICE_H
#define BTQT_EARLYDEVICE_H

#include <memory>

class QQuickWindow;

/* The main window's Direct3D 11 device, made at the very start of the
 * program on a thread of its own and handed to the window before its first
 * frame. Qt Quick would make it on its render thread once the window is
 * shown, with the window waiting: 0.2-0.3 s on a discrete GPU. Made here it
 * is ready while the application object, Main.qml and the native window are
 * still being made.
 *
 * It applies only where Qt would use Direct3D 11 on the "windows" platform
 * with no special device (no BURRTOOLS_RHI or QSG_RHI_BACKEND naming
 * another API, no software rasteriser or debug layer asked for), on the
 * adapter Qt would pick (QT_D3D_ADAPTER_INDEX, else the first);
 * BURRTOOLS_EARLY_DEVICE=0 turns it off. Anywhere else, and when the device
 * cannot be made, Qt makes its own as before. If the device is ever lost,
 * the window goes back to a device of Qt's own.
 *
 * It must outlive the window it was given to.
 */
class EarlyGraphicsDevice {
public:

  /* starts making the device, if it applies; before QGuiApplication */
  EarlyGraphicsDevice(void);
  ~EarlyGraphicsDevice();

  EarlyGraphicsDevice(const EarlyGraphicsDevice &) = delete;
  EarlyGraphicsDevice & operator=(const EarlyGraphicsDevice &) = delete;

  /* Give the device to the window (waiting for it if it is still being
   * made); before the window's first frame. False when there is none:
   * Qt then makes its own. */
  bool adopt(QQuickWindow * window);

private:

  struct Impl;
  std::unique_ptr<Impl> d;
};

#endif
