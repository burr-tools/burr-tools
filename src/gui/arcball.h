/* Burr Solver
 * Copyright (C) 2003-2007  Andreas Röver
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
#ifndef __ARCBALL_H__
#define __ARCBALL_H__

/**
 * This class provides an implementation of an arcball. The mathematics of this is beyond me.
 * Arcball is an algorithm that allows you to meaningful rotate objects by dragging them.
 * The problem normally is that you must rotate the objects differently depending on their
 * current rotation, when you want to drag them. This is handled in here.
 *
 * The class needs to know the size of the area where you can drag, so that it can make sense
 * out of the given mouse positions. The rest of the handling is basic
 */
class arcBall_c {

protected:
  void mapToSphere(float x, float y, float NewVec[3]) const;
  void getDrag(float NewRot[4]) const;

public:
  /**
   * create arcball class with an initial size for the drag area
   */
  arcBall_c(float NewWidth, float NewHeight);

  /**
   * change the size of the area where the mouse can move to
   */
  void setBounds(float NewWidth, float NewHeight);

  /**
   * the mouse starts to drag, give the position of the cursor
   */
  void click(float x, float y);

  /**
   * end the mouse dragging at the given position
   */
  void clack(float x, float y);

  /**
   * update the position of the mouse cursor, while dragging is active
   */
  void drag(float x, float y);

  /**
   * adds the current arcball transformation to the OpenGL transformation matrix
   */
  void addTransform(void) const;

protected:
  float AdjustWidth;       //Mouse bounds width
  float AdjustHeight;      //Mouse bounds height

  float StVec[3];          //Saved click vector
  float EnVec[3];          //Saved drag vector

  float LastRot[9];

  bool mouseDown;
};

#endif
