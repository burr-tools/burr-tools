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
#include "systempointer.h"

#import <AppKit/AppKit.h>

#include <algorithm>
#include <cmath>

namespace systempointer {

  /* The system enlarges every cursor, custom ones too, by Accessibility ▸
   * Display ▸ Pointer size: drawn at its own size, ours keeps in step. */
  double sizeFactor(void) {
    return 1.0;
  }

  Arrow arrow(double scale) {
    scale = std::max(1.0, scale);
    @autoreleasepool {
      NSCursor * cursor = [NSCursor arrowCursor];
      NSImage * image = cursor.image;
      if (!image || image.size.width <= 0 || image.size.height <= 0)
        return {};
      const int w = int(std::lround(image.size.width * scale)), h = int(std::lround(image.size.height * scale));
      QImage out(w, h, QImage::Format_ARGB32_Premultiplied);
      out.fill(Qt::transparent);
      CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
      CGContextRef context = CGBitmapContextCreate(out.bits(), size_t(w), size_t(h), 8, size_t(out.bytesPerLine()), space,
                                                   uint32_t(kCGImageAlphaPremultipliedFirst) | uint32_t(kCGBitmapByteOrder32Little));
      CGColorSpaceRelease(space);
      if (!context)
        return {};
      [NSGraphicsContext saveGraphicsState];
      [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithCGContext:context flipped:NO]];
      [image drawInRect:NSMakeRect(0, 0, w, h) fromRect:NSZeroRect operation:NSCompositingOperationCopy fraction:1.0];
      [NSGraphicsContext restoreGraphicsState];
      CGContextRelease(context);
      out.setDevicePixelRatio(scale);
      // the hot spot is in the image's points, from its top left
      const NSPoint hot = cursor.hotSpot;
      return { out, QPoint(int(std::lround(hot.x)), int(std::lround(hot.y))) };
    }
  }
}
