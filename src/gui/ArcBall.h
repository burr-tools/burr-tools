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

class ArcBall_c {

protected:
  void mapToSphere(GLfloat x, GLfloat y, GLfloat NewVec[3]) const;
  void getDrag(GLfloat NewRot[4]) const;

public:
  //Create/Destroy
  ArcBall_c(GLfloat NewWidth, GLfloat NewHeight);

  //Set new bounds
  void setBounds(GLfloat NewWidth, GLfloat NewHeight);

  //Mouse down
  void click(GLfloat x, GLfloat y);

  // Mouse up
  void clack(GLfloat x, GLfloat y);

  //Mouse drag, calculate rotation
  void drag(GLfloat x, GLfloat y);

  // adds the current arcball transformation to the OpenGL transformation matrix
  void addTransform(void);

protected:
  GLfloat AdjustWidth;       //Mouse bounds width
  GLfloat AdjustHeight;      //Mouse bounds height

  GLfloat StVec[3];          //Saved click vector
  GLfloat EnVec[3];          //Saved drag vector

  GLfloat LastRot[9];
  GLfloat ThisRot[9];

  bool mouseDown;
};

#endif
