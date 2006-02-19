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
#include "ArcBall.h"

#include <math.h>
#include <assert.h>

#define Epsilon 1.0e-5

/**
 * Sets the vector c to be the vector cross product of vectors a and b.
 * don't do Vector3fCross(a, a, b);
 */
static void Vector3fCross(const GLfloat a[3], const GLfloat b[3], GLfloat c[3])
{
  c[0] = a[1]*b[2] - a[2]*b[1];
  c[1] = a[2]*b[0] - a[0]*b[2];
  c[2] = a[0]*b[1] - a[1]*b[0];
}

/**
 * Computes the dot product of the vector a and b
 */
static GLfloat Vector3fDot(const GLfloat a[3], const GLfloat b[3])
{
  return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

/**
 * Returns the length of this vector.
 * @return the length of this vector
 */
static GLfloat Vector3fLength(const GLfloat a[3])
{
  return sqrt(a[0]*a[0] + a[1]*a[1] + a[2]*a[2]);
}

/**
 * Sets the value of this matrix to the matrix conversion of the
 * quaternion argument.
 * @param q1 the quaternion to be converted
 */
static void Matrix3fSetRotationFromQuat4f(GLfloat m[9], const GLfloat q1[4])
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
static void Matrix3fMulMatrix3f(GLfloat a[9], const GLfloat b[9])
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
 * @param rot4 the rotation factor(Matrix4) only upper 3x3 elements are changed. if null, ignored
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
static void Matrix4fSetRotationFromMatrix3f(GLfloat a[16], const GLfloat m[9])
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
void ArcBall_c::mapToSphere(GLfloat x, GLfloat y, GLfloat NewVec[3]) const
{
  GLfloat TempPt[2];

  //Adjust point coords and scale down to range of [-1 ... 1]
  TempPt[0] =        (x * AdjustWidth)  - 1.0f;
  TempPt[1] = 1.0f - (y * AdjustHeight);

  //Compute the square of the length of the vector to the point from the center
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
ArcBall_c::ArcBall_c(GLfloat NewWidth, GLfloat NewHeight) {

  LastRot[0] = 1;  LastRot[1] = 0;  LastRot[2] = 0;
  LastRot[3] = 0;  LastRot[4] = 1;  LastRot[5] = 0;
  LastRot[6] = 0;  LastRot[7] = 0;  LastRot[8] = 1;

  //Set initial bounds
  setBounds(NewWidth, NewHeight);

  mouseDown = false;
}

void ArcBall_c::click(GLfloat x, GLfloat y) {

  //Map the point to the sphere
  mapToSphere(x, y, StVec);
  mapToSphere(x, y, EnVec);

  mouseDown = true;
}

void ArcBall_c::clack(GLfloat x, GLfloat y) {

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


void ArcBall_c::drag(GLfloat x, GLfloat y) {

  //Map the point to the sphere
  mapToSphere(x, y, EnVec);
}


void ArcBall_c::getDrag(GLfloat NewRot[4]) const
{
  GLfloat Perp[3];

  //Compute the vector perpendicular to the begin and end vectors
  Vector3fCross(StVec, EnVec, Perp);

  //Compute the length of the perpendicular vector
  if (Vector3fLength(Perp) > Epsilon) {   //if its non-zero

    //We're ok, so return the perpendicular vector as the transform after all
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

void ArcBall_c::setBounds(GLfloat NewWidth, GLfloat NewHeight) {


  if ((NewWidth > 1.0f) && (NewHeight > 1.0f)) {

    //Set adjustment factor for width/height
    AdjustWidth  = 1.0f / ((NewWidth  - 1.0f) * 0.5f);
    AdjustHeight = 1.0f / ((NewHeight - 1.0f) * 0.5f);

  } else {

    AdjustWidth  = 1.0f;
    AdjustHeight = 1.0f;

  }
}

void ArcBall_c::addTransform(void) const {

  GLfloat Transform[16];

  Transform[ 0] = 1; Transform[ 1] = 0; Transform[ 2] = 0; Transform[ 3] = 0;
  Transform[ 4] = 0; Transform[ 5] = 1; Transform[ 6] = 0; Transform[ 7] = 0;
  Transform[ 8] = 0; Transform[ 9] = 0; Transform[10] = 1; Transform[11] = 0;
  Transform[12] = 0; Transform[13] = 0; Transform[14] = 0; Transform[15] = 1;

  if (mouseDown) {

    GLfloat ThisQuat[4];
    GLfloat ThisRot[9];

    getDrag(ThisQuat);                                              // Update End Vector And Get Rotation As Quaternion
    Matrix3fSetRotationFromQuat4f(ThisRot, ThisQuat);               // Convert Quaternion Into Matrix3fT
    Matrix3fMulMatrix3f(ThisRot, LastRot);                          // Accumulate Last Rotation Into This One
    Matrix4fSetRotationFromMatrix3f(Transform, ThisRot);            // Set Our Final Transform's Rotation From This One
    glMultMatrixf(Transform);                                       // NEW: Apply Dynamic Transform

  } else {

    Matrix4fSetRotationFromMatrix3f(Transform, LastRot);            // Set Our Final Transform's Rotation From This One
    glMultMatrixf(Transform);                                       // NEW: Apply Dynamic Transform
  }
}
