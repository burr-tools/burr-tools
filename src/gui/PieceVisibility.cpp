#include "PieceVisibility.h"
#include "pieceColor.h"

#include <FL/fl_draw.h>

#include <stdio.h>

#include "../lib/voxel.h"

void PieceVisibility::draw() {

  char txt[10];
  
  if (!vis) return;

  int zpos = 0;
  int maxz = 0;
  int xpos = 0;

  fl_push_clip(x(), y(), w(), h());

  fl_color(color());
  fl_rectf(x(), y(), w(), h());
  fl_font(labelfont(), labelsize());

  int piece = 0;

  for (int i = 0; i < shapenumber; i++) {
    for (int j = 0; j < pieceNumbers[i]; j++) {

      if (pieceNumbers[i] > 1)
        snprintf(txt, 9, "%3i%c", i, 'a'+j);
      else
        snprintf(txt, 9, "%3i", i);

      int wi, hi;
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
               int(255*pieceColorR(i, j)),
               int(255*pieceColorG(i, j)),
               int(255*pieceColorB(i, j)));

      switch(vis[piece]) {
      case 0:
        if (pieceColorR(i) + pieceColorG(i) + pieceColorB(i) > 1.5)
          fl_color(0, 0, 0);
        else
          fl_color(255, 255, 255);
        break;
      case 1:
          fl_rectf(x()+xpos+2, y()+zpos-shift+2, wi-4, maxz-4,
                   int(170*pieceColorR(i, j)),
                   int(170*pieceColorG(i, j)),
                   int(170*pieceColorB(i, j)));
          fl_color(255, 255, 255);
          break;
      case 2:
          fl_rectf(x()+xpos+2, y()+zpos-shift+2, wi-4, maxz-4,
                   int(85*pieceColorR(i, j)),
                   int(85*pieceColorG(i, j)),
                   int(85*pieceColorB(i, j)));
          fl_color(255, 255, 255);
          break;
      }

      fl_draw(txt, x()+xpos+2, y()+zpos-shift+maxz-2);
  
      xpos += wi;

      piece++;
    }
  }

  if (lastHight != zpos + maxz) {
    lastHight = zpos + maxz;
    do_callback();
  }

  fl_pop_clip();
}

void PieceVisibility::setShift(int z) {
  shift = z;
  redraw();
}

void PieceVisibility::setPieceNumber(int numShapes, int *numPieces, char * visible) {

  shapenumber = numShapes;

  if (pieceNumbers)
    delete [] pieceNumbers;

  pieceNumbers = numPieces;
  vis = visible;
  redraw();

}

int PieceVisibility::handle(int event) {

  if (!vis) return 0;
  
  if (event != FL_PUSH)
    return 0;

  char txt[10];

  int zpos = 0;
  int maxz = 0;
  int xpos = 0;

  fl_font(labelfont(), labelsize());

  int piece = 0;

  for (int i = 0; i < shapenumber; i++) {
    for (int j = 0; j < pieceNumbers[i]; j++) {

      if (pieceNumbers[i] > 1)
        snprintf(txt, 9, "%3i%c", i, 'a'+j);
      else
        snprintf(txt, 9, "%3i", i);
  
      int wi, hi;
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
        vis[piece]++;
        if (vis[piece] == 3) vis[piece] = 0;
        redraw();
        do_callback();
        return 1;
      }  
      xpos += wi;
      piece++;
    }
  }

  return 1;
}

int PieceVisibility::calcHeight(void) {

  return lastHight - h();

  char txt[10];

  int zpos = 0;
  int maxz = 0;
  int xpos = 0;

  for (int i = 0; i < shapenumber; i++) {
    for (int j = 0; j < pieceNumbers[i]; j++) {

      if (pieceNumbers[i] > 1)
        snprintf(txt, 9, "%3i%c", i, 'a'+j);
      else
        snprintf(txt, 9, "%3i", i);
  
      int wi, hi;
      fl_measure(txt, wi, hi);
      wi+=4;
      hi+=4;
  
      if ((xpos > 0) && (xpos + wi > w())) {
        zpos += maxz;
        maxz = 0;
        xpos = 0;
      }
  
      if (hi > maxz) maxz = hi;
  
      xpos += wi;
    }
  }
}
