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


#include "VoxelView.h"
#include <math.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <stdlib.h>

#include "../lib/puzzle.h"

#include "pieceColor.h"

#include <Fl/Fl.h>

VoxelView::VoxelView(int x,int y,int w,int h,const char *l) : Fl_Gl_Window(x,y,w,h,l),
  arcBall(new ArcBall_c(w, h)), doUpdates(true), size(10)
{
};


static void gluPerspective(double fovy, double aspect, double zNear, double zFar) {

  double xmin, xmax, ymin, ymax;
  ymax = zNear * tan(fovy * 3.1415927 / 360.0);
  ymin = -ymax;
  xmin = ymin * aspect;
  xmax = ymax * aspect;
  glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

void VoxelView::draw() {

  if (!doUpdates)
    return;

  if (!valid()) {

    GLfloat LightAmbient[]= { 0.01f, 0.01f, 0.01f, 1.0f };
    GLfloat LightDiffuse[]= { 1.5f, 1.5f, 1.5f, 1.0f };
    GLfloat LightPosition[]= { 700.0f, 200.0f, -90.0f, 1.0f };

    GLfloat AmbientParams[] = {0.1, 0.1, 0.1, 1};
    GLfloat DiffuseParams[] = {0.7, 0.7, 0.7, 0.1};
    GLfloat SpecularParams[] = {0.4, 0.4, 0.4, 0.5};

    glLoadIdentity();
    glViewport(0,0,w(),h());

    glEnable(GL_COLOR_MATERIAL);
    glLightfv(GL_LIGHT1, GL_AMBIENT, LightAmbient);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, LightDiffuse);
    glLightfv(GL_LIGHT1, GL_POSITION, LightPosition);
    glEnable(GL_LIGHT1);

    glMaterialf(GL_FRONT, GL_SHININESS, 0.5);
    glMaterialfv(GL_FRONT, GL_AMBIENT, AmbientParams);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, DiffuseParams);
    glMaterialfv(GL_FRONT, GL_SPECULAR, SpecularParams);

    arcBall->setBounds(w(), h());

    unsigned char r, g, b;
    Fl::get_color(color(), r, g, b);
    glClearColor(r/255.0, g/255.0, b/255.0, 0);
  }

  glPushMatrix();
  gluPerspective(5+size, 1.0*w()/h(), 10, 1100);
  drawData();
  glPopMatrix();
}

void VoxelView::update(bool doIt) {
  doUpdates = doIt;
  if (doIt)
    redraw();
}

int VoxelView::handle(int event) {

  if (Fl_Gl_Window::handle(event))
    return 1;

  switch(event) {
  case FL_PUSH:
    arcBall->click(Fl::event_x(), Fl::event_y());
    return 1;

  case FL_DRAG:
    arcBall->drag(Fl::event_x(), Fl::event_y());
    redraw();
    return 1;

  case FL_RELEASE:
    arcBall->clack(Fl::event_x(), Fl::event_y());
    return 1;
  }

  return 0;
}

void VoxelView::addRotationTransformation(void) {
  arcBall->addTransform();
}

void VoxelView::updateRequired(void) {
  redraw();
}

void VoxelView::setSize(double sz) {
  size = sz;
  redraw();
}

