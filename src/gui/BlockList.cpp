/* Burr Solver
 * Copyright (C) 2003-2006  Andreas Röver
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

#include "BlockList.h"

#include "pieceColor.h"

#include <FL/fl_draw.h>
#include <FL/Fl.h>


void BlockList::draw() {

  unsigned int zpos = 0;
  unsigned int maxz = 0;
  unsigned int xpos = 0;

  if ((w() <= 0) || (h() <= 0)) return;

  fl_push_clip(x(), y(), w(), h());

  fl_color(color());
  fl_rectf(x(), y(), w(), h());

  for (unsigned int i = 0; i < blockNumber(); i++) {

    unsigned int wi, hi;

    blockSize(i, &wi, &hi);

    if ((xpos > 0) && (xpos + wi > (unsigned int)w())) {
      zpos += maxz;
      maxz = 0;
      xpos = 0;
    }

    if (hi > maxz) maxz = hi;

    blockDraw(i, x()+xpos, y()+zpos-shift);

    xpos += wi;
  }

  unsigned int scroll = zpos + maxz;

  if (scroll > (unsigned int)h())
    scroll -= h();
  else
    scroll = 0;

  if (lastHight != scroll) {
    lastHight = scroll;
    do_callback(RS_CHANGEDHIGHT);
  }

  fl_pop_clip();
}

int BlockList::handle(int event) {

  static int state = 0;
  static int mx, my;
  static int current_block = -1;

  unsigned int zpos = 0;
  unsigned int maxz = 0;
  unsigned int xpos = 0;

  if ((w() <= 0) || (h() <= 0)) return 0;

  switch (event) {

  case FL_PUSH:
    {
      for (unsigned int i = 0; i < blockNumber(); i++) {

        unsigned int wi, hi;
        blockSize(i, &wi, &hi);

        if ((xpos > 0) && (xpos + wi > (unsigned int)w())) {
          zpos += maxz;
          maxz = 0;
          xpos = 0;
        }

        if (hi > maxz) maxz = hi;

        if ((Fl::event_x() >= x() + (int)xpos) && (Fl::event_x() <= x() + (int)xpos + (int)wi) &&
            (Fl::event_y() >= y() + (int)zpos - (int)shift) && (Fl::event_y() <= y() + (int)zpos - (int)shift + (int)maxz)) {
          current_block = i;
          push(i);

          state = 1;
          mx = Fl::event_x();
          my = Fl::event_y();

          break;
        }

        xpos += wi;
      }
    }
    return 1;

  case FL_RELEASE:
    release(current_block);
    state = 0;
    current_block = -1;
    return 1;

  case FL_DRAG:
    if (state == 1)
      drag(current_block, Fl::event_x()-mx, Fl::event_y()-my);
    return 1;
  }
  return 0;
}

void SelectableTextList::blockDraw(unsigned int block, int x, int y) {
  int w, h;
  unsigned char r, g, b;
  char txt[200];

  getColor(block, &r, &g, &b);
  getText(block, txt);

  w = 0;
  fl_measure(txt, w, h);
  w += 8;
  h += 4;

  fl_rectf(x, y, w, h, r, g, b);

  if ((int)3*r + 6*g + 1*b > 1275)
    fl_color(0, 0, 0);
  else
    fl_color(255, 255, 255);

  fl_font(labelfont(), labelsize());
  fl_draw(txt, x+4, y+h-4);

  if (block == getSelection()) {
    fl_color(0, 0, 0);
    fl_rect(x, y, w, h);
    fl_color(255, 255, 255);
    fl_rect(x+1, y+1, w-2, h-2);
    fl_color(0, 0, 0);
    fl_rect(x+2, y+2, w-4, h-4);
  }
}

void SelectableTextList::blockSize(unsigned int block, unsigned int *w, unsigned int *h) {
  char txt[200];
  getText(block, txt);

  int wi, hi;
  fl_font(labelfont(), labelsize());
  wi = 0;
  fl_measure(txt, wi, hi);
  *w = wi + 8;
  *h = hi + 4;
}


void TextList::blockDraw(unsigned int block, int x, int y) {
  int w, h;
  unsigned char r, g, b;
  char txt[200];

  getColor(block, &r, &g, &b);
  getText(block, txt);

  w = 0;
  fl_measure(txt, w, h);
  w += 8;
  h += 4;

  fl_rectf(x, y, w, h, r, g, b);

  if ((int)3*r + 6*g + 1*b > 1275)
    fl_color(0, 0, 0);
  else
    fl_color(255, 255, 255);

  fl_font(labelfont(), labelsize());
  fl_draw(txt, x+4, y+h-4);
}

void TextList::blockSize(unsigned int block, unsigned int *w, unsigned int *h) {
  char txt[200];
  getText(block, txt);

  int wi, hi;
  fl_font(labelfont(), labelsize());
  wi = 0;
  fl_measure(txt, wi, hi);
  *w = wi + 8;
  *h = hi + 4;
}

void ColorSelector::setPuzzle(puzzle_c *pz) {
  bt_assert(pz);
  puzzle = pz;
  setSelection(0);
}

unsigned int ColorSelector::blockNumber(void) {
  if (includeNeutral)
    return puzzle->colorNumber() + 1;
  else
    return puzzle->colorNumber();
}

/* return the color for the block */
void ColorSelector::getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b) {
  // the block 0 is the neutral color and always available and
  // not saved in the color list
  if (!includeNeutral)
    block++;

  if (block == 0)
    Fl::get_color(color(), *r, *g, *b);
  else
    puzzle->getColor(block - 1, r, g, b);
}

/* return the text for the block (not more than 20 characters */
void ColorSelector::getText(unsigned int block, char * text) {
  if (!includeNeutral)
    block++;

  if (block == 0)
    snprintf(text, 200, "Neutral");
  else
    snprintf(text, 200, "%i", block);
}

void PieceSelector::setPuzzle(puzzle_c *pz) {
  bt_assert(pz);
  puzzle = pz;
  setSelection(0);
}

unsigned int PieceSelector::blockNumber(void) {
  return puzzle->shapeNumber();
}

void PieceSelector::getText(unsigned int block, char * text) {
  snprintf(text, 200, "%i", block+1);
}

void PieceSelector::getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b) {
  *r = (int)(255*pieceColorR(block));
  *g = (int)(255*pieceColorG(block));
  *b = (int)(255*pieceColorB(block));
}

void ProblemSelector::setPuzzle(puzzle_c *pz) {
  bt_assert(pz);
  puzzle = pz;
  setSelection(0);
}

unsigned int ProblemSelector::blockNumber(void) {
  return puzzle->problemNumber();
}

void ProblemSelector::getText(unsigned int block, char * text) {
  snprintf(text, 200, "%s", puzzle->probGetName(block).c_str());
}

void ProblemSelector::getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b) {
  *r = (int)(255*pieceColorR(block));
  *g = (int)(255*pieceColorG(block));
  *b = (int)(255*pieceColorB(block));
}

void PiecesList::setPuzzle(puzzle_c *pz, unsigned int prob) {
  bt_assert(pz);
  puzzle = pz;
  problem = prob;
  redraw();
}

unsigned int PiecesList::blockNumber(void) {
  if (problem >= puzzle->problemNumber())
    return 0;

  return puzzle->probShapeNumber(problem);
}

void PiecesList::getText(unsigned int block, char * text) {

  int txtLen = 200;
  int len;

  len = snprintf(text, txtLen, "%i", puzzle->probGetShape(problem, block)+1);
  text += len;
  txtLen -= len;

  if (puzzle->probGetShapeCount(problem, block) != 1) {
    len = snprintf(text, txtLen, "(%i)", puzzle->probGetShapeCount(problem, block));
    text += len;
    txtLen -= len;
  }

  for (int i = 0; i < puzzle->probGetShapeGroupNumber(problem, block); i++) {
    if (puzzle->probGetShapeGroupCount(problem, block, i) != puzzle->probGetShapeCount(problem, block))
      len = snprintf(text, txtLen, ", G%i(%i)", puzzle->probGetShapeGroup(problem, block, i), puzzle->probGetShapeGroupCount(problem, block, i)+1);
    else
      len = snprintf(text, txtLen, ", G%i", puzzle->probGetShapeGroup(problem, block, i)+1);
    text += len;
    txtLen -= len;
  }
}

void PiecesList::getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b) {
  *r = (int)(255*pieceColorR(puzzle->probGetShape(problem, block)));
  *g = (int)(255*pieceColorG(puzzle->probGetShape(problem, block)));
  *b = (int)(255*pieceColorB(puzzle->probGetShape(problem, block)));
}

PieceVisibility::PieceVisibility(int x, int y, int w, int h, puzzle_c * p) : BlockList(x, y, w, h), puzzle(p), problem(0) {
  bt_assert(p);
  if (p->problemNumber() > 0) {
    visState = new unsigned char[p->probPieceNumber(0)];
    for (unsigned int i = 0; i < p->probPieceNumber(0); i++)
      visState[i] = 0;
  } else
    visState = 0;
}

unsigned int PieceVisibility::blockNumber(void) {
  if (problem < puzzle->problemNumber())
    return puzzle->probPieceNumber(problem);
  else
    return 0;
}

void PieceVisibility::blockDraw(unsigned int block, int x, int y) {
  int w, h;
  unsigned char r, g, b;
  char txt[200];

  int shape = 0;

  unsigned int subBlock = block;

  while (subBlock >= puzzle->probGetShapeCount(problem, shape)) {
    subBlock -= puzzle->probGetShapeCount(problem, shape);
    shape++;
  }
  int shapeID = puzzle->probGetShape(problem, shape);

  if (puzzle->probGetShapeCount(problem, shape) > 1)
    snprintf(txt, 199, "%i.%i", shapeID+1, subBlock+1);
  else
    snprintf(txt, 199, "%i", shapeID+1);

  r = int(255*pieceColorR(shapeID, subBlock));
  g = int(255*pieceColorG(shapeID, subBlock));
  b = int(255*pieceColorB(shapeID, subBlock));

  w = 0;
  fl_measure(txt, w, h);
  w += 8;
  h += 4;

  switch(visState[block]) {
  case 0:
    fl_rectf(x, y, w, h, r, g, b);
    break;
  case 1:
    fl_rectf(x+2, y+2, w-4, h-4, r, g, b);
    break;
  case 2:
    fl_rectf(x, y, w, 2, r, g, b);
    fl_rectf(x, y+h-2, w, 2, r, g, b);
    fl_rectf(x, y, 2, h, r, g, b);
    fl_rectf(x+w-2, y, 2, h, r, g, b);
    break;
  }

  if ((int)3*r + 6*g + 1*b > 1275)
    fl_color(0, 0, 0);
  else
    fl_color(255, 255, 255);

  fl_font(labelfont(), labelsize());
  fl_draw(txt, x+4, y+h-4);
}

void PieceVisibility::blockSize(unsigned int block, unsigned int *w, unsigned int *h) {
  char txt[200];

  int shape = 0;

  while (block >= puzzle->probGetShapeCount(problem, shape)) {
    block -= puzzle->probGetShapeCount(problem, shape);
    shape++;
  }

  int shapeID = puzzle->probGetShape(problem, shape);

  if (puzzle->probGetShapeCount(problem, shape) > 1)
    snprintf(txt, 199, "%i.%i", shapeID+1, block+1);
  else
    snprintf(txt, 199, "%i", shapeID+1);

  int wi, hi;
  fl_font(labelfont(), labelsize());
  wi = 0;
  fl_measure(txt, wi, hi);
  *w = wi + 8;
  *h = hi + 4;
}

void PieceVisibility::setPuzzle(puzzle_c *pz, unsigned int prob) {
  bt_assert(pz);
  puzzle = pz;
  problem = prob;

  if (visState)
    delete [] visState;

  visState = 0;

  if (prob < pz->problemNumber() && (pz->probPieceNumber(prob) > 0)) {
    visState = new unsigned char[pz->probPieceNumber(prob)];

    for (unsigned int i = 0; i < pz->probPieceNumber(prob); i++)
      visState[i] = 0;
  }

  redraw();
}

void PieceVisibility::push(unsigned int block) {

  visState[block]++;

  if (visState[block] == 3) visState[block] = 0;

  redraw();

  do_callback(RS_CHANGEDSELECTION);
}



#define CC_ADD_LENGTH 10
#define CC_BLOCK_GAP 2
#define CC_GROUP_GAP 4
#define CC_ARROW_GAP 6
#define CC_BLOCK_WIDTH 20
#define CC_LR_GAP 5
#define CC_ARROW_LR 5
#define CC_HORIZ_LENGTH 20
#define CC_ARROW_LENGTH 8
#define CC_ARROW_WIDTH 2

void ColorConstraintsEdit::draw(void) {

  if (problem >= puzzle->problemNumber())
    return;

  if ((w() <= 0) || (h() <= 0)) return;

  unsigned int ypos = 0;
  unsigned char r, g, b;

  fl_push_clip(x(), y(), w(), h());

  fl_color(color());
  fl_rectf(x(), y(), w(), h());

  fl_color(fl_darker(color()));
  if (sortByResult) {
    fl_rectf(x(),
             y(),
             CC_LR_GAP+CC_BLOCK_WIDTH+CC_ARROW_LR+CC_HORIZ_LENGTH/2,
             h());
  } else {
    fl_rectf(x()+w()-(CC_LR_GAP+CC_BLOCK_WIDTH+CC_ARROW_LR+CC_HORIZ_LENGTH/2),
             y(),
             CC_LR_GAP+CC_BLOCK_WIDTH+CC_ARROW_LR+CC_HORIZ_LENGTH/2,
             h());
  }
  fl_color(labelcolor());

  for (unsigned int c1 = 0; c1 < puzzle->colorNumber(); c1++) {

    unsigned int cnt = 0;
    unsigned int c2;
    for (c2 = 0; c2 < puzzle->colorNumber(); c2++)
      if ((!sortByResult && puzzle->probPlacementAllowed(problem, c1+1, c2+1)) ||
          (sortByResult && puzzle->probPlacementAllowed(problem, c2+1, c1+1)))
        cnt++;

    unsigned int height;
    unsigned int groupblockheight;

    if (cnt == 0) {
      height = 2*CC_ADD_LENGTH+1;
      groupblockheight = height;
    } else {
      height = (2*CC_ADD_LENGTH+1)*cnt + CC_BLOCK_GAP*(cnt-1);
      groupblockheight = (2 * CC_ADD_LENGTH) + cnt + (cnt-1) * CC_ARROW_GAP;
    }

    if (c1 == currentSelect) {
      fl_color(fl_darker(color()));
      fl_rectf(x(), ypos + y() - shift, w(), height+2*CC_GROUP_GAP);
      fl_color(labelcolor());
      fl_rect(x(), ypos + y() - shift, w(), height+2*CC_GROUP_GAP);
    }

    unsigned int groupblockshift = (height-groupblockheight)/2;

    ypos += CC_GROUP_GAP;

    if (sortByResult) {
      puzzle->getColor(c1, &r, &g, &b);

      fl_rectf(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP, ypos + y()-shift + groupblockshift, CC_BLOCK_WIDTH, groupblockheight, r, g, b);
      fl_rect(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP, ypos + y()-shift + groupblockshift, CC_BLOCK_WIDTH, groupblockheight);
    } else {
      puzzle->getColor(c1, &r, &g, &b);
      fl_rectf(x()+CC_LR_GAP, ypos + y()-shift + groupblockshift, CC_BLOCK_WIDTH, groupblockheight, r, g, b);
      fl_rect(x()+CC_LR_GAP, ypos + y()-shift + groupblockshift, CC_BLOCK_WIDTH, groupblockheight);
    }

    unsigned int yp1 = ypos + y()-shift;
    unsigned int yp2 = ypos + groupblockshift + y()-shift;

    for (c2 = 0; c2 < puzzle->colorNumber(); c2++)
      if ((!sortByResult && puzzle->probPlacementAllowed(problem, c1+1, c2+1)) ||
          (sortByResult && puzzle->probPlacementAllowed(problem, c2+1, c1+1))) {

        if (sortByResult) {
          puzzle->getColor(c2, &r, &g, &b);
          fl_rectf(x()+CC_LR_GAP, yp1, CC_BLOCK_WIDTH, 2*CC_ADD_LENGTH+1, r, g, b);
          fl_rect(x()+CC_LR_GAP, yp1, CC_BLOCK_WIDTH, 2*CC_ADD_LENGTH+1);

          fl_color(labelcolor());

          fl_xyline(x() + CC_LR_GAP + CC_BLOCK_WIDTH + CC_ARROW_LR, yp1 + CC_ADD_LENGTH,
                    x() + CC_LR_GAP + CC_BLOCK_WIDTH + CC_ARROW_LR + CC_HORIZ_LENGTH);

          fl_xyline(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR, yp2 + CC_ADD_LENGTH,
                    x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR - CC_HORIZ_LENGTH);

          fl_line(x() + CC_LR_GAP + CC_BLOCK_WIDTH + CC_ARROW_LR + CC_HORIZ_LENGTH, yp1 + CC_ADD_LENGTH,
                  x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR - CC_HORIZ_LENGTH, yp2 + CC_ADD_LENGTH);

          fl_line(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR, yp2 + CC_ADD_LENGTH,
                  x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR - CC_ARROW_LENGTH, yp2 + CC_ADD_LENGTH - CC_ARROW_WIDTH);
          fl_line(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR, yp2 + CC_ADD_LENGTH,
                  x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR - CC_ARROW_LENGTH, yp2 + CC_ADD_LENGTH + CC_ARROW_WIDTH);


        } else {
          puzzle->getColor(c2, &r, &g, &b);
          fl_rectf(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP, yp1, CC_BLOCK_WIDTH, 2*CC_ADD_LENGTH+1, r, g, b);
          fl_rect(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP, yp1, CC_BLOCK_WIDTH, 2*CC_ADD_LENGTH+1);

          fl_color(labelcolor());

          fl_xyline(x() + CC_LR_GAP + CC_BLOCK_WIDTH + CC_ARROW_LR, yp2 + CC_ADD_LENGTH,
                    x() + CC_LR_GAP + CC_BLOCK_WIDTH + CC_ARROW_LR + CC_HORIZ_LENGTH);

          fl_xyline(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR, yp1 + CC_ADD_LENGTH,
                    x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR - CC_HORIZ_LENGTH);

          fl_line(x() + CC_LR_GAP + CC_BLOCK_WIDTH + CC_ARROW_LR + CC_HORIZ_LENGTH, yp2 + CC_ADD_LENGTH,
                  x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR - CC_HORIZ_LENGTH, yp1 + CC_ADD_LENGTH);

          fl_line(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR, yp1 + CC_ADD_LENGTH,
                  x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR - CC_ARROW_LENGTH, yp1 + CC_ADD_LENGTH - CC_ARROW_WIDTH);
          fl_line(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR, yp1 + CC_ADD_LENGTH,
                  x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR - CC_ARROW_LENGTH, yp1 + CC_ADD_LENGTH + CC_ARROW_WIDTH);
        }

        yp1 += 2*CC_ADD_LENGTH+1 + CC_BLOCK_GAP;
        yp2 += CC_ARROW_GAP;
      }

    ypos += CC_GROUP_GAP + height;
  }

  if (ypos > (unsigned int)h())
    ypos -= h();
  else
    ypos = 0;

  if (lastHight != ypos) {
    lastHight = ypos;

    // changed hight
    do_callback(RS_CHANGEDHIGHT);
  }

  fl_pop_clip();
}

int ColorConstraintsEdit::handle(int event) {

  unsigned int ypos = 0;

  if ((w() <= 0) || (h() <= 0)) return 1;

  if (event != FL_PUSH)
    return 1;

  if (problem >= puzzle->problemNumber())
    return 1;

  for (unsigned int c1 = 0; c1 < puzzle->colorNumber(); c1++) {
    unsigned int cnt = 0;
    unsigned int c2;
    for (c2 = 0; c2 < puzzle->colorNumber(); c2++)
      if ((!sortByResult && puzzle->probPlacementAllowed(problem, c1+1, c2+1)) ||
          (sortByResult && puzzle->probPlacementAllowed(problem, c2+1, c1+1)))
        cnt++;

    unsigned int height;

    if (cnt == 0) {
      height = 2*CC_ADD_LENGTH+1;
    } else {
      height = (2*CC_ADD_LENGTH+1)*cnt + CC_BLOCK_GAP*(cnt-1);
    }

    ypos += 2*CC_GROUP_GAP + height;

    if (Fl::event_y() < y() + (int)ypos - (int)shift) {
      if (currentSelect != c1) {
        currentSelect = c1;
        redraw();
        do_callback(RS_CHANGEDSELECTION);
      }
      break;
    }
  }

  return 1;
}

void ColorConstraintsEdit::setSelection(unsigned int num) {
  currentSelect = num;
  redraw();
}


void ColorConstraintsEdit::setPuzzle(puzzle_c *pz, unsigned int prob) {
  bt_assert(pz);
  if ((puzzle != pz) || (problem != prob)) {
    puzzle = pz;
    problem = prob;
    setSelection(0);
  }
}

