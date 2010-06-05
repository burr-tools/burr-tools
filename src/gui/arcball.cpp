/* Burr Solver
 * Copyright (C) 2003-2009  Andreas Röver
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
#include "arcball.h"

#include <math.h>
#include <assert.h>

#include <FL/gl.h>

/* The idea behind this is similar for both methods identical
 *
 * They put a virtual sphere onto the center of the screen area. The sphere is
 * as big as the 3D area allows (that is why we need to know about the area size
 *
 * When dragging between 2 points the following happens:
 * 1) the start and end point are "projected" onto the sphere. We find out where on
 *    the sphere we are (x, y and z) if we are not on the sphere we try to do something
 *    sensible (this is different for the 2 methods)
 * 2) we then draw an arc between these 2 points and and rotate the object around this
 *    arc. This is done by calculating 2 vectors to the center of the sphere. With those
 *    we calculate the rotation axis as the orthogonal vector to those 2 vectors using the
 *    cross product. The rotation angle is the angle between those 2 vectors
 *
 * internally quaternions are used to handle the rotation. A quaternion contains the
 * rotation axis in its first 3 values and the angle in the 4th value. It is relatively
 * easy to connect 2 quaternions and also to find the rotation matrix from a quaternion
 */

#define Epsilon 1.0e-5

/**
 * Sets the vector c to be the vector cross product of vectors a and b.
 * don't do Vector3fCross(a, a, b);
 */
static void Vector3fCross(const float a[3], const float b[3], float c[3])
{
  c[0] = a[1]*b[2] - a[2]*b[1];
  c[1] = a[2]*b[0] - a[0]*b[2];
  c[2] = a[0]*b[1] - a[1]*b[0];
}

/**
 * Computes the dot product of the vector a and b
 */
static GLfloat Vector3fDot(const float a[3], const float b[3])
{
  return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

/**
 * Returns the length of this vector.
 * @return the length of this vector
 */
static float Vector3fLength(const float a[3])
{
  return sqrtf(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
}

/**
 * Sets the value of this matrix to rotation matrix of the
 * quaternion argument.
 * @param q1 the quaternion to be converted
 */
static void Matrix3fSetRotationFromQuat4f(float m[9], const float q1[4])
{
  GLfloat n, s;
  GLfloat xs, ys, zs;
  GLfloat wx, wy, wz;
  GLfloat xx, xy, xz;
  GLfloat yy, yz, zz;

  n = (q1[0] * q1[0]) + (q1[1] * q1[1]) + (q1[2] * q1[2]) + (q1[3] * q1[3]);
  s = (n > 0.0f) ? (2.0f / n) : 0.0f;

  xs = q1[0] * s;  ys = q1[1] * s;  zs = q1[2] * s;
  wx = q1[3] * xs; wy = q1[3] * ys; wz = q1[3] * zs;
  xx = q1[0] * xs; xy = q1[0] * ys; xz = q1[0] * zs;
  yy = q1[1] * ys; yz = q1[1] * zs; zz = q1[2] * zs;

  m[0] = 1.0f - (yy + zz); m[3] =         xy - wz;  m[6] =         xz + wy;
  m[1] =         xy + wz;  m[4] = 1.0f - (xx + zz); m[7] =         yz - wx;
  m[2] =         xz - wy;  m[5] =         yz + wx;  m[8] = 1.0f - (xx + yy);
}

/**
 * a = a matrix mult b
 */
static void Matrix3fMulMatrix3f(float a[9], const float b[9])
{
  GLfloat Result[9];

  Result[0] = a[0]*b[0] + a[3]*b[1] + a[6]*b[2];
  Result[3] = a[0]*b[3] + a[3]*b[4] + a[6]*b[5];
  Result[6] = a[0]*b[6] + a[3]*b[7] + a[6]*b[8];

  Result[1] = a[1]*b[0] + a[4]*b[1] + a[7]*b[2];
  Result[4] = a[1]*b[3] + a[4]*b[4] + a[7]*b[5];
  Result[7] = a[1]*b[6] + a[4]*b[7] + a[7]*b[8];

  Result[2] = a[2]*b[0] + a[5]*b[1] + a[8]*b[2];
  Result[5] = a[2]*b[3] + a[5]*b[4] + a[8]*b[5];
  Result[8] = a[2]*b[6] + a[5]*b[7] + a[8]*b[8];

  for (int i = 0; i < 9; i++)
    a[i] = Result[i];
}

/**
 * Performs SVD on this matrix and gets scale and rotation.
 * Rotation is placed into rot3, and rot4.
 * @param rot3 the rotation factor(Matrix3d). if null, ignored
 * @param rot4 the rotation factor(Matrix4) only upper 3x3 elements are changed. If null, ignored
 * @return scale factor
 */
static GLfloat Matrix4fSVD(const GLfloat a[16])
{
  return sqrtf((a[0]*a[0] + a[1]*a[1] + a[ 2]*a[ 2] +
                a[4]*a[4] + a[5]*a[5] + a[ 6]*a[ 6] +
                a[8]*a[8] + a[9]*a[9] + a[10]*a[10]) / 3.0f);
}

/**
 * Sets the rotational component (upper 3x3) of this matrix to the matrix
 * values in the T precision Matrix3d argument; the other elements of
 * this matrix are unchanged; a singular value decomposition is performed
 * on this object's upper 3x3 matrix to factor out the scale, then this
 * object's upper 3x3 matrix components are replaced by the passed rotation
 * components, and then the scale is reapplied to the rotational
 * components.
 * @param m1 T precision 3x3 matrix
 */
static void Matrix4fSetRotationFromMatrix3f(GLfloat a[16], const float m[9])
{
  GLfloat scale = Matrix4fSVD(a);

  a[ 0] = m[0] * scale;
  a[ 1] = m[1] * scale;
  a[ 2] = m[2] * scale;
  a[ 4] = m[3] * scale;
  a[ 5] = m[4] * scale;
  a[ 6] = m[5] * scale;
  a[ 8] = m[6] * scale;
  a[ 9] = m[7] * scale;
  a[10] = m[8] * scale;
}

/**
 * find the point on the halve-sphere that is closest to the given coordinates
 */
void arcBall_c::mapToSphere(float x, float y, float NewVec[3]) const
{
  GLfloat TempPt[2];

  //Adjust point coordinates and scale down to range of [-1 ... 1]
  TempPt[0] =        (x * AdjustWidth)  - 1.0f;
  TempPt[1] = 1.0f - (y * AdjustHeight);

  //Compute the square of the length of the vector to the point from the centre
  GLfloat length = (TempPt[0] * TempPt[0]) + (TempPt[1] * TempPt[1]);

  //If the point is mapped outside of the sphere... (length > radius squared)
  if (length > 1.0f) {

    //Compute a normalizing factor (radius / sqrt(length))
    GLfloat norm = 1.0f / sqrtf(length);

    //Return the "normalized" vector, a point on the sphere
    NewVec[0] = TempPt[0] * norm;
    NewVec[1] = TempPt[1] * norm;
    NewVec[2] = 0.0f;

  } else {    //Else it's on the inside

    //Return a vector to a point mapped inside the sphere sqrt(radius squared - length)
    NewVec[0] = TempPt[0];
    NewVec[1] = TempPt[1];
    NewVec[2] = sqrtf(1.0f - length);
  }
}

//Create/Destroy
arcBall_c::arcBall_c(float NewWidth, float NewHeight) {

  LastRot[0] = 1;  LastRot[1] = 0;  LastRot[2] = 0;
  LastRot[3] = 0;  LastRot[4] = 1;  LastRot[5] = 0;
  LastRot[6] = 0;  LastRot[7] = 0;  LastRot[8] = 1;

  //Set initial bounds
  setBounds(NewWidth, NewHeight);

  mouseDown = false;
}

void arcBall_c::click(float x, float y) {

  //Map the point to the sphere
  mapToSphere(x, y, StVec);
  mapToSphere(x, y, EnVec);

  mouseDown = true;
}

void arcBall_c::clack(float x, float y) {

  if (!mouseDown) return;

  mapToSphere(x, y, EnVec);

  GLfloat ThisQuat[4];
  GLfloat ThisRot[9];

  getDrag(ThisQuat);                                              // Update End Vector And Get Rotation As Quaternion
  Matrix3fSetRotationFromQuat4f(ThisRot, ThisQuat);               // Convert Quaternion Into Matrix3fT
  Matrix3fMulMatrix3f(ThisRot, LastRot);                          // Accumulate Last Rotation Into This One

  for (int i = 0; i < 9; i++)
    LastRot[i] = ThisRot[i];

  mouseDown = false;
}


void arcBall_c::drag(float x, float y) {

  if (!mouseDown) return;

  //Map the point to the sphere
  mapToSphere(x, y, EnVec);
}


void arcBall_c::getDrag(float NewRot[4]) const
{
  float Perp[3];

  //Compute the vector perpendicular to the begin and end vectors
  Vector3fCross(StVec, EnVec, Perp);

  //Compute the length of the perpendicular vector
  if (Vector3fLength(Perp) > Epsilon) {   //if its non-zero

    //We're OK, so return the perpendicular vector as the transform after all
    NewRot[0] = Perp[0];
    NewRot[1] = Perp[1];
    NewRot[2] = Perp[2];

    //In the quaternion values, w is cosine (theta / 2), where theta is rotation angle
    NewRot[3]= Vector3fDot(StVec, EnVec);

  } else {                                   //if its zero

    //The begin and end vectors coincide, so return an identity transform
    NewRot[0] = NewRot[1] = NewRot[2] = NewRot[3] = 0.0f;
  }
}

void arcBall_c::setBounds(GLfloat NewWidth, GLfloat NewHeight) {


  if ((NewWidth > 1.0f) && (NewHeight > 1.0f)) {

    //Set adjustment factor for width/height
    AdjustWidth  = 1.0f / ((NewWidth  - 1.0f) * 0.5f);
    AdjustHeight = 1.0f / ((NewHeight - 1.0f) * 0.5f);

  } else {

    AdjustWidth  = 1.0f;
    AdjustHeight = 1.0f;

  }
}

void arcBall_c::addTransform(void) const {

  GLfloat Transform[16];

  Transform[ 0] = 1; Transform[ 1] = 0; Transform[ 2] = 0; Transform[ 3] = 0;
  Transform[ 4] = 0; Transform[ 5] = 1; Transform[ 6] = 0; Transform[ 7] = 0;
  Transform[ 8] = 0; Transform[ 9] = 0; Transform[10] = 1; Transform[11] = 0;
  Transform[12] = 0; Transform[13] = 0; Transform[14] = 0; Transform[15] = 1;

  if (mouseDown) {

    float ThisQuat[4];
    float ThisRot[9];

    getDrag(ThisQuat);                                              // Update End Vector And Get Rotation As Quaternion
    Matrix3fSetRotationFromQuat4f(ThisRot, ThisQuat);               // Convert Quaternion Into Matrix3fT
    Matrix3fMulMatrix3f(ThisRot, LastRot);                          // Accumulate Last Rotation Into This One
    Matrix4fSetRotationFromMatrix3f(Transform, ThisRot);            // Set Our Final Transformations Rotation From This One

  } else {

    Matrix4fSetRotationFromMatrix3f(Transform, LastRot);            // Set Our Final Transformations Rotation From This One
  }

  glMultMatrixf(Transform);                                       // NEW: Apply Dynamic Transform
}




method2_c::method2_c(float NewWidth, float NewHeight) : AdjustWidth(NewWidth), AdjustHeight(NewHeight), mouseDown(false)
{
  rotation[0] = 0;
  rotation[1] = 0;
  rotation[2] = 0;
  rotation[3] = 1;
}

void method2_c::setBounds(float NewWidth, float NewHeight)
{
  AdjustWidth = NewWidth;
  AdjustHeight = NewHeight;
}

void method2_c::click(float x, float y)
{
  last_x = x;
  last_y = y;

  mouseDown = true;
}

void method2_c::clack(float x, float y)
{
  drag(x, y);
  mouseDown = false;
}



void method2_c::spherePoint(float px, float py, float v[3])
{
  float screenmin;

  if (AdjustWidth > AdjustHeight)
  {
    screenmin = AdjustHeight;
  }
  else
  {
    screenmin = AdjustWidth;
  }

  v[0] = 2.0*(px-0.5*AdjustWidth)/screenmin;
  v[1] = 2.0*(0.5*AdjustHeight-py)/screenmin;
  float d = v[0]*v[0]+v[1]*v[1];

  if (d < 0.75)
  {
    v[2] = sqrt(1.0-d);
  }
  else if (d < 3.0)
  {
    d = sqrt(3) - sqrt(d);
    float t = 1.0f - d*d;
    if (t < 0)
      t = 0.0f;

    v[2]= 1.0f - sqrt(t);
  }
  else
  {
    v[2] = 0;
  }

  float l = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
  l = 1.0f/sqrt(l);

  v[0] *= l;
  v[1] *= l;
  v[2] *= l;
}




void method2_c::drag(float x, float y)
{
  if (!mouseDown) return;

  float v1[3], v2[3];

  spherePoint(last_x, last_y, v1);
  spherePoint(x, y, v2);

  float q[4];

  float dot = Vector3fDot(v1, v2);

  if(dot > 0.999999f)
  {
    q[0] = 0.0f;
    q[1] = 0.0f;
    q[2] = 0.0f;
    q[3] = 1.0f;
  }
  else if (dot < -0.999999f)
  {
    q[0] = 0.0f;
    q[1] = 0.0f;
    q[2] = -1.0f;
    q[3] = 0.0f;
  }
  else
  {
    float div = sqrtf((dot+1.0f)*2.0f);
    q[0] = (v1[1]*v2[2]-v1[2]*v2[1])/div;
    q[1] = (v1[2]*v2[0]-v1[0]*v2[2])/div;
    q[2] = (v1[0]*v2[1]-v1[1]*v2[0])/div;
    q[3] = div*0.5f;
  }

  float p[4];

  p[0] = q[3]*rotation[0]+q[0]*rotation[3]+q[1]*rotation[2]-q[2]*rotation[1];
  p[1] = q[3]*rotation[1]+q[1]*rotation[3]+q[2]*rotation[0]-q[0]*rotation[2];
  p[2] = q[3]*rotation[2]+q[2]*rotation[3]+q[0]*rotation[1]-q[1]*rotation[0];
  p[3] = q[3]*rotation[3]-q[0]*rotation[0]-q[1]*rotation[1]-q[2]*rotation[2];

  rotation[0] = p[0];
  rotation[1] = p[1];
  rotation[2] = p[2];
  rotation[3] = p[3];

  float t=rotation[0]*rotation[0]+rotation[1]*rotation[1]+rotation[2]*rotation[2]+rotation[3]*rotation[3];

  if (t > 0.0f)
  {
    float f = 1.0f/sqrtf(t);
    rotation[0] *= f;
    rotation[1] *= f;
    rotation[2] *= f;
    rotation[3] *= f;
  }

  last_x = x;
  last_y = y;
}

void method2_c::addTransform(void) const
{
  /* this is the original code, but as we always
   * multiply with the unit matrix, we can optimize
   *
  GLfloat r[3][3];

  GLfloat tx=2.0*rotation[0];
  GLfloat ty=2.0*rotation[1];
  GLfloat tz=2.0*rotation[2];
  GLfloat twx=tx*rotation[3];
  GLfloat twy=ty*rotation[3];
  GLfloat twz=tz*rotation[3];
  GLfloat txx=tx*rotation[0];
  GLfloat txy=ty*rotation[0];
  GLfloat txz=tz*rotation[0];
  GLfloat tyy=ty*rotation[1];
  GLfloat tyz=tz*rotation[1];
  GLfloat tzz=tz*rotation[2];

  r[0][0]=1.0-tyy-tzz;
  r[0][1]=txy+twz;
  r[0][2]=txz-twy;
  r[1][0]=txy-twz;
  r[1][1]=1.0-txx-tzz;
  r[1][2]=tyz+twx;
  r[2][0]=txz+twy;
  r[2][1]=tyz-twx;
  r[2][2]=1.0-txx-tyy;


  GLfloat m[16];

  m[ 0] = 1; m[ 1] = 0; m[ 2] = 0; m[ 3] = 0;
  m[ 4] = 0; m[ 5] = 1; m[ 6] = 0; m[ 7] = 0;
  m[ 8] = 0; m[ 9] = 0; m[10] = 1; m[11] = 0;
  m[12] = 0; m[13] = 0; m[14] = 0; m[15] = 1;

  x=m[0]; y=m[4]; z=m[8];
  m[ 0]=x*r[0][0]+y*r[0][1]+z*r[0][2];
  m[ 4]=x*r[1][0]+y*r[1][1]+z*r[1][2];
  m[ 8]=x*r[2][0]+y*r[2][1]+z*r[2][2];
  x=m[1]; y=m[5]; z=m[9];
  m[ 1]=x*r[0][0]+y*r[0][1]+z*r[0][2];
  m[ 5]=x*r[1][0]+y*r[1][1]+z*r[1][2];
  m[ 9]=x*r[2][0]+y*r[2][1]+z*r[2][2];
  x=m[2]; y=m[6]; z=m[10];
  m[ 2]=x*r[0][0]+y*r[0][1]+z*r[0][2];
  m[ 6]=x*r[1][0]+y*r[1][1]+z*r[1][2];
  m[10]=x*r[2][0]+y*r[2][1]+z*r[2][2];
  x=m[3]; y=m[7]; z=m[11];
  m[ 3]=x*r[0][0]+y*r[0][1]+z*r[0][2];
  m[ 7]=x*r[1][0]+y*r[1][1]+z*r[1][2];
  m[11]=x*r[2][0]+y*r[2][1]+z*r[2][2];
  */

  GLfloat tx = 2.0*rotation[0];
  GLfloat ty = 2.0*rotation[1];
  GLfloat tz = 2.0*rotation[2];
  GLfloat twx = tx*rotation[3];
  GLfloat twy = ty*rotation[3];
  GLfloat twz = tz*rotation[3];
  GLfloat txx = tx*rotation[0];
  GLfloat txy = ty*rotation[0];
  GLfloat txz = tz*rotation[0];
  GLfloat tyy = ty*rotation[1];
  GLfloat tyz = tz*rotation[1];
  GLfloat tzz = tz*rotation[2];

  GLfloat m[16];

  m[ 0] = 1.0-tyy-tzz;
  m[ 1] = txy+twz;
  m[ 2] = txz-twy;
  m[ 3] = 0;
  m[ 4] = txy-twz;
  m[ 5] = 1.0-txx-tzz;
  m[ 6] = tyz+twx;
  m[ 7] = 0;
  m[ 8] = txz+twy;
  m[ 9] = tyz-twx;
  m[10] = 1.0-txx-tyy;
  m[11] = 0;
  m[12] = 0;
  m[13] = 0;
  m[14] = 0;
  m[15] = 1;

  glMultMatrixf(m);
}

