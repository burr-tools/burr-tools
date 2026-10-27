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
#include "earlydevice.h"

#include <QQuickWindow>

#ifdef Q_OS_WIN
#include <QGuiApplication>
#include <QQuickGraphicsDevice>

#include <d3d11.h>
#include <dxgi.h>

#include <thread>
#endif

#ifdef Q_OS_WIN

namespace {

  /* Whether Qt Quick would make a Direct3D 11 device of its own on the
   * windows platform, as far as the environment tells before the
   * application object exists (QQuickWindow::graphicsApi() is checked
   * again in adopt()). */
  bool applies(void) {
    auto unset = [](const char * v) { return qEnvironmentVariableIsEmpty(v); };
    auto oneOf = [](const char * v, std::initializer_list<const char *> ok) {
      const QByteArray s = qgetenv(v).toLower();
      if (s.isEmpty())
        return true;
      for (const char * o : ok)
        if (s == o)
          return true;
      return false;
    };
    return qgetenv("BURRTOOLS_EARLY_DEVICE") != "0"
        && oneOf("BURRTOOLS_RHI", { "d3d11" })
        && oneOf("QSG_RHI_BACKEND", { "d3d11" })
        && unset("QT_QUICK_BACKEND")
        && unset("QSG_RHI_PREFER_SOFTWARE_RENDERER")
        && unset("QSG_RHI_DEBUG_LAYER")
        && (unset("QT_QPA_PLATFORM") || qgetenv("QT_QPA_PLATFORM").startsWith("windows"));
  }

  template <class T> void release(T *& p) {
    if (p)
      p->Release();
    p = nullptr;
  }
}

struct EarlyGraphicsDevice::Impl {
  std::thread maker;
  ID3D11Device * dev = nullptr;
  ID3D11DeviceContext * context = nullptr;

  /* as Qt's QRhiD3D11 makes it: the requested or the first adapter, no flags */
  void make(void) {
    IDXGIFactory1 * factory = nullptr;
    IDXGIAdapter1 * adapter = nullptr;
    const int index = qEnvironmentVariableIsSet("QT_D3D_ADAPTER_INDEX") ? qEnvironmentVariableIntValue("QT_D3D_ADAPTER_INDEX") : 0;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory)))
        && SUCCEEDED(factory->EnumAdapters1(UINT(index), &adapter))) {
      if (FAILED(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                                   &dev, nullptr, &context))) {
        release(context);
        release(dev);
      }
    }
    release(adapter);
    release(factory);
  }

  void finish(void) {
    if (maker.joinable())
      maker.join();
  }

  ~Impl() {
    finish();
    release(context);
    release(dev);
  }
};

EarlyGraphicsDevice::EarlyGraphicsDevice(void) : d(std::make_unique<Impl>()) {
  if (applies())
    d->maker = std::thread([this] { d->make(); });
}

EarlyGraphicsDevice::~EarlyGraphicsDevice() = default;

bool EarlyGraphicsDevice::adopt(QQuickWindow * window) {
  d->finish();
  if (!window || !d->dev || !d->context || QGuiApplication::platformName() != QLatin1String("windows")
      || QQuickWindow::graphicsApi() != QSGRendererInterface::Direct3D11)
    return false;
  window->setGraphicsDevice(QQuickGraphicsDevice::fromDeviceAndContext(d->dev, d->context));
  /* The scene graph lets go of its device when the device is lost (a driver
   * update or reset): a device of Qt's own then, which Qt can make again. */
  QObject::connect(window, &QQuickWindow::sceneGraphInvalidated, window, [window] {
    window->setGraphicsDevice(QQuickGraphicsDevice());
  }, Qt::QueuedConnection);
  return true;
}

#else

struct EarlyGraphicsDevice::Impl {};

EarlyGraphicsDevice::EarlyGraphicsDevice(void) : d(std::make_unique<Impl>()) {}
EarlyGraphicsDevice::~EarlyGraphicsDevice() = default;

bool EarlyGraphicsDevice::adopt(QQuickWindow *) {
  return false;
}

#endif
