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
#include "viewcubeitem.h"
#include "theme.h"
#include "scenecontroller.h"

#include <QCursor>
#include <QFont>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <array>
#include <cmath>

using btui::Quat;
using btui::Vec3;
using btui::ViewCube;

namespace {

  // C13 anatomy: face tones, edges and the highlight
  const QColor kBaseLight(0xd9, 0xdd, 0xe4), kBaseDark(0x4a, 0x52, 0x62);
  const QColor kEdgeLight(0x3a, 0x3f, 0x4a), kEdgeDark(0xaa, 0xb1, 0xbf);
  const QColor kHighlight(0x4f, 0x7d, 0xf0);

  const Vec3 kLight = btui::normalize(Vec3{ -0.35f, 0.75f, 0.55f });

  QPointF pt(Quat q, bool persp, Vec3 p) {
    Vec3 s = ViewCube::project(q, persp, p);
    return { s.x, s.y };
  }

  QColor shaded(const QColor & base, float f) {
    auto c = [f](int v) { return std::clamp(int(v * f + 0.5f), 0, 255); };
    return QColor(c(base.red()), c(base.green()), c(base.blue()));
  }

  QColor axisColor(int axis) {
    if (const Theme * t = Theme::instance())
      return axis == 0 ? t->axisX() : (axis == 1 ? t->axisY() : t->axisZ());
    return axis == 0 ? QColor(0xe5, 0x48, 0x4d) : (axis == 1 ? QColor(0x2f, 0xa8, 0x5a) : QColor(0x2f, 0x6d, 0xf6));
  }

  int axisOf(Vec3 n) {
    return std::fabs(n.x) > 0.5f ? 0 : (std::fabs(n.y) > 0.5f ? 1 : 2);
  }

  /* the quadrilateral of (u, v) in [u0, u1] x [v0, v1] on face f */
  QPolygonF facePoly(Quat q, bool persp, const ViewCube::Face & f, float u0, float u1, float v0, float v1) {
    auto at = [&](float u, float v) { return pt(q, persp, f.n + f.b * u + f.t * v); };
    return QPolygonF({ at(u0, v1), at(u1, v1), at(u1, v0), at(u0, v0) });
  }

  void drawArrowHead(QPainter * p, QPointF tip, QPointF dir, float len, float width) {
    QPointF back = tip - dir * len;
    QPointF side(-dir.y() * width / 2, dir.x() * width / 2);
    QPolygonF tri({ tip, back + side, back - side });
    p->drawPolygon(tri);
  }
}

ViewCubeItem::ViewCubeItem(QQuickItem * parent) : QQuickPaintedItem(parent) {
  setAntialiasing(true);
  setAcceptHoverEvents(true);
  setAcceptedMouseButtons(Qt::LeftButton);
  setImplicitWidth(ViewCube::kItemWidth);
  setImplicitHeight(ViewCube::kItemHeight);
}

void ViewCubeItem::setController(SceneController * c) {
  if (c == m_controller)
    return;
  disconnect(m_frameConnection);
  m_controller = c;
  if (c)
    m_frameConnection = connect(c, &SceneController::frameChanged, this, [this] { update(); });
  emit controllerChanged();
  update();
}

void ViewCubeItem::paintCube(QPainter * p, Quat q, bool persp, const ViewCube::Hit & hover, bool dark) {
  p->setRenderHint(QPainter::Antialiasing, true);
  p->setRenderHint(QPainter::TextAntialiasing, true);

  const QColor base = dark ? kBaseDark : kBaseLight;
  const QColor edge = dark ? kEdgeDark : kEdgeLight;
  const QColor muted = Theme::instance() ? Theme::instance()->muted() : QColor(0x66, 0x70, 0x85);
  const QColor softAccent = Theme::instance() ? Theme::instance()->accentSoft() : QColor(47, 109, 246, 30);

  // --- faces, far to near ---
  std::array<int, 6> order{ 0, 1, 2, 3, 4, 5 };
  std::sort(order.begin(), order.end(), [&](int a, int b) {
    return btui::rotate(q, ViewCube::face(a).n).z < btui::rotate(q, ViewCube::face(b).n).z;
  });

  const auto patches = hover.kind == ViewCube::Kind::Region ? ViewCube::regionPatches(hover.dir)
                                                             : std::vector<ViewCube::Patch>();

  for (int fi : order) {
    if (!ViewCube::faceVisible(q, persp, fi))
      continue;
    const ViewCube::Face & f = ViewCube::face(fi);

    // per-face shading so the faces always differ (C13, AC-C13-10)
    const float shade = 0.72f + 0.42f * std::max(0.0f, btui::dot(btui::rotate(q, f.n), kLight));
    p->setPen(QPen(edge, 1.2));
    p->setBrush(shaded(base, shade));
    p->drawPolygon(facePoly(q, persp, f, -1, 1, -1, 1));

    // the hovered region's patch on this face
    bool centreLit = false;
    for (const auto & pa : patches)
      if (pa.face == fi) {
        p->setPen(Qt::NoPen);
        p->setBrush(kHighlight);
        p->drawPolygon(facePoly(q, persp, f, pa.u0, pa.u1, pa.v0, pa.v1));
        if (hover.dir.count() == 1)
          centreLit = true;
      }

    // the label, drawn in the face's plane so it skews with it
    const QRectF box(0, 0, 100, 100);
    QTransform tr;
    const float a = 0.72f;
    if (QTransform::quadToQuad(QPolygonF({ box.topLeft(), box.topRight(), box.bottomRight(), box.bottomLeft() }),
                               facePoly(q, persp, f, -a, a, -a, a), tr)) {
      p->save();
      p->setTransform(tr, true);
      QFont font;
      font.setPixelSize(44);
      font.setBold(true);
      p->setFont(font);
      p->setPen(centreLit ? QColor(Qt::white) : axisColor(axisOf(f.n)));
      p->drawText(box, Qt::AlignCenter, QString::fromLatin1(f.label));
      p->restore();
    }
  }

  // --- Home ---
  {
    const QPointF c(ViewCube::kHomeX, ViewCube::kHomeY);
    if (hover.kind == ViewCube::Kind::Home) {
      p->setPen(Qt::NoPen);
      p->setBrush(softAccent);
      p->drawEllipse(c, 15, 15);
    }
    QPainterPath house;
    house.moveTo(c + QPointF(-9, -1));
    house.lineTo(c + QPointF(0, -9));
    house.lineTo(c + QPointF(9, -1));
    house.lineTo(c + QPointF(6.5, -1));
    house.lineTo(c + QPointF(6.5, 8));
    house.lineTo(c + QPointF(2, 8));
    house.lineTo(c + QPointF(2, 3));
    house.lineTo(c + QPointF(-2, 3));
    house.lineTo(c + QPointF(-2, 8));
    house.lineTo(c + QPointF(-6.5, 8));
    house.lineTo(c + QPointF(-6.5, -1));
    house.closeSubpath();
    p->setPen(Qt::NoPen);
    p->setBrush(hover.kind == ViewCube::Kind::Home ? kHighlight : (dark ? QColor(0xe7, 0xea, 0xf0) : QColor(0x1a, 0x1f, 0x29)));
    p->drawPath(house);
  }

  // --- 90-degree arrows and roll arrows, only face-aligned ---
  if (ViewCube::faceAligned(q)) {
    const QPointF c(ViewCube::kCentreX, ViewCube::kCentreY);
    const float o = ViewCube::kArrowOffset;
    const struct { ViewCube::Arrow a {}; QPointF dir; } arrows[] = {
      { ViewCube::Arrow::Up, { 0, -1 } }, { ViewCube::Arrow::Down, { 0, 1 } },
      { ViewCube::Arrow::Left, { -1, 0 } }, { ViewCube::Arrow::Right, { 1, 0 } },
    };
    for (const auto & ar : arrows) {
      const bool hot = hover.kind == ViewCube::Kind::Arrow && hover.arrow == ar.a;
      p->setPen(Qt::NoPen);
      p->setBrush(hot ? kHighlight : muted);
      // a 12 dp triangle centred on its anchor, pointing outward
      drawArrowHead(p, c + ar.dir * (o + 6), ar.dir, 12, 12);
    }

    const float r = ViewCube::kRollRadius;
    auto roll = [&](int sign, QPointF centre) {
      const bool hot = hover.kind == ViewCube::Kind::Roll && hover.roll == sign;
      const QColor col = hot ? kHighlight : muted;
      const QRectF rect(centre.x() - r, centre.y() - r, 2 * r, 2 * r);
      QPainterPath arc;
      if (sign > 0) {
        // counter-clockwise arch over the top, right to left; head at the
        // left end pointing down
        arc.arcMoveTo(rect, 0);
        arc.arcTo(rect, 0, 180);
      } else {
        // clockwise ")" down the right side, top to bottom; head at the
        // bottom end pointing left
        arc.arcMoveTo(rect, 90);
        arc.arcTo(rect, 90, -180);
      }
      p->setPen(QPen(col, 2.2, Qt::SolidLine, Qt::RoundCap));
      p->setBrush(Qt::NoBrush);
      p->drawPath(arc);
      p->setPen(Qt::NoPen);
      p->setBrush(col);
      if (sign > 0)
        drawArrowHead(p, QPointF(centre.x() - r, centre.y() + 8.5), QPointF(0, 1), 8.5, 12);
      else
        drawArrowHead(p, QPointF(centre.x() - 8.5, centre.y() + r), QPointF(-1, 0), 8.5, 12);
    };
    roll(+1, QPointF(ViewCube::kRollCcwX, ViewCube::kRollCcwY));
    roll(-1, QPointF(ViewCube::kRollCwX, ViewCube::kRollCwY));
  }

  // --- axis indicator, following the camera including roll ---
  {
    const QPointF o(ViewCube::kAxisX, ViewCube::kAxisY);
    std::array<int, 3> ax{ 0, 1, 2 };
    std::array<Vec3, 3> v;
    for (int i = 0; i < 3; i++)
      v[i] = btui::rotate(q, Vec3{ i == 0 ? 1.0f : 0.0f, i == 1 ? 1.0f : 0.0f, i == 2 ? 1.0f : 0.0f });
    std::sort(ax.begin(), ax.end(), [&](int a, int b) { return v[a].z < v[b].z; });
    QFont font;
    font.setPixelSize(10);
    font.setBold(true);
    p->setFont(font);
    for (int i : ax) {
      const QPointF d(v[i].x, -v[i].y);
      p->setPen(QPen(axisColor(i), 2, Qt::SolidLine, Qt::RoundCap));
      p->drawLine(o, o + d * ViewCube::kAxisLength);
      const QPointF l = o + d * 33;
      p->drawText(QRectF(l.x() - 6, l.y() - 6, 12, 12), Qt::AlignCenter, QString(QChar(u'X' + i)));
    }
  }
}

void ViewCubeItem::paint(QPainter * p) {
  if (!m_controller)
    return;
  const bool persp = m_controller->camera().projection() == btui::Camera::Projection::Perspective;
  const bool dark = Theme::instance() && Theme::instance()->dark();
  // the item may be larger than the widget design size; keep the design's dp,
  // with the axis indicator's room at the left
  p->scale(width() / ViewCube::kItemWidth, height() / ViewCube::kItemHeight);
  p->translate(ViewCube::kPadLeft, 0);
  paintCube(p, m_controller->camera().orientation(), persp, m_controller->cubeHover(), dark);
}

QPointF ViewCubeItem::toCube(QPointF pos) const {
  return { pos.x() * ViewCube::kItemWidth / width() - ViewCube::kPadLeft,
           pos.y() * ViewCube::kItemHeight / height() };
}

ViewCube::Hit ViewCubeItem::hitAt(QPointF pos) const {
  if (!m_controller)
    return {};
  const bool persp = m_controller->camera().projection() == btui::Camera::Projection::Perspective;
  const QPointF c = toCube(pos);
  return ViewCube::hitTest(m_controller->camera().orientation(), persp, float(c.x()), float(c.y()));
}

void ViewCubeItem::updateCursor(const ViewCube::Hit & h) {
  if (h.kind == ViewCube::Kind::None)
    unsetCursor();
  else
    setCursor(Qt::PointingHandCursor);
}

void ViewCubeItem::hoverMoveEvent(QHoverEvent * e) {
  const ViewCube::Hit h = hitAt(e->position());
  updateCursor(h);
  if (m_controller)
    m_controller->setCubeHover(h);
}

void ViewCubeItem::hoverLeaveEvent(QHoverEvent *) {
  unsetCursor();
  if (m_controller)
    m_controller->setCubeHover({});
}

void ViewCubeItem::mousePressEvent(QMouseEvent * e) {
  m_pressHit = hitAt(e->position());
  if (m_pressHit.kind == ViewCube::Kind::None) {
    // nothing of the cube here: the press belongs to the 3D view below
    e->ignore();
    return;
  }
  m_press = e->position();
  m_dragging = false;
  e->accept();
}

void ViewCubeItem::mouseMoveEvent(QMouseEvent * e) {
  if (!m_controller)
    return;
  const QPointF d = e->position() - m_press;
  if (!m_dragging && m_pressHit.kind == ViewCube::Kind::Region &&
      std::hypot(d.x(), d.y()) > SceneController::kClickSlopDp) {
    // dragging the cube orbits the view (legacy), from the press point
    m_dragging = true;
    const QPointF c = toCube(m_press);
    m_controller->cubeDragBegin(float(c.x()), float(c.y()));
  }
  if (m_dragging) {
    const QPointF c = toCube(e->position());
    m_controller->cubeDragMove(float(c.x()), float(c.y()));
  }
}

void ViewCubeItem::mouseReleaseEvent(QMouseEvent * e) {
  if (!m_controller)
    return;
  if (m_dragging) {
    m_dragging = false;
    m_controller->cubeDragEnd();
    return;
  }
  if (m_afterDoubleClick) {
    m_afterDoubleClick = false;
    return;
  }
  const ViewCube::Hit h = hitAt(e->position());
  if (h == m_pressHit)
    m_controller->cubeClick(h);
}

void ViewCubeItem::mouseDoubleClickEvent(QMouseEvent * e) {
  const ViewCube::Hit h = hitAt(e->position());
  if (h.kind == ViewCube::Kind::None) {
    e->ignore();
    return;
  }
  m_afterDoubleClick = true;
  if (m_controller)
    m_controller->cubeDoubleClick(h);
}
