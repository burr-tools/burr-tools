#ifndef __BLOCKLIST_H__
#define __BLOCKLIST_H__

#include <FL/Fl_Widget.h>

#include "../lib/puzzle.h"


class BlockList : public Fl_Widget {

  /* how many pixels is the whole thing shifted up. This is for
   * scrollbar to allow more pieces than there is size for the widget
   */
  int shift;

  /* the hight that the whole drawing had the last time */
  int lastHight;

  int callbackReason;

protected:

  void draw(void);

  unsigned int virtual blockNumber(void) = 0;
  void virtual blockDraw(unsigned int block, int x, int y) = 0;
  void virtual blockSize(unsigned int block, unsigned int *w, unsigned int *h) = 0;

  virtual void push(unsigned int block) {};
  virtual void release(unsigned int block) {};
  virtual void drag(unsigned int block, int dx, int dy) {};

  void do_callback(int reason) {
    callbackReason = reason;
    Fl_Widget::do_callback();
  }

public:

  BlockList(int x, int y, int w, int h) : Fl_Widget(x, y, w, h), shift(0) {}

  void setShift(int z) {
    shift = z;
    redraw();
  }

  int handle(int event);
  int calcHeight(void) { return lastHight; }

  enum {
    RS_CHANGEDHIGHT,
    RS_LIST_LAST
  };

  int getReason(void) { return callbackReason; }
};



class SelectableList : public BlockList {

private:
  /* currently selected puzzle */
  unsigned int currentSelect;

public:

  enum {
    RS_CHANGEDSELECTION = RS_LIST_LAST,
    RS_SELECTABLE_LAST
  };

  SelectableList(int x, int y, int w, int h) : BlockList(x, y, w, h), currentSelect(0) { }

  int getSelection(void) { return currentSelect; }

  void setSelection(unsigned int num) {
    currentSelect = num;
    redraw();
  }

  virtual void push(unsigned int block) {
    currentSelect = block;
    do_callback(RS_CHANGEDSELECTION);
    redraw();
  }
};

class SelectableTextList : public SelectableList {

public:

  SelectableTextList(int x, int y, int w, int h) : SelectableList(x, y, w, h) { }

  /* return the color for the block */
  void virtual getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b) = 0;

  /* return the text for the block (not more than 20 characters */
  void virtual getText(unsigned int block, char * text) = 0;
  void virtual blockDraw(unsigned int block, int x, int y);
  void virtual blockSize(unsigned int block, unsigned int *w, unsigned int *h);
};


class TextList : public BlockList {

public:

  TextList(int x, int y, int w, int h) : BlockList(x, y, w, h) { }

  /* return the color for the block */
  void virtual getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b) = 0;

  /* return the text for the block (not more than 20 characters */
  void virtual getText(unsigned int block, char * text) = 0;
  void virtual blockDraw(unsigned int block, int x, int y);
  void virtual blockSize(unsigned int block, unsigned int *w, unsigned int *h);
};


class ColorSelector : public SelectableTextList {

  puzzle_c * puzzle;
  bool includeNeutral;

public:

  ColorSelector(int x, int y, int w, int h, puzzle_c * p, bool incNeutr) : SelectableTextList(x, y, w, h), puzzle(p), includeNeutral(incNeutr) { assert(p); }

  void setPuzzle(puzzle_c *pz);

  unsigned int virtual blockNumber(void);

  /* return the color for the block */
  void virtual getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b);

  /* return the text for the block (not more than 20 characters */
  void virtual getText(unsigned int block, char * text);
};

/* a widget that shows a list of the puzzle pieces with their color
 * and alows the user to active one of it
 *
 * there are 2 reasons for the callback: user clicked and changed the selection
 * and the hight changed. This second callback is there to allow the application
 * to adjust the scrollbar connected with this widget
 */
class PieceSelector : public SelectableTextList {

private:

  puzzle_c * puzzle;

public:

  PieceSelector(int x, int y, int w, int h, puzzle_c * p) : SelectableTextList(x, y, w, h), puzzle(p) { assert(p); }

  void setPuzzle(puzzle_c *pz);
  unsigned int virtual blockNumber(void);
  void virtual getText(unsigned int block, char * text);
  void virtual getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b);
};


class ProblemSelector : public SelectableTextList {

private:

  puzzle_c * puzzle;

public:

  ProblemSelector(int x, int y, int w, int h, puzzle_c * p) :
    SelectableTextList(x, y, w, h),
    puzzle(p)
  { assert(p); }

  void setPuzzle(puzzle_c *pz);
  unsigned int virtual blockNumber(void);
  void virtual getText(unsigned int block, char * text);
  void virtual getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b);
};

class PiecesList : public TextList {

private:

  puzzle_c * puzzle;
  unsigned int problem;

public:

  PiecesList(int x, int y, int w, int h, puzzle_c * p) : TextList(x, y, w, h), puzzle(p), problem(0) { assert(p); }

  void setPuzzle(puzzle_c *pz, unsigned int prob);
  unsigned int virtual blockNumber(void);
  void virtual getText(unsigned int block, char * text);
  void virtual getColor(unsigned int block, unsigned char *r,  unsigned char *g, unsigned char *b);
};

class PieceVisibility : public BlockList {

private:

  puzzle_c * puzzle;
  unsigned int problem;
  unsigned char * visState;

public:

  PieceVisibility(int x, int y, int w, int h, puzzle_c * p);

  void setPuzzle(puzzle_c *pz, unsigned int prob);
  unsigned int virtual blockNumber(void);
  void virtual blockDraw(unsigned int block, int x, int y);
  void virtual blockSize(unsigned int block, unsigned int *w, unsigned int *h);
};


class ColorConstraintsEdit : public Fl_Widget {

  /* how many pixels is the whole thing shifted up. This is for
   * scrollbar to allow more pieces than there is size for the widget
   */
  int shift;

  /* the hight that the whole drawing had the last time */
  int lastHight;

  puzzle_c * puzzle;
  unsigned int problem;

  bool sortByResult;

  unsigned int currentSelect;

protected:

  void draw(void);

public:

  ColorConstraintsEdit(int x, int y, int w, int h, puzzle_c * p) :
    Fl_Widget(x, y, w, h), shift(0), puzzle(p), problem(0), sortByResult(false), currentSelect(0) {}

  void setPuzzle(puzzle_c *pz, unsigned int prob);

  void setShift(int z) {
    shift = z;
    redraw();
  }

  int handle(int event);

  int calcHeight(void) { return lastHight; }

  enum {
    RS_CHANGEDHIGHT,
    RS_LIST_LAST
  };

  int getReason(void) { return RS_CHANGEDHIGHT; }
  int getSelection(void) { return currentSelect; }

  void SetSortByResult(bool value) {
    sortByResult = value;
    redraw();
  }

  bool GetSortByResult(void) { return sortByResult; }

};



#endif

