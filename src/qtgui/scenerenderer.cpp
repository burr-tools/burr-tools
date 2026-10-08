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
#include "scenerenderer.h"

#include <rhi/qrhi.h>

#include <algorithm>
#include <cmath>
#include <cstring>

// defined in the generated shaders_embed.cpp (shaders/embed_shaders.py)
QShader burrtoolsShader(const char * name);

namespace {

  // uniform blocks, std140; must match the shaders
  struct MeshUniforms {
    float mvp[16];
    float modelView[16];
    float light[4];
    float dim[4];
    float outline[4];
    float misc[4];
  };
  static_assert(sizeof(MeshUniforms) == 192);

  /* the face outline of the flat voxel style (C06, the mock: a 1 dp stroke
   * centred on each side). Each face draws its own half, so a side shared by
   * two faces gets both halves; dashes as the mock's 3/3. */
  constexpr float kOutlineHalfWidthDp = 0.6f;
  constexpr float kOutlineDashDp = 3.0f;

  /* For the nearest translucent surface only, the translucent voxels are
   * drawn twice: depth only, then colour where the depth is still the same.
   * The second pass is pulled this much (in clip-space depth, times w)
   * toward the viewer, so a driver that computes the position a hair
   * differently in the two pipelines cannot make the colour fail its own
   * depth -- which shows as stray lines. */
  constexpr float kTranslucentBias = 2e-6f;

  constexpr float kLinearOverlayScale = 0.4f;

  /* The scene targets grow in steps of this many pixels and are kept while
   * the view still fits in them, so resizing the window does not make the
   * (multisampled, half-float) targets anew for every pixel of the drag. */
  constexpr int kTargetStep = 64;

  QSize roundedUp(QSize s) {
    auto up = [](int v) { return std::max(kTargetStep, (v + kTargetStep - 1) / kTargetStep * kTargetStep); };
    return QSize(up(s.width()), up(s.height()));
  }

  qint64 area(QSize s) {
    return qint64(s.width()) * s.height();
  }

  struct LineUniforms {
    float mvp[16];
    float viewport[4];
  };
  static_assert(sizeof(LineUniforms) == 80);

  struct CompositeUniforms {
    float target[4];
  };
  static_assert(sizeof(CompositeUniforms) == 16);

  struct LineVertex {
    float p0[3] {};
    float p1[3] {};
    float corner[2] {};
    btui::Rgba8 color;
    float style[4] {};
  };
  static_assert(sizeof(LineVertex) == 52);

  constexpr int kMeshStride = sizeof(btui::MeshVertex);

  // the spec's light direction, in view space (C13; the mock uses it for the scene too)
  constexpr const float (&kLight)[3] = btui::kSceneLight;

  void copyMat(float * dst, const btui::Mat4 & m) {
    std::memcpy(dst, m.m.data(), 16 * sizeof(float));
  }

  btui::Mat4 fromQt(const QMatrix4x4 & q) {
    btui::Mat4 m;
    std::memcpy(m.m.data(), q.constData(), 16 * sizeof(float));
    return m;
  }

  float toLinear(float c) {
    return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
  }

  /* a dynamic vertex buffer that grows to fit */
  struct GrowBuffer {
    std::unique_ptr<QRhiBuffer> buf;
    quint32 capacity = 0;

    void ensure(QRhi * rhi, quint32 bytes, QRhiBuffer::Type type, QRhiBuffer::UsageFlags usage) {
      if (buf && capacity >= bytes)
        return;
      capacity = std::max<quint32>(bytes, 4096);
      buf.reset(rhi->newBuffer(type, usage, capacity));
      buf->create();
    }
  };
}

struct SceneRenderer::Impl {
  QRhi * rhi = nullptr;
  QRhiRenderPassDescriptor * outRp = nullptr;   // the caller's targets
  int wantedSamples = 1;

  /* The scene's own target: linear-light RGBA16F where the device renders
   * to it (else RGBA8, blended as stored), multisampled into a renderbuffer
   * and resolved into sceneTex, which the composite pass reads. */
  bool linear = false;
  int samples = 1;                               // what the target got
  QSize sceneSize;                               // the targets' size, at least the view's
  std::unique_ptr<QRhiTexture> sceneTex;
  std::unique_ptr<QRhiRenderBuffer> sceneMsaa;
  std::unique_ptr<QRhiRenderBuffer> sceneDepth;
  std::unique_ptr<QRhiTextureRenderTarget> sceneRt;
  std::unique_ptr<QRhiRenderPassDescriptor> sceneRp;

  std::unique_ptr<QRhiBuffer> meshUbo, translucentUbo, overlayUbo, lineUbo, compositeUbo;
  std::unique_ptr<QRhiShaderResourceBindings> meshSrb, translucentSrb, overlaySrb, lineSrb, compositeSrb;
  std::unique_ptr<QRhiSampler> sampler;
  std::unique_ptr<QRhiBuffer> triangle;          // the composite pass's one big triangle
  bool triangleUploaded = false;

  std::unique_ptr<QRhiGraphicsPipeline> opaque, depthOnly, translucent, layered, xray, overlay, lines, linesFirst, composite;
  QRhiRenderPassDescriptor * compositeRp = nullptr;   // what `composite` was made for

  // the mesh, uploaded once per mesh object and revision: revisions alone
  // repeat between controllers. Held, so its address is not reused.
  std::unique_ptr<QRhiBuffer> meshVbuf;
  quint64 meshRev = ~quint64(0);
  std::shared_ptr<const btui::ShapeMesh> meshUploaded;
  quint32 opaqueCount = 0, translucentCount = 0;

  /* every translucent layer, farthest first: the triangles' centres, and
   * their indices sorted by view depth for the view they were sorted for */
  std::vector<btui::Vec3> translucentCentres;
  GrowBuffer layeredIndex;
  btui::Mat4 sortedView;
  bool sorted = false;

  GrowBuffer overlayVbuf, lineVbuf;

  void createShared(void);
  bool ensureSceneTarget(QSize size);
  bool makeSceneTarget(QSize size, QRhiTexture::Format format, int count);
  void createScenePipelines(void);
  void createComposite(void);
  void releaseScene(void);
};

SceneRenderer::SceneRenderer(void) : d(std::make_unique<Impl>()) {}
SceneRenderer::~SceneRenderer(void) = default;

void SceneRenderer::release(void) {
  auto * keep = d.release();
  delete keep;
  d = std::make_unique<Impl>();
}

void SceneRenderer::initialize(QRhi * rhi, QRhiRenderPassDescriptor * rp, int sampleCount) {
  if (d->rhi != rhi) {
    release();
    d->rhi = rhi;
  }
  d->outRp = rp;
  if (sampleCount != d->wantedSamples) {
    d->wantedSamples = sampleCount;
    d->releaseScene();            // remade, with its pipelines, at the next frame
  }
}

int SceneRenderer::sampleCount(void) const {
  return d->samples;
}

bool SceneRenderer::linearLight(void) const {
  return d->linear;
}

QSize SceneRenderer::targetSize(void) const {
  return d->sceneSize;
}

void SceneRenderer::Impl::releaseScene(void) {
  opaque.reset();
  depthOnly.reset();
  translucent.reset();
  layered.reset();
  xray.reset();
  overlay.reset();
  lines.reset();
  linesFirst.reset();
  composite.reset();
  compositeSrb.reset();
  sceneRt.reset();
  sceneRp.reset();
  sceneDepth.reset();
  sceneMsaa.reset();
  sceneTex.reset();
  sceneSize = QSize();
}

void SceneRenderer::Impl::createShared(void) {
  if (meshUbo)
    return;
  auto ubo = [this](int size) {
    std::unique_ptr<QRhiBuffer> b(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, quint32(size)));
    b->create();
    return b;
  };
  meshUbo = ubo(sizeof(MeshUniforms));
  translucentUbo = ubo(sizeof(MeshUniforms));
  overlayUbo = ubo(sizeof(MeshUniforms));
  lineUbo = ubo(sizeof(LineUniforms));
  compositeUbo = ubo(sizeof(CompositeUniforms));

  const auto stages = QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;
  auto srb = [this, stages](QRhiBuffer * b) {
    std::unique_ptr<QRhiShaderResourceBindings> r(rhi->newShaderResourceBindings());
    r->setBindings({ QRhiShaderResourceBinding::uniformBuffer(0, stages, b) });
    r->create();
    return r;
  };
  meshSrb = srb(meshUbo.get());
  translucentSrb = srb(translucentUbo.get());
  overlaySrb = srb(overlayUbo.get());
  lineSrb = srb(lineUbo.get());

  sampler.reset(rhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                                QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
  sampler->create();

  triangle.reset(rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, 6 * sizeof(float)));
  triangle->create();
  triangleUploaded = false;
}

bool SceneRenderer::Impl::makeSceneTarget(QSize size, QRhiTexture::Format format, int count) {
  sceneRt.reset();
  sceneDepth.reset();
  sceneMsaa.reset();
  sceneTex.reset(rhi->newTexture(format, size, 1, QRhiTexture::RenderTarget));
  if (!sceneTex->create())
    return false;
  QRhiColorAttachment colour(sceneTex.get());
  if (count > 1) {
    sceneMsaa.reset(rhi->newRenderBuffer(QRhiRenderBuffer::Color, size, count, {}, format));
    if (!sceneMsaa->create())
      return false;
    colour = QRhiColorAttachment(sceneMsaa.get());
    colour.setResolveTexture(sceneTex.get());
  }
  sceneDepth.reset(rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, size, count));
  if (!sceneDepth->create())
    return false;
  QRhiTextureRenderTargetDescription desc{ colour };
  desc.setDepthStencilBuffer(sceneDepth.get());
  sceneRt.reset(rhi->newTextureRenderTarget(desc));
  // one render pass layout for every size of the same format and count
  if (!sceneRp)
    sceneRp.reset(sceneRt->newCompatibleRenderPassDescriptor());
  sceneRt->setRenderPassDescriptor(sceneRp.get());
  return sceneRt->create();
}

/* Scene targets this frame's size fits in. Their format and sample count
 * are settled at the first frame: RGBA16F (linear light) if the device
 * renders to it, the wanted multisampling or the most below it that works.
 * They are kept while the view fits and still uses half of them or more. */
bool SceneRenderer::Impl::ensureSceneTarget(QSize view) {
  const QSize size = roundedUp(view);
  if (sceneRt && view.width() <= sceneSize.width() && view.height() <= sceneSize.height() &&
      area(sceneSize) <= 2 * area(size))
    return true;
  if (sceneRt) {
    // same format and count, a new size: the pipelines stay
    if (!makeSceneTarget(size, linear ? QRhiTexture::RGBA16F : QRhiTexture::RGBA8, samples))
      return false;
  } else {
    bool made = false;
    const bool canFloat = rhi->isTextureFormatSupported(QRhiTexture::RGBA16F);
    const bool canMsaa = rhi->isFeatureSupported(QRhi::MultisampleRenderBuffer);
    const QList<int> supported = rhi->supportedSampleCounts();
    for (bool lin : { true, false }) {
      if (lin && !canFloat)
        continue;
      for (int n : { wantedSamples, 8, 4, 2, 1 }) {
        if (n > wantedSamples || (n > 1 && (!canMsaa || !supported.contains(n))))
          continue;
        sceneRp.reset();
        if (makeSceneTarget(size, lin ? QRhiTexture::RGBA16F : QRhiTexture::RGBA8, n)) {
          linear = lin;
          samples = n;
          made = true;
          break;
        }
      }
      if (made)
        break;
    }
    if (!made)
      return false;
    createScenePipelines();
  }
  sceneSize = size;
  compositeSrb.reset(rhi->newShaderResourceBindings());
  compositeSrb->setBindings({
    QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::FragmentStage, compositeUbo.get()),
    QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, sceneTex.get(), sampler.get()),
  });
  compositeSrb->create();
  return true;
}

void SceneRenderer::Impl::createScenePipelines(void) {
  QRhiVertexInputLayout meshLayout;
  meshLayout.setBindings({ { kMeshStride } });
  meshLayout.setAttributes({
    { 0, 0, QRhiVertexInputAttribute::Float3, offsetof(btui::MeshVertex, pos) },
    { 0, 1, QRhiVertexInputAttribute::Float3, offsetof(btui::MeshVertex, normal) },
    { 0, 2, QRhiVertexInputAttribute::UNormByte4, offsetof(btui::MeshVertex, color) },
    { 0, 3, QRhiVertexInputAttribute::Float3, offsetof(btui::MeshVertex, cell) },
    { 0, 4, QRhiVertexInputAttribute::Float4, offsetof(btui::MeshVertex, edge) },
  });

  QRhiVertexInputLayout lineLayout;
  lineLayout.setBindings({ { sizeof(LineVertex) } });
  lineLayout.setAttributes({
    { 0, 0, QRhiVertexInputAttribute::Float3, offsetof(LineVertex, p0) },
    { 0, 1, QRhiVertexInputAttribute::Float3, offsetof(LineVertex, p1) },
    { 0, 2, QRhiVertexInputAttribute::Float2, offsetof(LineVertex, corner) },
    { 0, 3, QRhiVertexInputAttribute::UNormByte4, offsetof(LineVertex, color) },
    { 0, 4, QRhiVertexInputAttribute::Float4, offsetof(LineVertex, style) },
  });

  const QShader meshVs = burrtoolsShader("mesh.vert.qsb");
  const QShader meshFs = burrtoolsShader("mesh.frag.qsb");
  const QShader lineVs = burrtoolsShader("line.vert.qsb");
  const QShader lineFs = burrtoolsShader("line.frag.qsb");

  // premultiplied alpha: the fragment shaders output colour * alpha
  QRhiGraphicsPipeline::TargetBlend blend;
  blend.enable = true;
  blend.srcColor = QRhiGraphicsPipeline::One;
  blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
  blend.srcAlpha = QRhiGraphicsPipeline::One;
  blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;

  QRhiGraphicsPipeline::TargetBlend noColor;
  noColor.colorWrite = {};

  auto make = [&](const QShader & vs, const QShader & fs, const QRhiVertexInputLayout & layout,
                  QRhiShaderResourceBindings * srb, QRhiGraphicsPipeline::CullMode cull, bool depthWrite,
                  QRhiGraphicsPipeline::CompareOp depthOp, const QRhiGraphicsPipeline::TargetBlend * tb) {
    std::unique_ptr<QRhiGraphicsPipeline> p(rhi->newGraphicsPipeline());
    p->setShaderStages({ { QRhiShaderStage::Vertex, vs }, { QRhiShaderStage::Fragment, fs } });
    p->setVertexInputLayout(layout);
    p->setShaderResourceBindings(srb);
    p->setRenderPassDescriptor(sceneRp.get());
    p->setSampleCount(samples);
    p->setCullMode(cull);
    p->setFrontFace(QRhiGraphicsPipeline::CCW);
    p->setDepthTest(true);
    p->setDepthWrite(depthWrite);
    p->setDepthOp(depthOp);
    if (tb)
      p->setTargetBlends({ *tb });
    p->create();
    return p;
  };

  using GP = QRhiGraphicsPipeline;
  opaque = make(meshVs, meshFs, meshLayout, meshSrb.get(), GP::Back, true, GP::Less, nullptr);
  depthOnly = make(meshVs, meshFs, meshLayout, meshSrb.get(), GP::Back, true, GP::Less, &noColor);
  translucent = make(meshVs, meshFs, meshLayout, translucentSrb.get(), GP::Back, false, GP::LessOrEqual, &blend);
  // every layer, back to front, in front of the opaque voxels
  layered = make(meshVs, meshFs, meshLayout, meshSrb.get(), GP::Back, false, GP::LessOrEqual, &blend);
  xray = make(meshVs, meshFs, meshLayout, meshSrb.get(), GP::None, false, GP::Always, &blend);
  overlay = make(meshVs, meshFs, meshLayout, overlaySrb.get(), GP::None, false, GP::LessOrEqual, &blend);
  lines = make(lineVs, lineFs, lineLayout, lineSrb.get(), GP::None, false, GP::LessOrEqual, &blend);
  // before every translucent layer: a line inside a translucent block is
  // seen through the voxels in front of it, one in front stays on top
  linesFirst = make(lineVs, lineFs, lineLayout, lineSrb.get(), GP::None, true, GP::LessOrEqual, &blend);
}

/* The copy into the caller's target, made for its render pass layout. */
void SceneRenderer::Impl::createComposite(void) {
  QRhiVertexInputLayout layout;
  layout.setBindings({ { 2 * sizeof(float) } });
  layout.setAttributes({ { 0, 0, QRhiVertexInputAttribute::Float2, 0 } });
  composite.reset(rhi->newGraphicsPipeline());
  composite->setShaderStages({ { QRhiShaderStage::Vertex, burrtoolsShader("composite.vert.qsb") },
                               { QRhiShaderStage::Fragment, burrtoolsShader("composite.frag.qsb") } });
  composite->setVertexInputLayout(layout);
  composite->setShaderResourceBindings(compositeSrb.get());
  composite->setRenderPassDescriptor(outRp);
  composite->create();
  compositeRp = outRp;
}

void SceneRenderer::render(QRhiCommandBuffer * cb, QRhiRenderTarget * rt, const SceneFrame & f) {
  QRhi * rhi = d->rhi;
  const QSize px = rt->pixelSize();
  d->createShared();
  if (!d->ensureSceneTarget(px))
    return;
  if (!d->composite || d->compositeRp != d->outRp)
    d->createComposite();

  QRhiResourceUpdateBatch * u = rhi->nextResourceUpdateBatch();
  if (!d->triangleUploaded) {
    static const float tri[] = { -1, -1, 3, -1, -1, 3 };
    u->uploadStaticBuffer(d->triangle.get(), tri);
    d->triangleUploaded = true;
  }

  // --- the mesh, when it changed ---
  if (f.meshRevision != d->meshRev || f.mesh != d->meshUploaded) {
    d->meshRev = f.meshRevision;
    d->meshUploaded = f.mesh;
    d->opaqueCount = d->translucentCount = 0;
    d->meshVbuf.reset();
    if (f.mesh && !f.mesh->empty()) {
      const auto & o = f.mesh->opaque;
      const auto & t = f.mesh->translucent;
      const quint32 bytes = quint32((o.size() + t.size()) * kMeshStride);
      d->meshVbuf.reset(rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, bytes));
      d->meshVbuf->create();
      if (!o.empty())
        u->uploadStaticBuffer(d->meshVbuf.get(), 0, quint32(o.size() * kMeshStride), o.data());
      if (!t.empty())
        u->uploadStaticBuffer(d->meshVbuf.get(), quint32(o.size() * kMeshStride), quint32(t.size() * kMeshStride), t.data());
      d->opaqueCount = quint32(o.size());
      d->translucentCount = quint32(t.size());
    }
    d->translucentCentres.clear();
    d->sorted = false;
    if (f.mesh) {
      const auto & t = f.mesh->translucent;
      d->translucentCentres.reserve(t.size() / 3);
      for (size_t i = 0; i + 2 < t.size(); i += 3)
        d->translucentCentres.push_back(btui::Vec3{ t[i].pos[0] + t[i + 1].pos[0] + t[i + 2].pos[0],
                                                    t[i].pos[1] + t[i + 1].pos[1] + t[i + 2].pos[1],
                                                    t[i].pos[2] + t[i + 1].pos[2] + t[i + 2].pos[2] } * (1.0f / 3.0f));
    }
  }

  // --- every translucent layer: the triangles farthest first, re-sorted
  // when the view turns (voxel faces never cross, so this is exact) ---
  const bool layers = f.translucentLayers && !f.xray && d->translucentCount;
  if (layers && (!d->sorted || std::memcmp(f.view.m.data(), d->sortedView.m.data(), sizeof(float) * 16) != 0)) {
    const size_t n = d->translucentCentres.size();
    std::vector<std::pair<float, quint32>> order(n);
    for (size_t i = 0; i < n; i++) {
      const btui::Vec3 & c = d->translucentCentres[i];
      // the view looks down -z: the most negative z is the farthest
      order[i] = { f.view(2, 0) * c.x + f.view(2, 1) * c.y + f.view(2, 2) * c.z + f.view(2, 3), quint32(i) };
    }
    std::sort(order.begin(), order.end(), [](const auto & a, const auto & b) { return a.first < b.first; });
    std::vector<quint32> idx;
    idx.reserve(n * 3);
    for (const auto & o : order) {
      const quint32 base = d->opaqueCount + o.second * 3;
      idx.push_back(base);
      idx.push_back(base + 1);
      idx.push_back(base + 2);
    }
    const quint32 bytes = quint32(idx.size() * sizeof(quint32));
    d->layeredIndex.ensure(rhi, bytes, QRhiBuffer::Dynamic, QRhiBuffer::IndexBuffer);
    u->updateDynamicBuffer(d->layeredIndex.buf.get(), 0, bytes, idx.data());
    d->sortedView = f.view;
    d->sorted = true;
  }

  // --- uniforms ---
  const btui::Mat4 proj = fromQt(rhi->clipSpaceCorrMatrix()) * f.projection;
  const btui::Mat4 mvp = proj * f.view;
  const float lin = d->linear ? 1.0f : 0.0f;

  MeshUniforms mu{};
  copyMat(mu.mvp, mvp);
  copyMat(mu.modelView, f.view);
  mu.light[0] = kLight[0]; mu.light[1] = kLight[1]; mu.light[2] = kLight[2];
  mu.light[3] = f.lighting ? 1.0f : 0.0f;
  mu.dim[0] = float(f.dimAxis);
  mu.dim[1] = f.dimLayer;
  mu.dim[2] = f.dimAlpha;
  mu.dim[3] = 1.0f;
  mu.outline[0] = kOutlineHalfWidthDp * f.devicePixelRatio;
  /* outlineStrength is how much darker the mock's stroke makes a face, in
   * sRGB; in linear light the same darkening takes more black */
  mu.outline[1] = d->linear ? 1.0f - std::pow(1.0f - f.outlineStrength, 2.2f) : f.outlineStrength;
  mu.outline[2] = kOutlineDashDp * f.devicePixelRatio;
  mu.misc[0] = lin;
  u->updateDynamicBuffer(d->meshUbo.get(), 0, sizeof(mu), &mu);

  MeshUniforms tu = mu;
  tu.outline[3] = kTranslucentBias;
  u->updateDynamicBuffer(d->translucentUbo.get(), 0, sizeof(tu), &tu);

  MeshUniforms ou = mu;
  ou.light[3] = 0;          // overlays are flat
  ou.dim[0] = -1;           // and never dimmed
  ou.outline[1] = 0;        // nor outlined
  ou.outline[3] = 5e-4f;    // and drawn just in front of the faces they lie on
  /* the slab's 11 % accent (C06) is the mock's sRGB mix; blended in linear
   * light a light tint over dark voxels comes out about three times as
   * strong, so it is scaled back to read like the mock's */
  ou.dim[3] = d->linear ? kLinearOverlayScale : 1.0f;
  u->updateDynamicBuffer(d->overlayUbo.get(), 0, sizeof(ou), &ou);

  LineUniforms lu{};
  copyMat(lu.mvp, mvp);
  lu.viewport[0] = float(px.width());
  lu.viewport[1] = float(px.height());
  lu.viewport[2] = lin;
  u->updateDynamicBuffer(d->lineUbo.get(), 0, sizeof(lu), &lu);

  CompositeUniforms cu{};
  cu.target[0] = float(d->sceneSize.width());     // the scene texture, which the view may not fill
  cu.target[1] = float(d->sceneSize.height());
  cu.target[2] = lin;
  u->updateDynamicBuffer(d->compositeUbo.get(), 0, sizeof(cu), &cu);

  // --- overlay geometry (rebuilt every frame; a few dozen vertices) ---
  const quint32 overlayCount = quint32(f.overlayFaces.size());
  if (overlayCount) {
    d->overlayVbuf.ensure(rhi, overlayCount * kMeshStride, QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer);
    u->updateDynamicBuffer(d->overlayVbuf.buf.get(), 0, overlayCount * kMeshStride, f.overlayFaces.data());
  }

  std::vector<LineVertex> lv;
  lv.reserve(f.lines.size() * 6);
  for (const btui::LineSeg & s : f.lines) {
    const float corners[6][2] = { { 0, -1 }, { 1, -1 }, { 1, 1 }, { 0, -1 }, { 1, 1 }, { 0, 1 } };
    for (const auto & c : corners) {
      LineVertex v;
      v.p0[0] = s.a.x; v.p0[1] = s.a.y; v.p0[2] = s.a.z;
      v.p1[0] = s.b.x; v.p1[1] = s.b.y; v.p1[2] = s.b.z;
      v.corner[0] = c[0]; v.corner[1] = c[1];
      v.color = s.color;
      v.style[0] = s.widthDp * f.devicePixelRatio;
      v.style[1] = s.dashDp * f.devicePixelRatio;
      v.style[2] = s.gapDp * f.devicePixelRatio;
      v.style[3] = 0;
      lv.push_back(v);
    }
  }
  const quint32 lineCount = quint32(lv.size());
  if (lineCount) {
    d->lineVbuf.ensure(rhi, lineCount * sizeof(LineVertex), QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer);
    u->updateDynamicBuffer(d->lineVbuf.buf.get(), 0, lineCount * sizeof(LineVertex), lv.data());
  }

  // --- the scene, into its own (linear, multisampled) target ---
  QColor clear = f.clear;
  if (d->linear)
    clear = QColor::fromRgbF(toLinear(clear.redF()), toLinear(clear.greenF()), toLinear(clear.blueF()), clear.alphaF());
  // premultiplied, like everything drawn on it
  clear = QColor::fromRgbF(clear.redF() * clear.alphaF(), clear.greenF() * clear.alphaF(),
                           clear.blueF() * clear.alphaF(), clear.alphaF());
  cb->beginPass(d->sceneRt.get(), clear, { 1.0f, 0 }, u);
  /* The view's corner of the (possibly larger) scene texture: the rows the
   * composite pass reads back at the same fragment coordinates -- the
   * bottom ones where framebuffers are y-up (OpenGL), the top ones elsewhere
   * (a viewport's origin is its bottom left). */
  const float sceneY = rhi->isYUpInFramebuffer() ? 0.0f : float(d->sceneSize.height() - px.height());
  const QRhiViewport vp(0, sceneY, float(px.width()), float(px.height()));

  if (d->meshVbuf && d->opaqueCount) {
    cb->setGraphicsPipeline(d->opaque.get());
    cb->setViewport(vp);
    cb->setShaderResources();
    const QRhiCommandBuffer::VertexInput vi(d->meshVbuf.get(), 0);
    cb->setVertexInput(0, 1, &vi);
    cb->draw(d->opaqueCount);
  }

  if (lineCount && layers) {
    cb->setGraphicsPipeline(d->linesFirst.get());
    cb->setViewport(vp);
    cb->setShaderResources();
    const QRhiCommandBuffer::VertexInput vi(d->lineVbuf.buf.get(), 0);
    cb->setVertexInput(0, 1, &vi);
    cb->draw(lineCount);
  }

  if (d->meshVbuf && layers) {
    const QRhiCommandBuffer::VertexInput vi(d->meshVbuf.get(), 0);
    cb->setGraphicsPipeline(d->layered.get());
    cb->setViewport(vp);
    cb->setShaderResources();
    cb->setVertexInput(0, 1, &vi, d->layeredIndex.buf.get(), 0, QRhiCommandBuffer::IndexUInt32);
    cb->drawIndexed(d->translucentCount);
  } else if (d->meshVbuf && d->translucentCount && f.xray) {
    const QRhiCommandBuffer::VertexInput vi(d->meshVbuf.get(), d->opaqueCount * kMeshStride);
    cb->setGraphicsPipeline(d->xray.get());
    cb->setViewport(vp);
    cb->setShaderResources();
    cb->setVertexInput(0, 1, &vi);
    cb->draw(d->translucentCount);
  } else if (d->meshVbuf && d->translucentCount) {
    const QRhiCommandBuffer::VertexInput vi(d->meshVbuf.get(), d->opaqueCount * kMeshStride);
    // depth first, then colour on that depth: only the nearest translucent
    // surface is blended (legacy voxelframe.cpp, run 1)
    cb->setGraphicsPipeline(d->depthOnly.get());
    cb->setViewport(vp);
    cb->setShaderResources();
    cb->setVertexInput(0, 1, &vi);
    cb->draw(d->translucentCount);
    cb->setGraphicsPipeline(d->translucent.get());
    cb->setViewport(vp);
    cb->setShaderResources();
    cb->setVertexInput(0, 1, &vi);
    cb->draw(d->translucentCount);
  }

  if (overlayCount) {
    cb->setGraphicsPipeline(d->overlay.get());
    cb->setViewport(vp);
    cb->setShaderResources();
    const QRhiCommandBuffer::VertexInput vi(d->overlayVbuf.buf.get(), 0);
    cb->setVertexInput(0, 1, &vi);
    cb->draw(overlayCount);
  }

  if (lineCount && !layers) {
    cb->setGraphicsPipeline(d->lines.get());
    cb->setViewport(vp);
    cb->setShaderResources();
    const QRhiCommandBuffer::VertexInput vi(d->lineVbuf.buf.get(), 0);
    cb->setVertexInput(0, 1, &vi);
    cb->draw(lineCount);
  }
  cb->endPass();

  // --- into the caller's target, encoded to sRGB ---
  cb->beginPass(rt, Qt::transparent, { 1.0f, 0 });
  cb->setGraphicsPipeline(d->composite.get());
  cb->setViewport(QRhiViewport(0, 0, float(px.width()), float(px.height())));
  cb->setShaderResources(d->compositeSrb.get());
  const QRhiCommandBuffer::VertexInput tvi(d->triangle.get(), 0);
  cb->setVertexInput(0, 1, &tvi);
  cb->draw(3);
  cb->endPass();
}
