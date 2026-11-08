/* Reference-image checks for the Qt Quick tests (the spec's "visual
 * snapshots of the gallery"), as the QML singleton Snapshots in the module
 * BurrTools.Test.
 *
 * An item is grabbed as drawn, in device pixels, and compared with
 * test/qtgui/snapshots/<os>/<name>@<ratio>x.png by a tolerant diff: a pixel
 * differs when a channel is off by more than 24, and a grab matches while
 * at most 0.5 % of its pixels differ, so anti-aliasing noise passes and a
 * changed colour, border or label does not. References are per platform --
 * fonts and their rasterisers differ -- and per device-pixel ratio.
 *
 * On a mismatch the grab, the reference and a diff picture (differing pixels
 * in red) go to BURRTOOLS_SNAPSHOT_OUT, for a look and for CI to upload.
 * A missing reference is written there too and the check reports "missing",
 * which the test turns into a skip -- unless BURRTOOLS_REQUIRE_SNAPSHOTS is
 * set (CI where references exist), where it fails.
 * BURRTOOLS_UPDATE_SNAPSHOTS=1 writes the grabs as the new references --
 * only those missing or no longer matching, so unchanged images are left
 * alone (`just update-snapshots`). BURRTOOLS_SNAPSHOTS_ONLY=<name>[,<name>]
 * limits the checks to those gallery rows (`just update-snapshots button`).
 */
#ifndef BTTEST_SNAPSHOTS_H
#define BTTEST_SNAPSHOTS_H

#include <QImage>
#include <QObject>
#include <QSize>
#include <QVariantList>

class QQuickItem;

class Snapshots : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool required READ required CONSTANT)
  Q_PROPERTY(qreal expectedRatio READ expectedRatio CONSTANT)

public:

  bool required(void) const;
  /* the ratio the run asked for (QT_SCALE_FACTOR), 1 without one */
  qreal expectedRatio(void) const;

  /* The item and whatever is drawn over it (popups), in device pixels. */
  static QImage grab(QQuickItem * item);

  /* whether the gallery row name is to be checked: every row, unless
   * BURRTOOLS_SNAPSHOTS_ONLY names some */
  Q_INVOKABLE bool wanted(const QString & name) const;

  /* "" when the item matches its reference (or the reference was just
   * written), "missing: ..." when there is none, else what differs. */
  Q_INVOKABLE QString check(QQuickItem * item, const QString & name) const;

  /* The colours at points given in dp in each item's own coordinates
   * ([[item, x, y], ...]), all read from one grab of the first item's
   * window -- a grab draws the whole window, so tests batch their reads. */
  Q_INVOKABLE QVariantList colors(const QVariantList & points) const;

  /* the grab's size in device pixels, and the window's device-pixel ratio */
  Q_INVOKABLE QSize grabSize(QQuickItem * item) const;
  Q_INVOKABLE qreal ratio(QQuickItem * item) const;
};

#endif
