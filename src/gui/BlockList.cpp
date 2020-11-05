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
#include "BlockList.h"

#include "piececolor.h"

#include "../lib/puzzle.h"
#include "../lib/problem.h"
#include "../lib/voxel.h"
#include "../lib/assembly.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define GL_SILENCE_DEPRECATION 1
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#pragma GCC diagnostic pop

/* draw a blocklist */
void BlockList::draw() {

  /* current position and current line feed size */
  unsigned int zpos = 0;
  unsigned int maxz = 0;
  unsigned int xpos = 0;

  /* we are too small to draw anything return */
  if ((w() <= 0) || (h() <= 0)) return;

  /* push our clip area so that we don't paint on the outside */
  fl_push_clip(x(), y(), w(), h());

  /* clear area. This is necessary because we have empty background
   * at the end of lines where no block fits
   */
  fl_color(color());
  fl_rectf(x(), y(), w(), h());

  /* draw all blocks */
  for (unsigned int i = 0; i < blockNumber(); i++) {

    unsigned int wi, hi;

    /* get the size */
    blockSize(i, &wi, &hi);

    /* check if it still fits on the current row */
    if ((xpos > 0) && (xpos + wi > (unsigned int)w())) {
      zpos += maxz;
      maxz = 0;
      xpos = 0;
    }

    /* save line feed size */
    if (hi > maxz) maxz = hi;

    /* draw */
    if (((int)zpos >= (int)shift-(int)hi) && (zpos < shift+h()))
      blockDraw(i, x()+xpos, y()+zpos-shift);

    xpos += wi;
  }

  /* calculate how many how many pixels we can scroll */
  unsigned int scroll = zpos + maxz;

  if (scroll > (unsigned int)h())
    scroll -= h();
  else
    scroll = 0;

  /* if the hight value has changed inform the user */
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
      /* find out which block has been clicked */
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

          /* save the block number and call inherited class */
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

unsigned int SelectableList::getSelection(void) {
  if (currentSelect == (unsigned int)-1)
    if (blockNumber() > 0)
      currentSelect = 0;
  return currentSelect;
}

/* draw a selectable text block with a background colour */
void SelectableTextList::blockDraw(unsigned int block, int x, int y) {
  int w, h;
  unsigned char r, g, b;
  char txt[200];

  /* get colour and text from inheritor */
  getColor(block, &r, &g, &b);
  getText(block, txt);

  /* when we are deactivated, turn the colour into grey */
  if (!active()) {
    int gray = (3*g + 6*g + b) / 10;
    r = g = b = gray;
  }

  /* measure text and add border */
  w = 0;
  fl_measure(txt, w, h);
  w += 8;
  h += 4;

  /* draw the coloured rectangle */
  fl_rectf(x, y, w, h, r, g, b);

  /* text colour depends on how light the colour is */
  if ((int)3*r + 6*g + 1*b > 1275)
    fl_color(0, 0, 0);
  else
    fl_color(255, 255, 255);

  /* draw the text */
  fl_font(labelfont(), labelsize());
  fl_draw(txt, x+4, y+h-2-fl_descent());

  /* if this block is selected draw a black-white border around it */
  if (block == getSelection() && active()) {
    fl_color(0, 0, 0);
    fl_rect(x, y, w, h);
    fl_color(255, 255, 255);
    fl_rect(x+1, y+1, w-2, h-2);
    fl_color(0, 0, 0);
    fl_rect(x+2, y+2, w-4, h-4);
  }
}

/* return the size of a text block */
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


/* draw a non selectable text block with background colour */
void TextList::blockDraw(unsigned int block, int x, int y) {
  int w, h;
  unsigned char r, g, b;
  char txt[200];

  getColor(block, &r, &g, &b);
  getText(block, txt);

  /* when we are deactivated, turn the colour into grey */
  if (!active()) {
    int gray = (3*g + 6*g + b) / 10;
    r = g = b = gray;
  }

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
  fl_draw(txt, x+4, y+h-2-fl_descent());
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

/* return the colour for the block */
void ColorSelector::getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b) {

  // the block 0 is the neutral colour and always available and
  // not saved in the colour list
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
    snprintf(text, 200, "Default");
  else
    snprintf(text, 200, "C%i", block);
}

void PieceSelector::setPuzzle(puzzle_c *pz) {
  bt_assert(pz);
  puzzle = pz;
  if (pz->getNumberOfShapes())
    setSelection(0);
  else
    setSelection((unsigned int)-1);
}

unsigned int PieceSelector::blockNumber(void) {
  return puzzle->getNumberOfShapes();
}

void PieceSelector::getText(unsigned int block, char * text) {

  unsigned int start = 0;

  start += snprintf(text+start, 200-start, "S%i", block+1);

  if (puzzle->getShape(block)->getName().length())
    start += snprintf(text+start, 200-start, " - %s", puzzle->getShape(block)->getName().c_str());

  if (puzzle->getShape(block)->getWeight() != 1)
    start += snprintf(text+start, 200-start, " W(%i)", puzzle->getShape(block)->getWeight());
}

void PieceSelector::getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b) {
  *r = pieceColorRi(block);
  *g = pieceColorGi(block);
  *b = pieceColorBi(block);
}

void ProblemSelector::setPuzzle(const puzzle_c *pz) {
  bt_assert(pz);
  puzzle = pz;
  setSelection(0);
}

unsigned int ProblemSelector::blockNumber(void) {
  return puzzle->getNumberOfProblems();
}

void ProblemSelector::getText(unsigned int block, char * text) {
  if (puzzle->getProblem(block)->getName().length())
    snprintf(text, 200, "P%i - %s", block+1, puzzle->getProblem(block)->getName().c_str());
  else
    snprintf(text, 200, "P%i", block+1);
}

void ProblemSelector::getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b) {
  *r = pieceColorRi(block);
  *g = pieceColorGi(block);
  *b = pieceColorBi(block);
}

void PiecesList::setPuzzle(const problem_c *pz) {
  puzzle = pz;
  redraw();
}

unsigned int PiecesList::blockNumber(void) {
  if (!puzzle)
    return 0;

  return puzzle->getNumberOfParts();
}

void PiecesList::getText(unsigned int block, char * text) {

  if (!puzzle) return;

  int txtLen = 200;
  int len;

  /* first the shape name */
  if (puzzle->getPartShape(block)->getName().length())
    len = snprintf(text, txtLen, "S%i - %s", puzzle->getShapeIdOfPart(block)+1, puzzle->getPartShape(block)->getName().c_str());
  else
    len = snprintf(text, txtLen, "S%i", puzzle->getShapeIdOfPart(block)+1);
  text += len;
  txtLen -= len;

  /* now how many pieces of that shape are available */
  if (puzzle->getPartMinimum(block) != puzzle->getPartMaximum(block)) {
    len = snprintf(text, txtLen, "(%i-%i)", puzzle->getPartMinimum(block), puzzle->getPartMaximum(block));
  } else if (puzzle->getPartMinimum(block) != 1) {
    len = snprintf(text, txtLen, "(%i)", puzzle->getPartMinimum(block));
  } else
    len = 0;
  text += len;
  txtLen -= len;

  /* finally the group information */
  for (int i = 0; i < puzzle->getNumberOfPartGroups(block); i++) {
    if (puzzle->getPartGroupCount(block, i) != puzzle->getPartMaximum(block))
      len = snprintf(text, txtLen, ", G%i(%i)", puzzle->getPartGroupId(block, i), puzzle->getPartGroupCount(block, i));
    else
      len = snprintf(text, txtLen, ", G%i", puzzle->getPartGroupId(block, i));
    text += len;
    txtLen -= len;
  }
}

void PiecesList::getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b) {

  if (!puzzle) return;

  *r = pieceColorRi(puzzle->getShapeIdOfPart(block));
  *g = pieceColorGi(puzzle->getShapeIdOfPart(block));
  *b = pieceColorBi(puzzle->getShapeIdOfPart(block));
}

PieceVisibility::PieceVisibility(int x, int y, int w, int h) : BlockList(x, y, w, h), puzzle(0), count(0) {
  visState = 0;
  useState = 0;
}

unsigned int PieceVisibility::blockNumber(void) {
  if (puzzle)
    return puzzle->getNumberOfPieces();
  else
    return 0;
}

void PieceVisibility::blockDraw(unsigned int block, int x, int y) {
  int w, h;
  unsigned char r, g, b;
  char txt[200];

  int shape = 0;

  unsigned int subBlock = block;

  while (subBlock >= puzzle->getPartMaximum(shape)) {
    subBlock -= puzzle->getPartMaximum(shape);
    shape++;
  }
  int shapeID = puzzle->getShapeIdOfPart(shape);

  if (useState[block]) {

    if (puzzle->getPartShape(shape)->getName().length()) {
      if (puzzle->getPartMaximum(shape) > 1)
        snprintf(txt, 199, "S%i.%i - %s", shapeID+1, subBlock+1, puzzle->getPartShape(shape)->getName().c_str());
      else
        snprintf(txt, 199, "S%i - %s", shapeID+1, puzzle->getPartShape(shape)->getName().c_str());
    } else {
      if (puzzle->getPartMaximum(shape) > 1)
        snprintf(txt, 199, "S%i.%i", shapeID+1, subBlock+1);
      else
        snprintf(txt, 199, "S%i", shapeID+1);
    }
  } else {
    snprintf(txt, 199, " ");
  }

  r = pieceColorRi(shapeID, subBlock);
  g = pieceColorGi(shapeID, subBlock);
  b = pieceColorBi(shapeID, subBlock);

  /* when we are deactivated, turn the colour into grey */
  if (!active()) {
    int gray = (3*g + 6*g + b) / 10;
    r = g = b = gray;
  }

  w = 0;
  fl_measure(txt, w, h);
  w += 8;
  h += 4;

  /* depending on the state we draw 3 different things: */
  switch(visState[block]) {
  case 0:
    /* a normal block */
    fl_rectf(x, y, w, h, r, g, b);
    break;
  case 1:
    /* a block that is smaller and leaves a border around */
    fl_rectf(x+2, y+2, w-4, h-4, r, g, b);
    break;
  case 2:
    /* only a frame */
    fl_rectf(x, y, w, 2, r, g, b);
    fl_rectf(x, y+h-2, w, 2, r, g, b);
    fl_rectf(x, y, 2, h, r, g, b);
    fl_rectf(x+w-2, y, 2, h, r, g, b);
    break;
  }

  /* the colour of the label depends on the background colour. If the only frame mode
   * is on we normally have the light background colour, so use the dark colour, too
   */
  if (((int)3*r + 6*g + 1*b > 1275) || (visState[block] == 2))
    fl_color(0, 0, 0);
  else
    fl_color(255, 255, 255);

  fl_font(labelfont(), labelsize());
  fl_draw(txt, x+4, y+h-2-fl_descent());
}

void PieceVisibility::blockSize(unsigned int block, unsigned int *w, unsigned int *h) {
  char txt[200];

  int shape = 0;

  int blockNr = block;

  while (block >= puzzle->getPartMaximum(shape)) {
    block -= puzzle->getPartMaximum(shape);
    shape++;
  }

  int shapeID = puzzle->getShapeIdOfPart(shape);

  if (useState[blockNr]) {

    if (puzzle->getPartShape(shape)->getName().length()) {
      if (puzzle->getPartMaximum(shape) > 1)
        snprintf(txt, 199, "S%i.%i - %s", shapeID+1, block+1, puzzle->getPartShape(shape)->getName().c_str());
      else
        snprintf(txt, 199, "S%i - %s", shapeID+1, puzzle->getPartShape(shape)->getName().c_str());
    } else {
      if (puzzle->getPartMaximum(shape) > 1)
        snprintf(txt, 199, "S%i.%i", shapeID+1, block+1);
      else
        snprintf(txt, 199, "S%i", shapeID+1);
    }

  } else {
    snprintf(txt, 199, " ");
  }

  int wi, hi;
  fl_font(labelfont(), labelsize());
  wi = 0;
  fl_measure(txt, wi, hi);
  *w = wi + 8;
  *h = hi + 4;
}

void PieceVisibility::setPuzzle(const problem_c *pz) {

  unsigned int c = pz ? pz->getNumberOfPieces() : 0;

  /* if nothing changes, don't reset piece visibility */
  if ((pz == puzzle) && visState && (c == count))
    return;

  puzzle = pz;

  if (visState)
    delete [] visState;

  visState = 0;

  if (useState)
    delete [] useState;

  useState = 0;

  /* set up new visibility when a valid problem is available */
  if (c) {
    visState = new unsigned char[c];
    useState = new bool[c];

    for (unsigned int i = 0; i < c; i++) {
      visState[i] = 0;
      useState[i] = 1;
    }

    count = c;
  }

  redraw();
}

void PieceVisibility::setAssembly(assembly_c *assm) {
  bt_assert(assm->placementCount() == count);

  for (unsigned int i = 0; i < count; i++)
    useState[i] = assm->isPlaced(i);
}

void PieceVisibility::push(unsigned int block) {

  visState[block]++;

  if (visState[block] == 3) visState[block] = 0;

  redraw();

  do_callback(RS_CHANGEDSELECTION);
}

unsigned char PieceVisibility::getVisibility(unsigned int piece) {
  bt_assert(piece < puzzle->getNumberOfPieces());
  return visState[piece];
}

void PieceVisibility::hidePiece(unsigned int s) {
  bt_assert(s < puzzle->getNumberOfPieces());

  visState[s] = 2;
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

  /* no valid problem -> exit */
  if (problem >= puzzle->getNumberOfProblems())
    return;

  /* too small -> exit */
  if ((w() <= 0) || (h() <= 0)) return;

  unsigned int ypos = 0;
  unsigned char r, g, b;

  fl_push_clip(x(), y(), w(), h());

  /* clear area */
  fl_color(color());
  fl_rectf(x(), y(), w(), h());

  /* draw the vertical bar that shows which sort mode is active and where added colours will go */
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

  /* now for each colour we have a vertical area
   * on one side is the colour and on the other side is a list of colours
   * that are connected with the current colour
   */
  for (unsigned int c1 = 0; c1 < puzzle->colorNumber(); c1++) {

    /* count the number of colours on the "other" side */
    unsigned int cnt = 0;
    unsigned int c2;
    for (c2 = 0; c2 < puzzle->colorNumber(); c2++)
      if ((!sortByResult && puzzle->getProblem(problem)->placementAllowed(c1+1, c2+1)) ||
          (sortByResult && puzzle->getProblem(problem)->placementAllowed(c2+1, c1+1)))
        cnt++;

    unsigned int height;
    unsigned int groupblockheight;

    /* calculate the hight of the current colour block */
    if (cnt == 0) {
      height = 2*CC_ADD_LENGTH+1;
      groupblockheight = height;
    } else {
      height = (2*CC_ADD_LENGTH+1)*cnt + CC_BLOCK_GAP*(cnt-1);
      groupblockheight = (2 * CC_ADD_LENGTH) + cnt + (cnt-1) * CC_ARROW_GAP;
    }

    /* when we are selected, draw the horizontal bar */
    if (c1 == currentSelect) {
      fl_color(fl_darker(color()));
      fl_rectf(x(), ypos + y() - shift, w(), height+2*CC_GROUP_GAP);
      fl_color(labelcolor());
      fl_rect(x(), ypos + y() - shift, w(), height+2*CC_GROUP_GAP);
    }

    unsigned int groupblockshift = (height-groupblockheight)/2;

    ypos += CC_GROUP_GAP;

    /* draw the active colour on the "one" side with a frame around */
    if (sortByResult) {
      puzzle->getColor(c1, &r, &g, &b);
      fl_rectf(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP, ypos + y()-shift + groupblockshift, CC_BLOCK_WIDTH, groupblockheight, r, g, b);

      fl_color(labelcolor());
      fl_rect(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP, ypos + y()-shift + groupblockshift, CC_BLOCK_WIDTH, groupblockheight);

    } else {

      puzzle->getColor(c1, &r, &g, &b);
      fl_rectf(x()+CC_LR_GAP, ypos + y()-shift + groupblockshift, CC_BLOCK_WIDTH, groupblockheight, r, g, b);

      fl_color(labelcolor());
      fl_rect(x()+CC_LR_GAP, ypos + y()-shift + groupblockshift, CC_BLOCK_WIDTH, groupblockheight);
    }

    unsigned int yp1 = ypos + y()-shift;
    unsigned int yp2 = ypos + groupblockshift + y()-shift;

    /* draw the colours on the "other" side */
    for (c2 = 0; c2 < puzzle->colorNumber(); c2++)
      if ((!sortByResult && puzzle->getProblem(problem)->placementAllowed(c1+1, c2+1)) ||
          (sortByResult && puzzle->getProblem(problem)->placementAllowed(c2+1, c1+1))) {

        if (sortByResult) {

          /* draw the colour */
          puzzle->getColor(c2, &r, &g, &b);
          fl_rectf(x()+CC_LR_GAP, yp1, CC_BLOCK_WIDTH, 2*CC_ADD_LENGTH+1, r, g, b);

          fl_color(labelcolor());
          fl_rect(x()+CC_LR_GAP, yp1, CC_BLOCK_WIDTH, 2*CC_ADD_LENGTH+1);


          /* draw the arrow between both sides first the connection */
          fl_xyline(x() + CC_LR_GAP + CC_BLOCK_WIDTH + CC_ARROW_LR, yp1 + CC_ADD_LENGTH,
                    x() + CC_LR_GAP + CC_BLOCK_WIDTH + CC_ARROW_LR + CC_HORIZ_LENGTH);

          fl_xyline(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR, yp2 + CC_ADD_LENGTH,
                    x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR - CC_HORIZ_LENGTH - 1);

          fl_line(x() + CC_LR_GAP + CC_BLOCK_WIDTH + CC_ARROW_LR + CC_HORIZ_LENGTH, yp1 + CC_ADD_LENGTH,
                  x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR - CC_HORIZ_LENGTH, yp2 + CC_ADD_LENGTH);

          /* and then the point */
          fl_line(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR, yp2 + CC_ADD_LENGTH,
                  x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR - CC_ARROW_LENGTH, yp2 + CC_ADD_LENGTH - CC_ARROW_WIDTH);
          fl_line(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR, yp2 + CC_ADD_LENGTH,
                  x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR - CC_ARROW_LENGTH, yp2 + CC_ADD_LENGTH + CC_ARROW_WIDTH);


        } else {

          /* draw the colour */
          puzzle->getColor(c2, &r, &g, &b);
          fl_rectf(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP, yp1, CC_BLOCK_WIDTH, 2*CC_ADD_LENGTH+1, r, g, b);

          fl_color(labelcolor());
          fl_rect(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP, yp1, CC_BLOCK_WIDTH, 2*CC_ADD_LENGTH+1);


          /* draw the arrow between both sides first the connection */
          fl_xyline(x() + CC_LR_GAP + CC_BLOCK_WIDTH + CC_ARROW_LR, yp2 + CC_ADD_LENGTH,
                    x() + CC_LR_GAP + CC_BLOCK_WIDTH + CC_ARROW_LR + CC_HORIZ_LENGTH);

          fl_xyline(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR, yp1 + CC_ADD_LENGTH,
                    x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR - CC_HORIZ_LENGTH - 1);

          fl_line(x() + CC_LR_GAP + CC_BLOCK_WIDTH + CC_ARROW_LR + CC_HORIZ_LENGTH, yp2 + CC_ADD_LENGTH,
                  x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR - CC_HORIZ_LENGTH, yp1 + CC_ADD_LENGTH);

          /* and then the point */
          fl_line(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR, yp1 + CC_ADD_LENGTH,
                  x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR - CC_ARROW_LENGTH, yp1 + CC_ADD_LENGTH - CC_ARROW_WIDTH);
          fl_line(x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR, yp1 + CC_ADD_LENGTH,
                  x()+w()-CC_BLOCK_WIDTH-CC_LR_GAP - CC_ARROW_LR - CC_ARROW_LENGTH, yp1 + CC_ADD_LENGTH + CC_ARROW_WIDTH);
        }

        /* next positions for the arrow start and end */
        yp1 += 2*CC_ADD_LENGTH+1 + CC_BLOCK_GAP;
        yp2 += CC_ARROW_GAP;
      }

    /* next block */
    ypos += CC_GROUP_GAP + height;
  }

  /* calculate scroll hight */
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

  /* nothing there */
  if ((w() <= 0) || (h() <= 0)) return 1;

  /* only handle push */
  if (event != FL_PUSH)
    return 1;

  /* no valid problem available */
  if (problem >= puzzle->getNumberOfProblems())
    return 1;

  /* find out the group that we clicked onto */
  for (unsigned int c1 = 0; c1 < puzzle->colorNumber(); c1++) {

    /* count the number of colours in the current group */
    unsigned int cnt = 0;
    unsigned int c2;
    for (c2 = 0; c2 < puzzle->colorNumber(); c2++)
      if ((!sortByResult && puzzle->getProblem(problem)->placementAllowed(c1+1, c2+1)) ||
          (sortByResult && puzzle->getProblem(problem)->placementAllowed(c2+1, c1+1)))
        cnt++;

    unsigned int height;

    /* calculate the height of the current group */
    if (cnt == 0) {
      height = 2*CC_ADD_LENGTH+1;
    } else {
      height = (2*CC_ADD_LENGTH+1)*cnt + CC_BLOCK_GAP*(cnt-1);
    }

    ypos += 2*CC_GROUP_GAP + height;

    /* are we inside the group ? */
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
