#ifndef __PIECESELECTOR_H__
#define __PIECESELECTOR_H__

#include <FL/Fl.H>
#include <FL/Fl_Widget.h>

#include "../lib/puzzle.h"


/* a widget that shows a list of the puzzle pieces with their color
 * and alows the user to active one of it
 *
 * there are 2 reasons for the callback: user clicked and changed the selection
 * and the hight changed. This second callback is there to allow the application
 * to adjust the scrollbar connected with this widget
 */

class PieceSelector : public Fl_Widget {

private:

  /* currently selected puzzle */
  int currentSelect;

  /* how many pixels is the whole thing shifted up. This is for
   * scrollbar to allow more pieces than there is size for the widget
   */
  int shift;

  /* the hight that the whole drawing had the last time */
  int lastHight;

  puzzle_c * puzzle;

  int callbackReason;

public:

  PieceSelector(int x, int y, int w, int h, const char *label = 0) :
    Fl_Widget(x, y, w, h, label),
    currentSelect(0),
    shift(0),
    lastHight(-1),
    puzzle(0)
  {}

protected:

  void draw();

public:

  void setShift(int z);
  void setPuzzle(puzzle_c *pz);
  int handle(int event);

  int getSelectedPiece(void) { return currentSelect; }

  void setSelectedPiece(int num) {
    currentSelect = num;
    redraw();
  }

  int calcHeight(void) { return lastHight; }

  enum {
    RS_CHANGEDHIGHT,
    RS_CHANGEDSELECTION,
    RS_CHANGEDNUMBER
  };

  int getReason(void) { return callbackReason; }

};


#endif
