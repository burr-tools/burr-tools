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
#include "qmloutlines.h"

#include <QCursor>
#include <QFileInfo>
#include <QPainter>
#include <QQuickWindow>
#include <private/qqmlcontextdata_p.h>
#include <private/qqmldata_p.h>

#include <cstdio>

void QmlOutlines::installIfAsked(QQuickWindow * window) {
  if (window && !qEnvironmentVariableIsEmpty("BURRTOOLS_QML_OUTLINES"))
    new QmlOutlines(window);
}

QmlOutlines::QmlOutlines(QQuickWindow * window) : QQuickPaintedItem(window->contentItem()), m_window(window) {
  // over everything, the popup layer included, and taking no input
  setZ(1e7);
  setAcceptedMouseButtons(Qt::NoButton);
  setAcceptHoverEvents(false);
  setFillColor(Qt::transparent);
  auto fit = [this] {
    if (parentItem())
      setSize(parentItem()->size());
  };
  connect(parentItem(), &QQuickItem::widthChanged, this, fit);
  connect(parentItem(), &QQuickItem::heightChanged, this, fit);
  fit();
  m_timer.setInterval(150);
  connect(&m_timer, &QTimer::timeout, this, &QmlOutlines::poll);
  m_timer.start();
  fprintf(stderr, "BURRTOOLS_QML_OUTLINES: named items outlined; the one under the pointer and its parents print here\n");
}

QString QmlOutlines::declaredAt(const QObject * object) {
  const QQmlData * d = object ? QQmlData::get(object) : nullptr;
  if (!d || !d->outerContext)
    return {};
  const QString file = QFileInfo(d->outerContext->url().path()).fileName();
  return file.isEmpty() ? QString() : QStringLiteral("%1:%2").arg(file).arg(d->lineNumber);
}

QList<QQuickItem *> QmlOutlines::namedItems(QQuickItem * root) {
  QList<QQuickItem *> out;
  if (!root || !root->isVisible())
    return out;
  if (!root->objectName().isEmpty() && !declaredAt(root).isEmpty())
    out << root;
  for (QQuickItem * c : root->childItems())
    if (!dynamic_cast<QmlOutlines *>(c))
      out += namedItems(c);
  return out;
}

/* the item under the pointer: the last named one (drawn on top) that holds it */
void QmlOutlines::poll(void) {
  if (!m_window || !parentItem())
    return;
  const QPointF at = mapFromGlobal(QCursor::pos());
  QQuickItem * hot = nullptr;
  if (contains(at))
    for (QQuickItem * i : namedItems(parentItem()))
      if (i->contains(i->mapFromItem(this, at)))
        hot = i;
  if (hot != m_hot) {
    m_hot = hot;
    if (hot) {
      fprintf(stderr, "\n");
      for (QQuickItem * p = hot; p; p = p->parentItem())
        if (!p->objectName().isEmpty() && !declaredAt(p).isEmpty())
          fprintf(stderr, "%s%s  %s\n", p == hot ? "> " : "  ", qPrintable(p->objectName()), qPrintable(declaredAt(p)));
    }
  }
  update();
}

/* a thin outline round every named item; the one under the pointer
 * highlighted, with its label -- only that one, as labels on all of them
 * would cover each other and the UI */
void QmlOutlines::paint(QPainter * p) {
  if (!parentItem())
    return;
  const auto rectOf = [this](QQuickItem * i) {
    return i->mapRectToItem(this, QRectF(0, 0, i->width(), i->height())).intersected(boundingRect());
  };
  p->setPen(QPen(QColor(47, 109, 246, 150), 1));
  p->setBrush(Qt::NoBrush);
  for (QQuickItem * i : namedItems(parentItem())) {
    const QRectF r = rectOf(i);
    if (r.width() >= 2 && r.height() >= 2 && i != m_hot)
      p->drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
  }
  if (!m_hot || !m_hot->isVisible())
    return;
  const QRectF r = rectOf(m_hot);
  p->setPen(QPen(QColor(229, 72, 77), 2));
  p->setBrush(QColor(229, 72, 77, 28));
  p->drawRect(r.adjusted(1, 1, -1, -1));

  // above the item where there is room, else inside its top edge; kept on screen
  QFont f = p->font();
  f.setPixelSize(11);
  p->setFont(f);
  const QFontMetrics fm(f);
  const QString label = m_hot->objectName() + QStringLiteral("  ") + declaredAt(m_hot);
  const QSizeF size(fm.horizontalAdvance(label) + 10, fm.height() + 4);
  const qreal x = qBound<qreal>(0, r.left(), qMax<qreal>(0, width() - size.width()));
  const qreal y = r.top() >= size.height() ? r.top() - size.height() : r.top();
  const QRectF box(QPointF(x, y), size);
  p->setPen(Qt::NoPen);
  p->setBrush(QColor(229, 72, 77));
  p->drawRect(box);
  p->setPen(Qt::white);
  p->drawText(box.adjusted(5, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, label);
}
