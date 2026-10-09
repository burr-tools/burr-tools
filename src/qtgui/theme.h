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
#ifndef BTQT_THEME_H
#define BTQT_THEME_H

#include <QColor>
#include <QHash>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

/* Every design token the QML uses (foundations/design-tokens.json and
 * foundations/density.md), for the active theme and density.
 *
 * The tokens are read from the JSON file compiled into the resources, so the
 * file stays their single source. QML never writes a literal colour or size;
 * it binds to these properties, which all notify through one signal, so
 * switching theme or density re-styles every control at once.
 *
 * mode is "light", "dark" or "system"; with "system" the theme follows the
 * operating system's colour scheme live and falls back to light when the
 * platform does not report one (spec C12, AC-C12-08).
 */
class Theme : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY changed FINAL)
  Q_PROPERTY(QString density READ density WRITE setDensity NOTIFY changed FINAL)
  Q_PROPERTY(bool dark READ dark NOTIFY changed FINAL)
  Q_PROPERTY(bool minimal READ minimal NOTIFY changed FINAL)
  Q_PROPERTY(QString glyphFolder READ glyphFolder NOTIFY changed FINAL)

  // colours (design-tokens.md section 1)
  Q_PROPERTY(QColor outer READ outer NOTIFY changed FINAL)
  Q_PROPERTY(QColor bg READ bg NOTIFY changed FINAL)
  Q_PROPERTY(QColor panel READ panel NOTIFY changed FINAL)
  Q_PROPERTY(QColor panel2 READ panel2 NOTIFY changed FINAL)
  Q_PROPERTY(QColor line READ line NOTIFY changed FINAL)
  Q_PROPERTY(QColor line2 READ line2 NOTIFY changed FINAL)
  Q_PROPERTY(QColor text READ text NOTIFY changed FINAL)
  Q_PROPERTY(QColor muted READ muted NOTIFY changed FINAL)
  Q_PROPERTY(QColor accent READ accent NOTIFY changed FINAL)
  Q_PROPERTY(QColor accentSoft READ accentSoft NOTIFY changed FINAL)
  Q_PROPERTY(QColor canvas READ canvas NOTIFY changed FINAL)
  Q_PROPERTY(QColor axisX READ axisX NOTIFY changed FINAL)
  Q_PROPERTY(QColor axisY READ axisY NOTIFY changed FINAL)
  Q_PROPERTY(QColor axisZ READ axisZ NOTIFY changed FINAL)
  Q_PROPERTY(QColor danger READ danger NOTIFY changed FINAL)
  Q_PROPERTY(QColor scrim READ scrim NOTIFY changed FINAL)

  // type scale (design-tokens.md section 2), in dp
  Q_PROPERTY(double fontDialogTitle READ fontDialogTitle CONSTANT FINAL)
  Q_PROPERTY(double fontBody READ fontBody CONSTANT FINAL)
  Q_PROPERTY(double fontActionTitle READ fontActionTitle CONSTANT FINAL)
  Q_PROPERTY(double fontSecondary READ fontSecondary CONSTANT FINAL)
  Q_PROPERTY(double fontMicro READ fontMicro CONSTANT FINAL)
  Q_PROPERTY(double fontCaption READ fontCaption CONSTANT FINAL)
  Q_PROPERTY(double fontKeyBadge READ fontKeyBadge CONSTANT FINAL)
  Q_PROPERTY(QString monoFamily READ monoFamily CONSTANT FINAL)

  // radii (section 3)
  Q_PROPERTY(double radiusChip READ radiusChip CONSTANT FINAL)
  Q_PROPERTY(double radiusControl READ radiusControl CONSTANT FINAL)
  Q_PROPERTY(double radiusPopup READ radiusPopup CONSTANT FINAL)
  Q_PROPERTY(double radiusDialog READ radiusDialog CONSTANT FINAL)

  // geometry of the active density (density.md section 1)
  Q_PROPERTY(double topBar READ topBar NOTIFY changed FINAL)
  Q_PROPERTY(double statusBar READ statusBar NOTIFY changed FINAL)
  Q_PROPERTY(double workspaceRail READ workspaceRail NOTIFY changed FINAL)
  Q_PROPERTY(double collapsedRail READ collapsedRail NOTIFY changed FINAL)
  Q_PROPERTY(double bodyPadding READ bodyPadding NOTIFY changed FINAL)
  Q_PROPERTY(double cardGap READ cardGap NOTIFY changed FINAL)
  Q_PROPERTY(double cardRadius READ cardRadius NOTIFY changed FINAL)
  Q_PROPERTY(double cardHeader READ cardHeader NOTIFY changed FINAL)
  Q_PROPERTY(double listRow READ listRow NOTIFY changed FINAL)
  Q_PROPERTY(double button READ button NOTIFY changed FINAL)
  Q_PROPERTY(double toolbarButtonWidth READ toolbarButtonWidth NOTIFY changed FINAL)
  Q_PROPERTY(double toolbarButtonHeight READ toolbarButtonHeight NOTIFY changed FINAL)

  // motion (section 6), in ms
  Q_PROPERTY(int durCollapse READ durCollapse CONSTANT FINAL)
  Q_PROPERTY(int durChevron READ durChevron CONSTANT FINAL)
  Q_PROPERTY(int durFlash READ durFlash CONSTANT FINAL)
  Q_PROPERTY(int tooltipDelay READ tooltipDelay CONSTANT FINAL)

public:

  /* not default-constructible, so QML uses create() and shares this
   * instance rather than making its own (see App) */
  explicit Theme(QObject * parent);

  /* The one instance the application uses; QML's singleton factory hands
   * this out so QML and C++ see the same object.
   */
  static Theme * instance(void);
  static Theme * create(QQmlEngine *, QJSEngine *);

  const QString & mode(void) const { return m_mode; }
  void setMode(const QString & m);
  QString density(void) const { return m_minimal ? QStringLiteral("minimal") : QStringLiteral("standard"); }
  void setDensity(const QString & d);
  bool dark(void) const { return m_dark; }

  /* What the theme asks the platform for, so its own drawing (title bar,
   * native dialogs) matches: Light or Dark when forced, Unknown -- the
   * operating system decides -- for System. */
  Qt::ColorScheme requestedColorScheme(void) const;

  /* The operating system's light/dark choice, as the platform reports it;
   * System mode follows it. Connected to QStyleHints; tests call it to stand
   * in for an operating system that changes its mind. */
  void setSystemColorScheme(Qt::ColorScheme s);
  Qt::ColorScheme systemColorScheme(void) const { return m_systemScheme; }
  bool minimal(void) const { return m_minimal; }
  QString glyphFolder(void) const { return m_dark ? QStringLiteral("dark") : QStringLiteral("light"); }

  QColor outer(void) const { return col("outer"); }
  QColor bg(void) const { return col("bg"); }
  QColor panel(void) const { return col("panel"); }
  QColor panel2(void) const { return col("panel2"); }
  QColor line(void) const { return col("line"); }
  QColor line2(void) const { return col("line2"); }
  QColor text(void) const { return col("text"); }
  QColor muted(void) const { return col("muted"); }
  QColor accent(void) const { return col("accent"); }
  QColor accentSoft(void) const { return col("accentSoft"); }
  QColor canvas(void) const { return col("canvas"); }
  QColor axisX(void) const { return col("axisX"); }
  QColor axisY(void) const { return col("axisY"); }
  QColor axisZ(void) const { return col("axisZ"); }
  QColor danger(void) const { return col("danger"); }
  QColor scrim(void) const { return col("scrim"); }

  qreal fontDialogTitle(void) const { return num("font.dialogTitle"); }
  qreal fontBody(void) const { return num("font.body"); }
  qreal fontActionTitle(void) const { return num("font.actionTitle"); }
  qreal fontSecondary(void) const { return num("font.secondary"); }
  qreal fontMicro(void) const { return num("font.micro"); }
  qreal fontCaption(void) const { return num("font.glyphCaption"); }
  qreal fontKeyBadge(void) const { return num("font.keyBadge"); }
  /* The monospace family of design-tokens' font.mono stack (key badges). */
  QString monoFamily(void) const;
  /* The body font stack of design-tokens §2 as Qt family names, in order:
   * the system UI font for CSS's -apple-system on macOS only, and no
   * generic "sans-serif" -- Qt falls back by itself. */
  QStringList fontFamilies(void) const;

  qreal radiusChip(void) const { return num("radius.chip"); }
  qreal radiusControl(void) const { return num("radius.control"); }
  qreal radiusPopup(void) const { return num("radius.popup"); }
  qreal radiusDialog(void) const { return num("radius.dialog"); }

  qreal topBar(void) const { return dens("topBar"); }
  qreal statusBar(void) const { return dens("statusBar"); }
  qreal workspaceRail(void) const { return dens("workspaceRail"); }
  qreal collapsedRail(void) const { return dens("collapsedRail"); }
  qreal bodyPadding(void) const { return dens("bodyPadding"); }
  qreal cardGap(void) const { return dens("cardGap"); }
  qreal cardRadius(void) const { return dens("cardRadius"); }
  qreal cardHeader(void) const { return dens("cardHeader"); }
  qreal listRow(void) const { return dens("listRow"); }
  qreal button(void) const { return dens("button"); }
  qreal toolbarButtonWidth(void) const { return dens("toolbarButton.0"); }
  qreal toolbarButtonHeight(void) const { return dens("toolbarButton.1"); }

  int durCollapse(void) const { return motion("columnMs"); }
  int durChevron(void) const { return motion("chevronMs"); }
  int durFlash(void) const { return int(num("motion.flashMs")); }
  int tooltipDelay(void) const { return int(num("motion.tooltipDelayMs")); }

  /* A token by name for C++ consumers (the 3D renderer). */
  QColor color(const QString & name) const { return col(name); }

  /* Parse "#rrggbb", "#rgb" or "rgba(r, g, b, a)" (a in 0..1), the forms the
   * token file uses. Invalid input gives an invalid QColor.
   */
  static QColor parseColor(const QString & s);

signals:

  void changed(void);

private:

  void load(void);
  void updateDark(void);

  QColor col(const QString & name) const;
  qreal num(const QString & path) const { return m_numbers.value(path, 0.0); }
  qreal dens(const QString & key) const;
  int motion(const QString & key) const;

  QString m_mode = QStringLiteral("light");      // the settings' default
  bool m_dark = false;
  Qt::ColorScheme m_systemScheme = Qt::ColorScheme::Unknown;
  bool m_minimal = false;

  QHash<QString, QColor> m_light, m_darkColors;
  QHash<QString, qreal> m_numbers;   ///< flattened numeric tokens, "density.standard.topBar" etc.
  QString m_fontStack;               ///< font.family, as the token file writes it (CSS)
  QString m_monoStack;               ///< font.mono, likewise
};

#endif
