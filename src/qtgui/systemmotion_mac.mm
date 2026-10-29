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
#include "systemmotion.h"

#import <AppKit/AppKit.h>

#include <utility>

namespace systemmotion {

  namespace {
    std::function<void()> s_changed;
    id s_observer = nil;
  }

  int reduced(void) {
    return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceMotion ? 1 : 0;
  }

  void watch(std::function<void()> changed) {
    s_changed = std::move(changed);
    NSNotificationCenter * centre = NSWorkspace.sharedWorkspace.notificationCenter;
    if (!s_changed) {
      if (s_observer) {
        [centre removeObserver:s_observer];
        [s_observer release];
      }
      s_observer = nil;
      return;
    }
    if (s_observer)
      return;
    // the main queue: the GUI thread, where the theme lives (no ARC here: kept by hand)
    s_observer = [[centre addObserverForName:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
                                     object:nil
                                      queue:NSOperationQueue.mainQueue
                                 usingBlock:^(NSNotification *) {
                                   if (s_changed)
                                     s_changed();
                                 }] retain];
  }
}
