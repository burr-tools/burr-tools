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
#include "theme.h"

#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QStyleHints>

#include <functional>

namespace {
  Theme * s_instance = nullptr;
}

Theme::Theme(QObject * parent) : QObject(parent) {
  if (!s_instance)
    s_instance = this;

  load();

  if (auto * hints = QGuiApplication::styleHints()) {
    m_systemScheme = hints->colorScheme();
    connect(hints, &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme s) {
      // while a forced theme is requested the platform reports that request,
      // not the operating system's choice
      if (m_mode == QLatin1String("system"))
        setSystemColorScheme(s);
    });
  }

  updateDark();
}

Qt::ColorScheme Theme::requestedColorScheme(void) const {
  return m_mode == QLatin1String("dark") ? Qt::ColorScheme::Dark
       : m_mode == QLatin1String("light") ? Qt::ColorScheme::Light : Qt::ColorScheme::Unknown;
}

void Theme::setSystemColorScheme(Qt::ColorScheme s) {
  if (s == m_systemScheme)
    return;
  m_systemScheme = s;
  updateDark();
}

Theme * Theme::instance(void) {
  return s_instance;
}

Theme * Theme::create(QQmlEngine *, QJSEngine *) {
  // QML must not delete the instance C++ owns
  QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
  return s_instance;
}

QColor Theme::parseColor(const QString & s) {
  const QString t = s.trimmed();
  if (t.startsWith(QLatin1Char('#')))
    return QColor::fromString(t);

  static const QRegularExpression re(
    QStringLiteral("^rgba\\(\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*([0-9.]+)\\s*\\)$"));
  auto m = re.match(t);
  if (!m.hasMatch())
    return QColor();
  QColor c(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt());
  c.setAlphaF(m.captured(4).toFloat());
  return c;
}

/* Flatten every number in the token file into "a.b.c" keys -- array
 * elements get their index ("density.standard.toolbarButton.0") -- and read
 * the two colour tables. The file is compiled in, so a missing or broken one
 * is a build error, not something to recover from at run time.
 */
void Theme::load(void) {
  QFile f(QStringLiteral(":/burrtools/design-tokens.json"));
  if (!f.open(QIODevice::ReadOnly))
    qFatal("design-tokens.json missing from the resources");
  const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();

  std::function<void(const QString &, const QJsonValue &)> walk = [&](const QString & prefix, const QJsonValue & v) {
    if (v.isDouble()) {
      m_numbers.insert(prefix, v.toDouble());
    } else if (v.isObject()) {
      const QJsonObject o = v.toObject();
      for (auto it = o.begin(); it != o.end(); ++it)
        walk(prefix.isEmpty() ? it.key() : prefix + QLatin1Char('.') + it.key(), it.value());
    } else if (v.isArray()) {
      const QJsonArray a = v.toArray();
      for (qsizetype i = 0; i < a.size(); i++)
        walk(prefix + QLatin1Char('.') + QString::number(i), a.at(i));
    }
  };
  walk(QString(), root);
  m_fontStack = root.value(QStringLiteral("font")).toObject().value(QStringLiteral("family")).toString();

  auto readColors = [&](const QJsonObject & o, QHash<QString, QColor> & out) {
    for (auto it = o.begin(); it != o.end(); ++it)
      out.insert(it.key(), parseColor(it.value().toString()));
  };
  const QJsonObject colors = root.value(QStringLiteral("color")).toObject();
  readColors(colors.value(QStringLiteral("light")).toObject(), m_light);
  readColors(colors.value(QStringLiteral("dark")).toObject(), m_darkColors);
}

void Theme::setMode(const QString & m) {
  QString v = m;
  if (v != QLatin1String("light") && v != QLatin1String("dark"))
    v = QStringLiteral("system");
  if (v == m_mode)
    return;
  m_mode = v;
  /* Tell the platform too, so what it draws -- the title bar, native menus
   * and dialogs -- matches a forced light or dark theme. Unknown hands the
   * choice back to the operating system. */
  if (auto * hints = QGuiApplication::styleHints())
    hints->setColorScheme(requestedColorScheme());
  updateDark();
  emit changed();
}

void Theme::setDensity(const QString & d) {
  const bool minimal = (d == QLatin1String("minimal"));
  if (minimal == m_minimal)
    return;
  m_minimal = minimal;
  emit changed();
}

void Theme::updateDark(void) {
  bool d;
  if (m_mode == QLatin1String("dark"))
    d = true;
  else if (m_mode == QLatin1String("light"))
    d = false;
  else {
    // "system": the operating system's choice, Light when it gives none
    d = m_systemScheme == Qt::ColorScheme::Dark;
  }
  if (d == m_dark)
    return;
  m_dark = d;
  emit changed();
}

QColor Theme::col(const QString & name) const {
  return (m_dark ? m_darkColors : m_light).value(name);
}

qreal Theme::dens(const QString & key) const {
  return num(QStringLiteral("density.") + (m_minimal ? QStringLiteral("minimal.") : QStringLiteral("standard.")) + key);
}

int Theme::motion(const QString & key) const {
  return int(num(QStringLiteral("motion.") + key));
}

QString Theme::monoFamily(void) const {
  return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
}

QStringList Theme::fontFamilies(void) const {
  QStringList out;
  for (QString f : m_fontStack.split(QLatin1Char(','))) {
    f = f.trimmed().remove(QLatin1Char('\'')).remove(QLatin1Char('"'));
    if (f == QLatin1String("-apple-system")) {
#ifdef Q_OS_MACOS
      out.append(QFontDatabase::systemFont(QFontDatabase::GeneralFont).family());
#endif
    } else if (!f.isEmpty() && f != QLatin1String("sans-serif")) {
      out.append(f);
    }
  }
  return out;
}
