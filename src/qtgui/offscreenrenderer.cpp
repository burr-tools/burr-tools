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
#include "offscreenrenderer.h"
#include "pipelinecache.h"

#include <QOffscreenSurface>
#include <QPainter>
#include <rhi/qrhi.h>
#if BTQT_VULKAN
#include <QVulkanInstance>
#endif

#include <algorithm>

struct OffscreenRenderer::Impl {
  // what the QRhi is made from, declared first so they outlive it
  std::unique_ptr<QOffscreenSurface> surface;   // OpenGL
#if BTQT_VULKAN
  std::unique_ptr<QVulkanInstance> vulkan;      // Vulkan
#endif
  std::unique_ptr<QRhi> rhi;
  std::unique_ptr<QRhiTexture> tex;
  std::unique_ptr<QRhiRenderBuffer> depth;
  std::unique_ptr<QRhiTextureRenderTarget> rt;
  std::unique_ptr<QRhiRenderPassDescriptor> rp;
  SceneRenderer renderer;
  QString cacheFile;               // where the pipeline cache is kept, if anywhere
  bool cacheSeeded = false;
  QSize tileSize;
  int maxTile = 4096;

  void releaseTarget(void) {
    renderer.release();
    rt.reset();
    rp.reset();
    depth.reset();
    tex.reset();
    tileSize = QSize();
  }

  bool makeTarget(QSize size) {
    if (size == tileSize && rt)
      return true;
    // a new size keeps the render pass layout (below), and with it the scene
    // renderer's targets and pipelines: an export draws every picture at its
    // own size
    rt.reset();
    depth.reset();
    tex.reset();
    tileSize = QSize();
    tex.reset(rhi->newTexture(QRhiTexture::RGBA8, size, 1, QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    if (!tex->create())
      return false;
    depth.reset(rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, size, 1));
    if (!depth->create())
      return false;
    QRhiTextureRenderTargetDescription desc{ QRhiColorAttachment(tex.get()) };
    desc.setDepthStencilBuffer(depth.get());
    rt.reset(rhi->newTextureRenderTarget(desc));
    // every target here is RGBA8 with depth: one layout serves them all
    if (!rp)
      rp.reset(rt->newCompatibleRenderPassDescriptor());
    rt->setRenderPassDescriptor(rp.get());
    if (!rt->create())
      return false;
    renderer.initialize(rhi.get(), rp.get(), 1);
    tileSize = size;
    return true;
  }

  QImage renderTile(const SceneFrame & f) {
    QRhiCommandBuffer * cb = nullptr;
    if (rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
      return {};
    renderer.render(cb, rt.get(), f);
    QRhiReadbackResult rb;
    QRhiResourceUpdateBatch * u = rhi->nextResourceUpdateBatch();
    u->readBackTexture({ tex.get() }, &rb);
    cb->resourceUpdate(u);
    rhi->endOffscreenFrame();
    QImage img(reinterpret_cast<const uchar *>(rb.data.constData()), rb.pixelSize.width(), rb.pixelSize.height(),
               QImage::Format_RGBA8888_Premultiplied);
    img = img.copy();
    if (rhi->isYUpInFramebuffer())
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
      img = img.flipped(Qt::Vertical);
#else
      img = img.mirrored(false, true);  // flipped() arrived in 6.9; 6.8 is the floor
#endif
    return img;
  }
};

QString OffscreenRenderer::requestedApi(void) {
  return QString::fromLocal8Bit(qgetenv("BURRTOOLS_OFFSCREEN_RHI")).trimmed().toLower();
}

namespace {
  // the pipeline cache can be read back and kept (setPipelineCacheFile)
  constexpr QRhi::Flags kFlags = QRhi::EnablePipelineCacheDataSave;
}

OffscreenRenderer::OffscreenRenderer(Backend b) : d(std::make_unique<Impl>()) {
  QString api = requestedApi();
  if (api.isEmpty()) {
#if defined(Q_OS_WIN)
    api = QStringLiteral("d3d11");
#elif QT_CONFIG(metal)
    api = QStringLiteral("metal");
#else
    api = QStringLiteral("opengl");
#endif
  }

#if defined(Q_OS_WIN)
  if (api == QLatin1String("d3d11")) {
    QRhiD3D11InitParams p;
    d->rhi.reset(QRhi::create(QRhi::D3D11, &p, kFlags | (b == Backend::Software ? QRhi::PreferSoftwareRenderer : QRhi::Flags())));
  }
#else
  Q_UNUSED(b);
#endif
#if QT_CONFIG(metal)
  if (api == QLatin1String("metal")) {
    QRhiMetalInitParams p;
    d->rhi.reset(QRhi::create(QRhi::Metal, &p, kFlags));
  }
#endif
#if BTQT_VULKAN
  /* Vulkan needs an instance from the platform plugin, which the headless
   * "offscreen" one does not provide: on Linux CI this runs under Xvfb, with
   * Mesa's software lavapipe as the device */
  if (api == QLatin1String("vulkan")) {
    d->vulkan = std::make_unique<QVulkanInstance>();
    d->vulkan->setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());
    if (d->vulkan->create()) {
      QRhiVulkanInitParams p;
      p.inst = d->vulkan.get();
      d->rhi.reset(QRhi::create(QRhi::Vulkan, &p, kFlags));
    }
  }
#endif
  // OpenGL when asked for, and as the fallback when nothing else was made
  if (!d->rhi && (api == QLatin1String("opengl") || requestedApi().isEmpty())) {
    d->surface.reset(QRhiGles2InitParams::newFallbackSurface());
    QRhiGles2InitParams p;
    p.fallbackSurface = d->surface.get();
    d->rhi.reset(QRhi::create(QRhi::OpenGLES2, &p, kFlags));
  }
  if (d->rhi)
    d->maxTile = std::min(d->maxTile, d->rhi->resourceLimit(QRhi::TextureSizeMax));
}

bool OffscreenRenderer::linearLight(void) const {
  return d->renderer.linearLight();
}

QSize OffscreenRenderer::sceneTargetSize(void) const {
  return d->renderer.targetSize();
}

QRhi * OffscreenRenderer::rhi(void) const {
  return d->rhi.get();
}

QVulkanInstance * OffscreenRenderer::vulkanInstance(void) const {
#if BTQT_VULKAN
  return d->rhi ? d->vulkan.get() : nullptr;
#else
  return nullptr;
#endif
}

QString OffscreenRenderer::deviceName(void) const {
  return d->rhi ? QString::fromUtf8(d->rhi->driverInfo().deviceName) : QString();
}

OffscreenRenderer::~OffscreenRenderer() {
  if (d->rhi) {
    if (!d->cacheFile.isEmpty() && d->rhi->isFeatureSupported(QRhi::PipelineCacheDataLoadSave))
      PipelineCache::write(d->cacheFile, d->rhi->pipelineCacheData());
    d->releaseTarget();
  }
  d->rhi.reset();
}

void OffscreenRenderer::setPipelineCacheFile(const QString & file) {
  d->cacheFile = file;
  d->cacheSeeded = false;
  if (!d->rhi || file.isEmpty() || !d->rhi->isFeatureSupported(QRhi::PipelineCacheDataLoadSave))
    return;
  /* QRhi checks the data -- its version, backend, driver and device -- and
   * ignores what does not fit; a missing or unreadable file reads empty */
  const QByteArray seed = PipelineCache::read(file);
  if (seed.isEmpty())
    return;
  d->rhi->setPipelineCacheData(seed);
  d->cacheSeeded = !d->rhi->pipelineCacheData().isEmpty();
}

bool OffscreenRenderer::pipelineCacheSeeded(void) const {
  return d->cacheSeeded;
}

bool OffscreenRenderer::isValid(void) const { return d->rhi != nullptr; }

QString OffscreenRenderer::backendName(void) const {
  return d->rhi ? QString::fromLatin1(d->rhi->backendName()) : QString();
}

void OffscreenRenderer::setMaxTile(int px) {
  d->maxTile = std::max(16, px);
}

QImage OffscreenRenderer::render(const SceneFrame & frame, QSize size) {
  if (!d->rhi || size.isEmpty())
    return {};
  const int tw = std::min(size.width(), d->maxTile), th = std::min(size.height(), d->maxTile);
  if (!d->makeTarget(QSize(tw, th)))
    return {};

  if (tw == size.width() && th == size.height())
    return d->renderTile(frame);

  QImage out(size, QImage::Format_RGBA8888_Premultiplied);
  out.fill(Qt::transparent);
  QPainter painter(&out);
  painter.setCompositionMode(QPainter::CompositionMode_Source);
  const float W = float(size.width()), H = float(size.height());
  for (int y = 0; y < size.height(); y += th)
    for (int x = 0; x < size.width(); x += tw) {
      /* this tile's part of the view: scale and shift clip space so the
       * tile's rectangle fills it (y runs up in clip space) */
      const float x0 = -1.0f + 2.0f * x / W, x1 = -1.0f + 2.0f * (x + tw) / W;
      const float y1 = 1.0f - 2.0f * y / H, y0 = 1.0f - 2.0f * (y + th) / H;
      btui::Mat4 m;
      m(0, 0) = 2.0f / (x1 - x0);
      m(0, 3) = -(x0 + x1) / (x1 - x0);
      m(1, 1) = 2.0f / (y1 - y0);
      m(1, 3) = -(y0 + y1) / (y1 - y0);
      SceneFrame f = frame;
      f.projection = m * frame.projection;
      const QImage tile = d->renderTile(f);
      if (tile.isNull())
        return {};
      painter.drawImage(x, y, tile);
    }
  painter.end();
  return out;
}
