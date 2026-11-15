/* BurrTools
 *
 * Ported from BurrTools NG by Andreas Röver (https://gitlab.com/roever/burrtools),
 * src/lib/transformation.h at commit fdae7951, for grids as data
 * (design/2026-11-19-ng-merge-plan.md, step G1). NG is licensed under the
 * GNU General Public License version 3, and so is this file.
 *
 * Changes from NG's file: include path.
 * Keep the rest identical to NG's file, so changes can be exchanged both ways.
 */
#pragma once

#include "algebra.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>

// this module contains the necessary types to specify orientations and
// transformations within the confines of what BurrTools requires for the grids
//
// These are not general linear algebra types but rather similar types that are
// severely restricted but do their work more efficiently and with less memory
// hunger
//
// e.g. a normal 4x4 homogeneous transformation matrix will require 64 bytes
// while the equivalent matrix in here requires 1 byte and 4 integers of the
// required width (default 32 bit) which results in 20 bytes. And if you only
// want to encode a rotation you can get that for 1 byte

namespace BurrTools::PuzzleLibrary {

// Represents a rotation. But only the rotations that are required for BurrTools.
// Additionally, we can represent "invalid" rotations
// each rotation is represented by a single number that is used as index
// into all kinds of lookup tables
class TRotation
{
    public:
        // the rotations are represented by an single number which is an index
        // into a table of rotation matrices

        // Number of different rotation matrices, there are 30 rotations times
        // 2 for mirror transformations
        static constexpr unsigned int NUM_ROTATION_MATRICES = 30 * 2;

        // Represents a rotation using a direction and an angle
        struct Rotation
        {
                // the direction of the axis of rotation, the vector is not normalized
                BurrTools::Maths::Vector<int8_t> direction;

                // angle in degree
                BurrTools::Maths::DegreeInt angle;
        };

    private:
        // this array contains the description of all the rotation matrices
        static const std::array<Rotation, NUM_ROTATION_MATRICES / 2> rotationAngles;

        // all 3x3 rotation matrices for the rotations defined in
        // rotationAngles from above. The matrices in this array are
        // calculated at start-up time
        static std::array<BurrTools::Maths::RMatrix<int>, NUM_ROTATION_MATRICES + 1> rotationMatrices;

    public:
        // the rotation indices
        // rotations are always clockwise when looking down on the axis.
        // the axis is given by the name, capital letter signify positive
        // values, small letter negative, if a letter is given twice
        // the value is 2 along that direction, e.g. XyyZ is rotation around
        // the axis (1, -2, 1)
        //
        // ATTENTION the mirroring transformations are always first rotate
        // then mirror, and the mirror transformation done is a point reflection
        // at the origin
        //
        // We can not use a simple reflection along the y-z plane (invert
        // x-values) because then we end up with a result that is not part of
        // the triangular group of symmetries. The simplest transformation
        // containing a mirroring component was the reflection used here.
        static constexpr int ROT_IDENTITY = 0;
        static constexpr int ROT_090_Z    = 1;
        static constexpr int ROT_180_Z    = 2;
        static constexpr int ROT_270_Z    = 3;
        static constexpr int ROT_090_Y    = 4;
        static constexpr int ROT_180_Y    = 5;
        static constexpr int ROT_270_Y    = 6;
        static constexpr int ROT_090_X    = 7;
        static constexpr int ROT_180_X    = 8;
        static constexpr int ROT_270_X    = 9;
        static constexpr int ROT_180_YZ   = 10;
        static constexpr int ROT_180_Yz   = 11;
        static constexpr int ROT_180_XZ   = 12;
        static constexpr int ROT_180_Xz   = 13;
        static constexpr int ROT_180_XY   = 14;
        static constexpr int ROT_180_Xy   = 15;
        static constexpr int ROT_060_XYZ  = 16;
        static constexpr int ROT_120_XYZ  = 17;
        static constexpr int ROT_180_XYZ  = 18;
        static constexpr int ROT_240_XYZ  = 19;
        static constexpr int ROT_300_XYZ  = 20;
        static constexpr int ROT_120_XYz  = 21;
        static constexpr int ROT_240_XYz  = 22;
        static constexpr int ROT_120_XyZ  = 23;
        static constexpr int ROT_240_XyZ  = 24;
        static constexpr int ROT_120_xYZ  = 25;
        static constexpr int ROT_240_xYZ  = 26;
        static constexpr int ROT_180_XYzz = 27;
        static constexpr int ROT_180_XyyZ = 28;
        static constexpr int ROT_180_xxYZ = 29;
        static constexpr int ROT_MIRROR   = 30;

        static constexpr int ROT_MIRROR_060_XYZ = ROT_MIRROR + ROT_060_XYZ;

    private:
        // table for matrix multiplication the value inside cell [a][b] of the array is the
        // rotation index, when you multiply two rotation indices b*a
        static std::array<std::array<TRotation, NUM_ROTATION_MATRICES + 1>, NUM_ROTATION_MATRICES + 1> matrixMult;

        // rotation index to the inverse matrix
        static std::array<TRotation, NUM_ROTATION_MATRICES + 1> inverseMatrix;

        // the rotation index of this TRotation class as an index into the matrix array
        // invariant: will be <= NUM_ROTATION_MATRICES
        uint8_t rotation_;

    public:
        // this function must be called before using this class
        static void init_tables();

        // create invalid rotation
        TRotation() : rotation_(NUM_ROTATION_MATRICES) {}

        // copy construction
        TRotation(const TRotation & r) = default;

        // rotation with a specific index, use the ROT_xxxx constants from above
        // invalid indices will result in an invalid transformation
        template <std::integral I>
        explicit TRotation(I r) : rotation_(std::clamp<std::intmax_t>(r, 0, NUM_ROTATION_MATRICES))
        {
        }

        // tries to initialize the rotation with a matrix given by the axis and amount (in degree)
        // if no such rotation is handled by the class it will be initialized to invalid rotation
        explicit TRotation(const BurrTools::Maths::RVector<int> & axis, BurrTools::Maths::DegreeInt amount);

        // get the rotation matrix for this rotation
        [[nodiscard]] const BurrTools::Maths::RMatrix<int> & matrix() const noexcept { return rotationMatrices[rotation_]; }

        // get the description of this rotation, e.g. rotation around what axis and by how much
        // attention this only works for non-mirroring rotations, in other cases you will
        // get an exception... because the information doesn't really make sense for mirroring transformations
        [[nodiscard]] const Rotation & matrixInfo() const { return rotationAngles.at(rotation_); }

        // get the homogeneous 4x4 rotation matrix for this rotation,
        // D must be a floating point type
        template <std::floating_point D>
        explicit operator BurrTools::Maths::Matrix4<D>() const noexcept
        {
            const auto & r = matrix();

            return BurrTools::Maths::Matrix4<D>{D(r[0]) / r.denominator(),
                                                D(r[1]) / r.denominator(),
                                                D(r[2]) / r.denominator(),
                                                D(0),
                                                D(r[3]) / r.denominator(),
                                                D(r[4]) / r.denominator(),
                                                D(r[5]) / r.denominator(),
                                                D(0),
                                                D(r[6]) / r.denominator(),
                                                D(r[7]) / r.denominator(),
                                                D(r[8]) / r.denominator(),
                                                D(0),
                                                D(0),
                                                D(0),
                                                D(0),
                                                D(1)};
        }

        // get the 3x3 rotation matrix for this rotation
        template <std::floating_point D>
        explicit operator BurrTools::Maths::Matrix<D>() const noexcept
        {
            const auto & r = matrix();

            return BurrTools::Maths::Matrix<D>{D(r[0]) / r.denominator(),
                                               D(r[1]) / r.denominator(),
                                               D(r[2]) / r.denominator(),
                                               D(r[3]) / r.denominator(),
                                               D(r[4]) / r.denominator(),
                                               D(r[5]) / r.denominator(),
                                               D(r[6]) / r.denominator(),
                                               D(r[7]) / r.denominator(),
                                               D(r[8]) / r.denominator()};
        }

        // check if the rotation represents a valid transformation
        [[nodiscard]] explicit operator bool() const noexcept { return rotation_ < NUM_ROTATION_MATRICES; }

        // return the rotation index
        [[nodiscard]] int value() const noexcept { return rotation_; }

        // check if the transformation contains a mirroring component and not just rotation.
        [[nodiscard]] bool isMirror() const noexcept { return rotation_ >= NUM_ROTATION_MATRICES / 2; }

        // check, if the matrix is the identity matrix, e.g. no rotation at all
        [[nodiscard]] bool isIdendity() const noexcept { return rotation_ == 0; }

        TRotation & operator=(const TRotation & r) noexcept = default;

        [[nodiscard]] friend bool operator==(TRotation m1, TRotation m2) noexcept = default;

        // return inverse of the rotation
        [[nodiscard]] friend TRotation operator~(const TRotation m) noexcept { return inverseMatrix[m.rotation_]; }

        // get the rotation for the resulting transformation when
        // applying the 2 rotations one after the other
        [[nodiscard]] friend TRotation operator*(const TRotation lhs, const TRotation rhs) noexcept { return matrixMult[rhs.rotation_][lhs.rotation_]; }

        // apply rotation to a vector
        template <class T>
        [[nodiscard]] friend auto operator*(const TRotation t, const BurrTools::Maths::RVector<T> & in) noexcept
        {
            return rotationMatrices[t.rotation_] * in;
        }

        template <class T>
        [[nodiscard]] friend auto operator*(const TRotation t, const BurrTools::Maths::Vector<T> & in) noexcept
        {
            return rotationMatrices[t.rotation_] * in;
        }
};

// this class represents a set of TRotations.
// set here is a mathematical set, a collections of things
class TRotationSet
{
    private:
        // because we only have 60 different TRotation values, we simply use a 64 bit bitmask
        // to represent the set
        static_assert(TRotation::NUM_ROTATION_MATRICES < 64);
        uint64_t set = 0;

        explicit TRotationSet(const uint64_t val) : set(val) {}

    public:
        struct All
        {
        };

        // construct empty set
        TRotationSet() = default;

        // construct a set containing all possible rotations
        explicit TRotationSet([[maybe_unused]] All a) : set((UINT64_C(1) << TRotation::NUM_ROTATION_MATRICES) - 1) {}

    private:
        class EndIterator
        {
        };

        // the iterator keeps a copy of the set and eliminates the rotations from it one
        // after another by shifting right
        class Iterator
        {
            private:
                // the remainder of the set shifted down by pos bits
                uint64_t set;
                int pos;

                void jumpToNextSetBit()
                {
                    if (set)
                    {
                        // if the lowest bit is set countr will return zero and nothing will be done
                        // if all bits are zero, 64 will be returned and pos will go up
                        // but as you are not supposed to dereference an iterator that says it is at
                        // the end, everything will be nice
                        const int bits = std::countr_zero(set);
                        pos += bits;
                        set >>= bits;
                    }
                    else
                    {
                        pos += 64;
                    }
                }

            public:
                explicit Iterator(uint64_t s) : set(s), pos(0) { jumpToNextSetBit(); }

                // behavior undefined, if called after end
                Iterator & operator++() noexcept
                {
                    // we want to remove the lowest bit,
                    // as we know it is set because otherwise we would not be here
                    // so subtracting one results in removin the lowest bit
                    set--;
                    jumpToNextSetBit();
                    return *this;
                }

                // as usual with iterators, don't dereference, once the comparison with the end iterator
                // returns that the iterator is identical to the end iterator
                [[nodiscard]] TRotation operator*() const noexcept { return TRotation(pos); }

                [[nodiscard]] bool operator!=(EndIterator) const noexcept { return set != 0; }
        };

    public:
        // you can iterate over all the TRotations that are inside the set
        // the order will be according to the defines in TRotation so first
        // all non mirroring transformations and then the mirroring
        [[nodiscard]] Iterator begin() const noexcept { return Iterator{set}; }

        [[nodiscard]] static EndIterator end() noexcept { return EndIterator{}; }

        // check, if a rotation is in the set
        [[nodiscard]] bool contains(const TRotation r) const noexcept { return ((set >> r.value()) & 1) != 0; }

        // set operations on two sets
        [[nodiscard]] friend TRotationSet operator&(const TRotationSet a, const TRotationSet b) noexcept { return TRotationSet(a.set & b.set); }

        [[nodiscard]] friend TRotationSet operator-(const TRotationSet a, const TRotationSet b) noexcept { return TRotationSet(a.set & ~b.set); }

        [[nodiscard]] friend TRotationSet operator|(const TRotationSet a, const TRotationSet b) noexcept { return TRotationSet(a.set | b.set); }

        // add a single rotation to the set
        void add(const TRotation r) noexcept { set |= (UINT64_C(1) << r.value()); }

        // compare 2 sets
        [[nodiscard]] bool operator==(const TRotationSet & s) const noexcept = default;

        // number of elements inside the set
        [[nodiscard]] bool empty() const noexcept { return set == 0; }

        [[nodiscard]] int size() const noexcept { return std::popcount(set); }

        // this is only for printing debug messages... don't use in production or expect any values
        // this could potentially change
        [[nodiscard]] uint64_t value() const noexcept { return set; }

        // keep only the non-mirroring transformations inside the set
        void removeMirrorTransformations() noexcept
        {
            // simply mask out all bits above 30
            set &= ((UINT64_C(1) << TRotation::ROT_MIRROR) - 1);
        }

        // remove all BUT the mirror transformations
        void removeNonMirrorTransformations() noexcept
        {
            // simply mask out all bits above below
            set &= ~((UINT64_C(1) << TRotation::ROT_MIRROR) - 1);
        }

        // check if there is any mirror transformation inside the set
        [[nodiscard]] bool containsMirrorTransformations() const noexcept { return set >= (UINT64_C(1) << TRotation::ROT_MIRROR); }
};

struct TMatrixDefaultUserData
{
        [[nodiscard]] friend bool operator==(const TMatrixDefaultUserData & u1, const TMatrixDefaultUserData & u2) noexcept = default;
};

// this class represents a complete transformation matrix as required for the
// grids within BurrTools. That is: the rotations from TRotations and
// transformations by rational values.
//
// It is possible to add user data to this class to get a more memory efficient
// class. TMatrix will never touch that data, not in initialisation, not on
// copying, not when multiplying matrices, never ever, it is your data and you
// have to take care of it. Per default the user data will be nothing and not waste
// any space, use the 2nd template argument to add your own data. This will be a
// different type though (but assignment between different types is possible. You
// can access it via the user_ member variable.
template <std::signed_integral D, class U = TMatrixDefaultUserData>
class TMatrix
{
    public:
        using value_type = D;

    private:
        BurrTools::Maths::RVector<D> dir_;
        TRotation rotation_;

    public:
        [[no_unique_address]] U user_;

        // construct invalid matrix
        TMatrix() = default;

        // construct matrix containing only rotation
        explicit TMatrix(TRotation r) { rotation_ = r; }

        // full constructor using RVector or Vector for translation
        TMatrix(TRotation r, const BurrTools::Maths::RVector<D> & d)
        {
            dir_      = d;
            rotation_ = r;
        }

        TMatrix(TRotation r, const BurrTools::Maths::Vector<D> & d)
        {
            dir_      = d;
            rotation_ = r;
        }

        // translation only using RVector or Vector
        explicit TMatrix(const BurrTools::Maths::RVector<D> & d)
        {
            dir_      = d;
            rotation_ = TRotation{TRotation::ROT_IDENTITY};
        }

        explicit TMatrix(const BurrTools::Maths::Vector<D> & d)
        {
            dir_      = d;
            rotation_ = TRotation{TRotation::ROT_IDENTITY};
        }

        template <class M>
        explicit TMatrix(const TMatrix<M> & m) : dir_(m.translation()),
                                                 rotation_(m.rotation())
        {
        }

        // check if the matrix represents a valid transformation
        [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(rotation_); }

        // check if the transformation contains a mirroring component and not just rotation and translation
        [[nodiscard]] bool isMirror() const noexcept { return rotation_.isMirror(); }

        // check, if the matrix contains only translational components
        [[nodiscard]] bool isTranslationOnly() const noexcept { return rotation_.isIdendity(); }

        // check, if the matrix is the identity matrix
        [[nodiscard]] bool isIdendity() const noexcept { return rotation_.isIdendity() && dir_.isZero(); }

        // return the rotation, which uniquely represents what kind of rotation
        // this matrix stands for
        [[nodiscard]] TRotation rotation() const noexcept { return rotation_; }

        // return the translational component of the matrix
        [[nodiscard]] const auto & translation() const noexcept { return dir_; }

        [[nodiscard]] friend bool operator==(const TMatrix & m1, const TMatrix & m2) noexcept = default;

        // an assignment class that allows to assign from a TMatrix with different user data
        // the user data is NOT copied, only the other stuff, it is your task to copy
        // the user data
        template <class D2, class U2>
        TMatrix & operator=(const TMatrix<D2, U2> & o) noexcept
        {
            rotation_ = o.rotation();
            dir_      = o.translation();
            return *this;
        }

        // return inverse of the matrix
        [[nodiscard]] friend TMatrix operator~(const TMatrix & m) noexcept
        {
            TMatrix res;

            if (m)
            {
                auto inv = ~(m.rotation_);
                res      = TMatrix(inv, -(inv * m.dir_));
            }

            return res;
        }

        // multiply vector and matrix, as our vectors are column vectors, the matrix
        // needs to be on the left and the vector on the right the matrix needs to be a valid
        // matrix. Otherwise an exception is thrown
        [[nodiscard]] friend auto operator*(const TMatrix & lhs, const BurrTools::Maths::RVector<D> & rhs)
        {
            if (!lhs)
            {
                throw std::runtime_error("trying to apply an invalid TMatrix");
            }

            return (lhs.rotation_ * rhs) + lhs.dir_;
        }

        template <class S>
        [[nodiscard]] friend auto operator*(const TMatrix & lhs, const BurrTools::Maths::Vector<S> & rhs)
        {
            if (!lhs)
            {
                throw std::runtime_error("trying to apply an invalid TMatrix");
            }

            return (lhs.rotation_ * rhs) + lhs.dir_;
        }

        // get the 4x4 homogeneous transformation matrix for this matrix
        // D2 must be a floating point type
        template <class D2>
        explicit operator BurrTools::Maths::Matrix4<D2>() const noexcept
        {
            static_assert(std::numeric_limits<D2>::is_iec559);
            auto & r = rotation_.matrix();

            return BurrTools::Maths::Matrix4<D2>{D2(r[0]) / r.denominator(),
                                                 D2(r[1]) / r.denominator(),
                                                 D2(r[2]) / r.denominator(),
                                                 D2(dir_.x()) / dir_.denominator(),
                                                 D2(r[3]) / r.denominator(),
                                                 D2(r[4]) / r.denominator(),
                                                 D2(r[5]) / r.denominator(),
                                                 D2(dir_.y()) / dir_.denominator(),
                                                 D2(r[6]) / r.denominator(),
                                                 D2(r[7]) / r.denominator(),
                                                 D2(r[8]) / r.denominator(),
                                                 D2(dir_.z()) / dir_.denominator(),
                                                 D2(0),
                                                 D2(0),
                                                 D2(0),
                                                 D2(1)};
        }
};

// we explicitly do not support *= operator for TMatrix because you usually
// want to pre-multiply a matrix to add another transformation to an existing one
// but *= would post multiply keeping the accumulating matrix unchanged and changing
// the new to be accumulated matrix
//
// That is why there is only this normal multiplication, which makes this
// explicit. The resulting matrix will not contain any user data
template <class D1, class D2, class P1, class P2>
[[nodiscard]] auto operator*(const TMatrix<D1, P1> & lhs, const TMatrix<D2, P2> & rhs) noexcept
{
    using res_type = std::common_type_t<D1, D2>;

    TMatrix<res_type> res;

    if (TRotation m = lhs.rotation() * rhs.rotation(); m) res = TMatrix<res_type>(m, lhs.translation() + lhs.rotation() * rhs.translation());

    return res;
}

// 2 additional matrix multiplications to multiply TRotations an TMatrices together, we could write them out
// in detail, but the compiler should be smart enough to recognize that the temporary matrix created in here
// contains zeros in the translation part and optimize accordingly
// as above any user-data will get lost in the result.
template <class D, class P>
[[nodiscard]] TMatrix<D> operator*(const TRotation & lhs, const TMatrix<D, P> & rhs) noexcept
{
    return TMatrix<D>(lhs) * rhs;
}

template <class D, class P>
[[nodiscard]] TMatrix<D> operator*(const TMatrix<D, P> & lhs, const TRotation & rhs) noexcept
{
    return lhs * TMatrix<D>(rhs);
}

} // namespace BurrTools::PuzzleLibrary
