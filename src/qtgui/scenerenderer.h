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
#ifndef BTQT_SCENERENDERER_H
#define BTQT_SCENERENDERER_H

#include "../uicore/scenemesh.h"

#include <QColor>
#include <QSize>

#include <memory>
#include <vector>

class QRhi;
class QRhiBuffer;
class QRhiCommandBuffer;
class QRhiGraphicsPipeline;
class QRhiRenderPassDescriptor;
class QRhiRenderTarget;
class QRhiShaderResourceBindings;

/* Everything one frame of the 3D view shows, gathered on the GUI thread and
 * handed to the renderer, which may run on Qt Quick's render thread. */
struct SceneFrame {
  QColor clear;
  btui::Mat4 view;                        // world -> view
  btui::Mat4 projection;                  // OpenGL-style clip space; the renderer corrects per API

  std::shared_ptr<const btui::ShapeMesh> mesh;   // may be null: nothing to draw
  quint64 meshRevision = 0;               // changes whenever mesh does

  std::vector<btui::LineSeg> lines;       // overlay lines
  std::vector<btui::MeshVertex> overlayFaces;    // translucent overlay triangles (the layer slab)

  float devicePixelRatio = 1;
  bool lighting = true;
  int dimAxis = -1;                       // grid axis of the active layer when dimming, else -1
  float dimLayer = 0;
  float dimAlpha = 0.28f;                 // C06: voxels outside the layer at 28 %
  /* how dark the face outlines are, where the mesh has them (the flat voxel
   * style): C06 / the mock stroke them in black at 32 % */
  float outlineStrength = 0.32f;
  /* every translucent face blended, front and back, ignoring depth: the
   * STL preview's "insides" view (legacy setInsideVisible) */
  bool xray = false;
  /* every translucent surface blended, farthest first, so translucent
   * voxels inside show through the outer ones (the flat style's variable
   * voxels); otherwise only the nearest one, as legacy does */
  bool translucentLayers = false;
};

/* The 3D view's renderer on Qt's graphics abstraction (QRhi): Direct3D
 * 11/12, Vulkan, Metal or OpenGL, whichever the platform and the
 * BURRTOOLS_RHI override pick.
 *
 * It only knows a QRhi and a render target, not a window, so the same code
 * draws into the QQuickRhiItem on screen and into an offscreen texture in the
 * image export and the render tests. Draw order: opaque voxels; translucent
 * voxels with a depth pre-pass (only the nearest translucent surface is
 * blended, the way legacy voxelframe.cpp does it); translucent overlays;
 * overlay lines.
 *
 * Gamma-correct: the scene goes into a target of the renderer's own, in
 * linear light (RGBA16F), multisampled there and resolved, so anti-aliased
 * edges and translucent voxels blend evenly; a last pass encodes it to sRGB
 * into the caller's target. Faces are still shaded in sRGB -- the mock's and
 * legacy's colours, exactly -- and only then turned linear.
 *
 * QRhi's API has limited compatibility guarantees between Qt minor versions,
 * so all of it stays inside this class.
 */
class SceneRenderer {

public:

  SceneRenderer(void);
  ~SceneRenderer(void);

  SceneRenderer(const SceneRenderer &) = delete;
  SceneRenderer & operator=(const SceneRenderer &) = delete;

  /* The device, the render pass layout of the targets render() draws into
   * (single-sampled) and the multisampling wanted for the scene. Cheap when
   * nothing changed; call before every render. */
  void initialize(QRhi * rhi, QRhiRenderPassDescriptor * rp, int sampleCount);

  /* Record one frame into cb: uploads, the scene into the renderer's own
   * target, then a pass on rt that copies it there. */
  void render(QRhiCommandBuffer * cb, QRhiRenderTarget * rt, const SceneFrame & f);

  /* After the first frame: the multisampling the scene really got, and
   * whether it is drawn in linear light (RGBA16F) or, where the device
   * cannot render to that, blended in sRGB as stored. */
  int sampleCount(void) const;
  bool linearLight(void) const;

  /* Drop every GPU resource (the QRhi is going away). */
  void release(void);

private:

  struct Impl;
  std::unique_ptr<Impl> d;
};

#endif
