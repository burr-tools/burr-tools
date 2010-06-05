#include "vector3.h"

#include "../lib/bt_assert.h"

using namespace std;

#define A_LITTLE 0.1
#define VERY_LITTLE 1e-4
#define ALMOST_ZERO 1e-9

// Aux. function that translates a value to a valid interval
double normalize(double min, double max, double value)
{
  double distance = max - min;
  if (distance <= 0)
  {
    cerr << __FILE__ << ": min > max in \"normalize\" function" << endl;
    return value;
  }

  while (value > max)
    value -= distance;

  while (value < min)
    value += distance;

  return value;
}



// Default constructor
template <class T> Vector3D<T>::Vector3D()
{
  for (unsigned i=0 ; i<3 ; i++)
    _data[i] = (T) 0.0;
}

// Constructor
template <class T> Vector3D<T>::Vector3D(const T& x, const T& y, const T& z)
{
  _data[0] = x;
  _data[1] = y;
  _data[2] = z;
}

// Constructor
template <class T> Vector3D<T>::Vector3D(const T data[3])
{
  for (unsigned i=0 ; i<3 ; i++)
    _data[i] = data[i];
}

// Copy constructor
template <class T> Vector3D<T>::Vector3D(const Vector3D<T>& v)
{
  for (unsigned i=0 ; i<3 ; i++)
    _data[i] = v._data[i];
}

// Constructor
template <class T> Vector3D<T>::Vector3D(const Vector3D<T>* v)
{
  for (unsigned i=0 ; i<3 ; i++)
    _data[i] = v->_data[i];
}


// Simple global "set" method
template <class T> void Vector3D<T>::set(const T&x, const T& y, const T& z)
{
  _data[0] = x;
  _data[1] = y;
  _data[2] = z;
}


// Simple global "set" method
template <class T> void Vector3D<T>::set(const T data[3])
{
  for (unsigned i=0 ; i<3 ; i++)
    _data[i] = data[i];
}


// Copy operator
template <class T> Vector3D<T>& Vector3D<T>::operator=(const Vector3D<T>& src)
{
  for (unsigned i=0 ; i<3 ; i++)
    _data[i] = src._data[i];

  return *this;
}

// Comparison operator
template <class T> bool Vector3D<T>::operator==(const Vector3D<T>& src) const
{
  for (unsigned i=0 ; i<3 ; i++)
    if (fabs(_data[i] - src._data[i]) > ALMOST_ZERO)
      return false;

  return true;
}

// Comparison operator, admittedly arbitrary, yet necessary for
// using the class in many STL algorithms and containers.
template <class T> bool Vector3D<T>::operator<(const Vector3D<T>& src) const
{
  for (unsigned i=0 ; i<3 ; i++)
  {
    if (_data[i] < src._data[i])
      return true;
    if (_data[i] > src._data[i])
      return false;
  }

  return false;
}

// Normalizes a vector (sets the module to 1)
template <class T> void Vector3D<T>::normalize()
{
  double mod = module();
  *this /= mod;
}

// Sets the length of a vector
template <class T> void Vector3D<T>::setLength(T len)
{
  double mod = module();
  *this *= len/mod;
}


// Returns the square of the module of the vector.
// Equivalent to module(), faster when only relative comparisons are needed.
template <class T> double Vector3D<T>::squaredModule() const
{
  T sum(0);
  for (unsigned i=0 ; i<3 ; i++)
    sum += _data[i]*_data[i];

  return sum;
}


// Accesses an element of the vector
template <class T> T& Vector3D<T>::operator[](int index)
{
  return _data[index];
}

// Accesses an element of the vector
template <class T> const T& Vector3D<T>::operator[](int index) const
{
  return _data[index];
}

// Calculates the angle between 2 vectors
template <class T> double Vector3D<T>::angle(const Vector3D<T>& v) const
{
  Vector3D<T> local(*this);
  Vector3D<T> param(v);

  double m = module();
  double mv = v.module();
  bt_assert(!isnan(m));
  bt_assert(!isnan(mv));
  bt_assert(!isnan(m*mv));

  //   if ( mod < ALMOST_ZERO )
  //     return 0.0;
  local /= m;
  param /= mv;

  double dp = local*param;
  bt_assert(!isnan(dp));

  // Clamp the value to [-1,1], for sanity
  if (dp > 1.0)
    dp = 1.0;
  else if (dp < -1.0)
    dp = -1.0;

  double result = acos(dp);
  bt_assert(!isnan(result));
  return result;
}


// Calculates the square of the distance between 2 vectors.
// Faster than ::distance, for when only comparisons are needed.
template <class T> double Vector3D<T>::squaredDistance(const Vector3D<T>& v) const
{
  T sum(0);
  for (unsigned i=0 ; i<3 ; i++)
  {
    T diff = _data[i] - v._data[i];
    sum += diff * diff;
  }

  return sum;
}

// Computes the infinite-degree distance to 'v'
template <class T> double Vector3D<T>::infDistance(const Vector3D<T>& v) const
{
  T soFar(0);
  for (unsigned i=0 ; i<3 ; i++)
  {
    T diff = fabs(_data[i] - v._data[i]);
    if (diff > soFar)
      soFar = diff;
  }

  return soFar;
}

// Computes the degree-1 (Manhattan) distance to 'v'
template <class T> double Vector3D<T>::manhattanDistance(const Vector3D<T>& v) const
{
  T sum(0);
  for (unsigned i=0 ; i<3 ; i++)
    sum += fabs(_data[i] - v._data[i]);

  return sum;
}


// Calculates the distance between a point in space and a plane represented
// as a point on that plane and the plane normal.
// This distance is signed, as it considers the positive and negative sides of the plane.
template <class T> double Vector3D<T>::distanceToPlane(const Vector3D<T>& P, const Vector3D<T>& N) const
{
  bt_assert( fabs(N.squaredModule() - 1) < ALMOST_ZERO );

  return (P-*this) * N;
}


// Rotates a vector around the X axis
template <class T> void Vector3D<T>::rotateX(double angle)
{
  double y = _data[1];
  double z = _data[2];
  double cosine = cos(angle);
  double sine = sin(angle);

  _data[1] = (T) (y*cosine - z*sine);
  _data[2] = (T) (y*sine + z*cosine);
}


// Rotates a vector around the Y axis
template <class T> void Vector3D<T>::rotateY(double angle)
{
  double x = _data[0];
  double z = _data[2];
  double cosine = cos(angle);
  double sine = sin(angle);

  _data[2] = (T) (z*cosine - x*sine);
  _data[0] = (T) (z*sine + x*cosine);
}

// Rotates a vector around the Z axis
template <class T> void Vector3D<T>::rotateZ(double angle)
{
  double x = _data[0];
  double y = _data[1];
  double cosine = cos(angle);
  double sine = sin(angle);

  _data[0] = (T) (x*cosine - y*sine);
  _data[1] = (T) (x*sine + y*cosine);
}

template <class T> Vector3D<T> Vector3D<T>::operator*(const Vector3D<T>* mat) const
{
  Vector3D<T> V;
  for (unsigned i=0 ; i<3 ; i++)
  {
    V[i] = 0;
    for (unsigned j=0 ; j<3 ; j++)
      V[i] += mat[i][j];
  }
  return V;
}


template <class T> void Vector3D<T>::transpose(Vector3D<T>* mat)
{
  for (unsigned i=0 ; i<3 ; i++)
    for (unsigned j=i+1 ; j<3 ; j++)
      swap(mat[i][j], mat[j][i]);
}


// Calculates the dot product of 2 vectors
template <class T> T Vector3D<T>::operator*(const Vector3D<T>& v) const
{
  T sum(0);
  for (unsigned i=0 ; i<3 ; i++)
    sum += _data[i] * v._data[i];

  return sum;
}

// Divides a vector by a scalar factor
template <class T> Vector3D<T> Vector3D<T>::operator/(T factor) const
{
  return Vector3D<T>(_data[0]/factor, _data[1]/factor, _data[2]/factor);
}

// Multiplies a vector by a scalar factor and assigns the result to itself
template <class T> const Vector3D<T>& Vector3D<T>::operator/=(T factor)
{
  for (unsigned i=0 ; i<3 ; i++)
    _data[i] /= factor;

  return *this;
}

// Multiplies a vector by a scalar factor
template <class T> Vector3D<T> Vector3D<T>::operator*(T factor) const
{
  return Vector3D<T>(_data[0]*factor, _data[1]*factor, _data[2]*factor);
}

// Multiplies a vector by a scalar factor and assigns the result to itself
template <class T> const Vector3D<T>& Vector3D<T>::operator*=(T factor)
{
  for (unsigned i=0 ; i<3 ; i++)
    _data[i] *= factor;

  return *this;
}

// Returns the cross product of 2 vectors
template <class T> Vector3D<T> Vector3D<T>::operator^(const Vector3D<T>& v) const
{
  return Vector3D<T>( (_data[1] * v._data[2]) - (v._data[1] * _data[2]),
      (_data[2] * v._data[0]) - (v._data[2] * _data[0]),
      (_data[0] * v._data[1]) - (v._data[0] * _data[1]) );
}


// Returns the difference vector between 2 vectors
template <class T> Vector3D<T> Vector3D<T>::operator-(const Vector3D<T>& v) const
{
  return Vector3D<T>( _data[0] - v._data[0],
      _data[1] - v._data[1],
      _data[2] - v._data[2] );
}


// Substracts one vector to another
template <class T> const Vector3D<T>& Vector3D<T>::operator-=(const Vector3D<T>& v)
{
  for (unsigned i=0 ; i<3 ; i++)
    _data[i] -= v._data[i];

  return *this;
}


// Returns a vector summation
template <class T> Vector3D<T> Vector3D<T>::operator+(const Vector3D<T>& v) const
{
  return Vector3D<T>( _data[0] + v._data[0],
      _data[1] + v._data[1],
      _data[2] + v._data[2] );
}


// Adds a vector to another one
template <class T> const Vector3D<T>& Vector3D<T>::operator+=(const Vector3D<T>& v)
{
  for (unsigned i=0 ; i<3 ; i++)
    _data[i] += v._data[i];

  return *this;
}

// Calculates the spherical coordinates for the vector, except the distance
template <class T> void Vector3D<T>::getSpheric(double& theta, double& phi) const
{
  double distance = sqrt( x()*x() + y()*y() + z()*z() );
  bt_assert(distance != 0.0);

  theta = atan2(y(), x());
  phi = acos(z() / distance);

  //	theta = ::normalize(-M_PI, M_PI, theta);
  //	phi = ::normalize(0, M_PI, phi);
}

// Calculates the spherical coordinates for the vector
template <class T> void Vector3D<T>::getSpheric(double& distance, double& theta, double& phi) const
{
  distance = sqrt( x()*x() + y()*y() + z()*z() );
  theta = atan2(y(), x());
  phi = acos(z() / distance);

  bt_assert( (theta >= -M_PI) && (theta <= M_PI) );
  bt_assert( (phi >= 0) && (phi <= M_PI) );
}

// Sets the values of the vector given its spheric coordinates
template <class T> void Vector3D<T>::setSpheric(double distance, double theta, double phi)
{
  double sinPhi = sin(phi);

  x( (T) (distance * cos(theta) * sinPhi) );
  y( (T) (distance * sin(theta) * sinPhi) );
  z( (T) (distance * cos(phi)) );
}


// Projects the vector onto plane X
template <class T> Vector3D<T> Vector3D<T>::projectX() const
{
  return Vector3D<T>(0, _data[1], _data[2]);
}

// Projects the vector onto plane Y
template <class T> Vector3D<T> Vector3D<T>::projectY() const
{
  return Vector3D<T>(_data[0], 0, _data[2]);
}

// Projects the vector onto plane Z
template <class T> Vector3D<T> Vector3D<T>::projectZ() const
{
  return Vector3D<T>(_data[0], _data[1], 0);
}

// Projects the vector onto a plane crossing the origin, with normal v.
// Precondition: n is assumed to be a unit vector.
template <class T> Vector3D<T> Vector3D<T>::project(const Vector3D<T>& n) const
{
  return (*this) - n * dotProduct(n);
}


/**
 * Projects the Vector3D onto the plane normal to 'n' that intersects the origin, along direction 'v'.
 * @param n Normal to the projection plane.
 * @param v Direction along which the Vector3D will be projected.
 * @return The projected Vector3D.
 */
template <class T> Vector3D<T> Vector3D<T>::projectAlong(const Vector3D<T>& n, const Vector3D<T>& v) const
{
  return intersect(Vector3D<T>(0, 0, 0), n, v);
}


/// Project the Vector3D<T> onto a plane defined by a center and normal.
/// The vector is supposed to be origined at (0,0,0).
/// @see <http://members.tripod.com/~Paul_Kirby/vector/Vplanelineint.html> for details.
/// @note (a=0, b=*this, c=center, n=normal).
template <class T> Vector3D<T>
Vector3D<T>::project(const Vector3D<T>& center, const Vector3D<T>& normal) const
{
#if 0
  return (*this) * ((center*normal) / ((*this)*normal));
#else
  float t = (center-*this) * normal;
  return *this + normal*t;
#endif
}



template <class T> double
Vector3D<T>::squaredSignedDistanceToPlane(const Vector3D<T>& center, const Vector3D<T>& normal) const
{
  return normal * (*this - center);
}


/**
 * Computes the closest point in the line that passes through P along direction V.
 * @param P An arbitrary point on the line.
 * @param V Direction of the line.
 * @return Closest point on the line.
 */
template <class T> Vector3D<T>
Vector3D<T>::closestPointInLine(const Vector3D<T>& P, const Vector3D<T>& V) const
{
  bt_assert(V.squaredModule() > A_LITTLE);
  const Vector3Df U(*this-P);
  const float d = (V*U);
  const Vector3Df result(P + V*d);
  return result;
}

/**
 * Compute the intersection of plane [center,nrm] and the line [this,direction].
 * @param center A point on the plane to be intersected.
 * @param nrm Normal of the plane to be intersected.
 * @param direction Direction of the line to be intersected.
 * @see http://members.tripod.com/~Paul_Kirby/vector/Vplanelineint.html
 * @return The position of the intersection.
 */
template <class T> Vector3D<T>
Vector3D<T>::intersect(const Vector3D<T>& center, const Vector3D<T>& nrm, const Vector3D<T>& direction) const
{
  bt_assert(fabs(direction*nrm) > VERY_LITTLE);
  float t = ((center-*this) * nrm) / (direction*nrm);
  return *this + direction*t;
}


// Project the Vector3D<T> onto a plane defined by a center and normal
// The vector is supposed to be origined at (0,0,0)
// Check <http://members.tripod.com/~Paul_Kirby/vector/Vplanelineint.html>
// for details (a=0, b=*this, c=center, n=normal).
template <class T> Vector3D<T>
Vector3D<T>::intersect(const Vector3D<T>& center, const Vector3D<T>& normal) const
{
  return (*this) * ((center*normal) / ((*this)*normal));
}



// Returns a unit vector which is perpendicular to *this
template <class T> Vector3D<T>
Vector3D<T>::perpendicular() const
{
  static const Vector3D<T> X(1, 0, 0);
  static const Vector3D<T> Y(0, 1, 0);

  const Vector3D<T>& base = (fabs((*this) * X) <  fabs((*this) * Y) ? X : Y);
  Vector3D<T> result = base ^ (*this);
  result.normalize();

  return result;
}

//extern bool debugCrap;

/**
 * Finds the 2D affine coordinates of a point in a plane embedded in 3D.
 * @param A First affine axis.
 * @param B Second affine axis.
 * @return The computed coordinates.
 */
template <class T> Vector3D<T>
Vector3D<T>::affineCoordinates(const Vector3D<T>& A, const Vector3D<T>& B) const
{
  bt_assert(A.angle(B) > VERY_LITTLE);
  Vector3D<T> Z(A^B);
  Z.normalize();

  Vector3D<T> Na = A^Z;
  Na.normalize();
  Vector3D<T> Pa = projectAlong(Na, B); // P''
  float u = Pa.module() / A.module() * Pa.sign(A);

  Vector3D<T> Nb = B^Z;
  Nb.normalize();
  Vector3D<T> Pb = projectAlong(Nb, A); // P'
  float v = Pb.module() / B.module() * Pb.sign(B);

  return Vector3D<T>(u, v, 0);
}


// Instantiation of templates, needed by gcc
// The fewer of these we compile, the faster the compilation
//template class Vector3D<double>;
template class Vector3D<float>;
//template class Vector3D<int>;
//template class Vector3D<short>;
//template class Vector3D<char>;

