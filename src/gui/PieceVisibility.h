#ifndef __PIECEVISIBILITY_H__
#define __PIECEVISIBILITY_H__

#include <FL/Fl.H>
#include <FL/Fl_Widget.h>

class PieceVisibility : public Fl_Widget {


private:

  int shapenumber;
  int *pieceNumbers;
  int shift;

  int lastHight;
  
  char * vis;

public:

  PieceVisibility(int x, int y, int w, int h, const char *label = 0) :
    Fl_Widget(x, y, w, h, label),
    shapenumber(0),
    pieceNumbers(0),
    shift(0),
    lastHight(-1),
    vis(0) {}

  ~PieceVisibility(void) {
    if (pieceNumbers)
      delete [] pieceNumbers;
  }

protected:

  void draw();

public:

  void setShift(int z);

  // this function takes over the array numPieces and deletes it when neecessary
  void setPieceNumber(int numShapes, int *numPieces, char * visible);

  int handle(int event);

  int calcHeight(void);

};


#endif
