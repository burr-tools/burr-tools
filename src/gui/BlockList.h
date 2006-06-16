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
#ifndef __BLOCKLIST_H__
#define __BLOCKLIST_H__

#include "../lib/bt_assert.h"

#include <FL/Fl_Widget.h>

class puzzle_c;
/**
 * blocklist is a widget that displays a list of items in blocks. These blocks
 * have the size, so that the label of the item fits. The blocks are arranged
 * line by line like left aligned text
 *
 * this is the abstract base widget of all the different block lists
 */
class BlockList : public Fl_Widget {

  /* how many pixels is the whole thing shifted up. This is for
   * scrollbar to allow more pieces than there is size for the widget
   */
  unsigned int shift;

  /* the hight that the whole drawing had the last time */
  unsigned int lastHight;

  int callbackReason;

protected:

  void draw(void);

  /**
   * real block lists return the number of blocks with this function
   */
  unsigned int virtual blockNumber(void) = 0;

  /**
   * the inherited classes need to draw block number block at the given position
   */
  void virtual blockDraw(unsigned int block, int x, int y) = 0;

  /**
   * the inherited classes need to return the size of the block with the given
   * number in the w and h variables
   */
  void virtual blockSize(unsigned int block, unsigned int *w, unsigned int *h) = 0;

  /**
   * these are functions that can be filled with actions, when the given block is
   * clicked or whatever
   */
  virtual void push(unsigned int block) {};
  virtual void release(unsigned int block) {};
  virtual void drag(unsigned int block, int dx, int dy) {};

  void do_callback(int reason) {
    callbackReason = reason;
    Fl_Widget::do_callback(this, reason);
  }

public:

  BlockList(int x, int y, int w, int h) : Fl_Widget(x, y, w, h), shift(0), lastHight(0xFFFFFFFF) {}

  /**
   * this sets the amount of pixels that the block list is shifted upwards from 0
   */
  void setShift(unsigned int z) {
    shift = z;
    redraw();
  }

  int handle(int event);

  /**
   * this function returns the current height of the block list. The height depends on how many
   * lines are required to display all blocks
   * the hight is NOT the hight of the block list, but the number of pixels that need to be scrolled
   * to show everything, so when the block list is 100 pixels high and the widget 50 we get 100-50 = 50
   * if everything fits into the widget size 0 is returned
   */
  unsigned int calcHeight(void) { return lastHight; }

  enum {
    RS_CHANGEDHIGHT,
    RS_LIST_LAST
  };

  int getReason(void) { return callbackReason; }
};

/**
 * this class defines a blocklist, where different elements of the list
 * can be selected by clicking on the items with the left mouse button
 */
class SelectableList : public BlockList {

private:
  /* currently selected puzzle */
  unsigned int currentSelect;

  bool locked;

public:

  enum {
    RS_CHANGEDSELECTION = RS_LIST_LAST,
    RS_SELECTABLE_LAST
  };

  SelectableList(int x, int y, int w, int h) : BlockList(x, y, w, h), currentSelect((unsigned int)-1), locked(false) { }

  /**
   * returns the currently selected block, starting from 0
   */
  unsigned int getSelection(void) { return currentSelect; }

  /**
   * sets the currently selected block
   */
  void setSelection(unsigned int num) {
    if (currentSelect != num) {
      currentSelect = num;
      do_callback(RS_CHANGEDSELECTION);
      redraw();
    }
  }

  virtual void push(unsigned int block) {
    if (locked)
      return;

    if (currentSelect != block) {
      currentSelect = block;
      do_callback(RS_CHANGEDSELECTION);
      redraw();
    }
  }

  // locks the current selected position
  // mouse clicks are ignored
  void lockPosition(bool lock) { locked = lock; }

};

/**
 * a blocklist whose block labels is text and whose elements can
 * be selected you need to inherit from this class to actually use it
 * the inherited class needs to provide functions that return the colour
 * to use for the block and the text that should be displayed inside the block
 */
class SelectableTextList : public SelectableList {

public:

  SelectableTextList(int x, int y, int w, int h) : SelectableList(x, y, w, h) { }

  /* return the colour for the block */
  void virtual getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b) = 0;

  /* return the text for the block (not more than 20 characters */
  void virtual getText(unsigned int block, char * text) = 0;
  void virtual blockDraw(unsigned int block, int x, int y);
  void virtual blockSize(unsigned int block, unsigned int *w, unsigned int *h);
};

/**
 * a blocklist with text labels but not selectable blocks
 * as in the SelectableTextList you need to provide functions for the colour and
 * the labels of the blocks
 */
class TextList : public BlockList {

public:

  TextList(int x, int y, int w, int h) : BlockList(x, y, w, h) { }

  /* return the colour for the block */
  void virtual getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b) = 0;

  /* return the text for the block (not more than 20 characters */
  void virtual getText(unsigned int block, char * text) = 0;
  void virtual blockDraw(unsigned int block, int x, int y);
  void virtual blockSize(unsigned int block, unsigned int *w, unsigned int *h);
};

/**
 * a concrete block list that displays the defined colours inside a puzzle
 * and lets you select one. Additionally it also always displays a block at the
 * first position with the label Neutral for the transparent or neutral colour
 * that is always there and can not be deleted
 */
class ColorSelector : public SelectableTextList {

  puzzle_c * puzzle;
  bool includeNeutral;

public:

  ColorSelector(int x, int y, int w, int h, puzzle_c * p, bool incNeutr) : SelectableTextList(x, y, w, h), puzzle(p), includeNeutral(incNeutr) { bt_assert(p); if (incNeutr) setSelection(0); }

  void setPuzzle(puzzle_c *pz);

  unsigned int virtual blockNumber(void);

  /* return the colour for the block */
  void virtual getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b);

  /* return the text for the block (not more than 20 characters */
  void virtual getText(unsigned int block, char * text);
};

/**
 * a widget that shows a list of the puzzle pieces with their colour
 * and allows the user to activate one of it
 *
 * there are 2 reasons for the callback: user clicked and changed the selection
 * and the hight changed. This second callback is there to allow the application
 * to adjust the scrollbar connected with this widget
 */
class PieceSelector : public SelectableTextList {

private:

  puzzle_c * puzzle;

public:

  PieceSelector(int x, int y, int w, int h, puzzle_c * p) : SelectableTextList(x, y, w, h), puzzle(p) { bt_assert(p); }

  void setPuzzle(puzzle_c *pz);
  unsigned int virtual blockNumber(void);
  void virtual getText(unsigned int block, char * text);
  void virtual getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b);
};

/**
 * blocklist that show the problems and lets you select one of it
 */
class ProblemSelector : public SelectableTextList {

private:

  puzzle_c * puzzle;

public:

  ProblemSelector(int x, int y, int w, int h, puzzle_c * p) :
    SelectableTextList(x, y, w, h),
    puzzle(p)
  { bt_assert(p); }

  void setPuzzle(puzzle_c *pz);
  unsigned int virtual blockNumber(void);
  void virtual getText(unsigned int block, char * text);
  void virtual getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b);
};

/**
 * a blocklist that show the pieces of a problem and additional information
 * for each piece
 */
class PiecesList : public TextList {

private:

  puzzle_c * puzzle;
  unsigned int problem;

  unsigned int clicked;

public:

  enum {
    RS_CLICKED = RS_LIST_LAST,
    RS_PIECES_LAST
  };

  PiecesList(int x, int y, int w, int h, puzzle_c * p) : TextList(x, y, w, h), puzzle(p), problem(0) { bt_assert(p); }

  void setPuzzle(puzzle_c *pz, unsigned int prob);
  unsigned int virtual blockNumber(void);
  void virtual getText(unsigned int block, char * text);
  void virtual getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b);

  virtual void push(unsigned int block) {
    clicked = block;
    do_callback(RS_CLICKED);
  }

  unsigned int getClicked(void) { return clicked; }

};

/**
 * a blocklist to set the visibility mode for all pieces in a puzzle. The
 * user can circle between 3 possible states: 0, 1 or 2
 */
class PieceVisibility : public BlockList {

private:

  puzzle_c * puzzle;
  unsigned int problem;

  unsigned int count;
  unsigned char * visState;

public:

  enum {
    RS_CHANGEDSELECTION = RS_LIST_LAST,
    RS_VISIBILITY_LAST
  };

  PieceVisibility(int x, int y, int w, int h, puzzle_c * p);

  ~PieceVisibility(void) { if (visState) delete visState; }

  void setPuzzle(puzzle_c *pz, unsigned int prob);
  unsigned int virtual blockNumber(void);
  void virtual blockDraw(unsigned int block, int x, int y);
  void virtual blockSize(unsigned int block, unsigned int *w, unsigned int *h);

  virtual void push(unsigned int block);

  /**
   * return the visibility mode for the given piece. Currently
   * there are 3 return values 0, 1 or 2
   * it's up to you to say what these values mean
   */
  unsigned char getVisibility(unsigned int piece);
};

/**
 * A widget allowing to display and edit colour constraints.
 * In reality colour constraints are really colour permissions.
 * Each entry allows the placement of a cube of one colour into a
 * cube of another colour.
 * The editor currently shows a list of colours on the left and
 * a list of colours on the right and arrows between them to say
 * this colour on the left can go into this colour on the right
 */
class ColorConstraintsEdit : public Fl_Widget {

  /* how many pixels is the whole thing shifted up. This is for
   * scrollbar to allow more pieces than there is size for the widget
   */
  unsigned int shift;

  /* the hight that the whole drawing had the last time */
  unsigned int lastHight;

  /* the puzzle and the problem to be edited */
  puzzle_c * puzzle;
  unsigned int problem;

  bool sortByResult;

  unsigned int currentSelect;

  int callbackReason;

protected:

  void draw(void);

public:

  ColorConstraintsEdit(int x, int y, int w, int h, puzzle_c * p) :
    Fl_Widget(x, y, w, h), shift(0), lastHight(0), puzzle(p), problem(0), sortByResult(false), currentSelect(0) {}

  void setPuzzle(puzzle_c *pz, unsigned int prob);

  void setShift(unsigned int z) {
    shift = z;
    redraw();
  }

  int handle(int event);

  unsigned int calcHeight(void) { return lastHight; }

  enum {
    RS_CHANGEDHIGHT,
    RS_CHANGEDSELECTION,
    RS_LIST_LAST
  };

  unsigned int getReason(void) { return callbackReason; }
  unsigned int getSelection(void) { return currentSelect; }
  void setSelection(unsigned int num);

  void SetSortByResult(bool value) {
    sortByResult = value;
    redraw();
  }

  void do_callback(int reason) {
    callbackReason = reason;
    Fl_Widget::do_callback(this, reason);
  }

  bool GetSortByResult(void) { return sortByResult; }
};

#endif
