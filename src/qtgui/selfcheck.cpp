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
#include "selfcheck.h"
#include "commandcontroller.h"

#include "../uicore/commands.h"

#include <QFile>
#include <QKeySequence>
#include <QSet>
#include <QStringList>

QString selfCheck(void) {
  QStringList problems;

  QSet<QKeySequence> seen;
  for (const auto & c : btui::commandTable()) {
    for (auto s : CommandController::platformShortcuts(c)) {
      const QString text = QString::fromUtf8(s.data(), qsizetype(s.size()));
      QKeySequence k(text, QKeySequence::PortableText);
      if (k.count() != 1 || k[0].key() == Qt::Key_unknown)
        problems << QStringLiteral("shortcut '%1' of %2 does not parse").arg(text, QString::fromUtf8(c.key.data(), qsizetype(c.key.size())));
      else if (seen.contains(k))
        problems << QStringLiteral("shortcut '%1' is used twice").arg(text);
      seen.insert(k);
    }
  }

  for (const char * res : { ":/burrtools/design-tokens.json", ":/burrtools/icons/gear.svg",
                            ":/burrtools/glyphs/light/tool-fixed.svg", ":/burrtools/glyphs/dark/tool-fixed.svg" })
    if (!QFile::exists(QString::fromLatin1(res)))
      problems << QStringLiteral("resource %1 is missing").arg(QString::fromLatin1(res));

  return problems.join(QLatin1Char('\n'));
}
