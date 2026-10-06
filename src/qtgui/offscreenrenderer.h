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
#ifndef BTQT_OFFSCREENRENDERER_H
#define BTQT_OFFSCREENRENDERER_H

#include "scenerenderer.h"

#include <QImage>
#include <QSize>
#include <QtGui/qtguiglobal.h>

#include <memory>

/* Whether Qt's Vulkan classes can be used here -- Qt's own condition for
 * declaring them: built with Vulkan, and vulkan.h found where this is
 * compiled. Homebrew's Qt for macOS has the first without the second. */
#if QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>)
#define BTQT_VULKAN 1
#else
#define BTQT_VULKAN 0
#endif

class QRhi;
class QVulkanInstance;

/* The scene renderer without a window: a QRhi of its own and a texture to
 * draw into, read back as an image. The image export draws its pictures
 * with it, and the render tests check the renderer's output through it.
 *
 * Big pictures are drawn in tiles, each with its part of the projection
 * (legacy image_c did the same with OpenGL), so a picture may be larger
 * than the graphics API's biggest texture.
 */
class OffscreenRenderer {
public:

  enum class Backend {
    Default,     ///< Direct3D 11 on Windows, Metal on Apple systems, OpenGL elsewhere
    Software,    ///< as Default, but Direct3D's software rasteriser (WARP) on Windows
  };

  explicit OffscreenRenderer(Backend b = Backend::Default);
  ~OffscreenRenderer();

  OffscreenRenderer(const OffscreenRenderer &) = delete;
  OffscreenRenderer & operator=(const OffscreenRenderer &) = delete;

  bool isValid(void) const;
  /* "D3D11", "Metal", "Vulkan", "OpenGL", ... */
  QString backendName(void) const;
  /* the adapter: "llvmpipe (...)" for Mesa's software renderers */
  QString deviceName(void) const;

  /* The graphics API asked for in BURRTOOLS_OFFSCREEN_RHI -- d3d11, metal,
   * vulkan or opengl -- or empty for the platform's default. A requested API
   * that cannot be made leaves the renderer invalid rather than silently
   * using another, so a test run that asked for Vulkan really tests it. */
  static QString requestedApi(void);

  /* The device itself (null when invalid) and, for Vulkan, the instance it
   * was made from -- so Qt Quick can draw into it through a
   * QQuickRenderControl, as the tests of the real 3D view item do. */
  QRhi * rhi(void) const;
  QVulkanInstance * vulkanInstance(void) const;

  /* after a render: whether the scene was blended in linear light
   * (SceneRenderer::linearLight) */
  bool linearLight(void) const;

  /* Keep the device's pipeline cache in `file` (PipelineCache): seeded from
   * it now, when it holds a cache this device accepts, and written back
   * when the renderer goes away. Empty: no cache file. */
  void setPipelineCacheFile(const QString & file);
  /* whether setPipelineCacheFile() seeded the device from its file */
  bool pipelineCacheSeeded(void) const;

  /* largest tile edge; tests lower it to exercise the tiling */
  void setMaxTile(int px);

  /* The frame drawn at `size` pixels, premultiplied RGBA; null when there
   * is no graphics backend or a frame failed. The frame's clear colour is
   * used as is, so a transparent one gives a transparent background. */
  QImage render(const SceneFrame & frame, QSize size);

private:

  struct Impl;
  std::unique_ptr<Impl> d;
};

#endif
