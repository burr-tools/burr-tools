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
#ifndef BTQT_SELFCHECK_H
#define BTQT_SELFCHECK_H

#include <QString>

/* burrtools-qt --self-check: invariants that need Qt but no window -- every
 * command shortcut parses as a key sequence and is unique on this platform,
 * and the compiled-in resources the GUI depends on are present. Returns an
 * empty string when all hold, else one line per problem.
 */
QString selfCheck(void);

#endif
