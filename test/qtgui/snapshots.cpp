/* Reference-image checks for the Qt Quick tests (see snapshots.h). */
#include "snapshots.h"

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

  constexpr int kChannelTolerance = 24;
  constexpr double kPixelFraction = 0.005;

  QString platformFolder(void) {
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#else
    return QStringLiteral("linux");
#endif
  }

  QString envDir(const char * var, const QString & fallback) {
    const QString v = qEnvironmentVariable(var);
    return v.isEmpty() ? fallback : v;
  }

  QString referenceDir(void) {
    return envDir("BURRTOOLS_SNAPSHOT_DIR", QStringLiteral("test/qtgui/snapshots")) + QLatin1Char('/') + platformFolder();
  }

  QString outputDir(void) {
    return envDir("BURRTOOLS_SNAPSHOT_OUT", QDir::tempPath() + QStringLiteral("/burrtools-snapshots"));
  }

  /* "1", "1.5", "2" */
  QString ratioText(qreal r) {
    return QString::number(std::round(r * 100) / 100, 'g', 4);
  }

  bool save(const QImage & img, const QString & path) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    return img.save(path);
  }

  QImage windowGrab(QQuickItem * item) {
    if (!item || !item->window())
      return QImage();
    return item->window()->grabWindow().convertToFormat(QImage::Format_ARGB32);
  }
}

bool Snapshots::required(void) const {
  return qEnvironmentVariableIsSet("BURRTOOLS_REQUIRE_SNAPSHOTS");
}

qreal Snapshots::expectedRatio(void) const {
  bool ok = false;
  const double r = qEnvironmentVariable("QT_SCALE_FACTOR").toDouble(&ok);
  return ok && r > 0 ? r : 1.0;
}

QImage Snapshots::grab(QQuickItem * item) {
  const QImage all = windowGrab(item);
  if (all.isNull())
    return all;
  const qreal dpr = item->window()->effectiveDevicePixelRatio();
  const QRectF scene = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
  const QRect px = QRectF(scene.x() * dpr, scene.y() * dpr, scene.width() * dpr, scene.height() * dpr).toAlignedRect();
  return all.copy(px.intersected(all.rect()));
}

bool Snapshots::wanted(const QString & name) const {
  const QString only = qEnvironmentVariable("BURRTOOLS_SNAPSHOTS_ONLY");
  return only.isEmpty() || only.split(QLatin1Char(','), Qt::SkipEmptyParts).contains(name);
}

namespace {

  /* The pixels of actual that differ from ref by more than the tolerance,
   * and a picture of them: the reference faded, differing pixels in red. */
  qint64 countDifferences(const QImage & actual, const QImage & ref, QImage * diff) {
    *diff = QImage(actual.size(), QImage::Format_ARGB32);
    qint64 differing = 0;
    for (int y = 0; y < actual.height(); y++) {
      const QRgb * a = reinterpret_cast<const QRgb *>(actual.constScanLine(y));
      const QRgb * b = reinterpret_cast<const QRgb *>(ref.constScanLine(y));
      QRgb * d = reinterpret_cast<QRgb *>(diff->scanLine(y));
      for (int x = 0; x < actual.width(); x++) {
        const int delta = std::max({ std::abs(qRed(a[x]) - qRed(b[x])), std::abs(qGreen(a[x]) - qGreen(b[x])),
                                     std::abs(qBlue(a[x]) - qBlue(b[x])), std::abs(qAlpha(a[x]) - qAlpha(b[x])) });
        if (delta > kChannelTolerance) {
          differing++;
          d[x] = qRgb(255, 0, 0);
        } else {
          const int g = (qGray(b[x]) + 3 * 255) / 4;
          d[x] = qRgb(g, g, g);
        }
      }
    }
    return differing;
  }

  bool withinTolerance(qint64 differing, const QImage & actual) {
    return differing <= qint64(kPixelFraction * double(qint64(actual.width()) * actual.height()));
  }
}

QString Snapshots::check(QQuickItem * item, const QString & name) const {
  const QImage actual = grab(item);
  if (actual.isNull())
    return QStringLiteral("nothing to grab for %1").arg(name);

  const QString file = name + QLatin1Char('@') + ratioText(ratio(item)) + QStringLiteral("x.png");
  const QString refPath = referenceDir() + QLatin1Char('/') + file;
  const QString outBase = outputDir() + QLatin1Char('/') + platformFolder() + QLatin1Char('/') + name +
                          QLatin1Char('@') + ratioText(ratio(item)) + QLatin1Char('x');

  QImage ref(refPath);
  if (!ref.isNull())
    ref = ref.convertToFormat(QImage::Format_ARGB32);
  QImage diff;
  const bool sameSize = !ref.isNull() && ref.size() == actual.size();
  const qint64 differing = sameSize ? countDifferences(actual, ref, &diff) : 0;

  /* Updating rewrites only the references that no longer match (or are
   * missing), so a commit after `just update-snapshots` holds just the
   * images that really changed, not every file re-encoded. */
  if (qEnvironmentVariable("BURRTOOLS_UPDATE_SNAPSHOTS") == QLatin1String("1")) {
    if (sameSize && withinTolerance(differing, actual))
      return QString();
    return save(actual, refPath) ? QString() : QStringLiteral("could not write %1").arg(refPath);
  }

  if (ref.isNull()) {
    save(actual, outBase + QStringLiteral(".png"));
    const QString what = QStringLiteral("no reference %1; the grab is in %2.png").arg(refPath, outBase);
    return required() ? what : QStringLiteral("missing: ") + what;
  }

  if (!sameSize) {
    save(actual, outBase + QStringLiteral(".png"));
    return QStringLiteral("%1 is %2x%3 px, its reference %4x%5; the grab is in %6.png")
        .arg(name).arg(actual.width()).arg(actual.height()).arg(ref.width()).arg(ref.height()).arg(outBase);
  }

  if (withinTolerance(differing, actual))
    return QString();

  save(actual, outBase + QStringLiteral(".png"));
  save(ref, outBase + QStringLiteral("-reference.png"));
  save(diff, outBase + QStringLiteral("-diff.png"));
  return QStringLiteral("%1: %2 of %3 pixels differ from %4; see %5-diff.png")
      .arg(name).arg(differing).arg(qint64(actual.width()) * actual.height()).arg(refPath, outBase);
}

QVariantList Snapshots::colors(const QVariantList & points) const {
  QVariantList out;
  auto itemOf = [](const QVariant & v) { return qobject_cast<QQuickItem *>(v.value<QObject *>()); };
  QQuickItem * first = points.isEmpty() ? nullptr : itemOf(points.first().toList().value(0));
  const QImage all = windowGrab(first);
  if (all.isNull())
    return out;
  const qreal dpr = first->window()->effectiveDevicePixelRatio();
  for (const QVariant & v : points) {
    const QVariantList p = v.toList();
    QQuickItem * item = p.size() == 3 ? itemOf(p[0]) : nullptr;
    if (!item || item->window() != first->window()) {
      out.append(QColor());
      continue;
    }
    const QPointF s = item->mapToScene(QPointF(p[1].toReal(), p[2].toReal()));
    const QPoint px(int(std::floor(s.x() * dpr)), int(std::floor(s.y() * dpr)));
    out.append(all.rect().contains(px) ? QColor::fromRgba(all.pixel(px)) : QColor());
  }
  return out;
}

QSize Snapshots::grabSize(QQuickItem * item) const {
  return grab(item).size();
}

qreal Snapshots::ratio(QQuickItem * item) const {
  return item && item->window() ? item->window()->effectiveDevicePixelRatio() : 1.0;
}
