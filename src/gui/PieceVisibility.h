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
