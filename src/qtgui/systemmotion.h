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
#ifndef BTQT_SYSTEMMOTION_H
#define BTQT_SYSTEMMOTION_H

#include <functional>

/* The operating system's "reduce motion" switch, for Qt older than 6.12
 * (from 6.12 QAccessibilityHints::motionPreference reads it, theme.cpp):
 *
 *  - macOS: Accessibility ▸ Display ▸ Reduce motion
 *    (`NSWorkspace.accessibilityDisplayShouldReduceMotion`), watched;
 *  - Windows: Settings ▸ Accessibility ▸ Visual effects ▸ Animation effects
 *    (`SPI_GETCLIENTAREAANIMATION`), read anew on every ask;
 *  - Linux and the BSDs: not read (Qt 6.12 does).
 * GUI thread only.
 */
namespace systemmotion {

  /* 1: reduced, 0: full motion, -1: this platform does not say */
  int reduced(void);

  /* `changed` runs, on the GUI thread, when the switch changes (macOS);
   * an empty function stops it. Elsewhere it never runs. */
  void watch(std::function<void()> changed);
}

#endif
