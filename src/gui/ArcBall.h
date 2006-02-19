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
#ifndef _ArcBall_h
#define _ArcBall_h

#ifdef WIN32
#include <windows.h>
#endif

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

/**
 * This class provides an implementation of an arcball. The mathematics of this is beyond me.
 * Arcball is an algorithm that allows you to meaningful rotate objects by dragging them.
 * The problem normally is that you must rotate the objects differently depending on their
 * current rotation, when you want to drag them. This is handled in here.
 *
 * The class needs to know the size of the area where you can drag, so that it can make sense
 * out of the given mouse positions. The rest of the handling is basic
 */
class ArcBall_c {

protected:
  void mapToSphere(GLfloat x, GLfloat y, GLfloat NewVec[3]) const;
  void getDrag(GLfloat NewRot[4]) const;

public:
  /**
   * create arcball class with an initial size for the drag area
   */
  ArcBall_c(GLfloat NewWidth, GLfloat NewHeight);

  /**
   * change the size of the area where the mouse can move to
   */
  void setBounds(GLfloat NewWidth, GLfloat NewHeight);

  /**
   * the mouse starts to drag, give the position of the cursor
   */
  void click(GLfloat x, GLfloat y);

  /**
   * end the mouse draggin at the given position
   */
  void clack(GLfloat x, GLfloat y);

  /**
   * update the position of the mouse cursor, while dragging is active
   */
  void drag(GLfloat x, GLfloat y);

  /**
   * adds the current arcball transformation to the OpenGL transformation matrix
   */
  void addTransform(void) const;

protected:
  GLfloat AdjustWidth;       //Mouse bounds width
  GLfloat AdjustHeight;      //Mouse bounds height

  GLfloat StVec[3];          //Saved click vector
  GLfloat EnVec[3];          //Saved drag vector

  GLfloat LastRot[9];

  bool mouseDown;
};

#endif
