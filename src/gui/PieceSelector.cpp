/* Burr Solver
 * Copyright (C) 2003-2005  Andreas Röver
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


#include "PieceSelector.h"
#include "pieceColor.h"

#include <FL/fl_draw.h>

#include <stdio.h>

void PieceSelector::draw() {

  char txt[10];

  int zpos = 0;
  int maxz = 0;
  int xpos = 0;

  fl_push_clip(x(), y(), w(), h());

  fl_color(color());
  fl_rectf(x(), y(), w(), h());

  if (puzzle) {

    for (int i = 0; i < puzzle->getShapeNumber(); i++) {
      snprintf(txt, 9, "%i: %ix", i, puzzle->getShapeCount(i));
      int wi, hi;
      wi = 0;
      fl_measure(txt, wi, hi);
      wi+=4;
      hi+=4;
  
      if ((xpos > 0) && (xpos + wi > w())) {
        zpos += maxz;
        maxz = 0;
        xpos = 0;
      }
  
      if (hi > maxz) maxz = hi;
  
      fl_rectf(x()+xpos, y()+zpos-shift, wi, maxz,
               (int)(255*pieceColorR(i)),
               (int)(255*pieceColorG(i)),
               (int)(255*pieceColorB(i)));
  
      if (pieceColorR(i) + pieceColorG(i) + pieceColorB(i) > 1.5 )
        fl_color(0, 0, 0);
      else
        fl_color(255, 255, 255);
  
      fl_draw(txt, x()+xpos+2, y()+zpos-shift+maxz-2);
  
      if (i == currentSelect) {
        fl_rect(x()+xpos, y()+zpos-shift, wi, maxz);
      }
  
      xpos += wi;
    }
  }

  int scroll = zpos + maxz;
  if (scroll > h())
    scroll -= h();
  else
    scroll = 0;

  if (lastHight != scroll) {
    lastHight = scroll;
    callbackReason = RS_CHANGEDHIGHT;
    do_callback();
  }

  fl_pop_clip();
}

void PieceSelector::setShift(int z) {
  shift = z;
  redraw();
}

void PieceSelector::setPuzzle(puzzle_c * pz) {
  puzzle = pz;
  redraw();
}

int PieceSelector::handle(int event) {

  static int state = 0;
  static int mx, my, pcc;

  if (!puzzle) return 0;

  char txt[10];

  int zpos = 0;
  int maxz = 0;
  int xpos = 0;

  switch (event) {

  case FL_PUSH:
    { for (int i = 0; i < puzzle->getShapeNumber(); i++) {
        snprintf(txt, 9, "%i: %ix", i, puzzle->getShapeCount(i));
    
        int wi, hi;
        wi = 0;
        fl_measure(txt, wi, hi);
        wi+=4;
        hi+=4;
    
        if ((xpos > 0) && (xpos + wi > w())) {
          zpos += maxz;
          maxz = 0;
          xpos = 0;
        }
    
        if (hi > maxz) maxz = hi;
    
        if ((Fl::event_x() >= x() + xpos) && (Fl::event_x() <= x() + xpos + wi) &&
            (Fl::event_y() >= y() + zpos - shift) && (Fl::event_y() <= y() + zpos - shift + maxz)) {
          if (currentSelect != i) {
            currentSelect = i;
            redraw();
            callbackReason = RS_CHANGEDSELECTION;
            do_callback();
          }
          state = 1;
          mx = Fl::event_x();
          my = Fl::event_y();
          pcc = puzzle->getShapeCount(currentSelect);
        }
    
        xpos += wi;
      }
    }

    break;

  case FL_RELEASE:
    state = 0;
    break;

  case FL_DRAG:
    if (state == 1) {
      int diff = pcc + (Fl::event_x()-mx) / 10 + (my-Fl::event_y()) / 10;
      if (diff < 1) diff = 1;
      if (diff != puzzle->getShapeCount(currentSelect)) {
        puzzle->setShapeCount(currentSelect, diff);
        callbackReason = RS_CHANGEDNUMBER;
        do_callback();
        redraw();
      }
    }

    break;

  }

  return 1;
}

