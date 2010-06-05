#ifndef __VECTOR3D_H__
#define __VECTOR3D_H__

#include <cmath>
#include <iostream>

// forward declarations
template <class T> class Vector3D;

template <class T> Vector3D<T> operator* (T factor, const Vector3D<T>& v);

// Vector3D class
template <class T> class Vector3D
{
  public:
    Vector3D(const Vector3D<T>& v);
    explicit Vector3D();
    explicit Vector3D(const T& x, const T& y, const T& z);
    explicit Vector3D(const T data[3]);
    explicit Vector3D(const Vector3D<T>* v);

    static unsigned dim() { return 3; }

    void set(const T&x, const T& y, const T& z);
    void set(const T data[3]);

    Vector3D<T>& operator=(const Vector3D<T>& src);
    bool operator==(const Vector3D<T>& src) const;
    bool operator!=(const Vector3D<T>& src) const { return ! (*this == src); }
    bool operator<(const Vector3D<T>& src) const;

    void getSpheric(double& theta, double& phi) const;
    void getSpheric(double& distance, double& theta, double& phi) const;
    void setSpheric(double distance, double theta, double phi);
    Vector3D<T> affineCoordinates(const Vector3D<T>& u, const Vector3D<T>& v) const;

    const T& x() const { return _data[0]; };
    const T& y() const { return _data[1]; };
    const T& z() const { return _data[2]; };
    void x(const T& t) { _data[0] = t; };
    void y(const T& t) { _data[1] = t; };
    void z(const T& t) { _data[2] = t; };

    T& operator[](int index);
    const T& operator[](int index) const;
    const T* getData() const {return _data;};
    T* getDataRef() {return _data;};

    void normalize();
    void setLength(T len);
    T sign(const Vector3D<T>& v) const { return ((*this)*v>=0) ? 1 : -1; }
    double module() const { return sqrt(squaredModule()); }
    double squaredModule() const;
    double angle(const Vector3D<T>& v) const;
    double distance(const Vector3D<T>& v) const { return sqrt(squaredDistance(v)); }
    double squaredDistance(const Vector3D<T>& v) const;
    double infDistance(const Vector3D<T>& v) const;
    double manhattanDistance(const Vector3D<T>& v) const;
    double distanceToPlane(const Vector3D<T>& P, const Vector3D<T>& N) const;

    void rotateX(double angle);
    void rotateY(double angle);
    void rotateZ(double angle);

    T operator*(const Vector3D<T>& v) const;
    T dotProduct(const Vector3D<T>& v) const { return (*this) * v; }

    Vector3D<T> operator/(T factor) const;
    const Vector3D<T>& operator/=(T factor);

    Vector3D<T> operator*(T factor) const;
    const Vector3D<T>& operator*=(T factor);

    Vector3D<T> operator^(const Vector3D<T>& v) const;
    Vector3D<T> crossProduct(const Vector3D<T>& v) const{ return (*this)^v; }

    Vector3D<T> operator*(const Vector3D<T>* mat) const;
    static void transpose(Vector3D<T>* mat);

    Vector3D<T> operator-(const Vector3D<T>& v) const;
    Vector3D<T> difference(const Vector3D<T>& v) const{ return (*this) - v; }
    const Vector3D<T>& operator-=(const Vector3D<T>& v);
    const Vector3D<T>& substract(const Vector3D<T>& v) { return (*this) -= v; }

    Vector3D<T> projectX() const;
    Vector3D<T> projectY() const;
    Vector3D<T> projectZ() const;
    Vector3D<T> project(const Vector3D<T>& n) const;
    Vector3D<T> projectAlong(const Vector3D<T>& n, const Vector3D<T>& v) const;
    Vector3D<T> project(const Vector3D<T>& center, const Vector3D<T>& normal) const;
    Vector3D<T> closestPointInLine(const Vector3D<T>& P, const Vector3D<T>& V) const;
    Vector3D<T> intersect(const Vector3D<T>& center, const Vector3D<T>& normal) const;
    Vector3D<T> intersect(const Vector3D<T>& center, const Vector3D<T>& normal, const Vector3D<T>& direction) const;
    Vector3D<T> perpendicular() const;
    double squaredSignedDistanceToPlane(const Vector3D<T>& center, const Vector3D<T>& normal) const;

    Vector3D<T> operator+(const Vector3D<T>& v) const;
    Vector3D<T> sum(const Vector3D<T>& v) const { return (*this) + v; }
    const Vector3D<T>& operator+=(const Vector3D<T>& v);
    const Vector3D<T>& add(const Vector3D<T>& v){ return (*this) += v; }

  private:
    T _data[3];
};

// Needed operator* that I don't know how to move to the cpp
template <class T> Vector3D<T> operator* (T factor, const Vector3D<T>& v)
{
  return v * factor;
}

// Needed operator* that I don't know how to move to the cpp
template <class T> Vector3D<T> operator- (const Vector3D<T>& v)
{
  return Vector3D<T>(-v.x(), -v.y(), -v.z());
}

typedef Vector3D<int> Vector3Di;
typedef Vector3D<float> Vector3Df;
typedef Vector3D<double> Vector3Dd;
typedef Vector3D<short> Vector3Ds;
typedef Vector3D<char> Vector3Dc;


#endif
