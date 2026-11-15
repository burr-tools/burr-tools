/* BurrTools
 *
 * Ported from BurrTools NG by Andreas Röver (https://gitlab.com/roever/burrtools),
 * src/lib/symmetry.h at commit fdae7951, for grids as data
 * (design/2026-11-19-ng-merge-plan.md, step G1). NG is licensed under the
 * GNU General Public License version 3, and so is this file.
 *
 * Changes from NG's file: none.
 * Keep the rest identical to NG's file, so changes can be exchanged both ways.
 */
#pragma once

#include "issymmetricalong.h"
#include "transformation.h"

#include <array>

namespace BurrTools::PuzzleLibrary {

// forward declarations to prevent includes
class Grid;

// This module contains classes dealing with symmetry of a shape
//
// just as a quick introduction the common term symmetry is a bit fuzzy in its
// usage so we use the mathematical definitions here
//
// a shape has a symmetry x, if x is a transformation in 3d space that
// transforms the shape in such a way that it looks identical to before the
// transformation.  Depending on the situation this may or may not include
// translational components. E.g when someone talks about the symmetries of the
// cube one usually only mentions the rotational and mirror symmetries and
// discards the actual position in space of that cube. This is done here as
// well. One class only considers the rotational and mirror part
// (RotationalSymmetryGroup) and the other class includes the actual position
// in space of the object talked about (SymmetryGoup).
//
// A symmetry group is a set of transformations that together are all the
// symmetries of a shape. It is called "group" because the transformations in
// there form a mathematical group. Please look that up on Wikipedia under
// "Group_(mathematics)"
//
// The symmetry groups in here only handle symmetries and groups that can be
// represented by the types in transformation that BurrTools uses, e.g. with
// the finite set of 60 different transformations. Stuff like a 5-fold
// rotational symmetry is not possible to represent.
//
// With these symmetries there is only a limited set of possible groups.
// All the information for these groups is calculated at start-up.

// This class represents just the rotational symmetries of a shape.
// but because shapes can have a centre of symmetry somewhere in space
// it can not represent the real symmetry transformations of that shape
// this is done by the SymmetryGroup class below
class RotationalSymmetryGroup
{
    private:
        // id of the group with no symmetries (only self-symmetry)
        static constexpr int GRP_NONE = 0;
        // total number of possible different symmetry groups. A shape always has
        // one of the groups... if not it is a bug in BurrTools
        // the value 136 is a priory knowledge and verified once the internal
        // lookups have been calculated
        static constexpr int GRP_COUNT = 136;

        // the list of all possible Groups that exist plus invalid group
        static std::array<TRotationSet, GRP_COUNT + 1> GroupSymmetries;

        // the decision tree to find out which rotational symmetry group a
        // shape has by checking some of the possible transformations, but as
        // few as possible
        //
        // start with the first entry then do the following:
        // if the 0x8000 bit is set it is a terminal node and represents the index
        // into the Groups array, if not
        // it contains a transformation number in the low 6 bits, check that
        // transformation if the shape is symmetric according to this
        // transformation continue with the next entry within the array, if
        // not, then continue with the index that is given in the bits 6..14
        // bits (without the msb of course)
        static std::array<uint16_t, GRP_COUNT * 2 - 1> DecisionTree;

        // explanation for "normalized rotations"
        // suppose you have a shape whose symmetry group you know and
        // you have 2 different transformations of that shape. How do you quickly
        // find out whether the 2 orientations result in an identical looking
        // shape?
        //
        // By normalizing the transformation. A rotation is normalized for a
        // shape with a given symmetry, if the rotation is the one with the
        // smallest rotation index that achieves the same looking shape.
        // When comparing the normalized orientations is it possible to
        // conclude that the shape looks the same, or different simply by
        // comparing the applied transformation
        //
        // The normalized orientation depends on the symmetry of the shape.
        // Take a simple 2d example a rectangle: it can be rotated by
        // 0, 90, 180, 270 degrees, but the 180 degree rotation is
        // identical to 0, so when normalizing a 180 rotation becomes
        // a 0 rotation, similar the 270 becomes a 90 degree rotation.
        //
        // If you do the same for a square all four transformations become a
        // 0 degree rotation because all 4 look the same.

        // this array contains the rotation that needs to be applied to the
        // shape to transfer it to a normalized orientation. Applying
        // this transformation to a shape with the given symmetry group will
        // never change the shape (keep in mind that we are not considering
        // position in space only orientation)
        // but applying this transformation to the current transformation
        // matrix that transformed the shape will make the rotational
        // part inside that transformation matrix be normalized
        static std::array<TRotation, (GRP_COUNT + 1) * TRotation::NUM_ROTATION_MATRICES> Normalizer;

        // this array contains the normalized orientation itself
        static std::array<TRotation, (GRP_COUNT + 1) * TRotation::NUM_ROTATION_MATRICES> Normalized;

        // see below in getter to understand what this is
        static std::array<TRotationSet, (GRP_COUNT + 1) * (GRP_COUNT + 1)> Uniques;

        // what group do you get when you add a rotation to a rotational
        // symmetry group the new group might have more new orientations than
        // just the one added
        static std::array<uint8_t, (GRP_COUNT + 1) * TRotation::NUM_ROTATION_MATRICES> UnifySymmetryTransformation;

        // what group do you get when you unify or intersect with another group
        static std::array<uint8_t, (GRP_COUNT + 1) * (GRP_COUNT + 1)> UnifySymmetrySymmetry;
        static std::array<uint8_t, (GRP_COUNT + 1) * (GRP_COUNT + 1)> IntersectSymmetrySymmetry;

        // the group that this class instance stands for, index into Groups array
        uint8_t group = GRP_NONE;

        // the group of the grid of the shape that we calculated from
        uint8_t gridGroup = GRP_NONE;

        // helper function that calculates all possible symmetries that a shape
        // of the given grid can have that has its symmetry centre at the given
        // position
        static std::tuple<RotationalSymmetryGroup, uint8_t> calculateRotationalSubset(const Grid & g, const BurrTools::Maths::RVector<int> & center);

        explicit RotationalSymmetryGroup(int g) : group(std::min(g, GRP_COUNT)), gridGroup(group) {}

    public:
        // call this before using this class
        static void init_tables();

        // constructor for a no symmetries symmetry group
        RotationalSymmetryGroup() = default;

        // copy constructor
        RotationalSymmetryGroup(const RotationalSymmetryGroup & orig) = default;

    private:
        class allEndIterator
        {
        };

        // the iterator keeps a copy of the set and eliminates the rotations from is one
        // after another by shifting right
        class allIterator
        {
            private:
                int i = 0;

            public:
                allIterator() = default;

                allIterator & operator++() noexcept
                {
                    ++i;
                    return *this;
                }

                [[nodiscard]] auto operator*() const noexcept { return RotationalSymmetryGroup(i); }

                [[nodiscard]] bool operator!=(allEndIterator) const noexcept { return i < GRP_COUNT; }
        };

        // iterate stuff to iterate over all possible groups
        class allContainer
        {
            public:
                static auto begin() { return allIterator(); }

                static auto end() { return allEndIterator(); }
        };

    public:
        // this function returns a range that allows to iterate
        // over all possible symmetry groups of BurrTools
        // the elements of the range are the RotationalSymmetryGroups
        // one after the other
        [[nodiscard]] static auto allGroups() { return allContainer{}; }

        // check if the group is valid
        [[nodiscard]] explicit operator bool() const noexcept { return group < GRP_COUNT; }

        // this constructor calculates the symmetry that a given shape has
        // with the given the centre of symmetry. If the given centre is not
        // the symmetry centre of the shape you will get a different group
        template <class V>
        RotationalSymmetryGroup(const V & shape, const BurrTools::Maths::RVector<int> & center)
        {
            // collect all potential symmetries for the given shape
            // this is done by looking on what symmetry axes and mirror planes the
            // given symmetry centre is situated
            RotationalSymmetryGroup subset;

            std::tie(subset, gridGroup) = calculateRotationalSubset(shape.grid(), center);

            // now check, if the shape actually has that symmetry
            int pos = 0;

            while ((DecisionTree[pos] & 0x8000) == 0)
            {
                // find next rotation to check
                // in the lower 6 bits ... enough for 64 rotations... we need 60
                const TRotation rotation(DecisionTree[pos] & 0x3F);

                // we only check, rotations that are in the given subset, if not
                // we assume that the shape is not symmetrical along this transformation
                //
                // calculate the complete transformation to check, this transformation
                // is assembled out of first moving the symmetry centre to the origin,
                // then performing the rotation and then translating the centre
                // back to where it was
                //
                // in this place there also used to be a check that the translation part of the transformation
                // is actually on the grid. But this is no longer needed, as all the transformations
                // that we check are in the subset, which contains only valid transformations
                // of the given shape, e.g. transformations we already know that we are on the rotation axis
                // or on the mirror plane
                if (subset.symmetries().contains(rotation) && isSymmetricAlong(shape, TMatrix<int>(center) * TMatrix<int>(rotation) * TMatrix<int>(-center)))
                {
                    // shape is symmetrical, next node
                    pos++;
                }
                else
                {
                    // shape is not symmetric along the rotation we checked, so
                    // take the other node given in tree
                    pos = DecisionTree[pos] >> 6;
                }
            }

            // the resulting symmetry group is in the final node in the lower 7 bits
            group = DecisionTree[pos] & 0x7FFF;
        }

        // get the set of transformations for this symmetry group
        [[nodiscard]] TRotationSet symmetries() const { return GroupSymmetries[group]; }

        [[nodiscard]] friend bool operator==(const RotationalSymmetryGroup lhs, const RotationalSymmetryGroup rhs) noexcept { return lhs.group == rhs.group; }

        [[nodiscard]] friend bool operator!=(const RotationalSymmetryGroup lhs, const RotationalSymmetryGroup rhs) noexcept { return lhs.group != rhs.group; }

        RotationalSymmetryGroup & operator=(const RotationalSymmetryGroup & g) = default;

        // get RotationSet with all normalized unique rotations for this
        // symmetry group unique meaning each rotation in that set, when
        // applied to a shape of the given symmetry will result in a different
        // looking object. In fact with all the transformations in here you
        // will get _all_ different looking objects. So as an example: a
        // completely asymmetric object will result in all bits set, while a
        // mirror symmetric object will contain no mirror transformations.
        //
        // But there is more: when you also give the result shape symmetry the
        // resulting set will be further limited to all the orientations that
        // will give you a different arrangement within a shape of that result
        // shape. Example, a shape placed in a cube only needs to be placed
        // with one single orientations because all other orientations are
        // identical because you could re-orient the cube.
        //
        // the nomask argument, when set to true will prevent the removal of
        // rotations that are invalid for the grid that the shape belongs to
        // don't use this, this is only meant for debugging
        [[nodiscard]] TRotationSet uniqueRotations(RotationalSymmetryGroup result, bool nomask = false) const
        {
            if (nomask)
                return Uniques[group + result.group * (GRP_COUNT + 1)];
            else
                return Uniques[group + result.group * (GRP_COUNT + 1)] & GroupSymmetries[gridGroup];
        }

        [[nodiscard]] TRotationSet uniqueRotations(bool nomask = false) const { return uniqueRotations(RotationalSymmetryGroup{}, nomask); }

        // get the normalized orientation for the given orientation
        // for an explanation of "normalize" look above at the comment
        // for the Normalized array
        [[nodiscard]] TRotation normalize(const TRotation r) const { return Normalized[group * TRotation::NUM_ROTATION_MATRICES + r.value()]; }

        // get the transformation to apply to a shape with the symmetry of this object
        // that is rotated by r, such that the shape will be in normalized orientation
        [[nodiscard]] TRotation getNormalizerRotation(const TRotation r) const { return Normalizer[group * TRotation::NUM_ROTATION_MATRICES + r.value()]; }

        [[nodiscard]] uint8_t value() const { return group; }

        [[nodiscard]] uint8_t gridValue() const { return gridGroup; }

        // these are intentionally not operators, we could use a union operator for the group itself
        // but what about the grid group? Should we unify them as well? Or keep it from the first?
        // because this is not as intuitive we leave it as a member function, which makes it much
        // clearer, what is happening

        // take this group and add another symmetry, after this operation you might end up with an invalid
        // symmetry, make sure to check before you continue
        void addSymmetry(TRotation r) { group = UnifySymmetryTransformation[group * TRotation::NUM_ROTATION_MATRICES + r.value()]; }

        // unify or intersect this group with the given group
        void unify(RotationalSymmetryGroup s) { group = UnifySymmetrySymmetry[group * GRP_COUNT + s.group]; }

        void intersect(RotationalSymmetryGroup s) { group = IntersectSymmetrySymmetry[group * GRP_COUNT + s.group]; }

        // check, if there are mirror symmetries in the group
        [[nodiscard]] bool containsMirrorTransformations() const { return GroupSymmetries[group].containsMirrorTransformations(); }
};

// represents what symmetry group a certain shape has
// this is the symmetry centre and the rotational symmetry around that centre
// D is the type used for the vector of the symmetry centre
template <class D>
class SymmetryGroup
{
    private:
        RotationalSymmetryGroup group_;

        // centre of symmetry for the object
        BurrTools::Maths::RVector<D> center_;

    public:
        SymmetryGroup() = default;

        SymmetryGroup(const SymmetryGroup & orig) = default;

        // constructors that calculate the symmetry of the given voxel shape or voxel set
        template <class V>
        explicit SymmetryGroup(const V & shape)
        {
            BurrTools::Maths::Vector<int> center;

            int count = 0;
            // use centre of mass for symmetry centre
            for (auto [position, val] : shape)
            {
                center += position;
                count++;
            }

            if (count > 0)
            {
                // calculate the centre of the shape
                center_ = BurrTools::Maths::RVector(center, count);
                center_.reduce();

                group_ = RotationalSymmetryGroup(shape, center_);
            }
            // otherwise the default is ok
        }

        // gets the transformation matrix that re-orients a piece
        // according to its symmetries, e.g when you enter one of the
        // symmetry rotations in here you will re-orient the piece
        // without movement to its new orientation
        template <class D2 = D>
        [[nodiscard]] TMatrix<D2> getRotation(TRotation rot) const
        {
            return TMatrix<D2>(center_) * TMatrix<D2>(rot) * TMatrix<D2>(-center_);
        }

        // normalize the matrix given. Normalize means that of all possible transformation
        // that would result in the same shape as given by in, choose a unique one
        // and return a matrix that uses that transformation to transform the shape
        // the same way as in does
        template <class D2>
        [[nodiscard]] TMatrix<D2> normalize(const TMatrix<D2> & in) const
        {
            TRotation rot = group_.getNormalizerRotation(in.rotation());

            // final result is to first apply rot in such a way, that
            // it doesn't change the shape of the object due to its symmetry
            // and then apply the original transformation
            return in * getRotation<D2>(rot);
        }

        [[nodiscard]] const BurrTools::Maths::RVector<D> & center() const { return center_; }

        [[nodiscard]] RotationalSymmetryGroup group() const { return group_; }

        [[nodiscard]] friend bool operator==(const SymmetryGroup & lhs, const SymmetryGroup & rhs) noexcept = default;

        SymmetryGroup & operator=(const SymmetryGroup & r) = default;

        // get the rotation set with the unique rotations,
        // beware though: simply applying the rotations will move your object
        // around, if you want to apply the rotation "properly" use the get
        // rotation function from above with the rotations that are inside this set
        [[nodiscard]] TRotationSet uniqueRotations() const { return group_.uniqueRotations(); }

        [[nodiscard]] TRotationSet uniqueRotations(SymmetryGroup result) const { return group_.uniqueRotations(result.group_); }
};

} // namespace BurrTools::PuzzleLibrary
