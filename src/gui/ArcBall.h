#ifndef _ArcBall_h
#define _ArcBall_h

#ifdef WIN32
#include <windows.h>
#endif

#include <GL/gl.h>

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
  void clack(void);

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
