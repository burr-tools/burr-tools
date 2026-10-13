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
#include "voxelviewport.h"
#include "scenerenderer.h"
#include "scenecontroller.h"

#include <QQuickWindow>
#include <rhi/qrhi.h>

namespace {

  /* Runs on Qt Quick's render thread. synchronize() is called while the GUI
   * thread is blocked, so reading the controller there is safe. */
  class Renderer : public QQuickRhiItemRenderer {
  public:
    void initialize(QRhiCommandBuffer *) override {
      m_renderer.initialize(rhi(), renderTarget()->renderPassDescriptor(), m_samples);
    }

    void synchronize(QQuickRhiItem * item) override {
      auto * v = static_cast<VoxelViewport *>(item);
      const float dpr = item->window() ? float(item->window()->effectiveDevicePixelRatio()) : 1.0f;
      // the device's limits, asked on this thread: only the render thread
      // may use the window's QRhi
      if (QRhi * r = rhi()) {
        v->setSupportedSampleCounts(r->supportedSampleCounts());
        m_countsKnown = true;
      }
      m_samples = v->msaaSamples();
      if (SceneController * c = v->controller())
        m_frame = c->frame(dpr);
    }

    void render(QRhiCommandBuffer * cb) override {
      m_renderer.initialize(rhi(), renderTarget()->renderPassDescriptor(), m_samples);
      m_renderer.render(cb, renderTarget(), m_frame);
      // synchronized before the device existed: once more, to learn its limits
      if (!m_countsKnown)
        update();
    }

  private:
    SceneRenderer m_renderer;
    SceneFrame m_frame;
    int m_samples = 4;
    bool m_countsKnown = false;
  };
}

VoxelViewport::VoxelViewport(QQuickItem * parent) : QQuickRhiItem(parent) {
  /* The item's own texture is single-sampled: the scene renderer draws and
   * multisamples in linear light in a target of its own (msaaSamples) and
   * copies the result here, encoded to sRGB. */
  setSampleCount(1);
  setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton | Qt::RightButton);
  setFlag(ItemIsFocusScope, false);
  setActiveFocusOnTab(false);
}

void VoxelViewport::setController(SceneController * c) {
  if (c == m_controller)
    return;
  disconnect(m_frameConnection);
  m_controller = c;
  if (c) {
    m_frameConnection = connect(c, &SceneController::frameChanged, this, &QQuickItem::update);
    c->setViewportSize(float(width()), float(height()));
  }
  emit controllerChanged();
  update();
}

QQuickRhiItemRenderer * VoxelViewport::createRenderer(void) {
  return new Renderer;
}

int VoxelViewport::bestSampleCount(const QList<int> & supported, int wanted) {
  for (int n : { 8, 4, 2 })
    if (n <= wanted && supported.contains(n))
      return n;
  return 1;
}

/* Read on the render thread in synchronize(), while the GUI thread waits;
 * a change of the setting reaches here through the controller's
 * frameChanged, which every settings change emits. */
int VoxelViewport::msaaSamples(void) const {
  const int wanted = m_controller ? m_controller->wantedSamples() : 4;
  return m_supported.isEmpty() ? wanted : bestSampleCount(m_supported, wanted);
}


bool VoxelViewport::setSupportedSampleCounts(const QList<int> & counts) {
  if (counts == m_supported)
    return false;
  m_supported = counts;
  return true;
}
void VoxelViewport::geometryChange(const QRectF & newGeometry, const QRectF & oldGeometry) {
  QQuickRhiItem::geometryChange(newGeometry, oldGeometry);
  if (m_controller && newGeometry.size() != oldGeometry.size())
    m_controller->setViewportSize(float(newGeometry.width()), float(newGeometry.height()));
}

void VoxelViewport::mousePressEvent(QMouseEvent * e) {
  if (m_controller)
    m_controller->pointerPress(e->button(), float(e->position().x()), float(e->position().y()), e->modifiers());
  e->accept();
}

void VoxelViewport::mouseMoveEvent(QMouseEvent * e) {
  if (m_controller)
    m_controller->pointerMove(float(e->position().x()), float(e->position().y()));
  e->accept();
}

void VoxelViewport::mouseReleaseEvent(QMouseEvent * e) {
  if (m_controller)
    m_controller->pointerRelease(e->button(), float(e->position().x()), float(e->position().y()));
  e->accept();
}

void VoxelViewport::wheelEvent(QWheelEvent * e) {
  if (m_controller)
    m_controller->wheel(e->angleDelta().y());
  e->accept();
}
