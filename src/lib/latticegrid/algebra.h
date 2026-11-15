/* BurrTools
 *
 * Ported from BurrTools NG by Andreas Röver (https://gitlab.com/roever/burrtools),
 * src/maths/algebra.h at commit fdae7951, for grids as data
 * (design/2026-11-19-ng-merge-plan.md, step G1). NG is licensed under the
 * GNU General Public License version 3, and so is this file.
 *
 * Changes from NG's file: pssst::strong replaced by a plain DegreeInt with the same operators; intdiv_inf from src/tools/integer_overflow.h moved in; iterate3d returns a std::vector in place of a std::generator (C++23).
 * Keep the rest identical to NG's file, so changes can be exchanged both ways.
 */
#pragma once

#include <array>
#include <functional>
#include <numbers>
#include <numeric>
#include <optional>
#include <stdexcept>

#include <cassert>
#include <cmath>

#include <type_traits>
#include <vector>

namespace BurrTools::Maths {

// integer division rounding towards minus infinity even for negative values
// (NG: src/tools/integer_overflow.h)
template <class T>
constexpr T intdiv_inf(T x, T y)
{
    return x / y - ((x % y && (x % y ^ y) < 0) ? 1 : 0);
}

// strong type that represents and angle in degree with whole numbers
// (NG uses pssst::strong with Modulo, Order, UMinus, Add, Sub; this is the
// same set of operations without the library)
struct DegreeInt
{
        int value;

        friend constexpr auto operator<=>(const DegreeInt &, const DegreeInt &) = default;
        friend constexpr DegreeInt operator-(DegreeInt a) noexcept { return DegreeInt{-a.value}; }
        friend constexpr DegreeInt operator+(DegreeInt a, DegreeInt b) noexcept { return DegreeInt{a.value + b.value}; }
        friend constexpr DegreeInt operator-(DegreeInt a, DegreeInt b) noexcept { return DegreeInt{a.value - b.value}; }
        friend constexpr DegreeInt operator%(DegreeInt a, DegreeInt b) noexcept { return DegreeInt{a.value % b.value}; }
        constexpr DegreeInt & operator+=(DegreeInt b) noexcept { value += b.value; return *this; }
        constexpr DegreeInt & operator-=(DegreeInt b) noexcept { value -= b.value; return *this; }
        constexpr DegreeInt & operator%=(DegreeInt b) noexcept { value %= b.value; return *this; }
};

constexpr float degreeToRad(const DegreeInt d) { return static_cast<float>(d.value * std::numbers::pi / 180.0); }

// TODO this is not completely consistent, sometimes we can use the operator* sometimes we have to use rotate
// e.g. on quaternions * can mean the scalar product or the multiplication of the 2 quaternions... up till then
// we didn't need both in one type,

// class for rational numbers
template <std::signed_integral T>
class RNumber
{
    private:
        T numerator_;
        T denominator_; // not zero, invariant

    public:
        constexpr explicit RNumber(T numerator = 0, T denominator = 1) : numerator_(numerator), denominator_(denominator)
        {
            if (denominator == 0) throw std::runtime_error("denominator must not be 0");
        }

        [[nodiscard]] constexpr const T & numerator() const noexcept { return numerator_; }

        [[nodiscard]] constexpr const T & denominator() const noexcept { return denominator_; }

        [[nodiscard]] constexpr explicit operator float() const noexcept { return 1.0f * numerator_ / denominator_; }

        /// returns remainder of the rational number
        /// ATTENTION, this is by default only really usable for a check if the rational
        /// is a whole numbers. If you need the proper modulo for integer division
        /// that rounds towards minus infinity you need to provide the proper operator
        template <class D = std::modulus<T>>
        [[nodiscard]] constexpr auto getFraction(D d = std::modulus<T>()) const noexcept
        {
            return RNumber{d(numerator_, denominator_), denominator_};
        }

        /// returns the whole part of the rational
        /// whole meaning here the next smaller (towards minus infinity)
        /// value division is used
        /// if you don't need that slight overhead (e.g. you know that your number
        /// divide without remainder or are all positive, you can use std::divides() as argument for d
        /// to get the normal division which is slightly cheaper
        template <class D = decltype(intdiv_inf<T>)>
        [[nodiscard]] constexpr T getInteger(D d = intdiv_inf<T>) const noexcept
        {
            return d(numerator_, denominator_);
        }

        // return the whole part and the fractional part of the rational number as a pair
        template <class D = std::divides<T>>
        [[nodiscard]] constexpr std::pair<T, RNumber> getWholeFraction(D d = std::divides<T>()) const noexcept
        {
            auto quot = d(numerator_, denominator_);
            return std::pair<T, RNumber>(quot, RNumber(numerator_ - quot * denominator_, denominator_));
        }
};

template <std::signed_integral T>
class RVector;

// 3d vector, floating and integer values work
template <class T>
class Vector
{
    private:
        std::array<T, 3> values_;

    public:
        using value_type = T;

        constexpr Vector() : values_{0, 0, 0} {}

        constexpr Vector(T x, T y, T z) : values_{x, y, z} {}

        template <class S>
        constexpr Vector(const Vector<S> & a) : values_{static_cast<T>(a.x()), static_cast<T>(a.y()), static_cast<T>(a.z())}
        {
        }

        template <std::signed_integral S>
            requires(std::floating_point<T>)
        constexpr explicit Vector(const RVector<S> & a)
            : values_{1.0f * a.x() / a.denominator(), 1.0f * a.y() / a.denominator(), 1.0f * a.z() / a.denominator()}
        {
        }

        [[nodiscard]] constexpr T & x() noexcept { return values_[0]; }

        [[nodiscard]] constexpr T & y() noexcept { return values_[1]; }

        [[nodiscard]] constexpr T & z() noexcept { return values_[2]; }

        [[nodiscard]] constexpr const T & x() const noexcept { return values_[0]; }

        [[nodiscard]] constexpr const T & y() const noexcept { return values_[1]; }

        [[nodiscard]] constexpr const T & z() const noexcept { return values_[2]; }

        [[nodiscard]] constexpr const T & operator[](int i) const { return values_[i]; }

        [[nodiscard]] constexpr bool isZero() const noexcept { return values_[0] == 0 && values_[1] == 0 && values_[2] == 0; }

        constexpr Vector & operator*=(auto op) noexcept
        {
            values_[0] *= op;
            values_[1] *= op;
            values_[2] *= op;

            return *this;
        }

        constexpr Vector & operator-=(const Vector & r) noexcept
        {
            values_[0] -= r.values_[0];
            values_[1] -= r.values_[1];
            values_[2] -= r.values_[2];

            return *this;
        }

        constexpr Vector & operator+=(const Vector & r) noexcept
        {
            values_[0] += r.values_[0];
            values_[1] += r.values_[1];
            values_[2] += r.values_[2];

            return *this;
        }

        [[nodiscard]] friend constexpr Vector operator-(const Vector & op) noexcept { return Vector(-op.values_[0], -op.values_[1], -op.values_[2]); }

        constexpr Vector & operator/=(T op) noexcept
        {
            values_[0] /= op;
            values_[1] /= op;
            values_[2] /= op;
            return *this;
        }
};

// 2d vector, floating and integer values work
template <class T>
class Vector2d
{
    private:
        std::array<T, 2> values_;

    public:
        using value_type = T;

        constexpr Vector2d() : values_{0, 0} {}

        constexpr Vector2d(T x, T y) : values_{x, y} {}

        template <class S>
        constexpr Vector2d(const Vector2d<S> & a) : values_{static_cast<T>(a.x()), static_cast<T>(a.y())}
        {
        }

        [[nodiscard]] constexpr const T & x() const noexcept { return values_[0]; }

        [[nodiscard]] constexpr const T & y() const noexcept { return values_[1]; }

        [[nodiscard]] constexpr T & x() noexcept { return values_[0]; }

        [[nodiscard]] constexpr T & y() noexcept { return values_[1]; }

        [[nodiscard]] constexpr T operator[](int i) const { return values_[i]; }

        template <class D>
        constexpr Vector2d & operator*=(D op) noexcept
            requires(std::floating_point<T> && std::floating_point<D>) || (std::signed_integral<T> && std::signed_integral<D>)
        {
            values_[0] *= op;
            values_[1] *= op;

            return *this;
        }

        constexpr Vector2d & operator-=(const Vector2d & r) noexcept
        {
            values_[0] -= r.values_[0];
            values_[1] -= r.values_[1];

            return *this;
        }

        constexpr Vector2d & operator+=(const Vector2d & r) noexcept
        {
            values_[0] += r.values_[0];
            values_[1] += r.values_[1];

            return *this;
        }

        [[nodiscard]] friend constexpr Vector2d operator-(const Vector2d & op) noexcept { return Vector2d(-op.values_[0], -op.values_[1]); }

        constexpr Vector2d & operator/=(T op) noexcept
        {
            values_[0] /= op;
            values_[1] /= op;

            return *this;
        }
};

/// a 3d vector class using rational numbers, the denominator is identical
/// in all 3 components
/// all function will make sure that the denominator is not getting 0
/// many of these operations do not reduce the fraction, so if you perform
/// many operations one after another you might end up with big number
/// in your variable, call reduce to get them down
template <std::signed_integral T>
class RVector
{
    private:
        std::array<T, 4> values_;

    public:
        explicit constexpr RVector(T x = 0, T y = 0, T z = 0, T w = 1) : values_{x, y, z, w}
        {
            if (w == 0) throw std::runtime_error("w in RVector must not be 0");
        }

        template <std::signed_integral S>
        constexpr RVector(const RVector<S> & orig)
            : values_{static_cast<T>(orig.x()), static_cast<T>(orig.y()), static_cast<T>(orig.z()), static_cast<T>(orig.denominator())}
        {
        }

        template <std::signed_integral S>
        explicit constexpr RVector(const Vector<S> & orig, T w = 1) : values_{orig.x(), orig.y(), orig.z(), w}
        {
        }

        [[nodiscard]] constexpr const T & x() const noexcept { return values_[0]; }

        [[nodiscard]] constexpr const T & y() const noexcept { return values_[1]; }

        [[nodiscard]] constexpr const T & z() const noexcept { return values_[2]; }

        [[nodiscard]] constexpr const T & denominator() const noexcept
        {
            assert(values_[3] > 0);
            return values_[3];
        }

        /// returns a vector containing just the fractional part of the vector
        /// ATTENTION, this is by default only really usable for a check if the components
        /// of this vector are whole numbers. If you need the proper modulo for integer division
        /// that rounds towards minus infinity you need to provide the proper operator
        template <class D = std::modulus<T>>
        [[nodiscard]] constexpr RVector getFraction(D d = std::modulus<T>()) const noexcept
        {
            return RVector(d(values_[0], values_[3]), d(values_[1], values_[3]), d(values_[2], values_[3]), values_[3]);
        }

        /// returns the whole part of the vector
        /// whole meaning here the next smaller (towards minus infinity)
        /// value division is used
        /// if you don't need that slight overhead (e.g. you know that your number
        /// divide without remainder or are all positive, you can use std::divides() as argument for d
        /// to get the normal division which is slightly cheaper
        template <class D = decltype(intdiv_inf<T>)>
        [[nodiscard]] constexpr Vector<T> getInteger(D d = intdiv_inf<T>) const noexcept
        {
            return Vector<T>(d(values_[0], values_[3]), d(values_[1], values_[3]), d(values_[2], values_[3]));
        }

        // get the whole and fractional parts of the vector for all 3 components
        // the whole part is a normal Vector, the fractional part a RVector
        template <class D = std::divides<T>>
        [[nodiscard]] constexpr std::pair<Vector<T>, RVector<T>> getWholeFraction(D d = std::divides<T>()) const noexcept
        {
            auto quot = Vector(d(values_[0], values_[3]), d(values_[1], values_[3]), d(values_[2], values_[3]));

            auto rem = RVector<T>(x() - quot.x() * values_[3], y() - quot.y() * values_[3], z() - quot.z() * values_[3], values_[3]);

            return std::pair(quot, rem);
        }

        // check, if all components of a vector are zero
        [[nodiscard]] constexpr bool isZero() const noexcept { return values_[0] == 0 && values_[1] == 0 && values_[2] == 0; }

        constexpr RVector & operator=(const Vector<T> & r) noexcept
        {
            values_[0] = r.x();
            values_[1] = r.y();
            values_[2] = r.z();
            values_[3] = 1;
            return *this;
        }

        constexpr RVector & operator+=(const RVector & r) noexcept
        {
            auto lcm = std::lcm<T, T>(values_[3], r.values_[3]);

            auto w1 = lcm / values_[3];
            auto w2 = lcm / r.values_[3];

            values_[0] = values_[0] * w1 + r.values_[0] * w2;
            values_[1] = values_[1] * w1 + r.values_[1] * w2;
            values_[2] = values_[2] * w1 + r.values_[2] * w2;
            values_[3] = lcm;

            return *this;
        }

        constexpr RVector & operator-=(const RVector & r) noexcept
        {
            auto lcm = std::lcm<int, int>(values_[3], r.values_[3]);

            auto w1 = lcm / values_[3];
            auto w2 = lcm / r.values_[3];

            values_[0] = values_[0] * w1 - r.values_[0] * w2;
            values_[1] = values_[1] * w1 - r.values_[1] * w2;
            values_[2] = values_[2] * w1 - r.values_[2] * w2;
            values_[3] = lcm;

            return *this;
        }

        constexpr RVector & operator-=(const Vector<T> & r) noexcept
        {
            values_[0] -= r.x() * values_[3];
            values_[1] -= r.y() * values_[3];
            values_[2] -= r.z() * values_[3];

            return *this;
        }

        // scale the vector
        constexpr RVector & operator*=(std::signed_integral auto op) noexcept
        {
            values_[0] *= op;
            values_[1] *= op;
            values_[2] *= op;

            return *this;
        }

        // scale the vector
        constexpr RVector & operator/=(std::signed_integral auto op) noexcept
        {
            values_[3] *= op;

            if (values_[3] < 0)
            {
                values_[3] = -values_[3];
                values_[0] = -values_[0];
                values_[1] = -values_[1];
                values_[2] = -values_[2];
            }

            return *this;
        }

        [[nodiscard]] friend constexpr RVector operator-(const RVector & op) noexcept
        {
            return RVector(-op.values_[0], -op.values_[1], -op.values_[2], op.values_[3]);
        }

        // reduce the fraction within the vector without changing the value of the elements
        constexpr void reduce() noexcept
        {
            T t = std::gcd(std::gcd(values_[0], values_[1]), std::gcd(values_[2], values_[3]));

            values_[0] /= t;
            values_[1] /= t;
            values_[2] /= t;
            values_[3] /= t;
        }
};

// normal 3x3 matrix, floating point and integer values work, values are stored in row first order
template <class T>
class Matrix
{
    private:
        std::array<T, 9> values_;

    public:
        constexpr Matrix() : values_{0, 0, 0, 0, 0, 0, 0, 0, 0} {}

        constexpr explicit Matrix(T i) : values_{i, 0, 0, 0, i, 0, 0, 0, i} {}

        constexpr Matrix(T i0, T i1, T i2, T i3, T i4, T i5, T i6, T i7, T i8) : values_{i0, i1, i2, i3, i4, i5, i6, i7, i8} {}

        [[nodiscard]] constexpr T operator[](int i) const
        {
            assert(i < 9);
            return values_[i];
        }

        [[nodiscard]] constexpr T determinant() const noexcept
        {
            return values_[0] * values_[4] * values_[8] - values_[0] * values_[5] * values_[7] + values_[1] * values_[5] * values_[6]
                   - values_[1] * values_[3] * values_[8] + values_[2] * values_[3] * values_[7] - values_[2] * values_[4] * values_[6];
        }
};

// normal 4x4 matrix, floating point and integer values work
template <class T>
class Matrix4
{
    private:
        std::array<T, 16> values_;

    public:
        constexpr Matrix4() : values_{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0} {}

        constexpr explicit Matrix4(T i) : values_{i, 0, 0, 0, 0, i, 0, 0, 0, 0, i, 0, 0, 0, 0, i} {}

        constexpr Matrix4(T i0, T i1, T i2, T i3, T i4, T i5, T i6, T i7, T i8, T i9, T ia, T ib, T ic, T id, T ie, T ig)
            : values_{i0, i1, i2, i3, i4, i5, i6, i7, i8, i9, ia, ib, ic, id, ie, ig}
        {
        }

        constexpr explicit Matrix4(const Matrix<T> & o) : values_{o[0], o[1], o[2], 0, o[3], o[4], o[5], 0, o[6], o[7], o[8], 0, 0, 0, 0, 1} {}

        [[nodiscard]] constexpr T operator[](int i) const noexcept
        {
            assert(i < 16);
            return values_[i];
        }

        [[nodiscard]] constexpr T determinant() const noexcept
        {
            T a = values_[0];
            T b = values_[1];
            T c = values_[2];
            T d = values_[3];
            T e = values_[4];
            T f = values_[5];
            T g = values_[6];
            T h = values_[7];
            T i = values_[8];
            T j = values_[9];
            T k = values_[10];
            T l = values_[11];
            T m = values_[12];
            T n = values_[13];
            T o = values_[14];
            T p = values_[15];

            return a * f * k * p - a * f * l * o - a * g * j * p + a * g * l * n + a * h * j * o - a * h * k * n - b * e * k * p + b * e * l * o + b * g * i * p
                   - b * g * l * m - b * h * i * o + b * h * k * m + c * e * j * p - c * e * l * n - c * f * i * p + c * f * l * m + c * h * i * n
                   - c * h * j * m - d * e * j * o + d * e * k * n + d * f * i * o - d * f * k * m - d * g * i * n + d * g * j * m;
        }

        friend constexpr bool operator==(const Matrix4 & l, const Matrix4 & r) = default;
};

// 3x3 matrix with rational numbers, denominator is shared for all values
template <std::signed_integral T>
class RMatrix
{
    private:
        std::array<T, 10> values_;

    public:
        constexpr explicit RMatrix(T i = 1) : values_{i, 0, 0, 0, i, 0, 0, 0, i, 1} {}

        constexpr RMatrix(T i0, T i1, T i2, T i3, T i4, T i5, T i6, T i7, T i8, T i9 = 1) : values_{i0, i1, i2, i3, i4, i5, i6, i7, i8, i9} {}

        [[nodiscard]] constexpr T operator[](int i) const
        {
            assert(i < 9);
            return values_[i];
        }

        [[nodiscard]] constexpr T denominator() const noexcept
        {
            assert(values_[9] > 0);
            return values_[9];
        }
};

// quaternion class
template <class T>
class Quaternion
{
    private:
        std::array<T, 4> values_;

    public:
        constexpr Quaternion() : values_{0, 0, 0, 0} {}

        constexpr Quaternion(T w, T x, T y, T z) : values_{x, y, z, w} {}

        template <class S>
        constexpr Quaternion(const Quaternion<S> & a) : values_{a.x(), a.y(), a.z(), a.w()}
        {
        }

        [[nodiscard]] constexpr const T & w() const noexcept { return values_[3]; }

        [[nodiscard]] constexpr const T & x() const noexcept { return values_[0]; }

        [[nodiscard]] constexpr const T & y() const noexcept { return values_[1]; }

        [[nodiscard]] constexpr const T & z() const noexcept { return values_[2]; }

        [[nodiscard]] constexpr T & w() noexcept { return values_[3]; }

        [[nodiscard]] constexpr T & x() noexcept { return values_[0]; }

        [[nodiscard]] constexpr T & y() noexcept { return values_[1]; }

        [[nodiscard]] constexpr T & z() noexcept { return values_[2]; }
};

template <class T>
struct Plane
{
        Vector<T> normal;
        T distance = 0;

        constexpr Plane(Vector<T> n, T d) : normal(n), distance(d) {}

        constexpr Plane() = default;
};

template <class T>
struct Line2d
{
        Vector2d<T> normal;
        T distance = 0;

        constexpr Line2d(Vector2d<T> n, T d) : normal(n), distance(d) {}

        constexpr Line2d() = default;
};

// operators on these classes

// plus
template <class S, class U>
[[nodiscard]] constexpr auto operator+(const Vector<S> & l, const Vector<U> & r) noexcept
{
    return Vector<std::common_type_t<S, U>>(l.x() + r.x(), l.y() + r.y(), l.z() + r.z());
}

template <class S, class U>
[[nodiscard]] constexpr auto operator+(const Vector2d<S> & l, const Vector2d<U> & r) noexcept
{
    return Vector2d<std::common_type_t<S, U>>(l.x() + r.x(), l.y() + r.y());
}

template <class S, class T>
[[nodiscard]] constexpr auto operator+(const RVector<S> & l, const Vector<T> & r) noexcept
{
    return RVector<std::common_type_t<S, T>>(
        l.x() + r.x() * l.denominator(), l.y() + r.y() * l.denominator(), l.z() + r.z() * l.denominator(), l.denominator());
}

template <std::signed_integral S, std::signed_integral T>
[[nodiscard]] constexpr auto operator+(const Vector<S> & l, const RVector<T> & r) noexcept
{
    return RVector<std::common_type_t<S, T>>(
        l.x() * r.denominator() + r.x(), l.y() * r.denominator() + r.y(), l.z() * r.denominator() + r.z(), r.denominator());
}

template <std::signed_integral T, std::floating_point F>
[[nodiscard]] constexpr Vector<F> operator+(const RVector<T> & l, const Vector<F> & r) noexcept
{
    return Vector<F>(r.x() + 1.0f * l.x() / l.denominator(), r.y() + 1.0f * l.y() / l.denominator(), r.z() + 1.0f * l.z() * l.denominator());
}

template <std::signed_integral T, std::floating_point F>
[[nodiscard]] constexpr Vector<F> operator+(const Vector<F> & l, const RVector<T> & r) noexcept
{
    return Vector<F>(l.x() + 1.0f * r.x() / r.denominator(), l.y() + 1.0f * r.y() / r.denominator(), l.z() + 1.0f * r.z() / r.denominator());
}

template <class T, class S>
[[nodiscard]] constexpr auto operator+(RVector<T> l, const RVector<S> & r) noexcept
{
    auto lcm = std::lcm<S, T>(l.denominator(), r.denominator());

    auto w1 = lcm / l.denominator();
    auto w2 = lcm / r.denominator();

    return RVector<std::common_type_t<T, S>>(l.x() * w1 + r.x() * w2, l.y() * w1 + r.y() * w2, l.z() * w1 + r.z() * w2, lcm);
}

template <class T, class S>
[[nodiscard]] constexpr auto operator+(const Quaternion<T> & l, const Quaternion<S> & r) noexcept
{
    return Quaternion<std::common_type_t<T, S>>(l.w() + r.w(), l.x() + r.x(), l.y() + r.y(), l.z() + r.z());
}

template <class S, class T>
[[nodiscard]] constexpr auto operator+(const RNumber<S> & l, const RNumber<T> & r) noexcept
{
    auto lcm = std::lcm<S, T>(l.denominator(), r.denominator());

    auto w1 = lcm / l.denominator();
    auto w2 = lcm / r.denominator();

    return RNumber<std::common_type_t<S, T>>(l.numerator() * w1 + r.numerator() * w2, lcm);
}

template <class S, std::signed_integral T>
[[nodiscard]] constexpr auto operator+(const RNumber<S> & l, T r) noexcept
{
    return RNumber<std::common_type_t<S, T>>(l.numerator() + r * l.denominator(), l.denominator());
}

// minus
template <class S, class T>
[[nodiscard]] constexpr auto operator-(const Vector<S> & l, const Vector<T> & r) noexcept
{
    return Vector<std::common_type_t<S, T>>(l.x() - r.x(), l.y() - r.y(), l.z() - r.z());
}

template <class S, class T>
[[nodiscard]] constexpr auto operator-(const Vector2d<S> & l, const Vector2d<T> & r) noexcept
{
    return Vector2d<std::common_type_t<S, T>>(l.x() - r.x(), l.y() - r.y());
}

template <std::signed_integral S, std::signed_integral T>
[[nodiscard]] constexpr auto operator-(const RVector<S> & l, const Vector<T> & r) noexcept
{
    return RVector<std::common_type_t<S, T>>(
        l.x() - r.x() * l.denominator(), l.y() - r.y() * l.denominator(), l.z() - r.z() * l.denominator(), l.denominator());
}

template <std::signed_integral S, std::signed_integral T>
[[nodiscard]] constexpr auto operator-(const Vector<S> & l, const RVector<T> & r)
{
    return RVector<std::common_type_t<S, T>>(
        l.x() * r.denominator() - r.x(), l.y() * r.denominator() - r.y(), l.z() * r.denominator() - r.z(), r.denominator());
}

template <std::signed_integral S, std::signed_integral T>
[[nodiscard]] constexpr auto operator-(RVector<T> l, const RVector<S> & r) noexcept
{
    auto lcm = std::lcm<S, T>(l.denominator(), r.denominator());

    auto w1 = lcm / l.denominator();
    auto w2 = lcm / r.denominator();

    return RVector<std::common_type_t<T, S>>(l.x() * w1 - r.x() * w2, l.y() * w1 - r.y() * w2, l.z() * w1 - r.z() * w2, lcm);
}

template <std::signed_integral S, std::signed_integral T>
[[nodiscard]] constexpr auto operator-(const RNumber<S> & l, const RNumber<T> & r) noexcept
{
    auto lcm = std::lcm<S, T>(l.denominator(), r.denominator());

    auto w1 = lcm / l.denominator();
    auto w2 = lcm / r.denominator();

    return RNumber<std::common_type_t<S, T>>(l.numerator() * w1 - r.numerator() * w2, lcm);
}

template <std::signed_integral S, std::signed_integral T>
[[nodiscard]] constexpr auto operator-(const RNumber<S> & l, T r) noexcept
{
    return RNumber<std::common_type_t<S, T>>(l.numerator() - r * l.denominator(), l.denominator());
}

// multiplication
template <std::signed_integral S, std::signed_integral T>
[[nodiscard]] constexpr auto operator*(const RNumber<S> & l, const RNumber<T> & r) noexcept
{
    auto a = l.numerator() * r.numerator();
    auto b = l.denominator() * r.denominator();

    if (a == 0)
    {
        a = 0;
        b = 1;
    }
    else
    {
        auto gcd = std::gcd(a, b);
        a /= gcd;
        b /= gcd;
    }

    return RNumber<std::common_type_t<S, T>>(a, b);
}

template <class S, std::signed_integral T>
[[nodiscard]] constexpr auto operator*(const RNumber<S> & l, T r) noexcept
{
    auto a = l.numerator() * r;
    auto b = l.denominator();

    if (a == 0)
    {
        a = 0;
        b = 1;
    }
    else
    {
        auto gcd = std::gcd(a, b);
        a /= gcd;
        b /= gcd;
    }

    return RNumber<std::common_type_t<S, T>>(a, b);
}

// scalar multiplication vector, scalar
template <class T, class S>
[[nodiscard]] constexpr auto operator*(const Vector<T> & l, S r) noexcept
{
    return Vector<std::common_type_t<T, S>>(l.x() * r, l.y() * r, l.z() * r);
}

template <class T, class S>
    requires(std::floating_point<S> || std::signed_integral<S>)
[[nodiscard]] constexpr auto operator*(S r, const Vector<T> & l) noexcept
{
    return Vector<std::common_type_t<T, S>>(l.x() * r, l.y() * r, l.z() * r);
}

template <class T, class S>
[[nodiscard]] constexpr RVector<std::common_type_t<T, S>> operator*(RVector<T> l, S r) noexcept
{
    return RVector<std::common_type_t<T, S>>(r * l.x(), r * l.y(), r * l.z(), l.denominator());
}

template <std::signed_integral T, std::signed_integral S>
[[nodiscard]] constexpr auto operator*(T l, RVector<S> r) noexcept
{
    return RVector<std::common_type_t<T, S>>(l * r.x(), l * r.y(), l * r.z(), r.denominator());
}

template <class T, class S>
[[nodiscard]] constexpr auto operator*(T a, const Quaternion<S> & r) noexcept
{
    return Quaternion<std::common_type_t<T, S>>(a * r.w(), a * r.x(), a * r.y(), a * r.z());
}

// dot product between 2 vectors
template <class T, class S>
[[nodiscard]] constexpr std::common_type_t<T, S> operator*(const Vector<T> & l, const Vector<S> & r) noexcept
{
    return l.x() * r.x() + l.y() * r.y() + l.z() * r.z();
}

template <class T, class S>
[[nodiscard]] constexpr std::common_type_t<T, S> operator*(const Vector2d<T> & l, const Vector2d<S> & r) noexcept
{
    return l.x() * r.x() + l.y() * r.y();
}

template <std::signed_integral T, std::signed_integral S>
[[nodiscard]] constexpr auto operator*(const RVector<T> & l, const Vector<S> & r) noexcept
{
    return RNumber<std::common_type_t<T, S>>(l.x() * r.x() + l.y() * r.y() + l.z() * r.z(), l.denominator());
}

template <std::signed_integral T, std::signed_integral S>
[[nodiscard]] constexpr auto operator*(const Vector<T> & r, const RVector<S> & l) noexcept
{
    return RNumber<std::common_type_t<T, S>>(l.x() * r.x() + l.y() * r.y() + l.z() * r.z(), l.denominator());
}

template <std::signed_integral T, std::signed_integral S>
[[nodiscard]] constexpr auto operator*(const RVector<T> & l, const RVector<S> & r) noexcept
{
    return RNumber<std::common_type_t<T, S>>(l.x() * r.x() + l.y() * r.y() + l.z() * r.z(), l.denominator() * r.denominator());
}

template <class T, class S>
[[nodiscard]] constexpr std::common_type_t<T, S> operator*(const Quaternion<T> & l, const Quaternion<S> & r) noexcept
{
    return l.x() * r.x() + l.y() * r.y() + l.z() * r.z() + l.w() * r.w();
}

// multiply vector with matrix
template <class S, class T>
[[nodiscard]] constexpr Vector<std::common_type_t<S, T>> operator*(const Matrix<S> & l, const Vector<T> & r) noexcept
{
    return Vector<std::common_type_t<S, T>>(
        l[0] * r.x() + l[1] * r.y() + l[2] * r.z(), l[3] * r.x() + l[4] * r.y() + l[5] * r.z(), l[6] * r.x() + l[7] * r.y() + l[8] * r.z());
}

template <std::floating_point T>
[[nodiscard]] constexpr auto operator*(const Matrix<T> & l, const RVector<T> & r) noexcept
{
    return Vector<T>((l[0] * r.x() + l[1] * r.y() + l[2] * r.z()) / r.denominator(),
                     (l[3] * r.x() + l[4] * r.y() + l[5] * r.z()) / r.denominator(),
                     (l[6] * r.x() + l[7] * r.y() + l[8] * r.z()) / r.denominator());
}

template <std::signed_integral S, std::signed_integral T>
[[nodiscard]] constexpr auto operator*(const Matrix<S> & l, const RVector<T> & r) noexcept
{
    return RVector<std::common_type_t<S, T>>(
        l[0] * r.x() + l[1] * r.y() + l[2] * r.z(), l[3] * r.x() + l[4] * r.y() + l[5] * r.z(), l[6] * r.x() + l[7] * r.y() + l[8] * r.z(), r.denominator());
}

template <std::floating_point S, std::signed_integral T>
[[nodiscard]] constexpr auto operator*(const Matrix<S> & l, const RVector<T> & r) noexcept
{
    return Vector<std::common_type_t<S, T>>((l[0] * r.x() + l[1] * r.y() + l[2] * r.z()) / r.denominator(),
                                            (l[3] * r.x() + l[4] * r.y() + l[5] * r.z()) / r.denominator(),
                                            (l[6] * r.x() + l[7] * r.y() + l[8] * r.z()) / r.denominator());
}

template <std::signed_integral S, std::signed_integral T>
[[nodiscard]] constexpr auto operator*(const RMatrix<S> & l, const Vector<T> & r) noexcept
{
    return RVector<std::common_type_t<S, T>>(
        l[0] * r.x() + l[1] * r.y() + l[2] * r.z(), l[3] * r.x() + l[4] * r.y() + l[5] * r.z(), l[6] * r.x() + l[7] * r.y() + l[8] * r.z(), l.denominator());
}

template <std::signed_integral S, std::signed_integral T>
[[nodiscard]] constexpr auto operator*(const RMatrix<S> & l, const RVector<T> & r) noexcept
{
    return RVector<std::common_type_t<S, T>>(l[0] * r.x() + l[1] * r.y() + l[2] * r.z(),
                                             l[3] * r.x() + l[4] * r.y() + l[5] * r.z(),
                                             l[6] * r.x() + l[7] * r.y() + l[8] * r.z(),
                                             l.denominator() * r.denominator());
}

template <std::signed_integral T, std::floating_point F>
[[nodiscard]] constexpr auto operator*(const RMatrix<T> & l, const Vector<F> & r) noexcept
{
    return Vector<F>((l[0] * r.x() + l[1] * r.y() + l[2] * r.z()) / l.denominator(),
                     (l[3] * r.x() + l[4] * r.y() + l[5] * r.z()) / l.denominator(),
                     (l[6] * r.x() + l[7] * r.y() + l[8] * r.z()) / l.denominator());
}

// matrix multiplication
template <class T, class S>
[[nodiscard]] constexpr auto operator*(const Matrix<T> & l, const Matrix<S> & r) noexcept
{
    return Matrix<std::common_type_t<S, T>>(l[0] * r[0] + l[1] * r[3] + l[2] * r[6],
                                            l[0] * r[1] + l[1] * r[4] + l[2] * r[7],
                                            l[0] * r[2] + l[1] * r[5] + l[2] * r[8],
                                            l[3] * r[0] + l[4] * r[3] + l[5] * r[6],
                                            l[3] * r[1] + l[4] * r[4] + l[5] * r[7],
                                            l[3] * r[2] + l[4] * r[5] + l[5] * r[8],
                                            l[6] * r[0] + l[7] * r[3] + l[8] * r[6],
                                            l[6] * r[1] + l[7] * r[4] + l[8] * r[7],
                                            l[6] * r[2] + l[7] * r[5] + l[8] * r[8]);
}

template <std::signed_integral T, std::signed_integral S>
[[nodiscard]] constexpr auto operator*(const RMatrix<T> & l, const RMatrix<S> & r) noexcept
{
    return RMatrix<std::common_type_t<T, S>>(l[0] * r[0] + l[1] * r[3] + l[2] * r[6],
                                             l[0] * r[1] + l[1] * r[4] + l[2] * r[7],
                                             l[0] * r[2] + l[1] * r[5] + l[2] * r[8],
                                             l[3] * r[0] + l[4] * r[3] + l[5] * r[6],
                                             l[3] * r[1] + l[4] * r[4] + l[5] * r[7],
                                             l[3] * r[2] + l[4] * r[5] + l[5] * r[8],
                                             l[6] * r[0] + l[7] * r[3] + l[8] * r[6],
                                             l[6] * r[1] + l[7] * r[4] + l[8] * r[7],
                                             l[6] * r[2] + l[7] * r[5] + l[8] * r[8],
                                             l.denominator() * r.denominator());
}

template <std::signed_integral T, std::floating_point S>
[[nodiscard]] constexpr auto operator*(const RMatrix<T> & l, const Matrix<S> & r) noexcept
{
    return Matrix<S>((l[0] * r[0] + l[1] * r[3] + l[2] * r[6]) / l.denominator(),
                     (l[0] * r[1] + l[1] * r[4] + l[2] * r[7]) / l.denominator(),
                     (l[0] * r[2] + l[1] * r[5] + l[2] * r[8]) / l.denominator(),
                     (l[3] * r[0] + l[4] * r[3] + l[5] * r[6]) / l.denominator(),
                     (l[3] * r[1] + l[4] * r[4] + l[5] * r[7]) / l.denominator(),
                     (l[3] * r[2] + l[4] * r[5] + l[5] * r[8]) / l.denominator(),
                     (l[6] * r[0] + l[7] * r[3] + l[8] * r[6]) / l.denominator(),
                     (l[6] * r[1] + l[7] * r[4] + l[8] * r[7]) / l.denominator(),
                     (l[6] * r[2] + l[7] * r[5] + l[8] * r[8]) / l.denominator());
}

template <std::signed_integral T, std::signed_integral S>
[[nodiscard]] constexpr auto operator*(const RMatrix<T> & l, const Matrix<S> & r) noexcept
{
    return RMatrix<std::common_type_t<T, S>>(l[0] * r[0] + l[1] * r[3] + l[2] * r[6],
                                             l[0] * r[1] + l[1] * r[4] + l[2] * r[7],
                                             l[0] * r[2] + l[1] * r[5] + l[2] * r[8],
                                             l[3] * r[0] + l[4] * r[3] + l[5] * r[6],
                                             l[3] * r[1] + l[4] * r[4] + l[5] * r[7],
                                             l[3] * r[2] + l[4] * r[5] + l[5] * r[8],
                                             l[6] * r[0] + l[7] * r[3] + l[8] * r[6],
                                             l[6] * r[1] + l[7] * r[4] + l[8] * r[7],
                                             l[6] * r[2] + l[7] * r[5] + l[8] * r[8],
                                             l.denominator());
}

template <std::floating_point T, std::signed_integral S>
[[nodiscard]] constexpr auto operator*(const Matrix<T> & l, const RMatrix<S> & r) noexcept
{
    return Matrix<T>((l[0] * r[0] + l[1] * r[3] + l[2] * r[6]) / r.denominator(),
                     (l[0] * r[1] + l[1] * r[4] + l[2] * r[7]) / r.denominator(),
                     (l[0] * r[2] + l[1] * r[5] + l[2] * r[8]) / r.denominator(),
                     (l[3] * r[0] + l[4] * r[3] + l[5] * r[6]) / r.denominator(),
                     (l[3] * r[1] + l[4] * r[4] + l[5] * r[7]) / r.denominator(),
                     (l[3] * r[2] + l[4] * r[5] + l[5] * r[8]) / r.denominator(),
                     (l[6] * r[0] + l[7] * r[3] + l[8] * r[6]) / r.denominator(),
                     (l[6] * r[1] + l[7] * r[4] + l[8] * r[7]) / r.denominator(),
                     (l[6] * r[2] + l[7] * r[5] + l[8] * r[8]) / r.denominator());
}

template <std::signed_integral T, std::signed_integral S>
[[nodiscard]] constexpr auto operator*(const Matrix<T> & l, const RMatrix<S> & r) noexcept
{
    return Matrix<std::common_type_t<S, T>>(l[0] * r[0] + l[1] * r[3] + l[2] * r[6],
                                            l[0] * r[1] + l[1] * r[4] + l[2] * r[7],
                                            l[0] * r[2] + l[1] * r[5] + l[2] * r[8],
                                            l[3] * r[0] + l[4] * r[3] + l[5] * r[6],
                                            l[3] * r[1] + l[4] * r[4] + l[5] * r[7],
                                            l[3] * r[2] + l[4] * r[5] + l[5] * r[8],
                                            l[6] * r[0] + l[7] * r[3] + l[8] * r[6],
                                            l[6] * r[1] + l[7] * r[4] + l[8] * r[7],
                                            l[6] * r[2] + l[7] * r[5] + l[8] * r[8],
                                            r.denominator());
}

template <class T, class S>
[[nodiscard]] constexpr Matrix4<std::common_type_t<T, S>> operator*(const Matrix4<T> & l, const Matrix4<S> & r) noexcept
{
    return Matrix4<std::common_type_t<T, S>>{
        l[0] * r[0] + l[1] * r[4] + l[2] * r[8] + l[3] * r[12],
        l[0] * r[1] + l[1] * r[5] + l[2] * r[9] + l[3] * r[13],
        l[0] * r[2] + l[1] * r[6] + l[2] * r[10] + l[14] * r[6],
        l[0] * r[3] + l[1] * r[7] + l[2] * r[11] + l[3] * r[15],
        l[4] * r[0] + l[5] * r[4] + l[6] * r[8] + l[7] * r[12],
        l[4] * r[1] + l[5] * r[5] + l[6] * r[9] + l[7] * r[13],
        l[4] * r[2] + l[5] * r[6] + l[6] * r[10] + l[14] * r[6],
        l[4] * r[3] + l[5] * r[7] + l[6] * r[11] + l[7] * r[15],
        l[8] * r[0] + l[9] * r[4] + l[10] * r[8] + l[11] * r[12],
        l[8] * r[1] + l[9] * r[5] + l[10] * r[9] + l[11] * r[13],
        l[8] * r[2] + l[9] * r[6] + l[10] * r[10] + l[14] * r[6],
        l[8] * r[3] + l[9] * r[7] + l[10] * r[11] + l[11] * r[15],
        l[12] * r[0] + l[13] * r[4] + l[14] * r[8] + l[15] * r[12],
        l[12] * r[1] + l[13] * r[5] + l[14] * r[9] + l[15] * r[13],
        l[12] * r[2] + l[13] * r[6] + l[14] * r[10] + l[14] * r[6],
        l[12] * r[3] + l[13] * r[7] + l[14] * r[11] + l[15] * r[15],
    };
}

template <class T, class S>
[[nodiscard]] constexpr Matrix4<std::common_type_t<T, S>> operator*(const Matrix4<T> & l, const Matrix<S> & r) noexcept
{
    return Matrix4<std::common_type_t<T, S>>{
        l[0] * r[0] + l[1] * r[3] + l[2] * r[6],
        l[0] * r[1] + l[1] * r[4] + l[2] * r[7],
        l[0] * r[2] + l[1] * r[5] + l[2] * r[8],
        l[3],
        l[4] * r[0] + l[5] * r[3] + l[6] * r[6],
        l[4] * r[1] + l[5] * r[4] + l[6] * r[7],
        l[4] * r[2] + l[5] * r[5] + l[6] * r[8],
        l[7],
        l[8] * r[0] + l[9] * r[3] + l[10] * r[6],
        l[8] * r[1] + l[9] * r[4] + l[10] * r[7],
        l[8] * r[2] + l[9] * r[5] + l[10] * r[8],
        l[11],
        l[12] * r[0] + l[13] * r[3] + l[14] * r[6],
        l[12] * r[1] + l[13] * r[4] + l[14] * r[7],
        l[12] * r[2] + l[13] * r[5] + l[14] * r[8],
        l[15],
    };
}

template <class T, class S>
[[nodiscard]] constexpr Vector<std::common_type_t<T, S>> operator*(const Matrix4<T> & l, const Vector<S> & r) noexcept
{
    using R = std::common_type_t<T, S>;

    R x = l[0] * r.x() + l[1] * r.y() + l[2] * r.z() + l[3];
    R y = l[4] * r.x() + l[5] * r.y() + l[6] * r.z() + l[7];
    R z = l[8] * r.x() + l[9] * r.y() + l[10] * r.z() + l[11];
    R w = l[12] * r.x() + l[13] * r.y() + l[14] * r.z() + l[15];

    return Vector<R>{x / w, y / w, z / w};
}

// division
template <std::signed_integral T, std::signed_integral S>
[[nodiscard]] constexpr RVector<std::common_type_t<T, S>> operator/(RVector<T> l, S r)
{
    return RVector<std::common_type_t<T, S>>(l.x(), l.y(), l.z(), l.denominator() * r);
}

template <std::signed_integral T, std::signed_integral S>
[[nodiscard]] constexpr RNumber<std::common_type_t<S, T>> operator/(const RNumber<S> & l, T r)
{
    return RNumber<std::common_type_t<S, T>>(l.numerator(), l.denominator() * r);
}

template <std::signed_integral S, std::signed_integral T>
[[nodiscard]] constexpr auto operator/(const RNumber<S> & l, const RNumber<T> & r) noexcept
{
    auto a = l.numerator() * r.denominator();
    auto b = l.denominator() * r.numerator();

    if (a == 0)
    {
        a = 0;
        b = 1;
    }
    else
    {
        auto gcd = std::gcd(a, b);
        a /= gcd;
        b /= gcd;
    }

    return RNumber<std::common_type_t<S, T>>(a, b);
}

// comparison
template <class U, class S>
[[nodiscard]] constexpr bool operator==(const Vector<U> & v1, const Vector<S> & v2) noexcept
{
    return v1.x() == v2.x() && v1.y() == v2.y() && v1.z() == v2.z();
}

template <class T, class S>
[[nodiscard]] constexpr bool operator!=(const Vector<T> & l, const Vector<S> & r) noexcept
{
    return !(l == r);
}

template <std::signed_integral U, std::signed_integral S>
[[nodiscard]] constexpr bool operator==(const RVector<U> & v1, const RVector<S> & v2) noexcept
{
    return v1.x() * v2.denominator() == v2.x() * v1.denominator() && v1.y() * v2.denominator() == v2.y() * v1.denominator()
           && v1.z() * v2.denominator() == v2.z() * v1.denominator();
}

template <std::signed_integral T, std::signed_integral S>
[[nodiscard]] constexpr bool operator!=(const RVector<T> & l, const RVector<S> & r) noexcept
{
    return !(l == r);
}

template <std::signed_integral U, class S>
[[nodiscard]] constexpr bool operator==(const RVector<U> & v1, const Vector<S> & v2) noexcept
{
    return v1.x() == v2.x() * v1.denominator() && v1.y() == v2.y() * v1.denominator() && v1.z() == v2.z() * v1.denominator();
}

template <class U, std::signed_integral S>
[[nodiscard]] constexpr bool operator==(const Vector<U> & v1, const RVector<S> & v2) noexcept
{
    return v1.x() * v2.denominator() == v2.x() && v1.y() * v2.denominator() == v2.y() && v1.z() * v2.denominator() == v2.z();
}

template <class T, class S>
[[nodiscard]] constexpr bool operator==(const Matrix<T> & l, const Matrix<S> & r)
{
    return l[0] == r[0] && l[1] == r[1] && l[2] == r[2] && l[3] == r[3] && l[4] == r[4] && l[5] == r[5] && l[6] == r[6] && l[7] == r[7] && l[8] == r[8];
}

template <class T, class S>
[[nodiscard]] constexpr bool operator!=(const Matrix<T> & l, const Matrix<S> & r)
{
    return !(l == r);
}

template <std::signed_integral T, std::signed_integral S>
[[nodiscard]] constexpr bool operator==(const RMatrix<T> & l, const RMatrix<S> & r)
{
    return l[0] * r.denominator() == r[0] * l.denominator() && l[1] * r.denominator() == r[1] * l.denominator()
           && l[2] * r.denominator() == r[2] * l.denominator() && l[3] * r.denominator() == r[3] * l.denominator()
           && l[4] * r.denominator() == r[4] * l.denominator() && l[5] * r.denominator() == r[5] * l.denominator()
           && l[6] * r.denominator() == r[6] * l.denominator() && l[7] * r.denominator() == r[7] * l.denominator()
           && l[8] * r.denominator() == r[8] * l.denominator();
}

template <std::signed_integral T, std::signed_integral S>
[[nodiscard]] constexpr bool operator!=(const RMatrix<T> & l, const RMatrix<S> & r)
{
    return !(l == r);
}

template <std::signed_integral T, std::signed_integral S>
[[nodiscard]] constexpr bool operator==(const RNumber<T> & l, const RNumber<S> & r)
{
    return l.numerator() * r.denominator() == r.numerator() * l.denominator();
}

template <std::signed_integral T, std::signed_integral S>
[[nodiscard]] constexpr bool operator!=(const RNumber<T> & l, const RNumber<S> & r)
{
    return l.numerator() * r.denominator() != r.numerator() * l.denominator();
}

template <std::signed_integral T, std::signed_integral S>
[[nodiscard]] constexpr auto operator<=>(const RNumber<T> & l, const RNumber<S> & r)
{
    auto ab = l.numerator() * r.denominator();
    auto cd = r.numerator() * l.denominator();

    if ((r.denominator() < 0) != (l.denominator() < 0))
    {
        ab = -ab;
        cd = -cd;
    }

    return ab <=> cd;
}

template <class T>
[[nodiscard]] constexpr Vector<T> min(const Vector<T> & a, const Vector<T> & b)
{
    return Vector<T>(std::min(a.x(), b.x()), std::min(a.y(), b.y()), std::min(a.z(), b.z()));
}

template <class T>
[[nodiscard]] constexpr Vector<T> max(const Vector<T> & a, const Vector<T> & b)
{
    return Vector<T>(std::max(a.x(), b.x()), std::max(a.y(), b.y()), std::max(a.z(), b.z()));
}

// inversion of matrices
template <std::floating_point T>
[[nodiscard]] [[nodiscard]] constexpr auto invert(const Matrix<T> & m, T det)
{
    return Matrix<T>((m[4] * m[8] - m[5] * m[7]) / det,
                     (m[2] * m[7] - m[1] * m[8]) / det,
                     (m[1] * m[5] - m[2] * m[4]) / det,
                     (m[5] * m[6] - m[3] * m[8]) / det,
                     (m[0] * m[8] - m[2] * m[6]) / det,
                     (m[2] * m[3] - m[0] * m[5]) / det,
                     (m[3] * m[7] - m[4] * m[6]) / det,
                     (m[1] * m[6] - m[0] * m[7]) / det,
                     (m[0] * m[4] - m[1] * m[3]) / det);
}

template <std::signed_integral T>
[[nodiscard]] constexpr auto invert(const Matrix<T> & m, T det)
{
    std::array<T, 10> n;

    n[9] = det;

    n[0] = m[4] * m[8] - m[5] * m[7];
    n[1] = m[2] * m[7] - m[1] * m[8];
    n[2] = m[1] * m[5] - m[2] * m[4];
    n[3] = m[5] * m[6] - m[3] * m[8];
    n[4] = m[0] * m[8] - m[2] * m[6];
    n[5] = m[2] * m[3] - m[0] * m[5];
    n[6] = m[3] * m[7] - m[4] * m[6];
    n[7] = m[1] * m[6] - m[0] * m[7];
    n[8] = m[0] * m[4] - m[1] * m[3];

    // if denominator is negative, invert everything
    if (n[9] < 0)
        for (T & v : n) v = -v;

    // reduce the fraction
    T gcd = std::gcd(n[0], n[1]);
    for (size_t j = 2; j < n.size(); j++) gcd = std::gcd(gcd, n[j]);

    for (int & v : n) v /= gcd;

    return RMatrix<T>(n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7], n[8], n[9]);
}

// cross product
template <class S, class T>
[[nodiscard]] constexpr auto cross(const Vector<S> & a, const Vector<T> & b)
{
    return Vector<std::common_type_t<S, T>>(a.y() * b.z() - a.z() * b.y(), a.z() * b.x() - a.x() * b.z(), a.x() * b.y() - a.y() * b.x());
}

// cross product of 2d vector, the x and y components will always be 0, so this returns only the z component
template <class S, class T>
[[nodiscard]] constexpr std::common_type_t<S, T> cross(const Vector2d<S> & a, const Vector2d<T> & b)
{
    return a.x() * b.y() - a.y() * b.x();
}

// calculate cross(cross(a, b), a), for 2d vectors the result is again in the same plane as
// the 2d vectors, even though the first cross product will point outside of the plane of the
// 2d vectors
template <class T>
[[nodiscard]] constexpr Vector2d<T> triplecross(const Vector2d<T> & a, const Vector2d<T> & b)
{
    // a x b only has a z-component
    T axb_z = cross(a, b);

    // now calculate cross product of axb with a again
    return Vector2d<T>(-axb_z * a.y(), axb_z * a.x());
}

template <std::signed_integral S, std::signed_integral T>
[[nodiscard]] constexpr RVector<std::common_type_t<S, T>> cross(const Vector<S> & a, const RVector<T> & b)
{
    return RVector<std::common_type_t<S, T>>(a.y() * b.z() - a.z() * b.y(), a.z() * b.x() - a.x() * b.z(), a.x() * b.y() - a.y() * b.x(), b.denominator());
}

// other operations

// returns the distance of the point from the plane
// 0 means on the > 0 above (in direction of the normal), < 0 below (opposite of the normal)
// the absolute value of the distance is only right when the normal vector of the plane is unit length
template <class T, class S>
[[nodiscard]] constexpr std::common_type_t<T, S> pointPlaneDistance(const Vector<T> & point, const Plane<S> & plane)
{
    return plane.normal * point - plane.distance;
}

template <std::signed_integral T, std::signed_integral S>
[[nodiscard]] constexpr RNumber<std::common_type_t<T, S>> pointPlaneDistance(const RVector<T> & point, const Plane<S> & plane)
{
    return plane.normal * point - plane.distance;
}

template <class T, class S>
[[nodiscard]] constexpr std::common_type_t<T, S> pointLine2dDistance(const Vector2d<T> & point, const Line2d<S> & line)
{
    return line.normal * point - line.distance;
}

// calculate intersection point between line and plane
// TODO create a type for a line
template <class T, class S>
[[nodiscard]] constexpr std::optional<Vector<float>> intersectLinePlane(const Vector<T> & linepoint, const Vector<T> & linedir, const Plane<S> & plane)
{
    float dot = linedir * plane.normal;

    if (std::fabs(dot) > 0.00001)
    {
        // find a point on the plane
        auto ponp = plane.normal * (-plane.distance / (plane.normal * plane.normal));

        auto w   = linepoint - ponp;
        auto fac = -(plane.normal * w) / dot;
        return linepoint + (linedir * fac);
    }

    return std::nullopt;
}

template <std::signed_integral T, std::signed_integral S>
[[nodiscard]] constexpr RNumber<std::common_type_t<T, S>> pointLineSquaredDistance(const RVector<T> & p0, const Vector<T> & dir, const RVector<S> & point)
{
    auto c = cross(dir, p0 - point);

    return (c * c) / (dir * dir);
}

template <class T>
[[nodiscard]] constexpr Plane<T> planeFromNormalAndPoint(const Vector<T> & normal, const Vector<T> & p)
{
    return Plane<T>(normal, p * normal);
}

template <class T>
[[nodiscard]] constexpr Line2d<T> lineFromNormalAndPoint(const Vector2d<T> & normal, const Vector2d<T> & p)
{
    return Line2d<T>(normal, p * normal);
}

template <class T>
[[nodiscard]] constexpr Plane<T> planeFromThreePoints(const Vector<T> & p1, const Vector<T> & p2, const Vector<T> & p3)
{
    return planeFromNormalAndPoint(normalize(cross(p3 - p2, p1 - p2)), p1);
}

// constructors for some classes

// construct a Matrix from 3 row vectors
template <class T>
[[nodiscard]] constexpr Matrix<T> rowMatrix(const Vector<T> & c1, const Vector<T> & c2, const Vector<T> & c3)
{
    return Matrix<T>(c1.x(), c1.y(), c1.z(), c2.x(), c2.y(), c2.z(), c3.x(), c3.y(), c3.z());
}

template <class T>
[[nodiscard]] constexpr Matrix4<T> translationMatrix(const Vector<T> & t)
{
    return Matrix4<T>{1, 0, 0, t.x(), 0, 1, 0, t.y(), 0, 0, 1, t.z(), 0, 0, 0, 1};
}

template <std::floating_point F, std::signed_integral T>
[[nodiscard]] constexpr Matrix4<F> translationMatrix(const RVector<T> & t)
{
    return Matrix4<F>{1,
                      0,
                      0,
                      static_cast<F>(t.x()) / t.denominator(),
                      0,
                      1,
                      0,
                      static_cast<F>(t.y()) / t.denominator(),
                      0,
                      0,
                      1,
                      static_cast<F>(t.z()) / t.denominator(),
                      0,
                      0,
                      0,
                      1};
}

// axis must be normalized
template <std::floating_point F>
[[nodiscard]] inline Matrix<F> rotationMatrix(F angle, const Vector<F> & axis)
{
    F const c = std::cos(angle);
    F const s = std::sin(angle);

    return Matrix<F>{
        c + (1.0 - c) * axis.x() * axis.x(),
        (1.0 - c) * axis.y() * axis.x() - s * axis.z(),
        (1.0 - c) * axis.z() * axis.x() + s * axis.y(),

        (1.0 - c) * axis.x() * axis.y() + s * axis.z(),
        c + (1.0 - c) * axis.y() * axis.y(),
        (1.0 - c) * axis.z() * axis.y() - s * axis.x(),

        (1.0 - c) * axis.x() * axis.z() - s * axis.y(),
        (1.0 - c) * axis.y() * axis.z() + s * axis.x(),
        c + (1.0 - c) * axis.z() * axis.z(),
    };
}

template <std::floating_point F>
[[nodiscard]] inline Matrix4<F> perspectiveMatrix(F fovy, F aspect, F zNear, F zFar)
{
    F c1 = 1;
    F c2 = 2;

    assert(std::abs(aspect - std::numeric_limits<F>::epsilon()) > 0);

    F tanHalfFovy = tan(fovy / c2);

    return Matrix4<F>{
        c1 / (aspect * tanHalfFovy), 0, 0, 0, 0, -c1 / tanHalfFovy, 0, 0, 0, 0, zFar / (zNear - zFar), (zFar * zNear) / (zNear - zFar), 0, 0, -c1, 0};
}

template <typename T>
[[nodiscard]] constexpr Matrix<T> QuarternionToMatrix(const Quaternion<T> & q)
{
    T qxx(q.x() * q.x());
    T qyy(q.y() * q.y());
    T qzz(q.z() * q.z());
    T qxz(q.x() * q.z());
    T qxy(q.x() * q.y());
    T qyz(q.y() * q.z());
    T qwx(q.w() * q.x());
    T qwy(q.w() * q.y());
    T qwz(q.w() * q.z());

    return Matrix<T>(T(1) - T(2) * (qyy + qzz),
                     T(2) * (qxy - qwz),
                     T(2) * (qxz + qwy),
                     T(2) * (qxy + qwz),
                     T(1) - T(2) * (qxx + qzz),
                     T(2) * (qyz - qwx),
                     T(2) * (qxz - qwy),
                     T(2) * (qyz + qwx),
                     T(1) - T(2) * (qxx + qyy));
}

template <std::floating_point T>
[[nodiscard]] constexpr T mix(T x, T y, float a)
{
    return (1 - a) * x + a * y;
}

template <std::floating_point T>
[[nodiscard]] constexpr Quaternion<T> slerp(const Quaternion<T> & x, const Quaternion<T> & y, T a)
{
    Quaternion<T> z = y;

    T cosTheta = x * y;

    // If cosTheta < 0, the interpolation will take the long way around the sphere.
    // To fix this, one quat must be negated.
    if (cosTheta < T{0})
    {
        z        = Quaternion<T>(-y.w(), -y.x(), -y.y(), -y.z());
        cosTheta = -cosTheta;
    }

    // Perform a linear interpolation when cosTheta is close to 1 to avoid side effect of sin(angle) becoming a zero denominator
    if (cosTheta > T{1} - T{0.00001})
    {
        // Linear interpolation
        return Quaternion<T>(mix(x.w(), z.w(), a), mix(x.x(), z.x(), a), mix(x.y(), z.y(), a), mix(x.z(), z.z(), a));
    }
    else
    {
        // Essential Mathematics, page 467
        T angle = std::acos(cosTheta);
        T p1    = std::sin((1 - a) * angle);
        T p2    = std::sin(a * angle);
        T p3    = 1.0 / std::sin(angle);

        return p3 * (p1 * x + p2 * z);
    }
}

template <std::floating_point F, typename T>
[[nodiscard]] constexpr F length(Vector<T> v)
{
    return std::hypot(static_cast<F>(v.x()), v.y(), v.z());
}

template <std::floating_point F, typename T>
[[nodiscard]] constexpr F length(Vector2d<T> v)
{
    return std::hypot(static_cast<F>(v.x()), v.y());
}

template <std::floating_point F, std::signed_integral T>
[[nodiscard]] constexpr F length(RVector<T> v)
{
    return std::sqrt(static_cast<F>(v * v));
}

template <std::floating_point T>
[[nodiscard]] constexpr Vector<T> normalize(Vector<T> v)
{
    auto f = length<T>(v);
    v /= f;
    return v;
}

template <std::floating_point T>
[[nodiscard]] constexpr Vector2d<T> normalize(Vector2d<T> v)
{
    auto f = length<T>(v);
    v /= f;
    return v;
}

template <typename T, std::floating_point F>
[[nodiscard]] constexpr Quaternion<F> MatrixToQuarternion(const Matrix<T> & m)
{
    F fourXSquaredMinus1 = m[0] - m[4] - m[8];
    F fourYSquaredMinus1 = m[4] - m[0] - m[8];
    F fourZSquaredMinus1 = m[8] - m[0] - m[4];
    F fourWSquaredMinus1 = m[0] + m[4] + m[8];

    int biggestIndex           = 0;
    F fourBiggestSquaredMinus1 = fourWSquaredMinus1;
    if (fourXSquaredMinus1 > fourBiggestSquaredMinus1)
    {
        fourBiggestSquaredMinus1 = fourXSquaredMinus1;
        biggestIndex             = 1;
    }
    if (fourYSquaredMinus1 > fourBiggestSquaredMinus1)
    {
        fourBiggestSquaredMinus1 = fourYSquaredMinus1;
        biggestIndex             = 2;
    }
    if (fourZSquaredMinus1 > fourBiggestSquaredMinus1)
    {
        fourBiggestSquaredMinus1 = fourZSquaredMinus1;
        biggestIndex             = 3;
    }

    F biggestVal = std::sqrt(fourBiggestSquaredMinus1 + F{1}) * F{0.5};
    F mult       = F{0.25} / biggestVal;

    switch (biggestIndex)
    {
        case 0: return Quaternion<F>(biggestVal, (m[7] - m[5]) * mult, (m[2] - m[6]) * mult, (m[3] - m[1]) * mult);
        case 1: return Quaternion<F>((m[7] - m[5]) * mult, biggestVal, (m[3] + m[1]) * mult, (m[2] + m[6]) * mult);
        case 2: return Quaternion<F>((m[2] - m[6]) * mult, (m[3] + m[1]) * mult, biggestVal, (m[7] + m[5]) * mult);
        case 3: return Quaternion<F>((m[3] - m[1]) * mult, (m[2] + m[6]) * mult, (m[7] + m[5]) * mult, biggestVal);
        default: // Silence a -Wswitch-default warning in GCC. Should never actually get here. Assert is just for sanity.
            assert(false);
            return Quaternion<F>(1, 0, 0, 0);
    }
}

template <std::floating_point F, typename T>
[[nodiscard]] constexpr Quaternion<F> angleAxis(F a, const Vector<T> & v)
{
    F s = std::sin(a * F{0.5});

    return Quaternion<F>(std::cos(a * F{0.5}), v.x() * s, v.y() * s, v.z() * s);
}

template <std::floating_point F, typename T>
[[nodiscard]] constexpr Quaternion<F> rotationQuarternion(const Vector<T> & orig, const Vector<T> & dest)
{
    F cosTheta = orig * dest;
    Vector<F> rotationAxis;

    if (cosTheta >= F{1} - F{0.00001})
    {
        // orig and dest point in the same direction
        return Quaternion<T>(1.0, 0, 0, 0);
    }

    if (cosTheta < F{-1} + F{0.00001})
    {
        // special case when vectors in opposite directions :
        // there is no "ideal" rotation axis
        // So guess one; any will do as long as it's perpendicular to start
        // This implementation favors a rotation around the Up axis (Y),
        // since it's often what you want to do.
        rotationAxis = cross(Vector<F>(0, 0, 1), orig);

        if (rotationAxis * rotationAxis < 0.00001) // bad luck, they were parallel, try again!
            rotationAxis = cross(Vector<F>(1, 0, 0), orig);

        rotationAxis = normalize(rotationAxis);
        return angleAxis<float>(std::numbers::pi, rotationAxis);
    }

    // Implementation from Stan Melax's Game Programming Gems 1 article
    rotationAxis = cross(orig, dest);

    F s    = std::sqrt((F{1} + cosTheta) * F{2});
    F invs = F{1} / s;

    return Quaternion<F>(s * F{0.5f}, rotationAxis.x() * invs, rotationAxis.y() * invs, rotationAxis.z() * invs);
}

template <class T>
[[nodiscard]] constexpr Quaternion<T> rotate(const Quaternion<T> & l, const Quaternion<T> & r)
{
    return Quaternion<T>(l.w() * r.w() - l.x() * r.x() - l.y() * r.y() - l.z() * r.z(),
                         l.w() * r.x() + l.x() * r.w() + l.y() * r.z() - l.z() * r.y(),
                         l.w() * r.y() + l.y() * r.w() + l.z() * r.x() - l.x() * r.z(),
                         l.w() * r.z() + l.z() * r.w() + l.x() * r.y() - l.y() * r.x());
}

template <std::floating_point T>
[[nodiscard]] constexpr Vector<T> operator*(const Quaternion<T> & l, const Vector<T> & r)
{
    Quaternion<T> q(l);
    Quaternion<T> qinv = invert(l);
    Quaternion<T> p(0, r.x(), r.y(), r.z());

    Quaternion<T> res = rotate(rotate(q, p), qinv);

    return Vector<T>(res.x(), res.y(), res.z());
}

template <class T>
[[nodiscard]] constexpr Quaternion<T> invert(const Quaternion<T> & q)
{
    return Quaternion<T>(q.w(), -q.x(), -q.y(), -q.z());
}

template <class U, class S>
[[nodiscard]] constexpr bool operator==(const Quaternion<U> & v1, const Quaternion<S> & v2) noexcept
{
    return v1.x() == v2.x() && v1.y() == v2.y() && v1.z() == v2.z() && v1.w() == v2.w();
}

template <class U, class S>
[[nodiscard]] constexpr bool operator!=(const Quaternion<U> & v1, const Quaternion<S> & v2) noexcept
{
    return !(v1 == v2);
}

template <class T, class S>
[[nodiscard]] constexpr bool equal(const Vector<T> & a, const Vector<S> & b, T epsilon = 0)
{
    return (std::fabs(a.x() - b.x()) <= epsilon) && (std::fabs(a.y() - b.y()) <= epsilon) && (std::fabs(a.z() - b.z()) <= epsilon);
}

// all vectors between -d <= .. <= d for all 3 dimensions, z fastest
// (NG yields them from a std::generator, which is C++23)
template <class V>
std::vector<V> iterate3d(int size)
{
    std::vector<V> out;
    out.reserve(static_cast<size_t>(2 * size + 1) * (2 * size + 1) * (2 * size + 1));
    V i(-size, -size, -size);

    for (i.x() = -size; i.x() <= size; ++i.x())
        for (i.y() = -size; i.y() <= size; ++i.y())
            for (i.z() = -size; i.z() <= size; ++i.z()) out.push_back(i);

    return out;
}

} // namespace BurrTools::Maths

// structured binding tuple protocol for some of the types:
// - RNumber<T>
// - RVector<T>
// - Vector<T>
// - Vector2d<T>
// Skip Matrix, Matrix4, RMatrix (due to too many values)
// - Quaternion<T>
// - Plane<T>, Line2d<T> are implicitly done

template <typename T>
struct std::tuple_size<BurrTools::Maths::RNumber<T>> : std::integral_constant<std::size_t, 2>
{
};

template <typename T>
struct std::tuple_element<0, BurrTools::Maths::RNumber<T>>
{
        using type = T;
};

template <typename T>
struct std::tuple_element<1, BurrTools::Maths::RNumber<T>>
{
        using type = T;
};

namespace BurrTools::Maths {
template <std::size_t I, typename T>
auto & get(const RNumber<T> & p)
{
    if constexpr (I == 0)
        return p.numerator();
    else if constexpr (I == 1)
        return p.denominator();
}
} // namespace BurrTools::Maths

template <typename T>
struct std::tuple_size<BurrTools::Maths::RVector<T>> : std::integral_constant<std::size_t, 4>
{
};

template <typename T>
struct std::tuple_element<0, BurrTools::Maths::RVector<T>>
{
        using type = T;
};

template <typename T>
struct std::tuple_element<1, BurrTools::Maths::RVector<T>>
{
        using type = T;
};

template <typename T>
struct std::tuple_element<2, BurrTools::Maths::RVector<T>>
{
        using type = T;
};

template <typename T>
struct std::tuple_element<3, BurrTools::Maths::RVector<T>>
{
        using type = T;
};

namespace BurrTools::Maths {
template <std::size_t I, typename T>
auto & get(const RVector<T> & p)
{
    if constexpr (I == 0)
        return p.x();
    else if constexpr (I == 1)
        return p.y();
    else if constexpr (I == 2)
        return p.z();
    else if constexpr (I == 3)
        return p.denominator();
}
} // namespace BurrTools::Maths

template <typename T>
struct std::tuple_size<BurrTools::Maths::Vector<T>> : std::integral_constant<std::size_t, 3>
{
};

template <typename T>
struct std::tuple_element<0, BurrTools::Maths::Vector<T>>
{
        using type = T;
};

template <typename T>
struct std::tuple_element<1, BurrTools::Maths::Vector<T>>
{
        using type = T;
};

template <typename T>
struct std::tuple_element<2, BurrTools::Maths::Vector<T>>
{
        using type = T;
};

namespace BurrTools::Maths {
template <std::size_t I, typename T>
auto & get(Vector<T> & p)
{
    if constexpr (I == 0)
        return p.x();
    else if constexpr (I == 1)
        return p.y();
    else if constexpr (I == 2)
        return p.z();
}

template <std::size_t I, typename T>
const auto & get(const Vector<T> & p)
{
    if constexpr (I == 0)
        return p.x();
    else if constexpr (I == 1)
        return p.y();
    else if constexpr (I == 2)
        return p.z();
}

} // namespace BurrTools::Maths

template <typename T>
struct std::tuple_size<BurrTools::Maths::Vector2d<T>> : std::integral_constant<std::size_t, 2>
{
};

template <typename T>
struct std::tuple_element<0, BurrTools::Maths::Vector2d<T>>
{
        using type = T;
};

template <typename T>
struct std::tuple_element<1, BurrTools::Maths::Vector2d<T>>
{
        using type = T;
};

namespace BurrTools::Maths {
template <std::size_t I, typename T>
auto & get(Vector2d<T> & p)
{
    if constexpr (I == 0)
        return p.x();
    else if constexpr (I == 1)
        return p.y();
}

template <std::size_t I, typename T>
const auto & get(const Vector2d<T> & p)
{
    if constexpr (I == 0)
        return p.x();
    else if constexpr (I == 1)
        return p.y();
}
} // namespace BurrTools::Maths

template <typename T>
struct std::tuple_size<BurrTools::Maths::Quaternion<T>> : std::integral_constant<std::size_t, 4>
{
};

template <typename T>
struct std::tuple_element<0, BurrTools::Maths::Quaternion<T>>
{
        using type = T;
};

template <typename T>
struct std::tuple_element<1, BurrTools::Maths::Quaternion<T>>
{
        using type = T;
};

template <typename T>
struct std::tuple_element<2, BurrTools::Maths::Quaternion<T>>
{
        using type = T;
};

template <typename T>
struct std::tuple_element<3, BurrTools::Maths::Quaternion<T>>
{
        using type = T;
};

namespace BurrTools::Maths {
template <std::size_t I, typename T>
auto & get(Quaternion<T> & p)
{
    if constexpr (I == 0)
        return p.w();
    else if constexpr (I == 1)
        return p.x();
    else if constexpr (I == 2)
        return p.y();
    else if constexpr (I == 3)
        return p.z();
}

template <std::size_t I, typename T>
const auto & get(const Quaternion<T> & p)
{
    if constexpr (I == 0)
        return p.w();
    else if constexpr (I == 1)
        return p.x();
    else if constexpr (I == 2)
        return p.y();
    else if constexpr (I == 3)
        return p.z();
}
} // namespace BurrTools::Maths
