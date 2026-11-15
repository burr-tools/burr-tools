/* BurrTools
 *
 * Ported from BurrTools NG by Andreas Röver (https://gitlab.com/roever/burrtools),
 * src/lib/transformation.cpp at commit fdae7951, for grids as data
 * (design/2026-11-19-ng-merge-plan.md, step G1). NG is licensed under the
 * GNU General Public License version 3, and so is this file.
 *
 * Changes from NG's file: std::print (C++23) replaced by fprintf, exit by abort.
 * Keep the rest identical to NG's file, so changes can be exchanged both ways.
 */
#include "transformation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using BurrTools::Maths::DegreeInt;
using BurrTools::Maths::RMatrix;

namespace BurrTools::PuzzleLibrary {

// this array contains "descriptions" of all the rotations that we need in
// BurrTools descriptions means the rotation axis and the angle around that
// axis
//
// beside this set you additionally have a set of the same size that contains a
// mirror component
//
// usually you'd assume that this set forms a mathematical group. But BurrTools
// supports 2 kinds of geometry and the 2 groups needed for those geometries
// can not be unified in a finite group, so this list contains all
// transformations for 2 separate groups
//
// theoretically we could also create this list of transformations using just a
// few "generator" transformations, e.g. a 90° rotation around x and y each and
// a mirror transformation for the cube grid. But we need the transformations to
// be in a certain order for the ROT_xxxx constants to properly work. That is
// why we enumerate them here manually
// clang-format off
const std::array<TRotation::Rotation, TRotation::NUM_ROTATION_MATRICES / 2> TRotation::rotationAngles = {
    {
    // identity matrix
    { .direction={ 0,  0,  0},   .angle=DegreeInt {   0 } },  //  0
    // rotation around z axis
    { .direction={ 0,  0,  1},   .angle=DegreeInt {  90 } },  //  1
    { .direction={ 0,  0,  1},   .angle=DegreeInt { 180 } },  //  2
    { .direction={ 0,  0,  1},   .angle=DegreeInt { 270 } },  //  3
    // rotation around y axis
    { .direction={ 0,  1,  0},   .angle=DegreeInt {  90 } },  //  4
    { .direction={ 0,  1,  0},   .angle=DegreeInt { 180 } },  //  5
    { .direction={ 0,  1,  0},   .angle=DegreeInt { 270 } },  //  6
    // rotation around x axis
    { .direction={ 1,  0,  0},   .angle=DegreeInt {  90 } },  //  7
    { .direction={ 1,  0,  0},   .angle=DegreeInt { 180 } },  //  8
    { .direction={ 1,  0,  0},   .angle=DegreeInt { 270 } },  //  9
    // rotations around 45° diagonals
    { .direction={ 0,  1,  1},   .angle=DegreeInt { 180 } },  // 10
    { .direction={ 0,  1, -1},   .angle=DegreeInt { 180 } },  // 11
    { .direction={ 1,  0,  1},   .angle=DegreeInt { 180 } },  // 12
    { .direction={ 1,  0, -1},   .angle=DegreeInt { 180 } },  // 13
    { .direction={ 1,  1,  0},   .angle=DegreeInt { 180 } },  // 14
    { .direction={ 1, -1,  0},   .angle=DegreeInt { 180 } },  // 15
    // rotations around the vector [1,1,1] with 60° steps
    { .direction={ 1,  1,  1},   .angle=DegreeInt {  60 } },  // 16
    { .direction={ 1,  1,  1},   .angle=DegreeInt { 120 } },  // 17
    { .direction={ 1,  1,  1},   .angle=DegreeInt { 180 } },  // 18
    { .direction={ 1,  1,  1},   .angle=DegreeInt { 240 } },  // 19
    { .direction={ 1,  1,  1},   .angle=DegreeInt { 300 } },  // 20
    // rotations around the other fully diagonal axes with 120° steps only
    { .direction={ 1,  1, -1},   .angle=DegreeInt { 120 } },  // 21
    { .direction={ 1,  1, -1},   .angle=DegreeInt { 240 } },  // 22
    { .direction={ 1, -1,  1},   .angle=DegreeInt { 120 } },  // 23
    { .direction={ 1, -1,  1},   .angle=DegreeInt { 240 } },  // 24
    { .direction={-1,  1,  1},   .angle=DegreeInt { 120 } },  // 25
    { .direction={-1,  1,  1},   .angle=DegreeInt { 240 } },  // 26
    // some crazy stuff
    { .direction={ 1,  1, -2},   .angle=DegreeInt { 180 } },  // 27
    { .direction={ 1, -2,  1},   .angle=DegreeInt { 180 } },  // 28
    { .direction={-2,  1,  1},   .angle=DegreeInt { 180 } },  // 29
} };
// clang-format on

std::array<RMatrix<int>, TRotation::NUM_ROTATION_MATRICES + 1> TRotation::rotationMatrices;
std::array<std::array<TRotation, TRotation::NUM_ROTATION_MATRICES + 1>, TRotation::NUM_ROTATION_MATRICES + 1> TRotation::matrixMult;
std::array<TRotation, TRotation::NUM_ROTATION_MATRICES + 1> TRotation::inverseMatrix;

// get integer close to a float, error, when not close enough
static int toInt(float f)
{
    const float fr = std::round(f);

    // if not close to a whole integer... something is wrong
    const float d = std::fabs(f - fr);

    if (d > 0.01)
    {
        fprintf(stderr, "one transformation matrix can not be created value %f needed\n", f);
        std::abort();
    }

    return static_cast<int>(fr);
}

// this function is used to calculate the lookups from above
void TRotation::init_tables()
{
    // calculate the matrix from the rotation angles
    for (unsigned int i = 0; i < NUM_ROTATION_MATRICES; i++)
    {
        // in theory, we could use a reference here as matrixInfo returns a reference
        // to a static array, but if we ever change that this would break, so better copy
        // the information
        const auto r     = TRotation(i % (NUM_ROTATION_MATRICES / 2)).matrixInfo();
        RMatrix<int> & m = TRotation::rotationMatrices[i];

        const int tm = TRotation(i).isMirror() ? -1 : 1;

        // calculate the matrix from axis and angle

        const float t  = BurrTools::Maths::degreeToRad(r.angle);
        const float s  = std::sin(t);
        const float c  = std::cos(t);
        const float ul = std::sqrtf(r.direction * r.direction);

        if (std::fabs(ul) < 0.1)
        {
            // no axis given, use identity matrix
            m = RMatrix(tm);
        }
        else
        {
            const float ux  = r.direction.x() / ul;
            const float uy  = r.direction.y() / ul;
            const float uz  = r.direction.z() / ul;
            const float tm3 = TRotation(i).isMirror() ? -3.0f : 3.0f;

            // source for the following formulas: wikipedia article on transformation matrices
            // the tm part performs the point reflection across the origin
            //
            // the factor of three is a guess that all matrices that
            // we get here use maximally fractions with a denominator of 3
            // so simply always multiply that in
            m = RMatrix(toInt(tm3 * (c + ux * ux * (1 - c))),
                        toInt(tm3 * (ux * uy * (1 - c) - uz * s)),
                        toInt(tm3 * (ux * uz * (1 - c) + uy * s)),
                        toInt(tm3 * (uy * ux * (1 - c) + uz * s)),
                        toInt(tm3 * (c + uy * uy * (1 - c))),
                        toInt(tm3 * (uy * uz * (1 - c) - ux * s)),
                        toInt(tm3 * (uz * ux * (1 - c) - uy * s)),
                        toInt(tm3 * (uz * uy * (1 - c) + ux * s)),
                        toInt(tm3 * (c + uz * uz * (1 - c))),
                        3);
        }
    }

    // append the final invalid matrix (use identity matrix here)
    TRotation::rotationMatrices[NUM_ROTATION_MATRICES] = RMatrix<int>();

    // calculate multiplication lookup and inversion lookup
    for (unsigned int i = 0; i < NUM_ROTATION_MATRICES; i++)
    {
        for (unsigned int j = 0; j < NUM_ROTATION_MATRICES; j++)
        {
            // try to find the matrix the corresponds to the multiplication resulting matrix within all rotation matrices
            const auto f = std::ranges::find(rotationMatrices, rotationMatrices[j] * rotationMatrices[i]);

            // as the complete set of transformations is not closed
            // and thus not a group is may happen that we don't find
            // the multiplication. In this case we simply leave the
            // default invalid rotation inside the matrixMult array
            if (f != rotationMatrices.end()) matrixMult[i][j] = TRotation(std::distance(rotationMatrices.begin(), f));

            // when the resulting matrix is the identity matrix we found the inverse
            if (f == rotationMatrices.begin()) inverseMatrix[i] = TRotation(j);
        }
    }

    // the remaining entries in inverse and matrixMult are automatically initialized
    // to invalid by the default constructor
}

TRotation::TRotation(const BurrTools::Maths::RVector<int> & axis, DegreeInt amount)
{
    // this is mainly a big switch that classifies the directions
    // of the axis

    // first though get the angle into the range 0..359
    if (amount < DegreeInt{0})
        amount = (DegreeInt{360} - ((-amount) % DegreeInt{360})) % DegreeInt{360};
    else
        amount %= DegreeInt{360};

    // if amount is zero, no rotation takes place
    if (amount == DegreeInt{0})
    {
        rotation_ = ROT_IDENTITY;
        return;
    }

    // now amount is in the range of 1..359

    // calculate a bitmask of which of the axis dimensions are not zero
    int values = 0;
    if (axis.x() != 0) values |= 1;
    if (axis.y() != 0) values |= 2;
    if (axis.z() != 0) values |= 4;

    // depending on the non-zero-ness of the axis dimensions we can categorize the axis
    switch (values)
    {
        case 1:
            // only x is not zero, the value of the x-component is not important
            // if the given axis points in the inverse direction as the rotation axis, invert
            // the amount, because it is 1..359, the inversion will again leave us with 1..359
            if (axis.x() < 0) amount = DegreeInt{360} - amount;

            if (amount == DegreeInt{90})
            {
                rotation_ = ROT_090_X;
                return;
            }
            if (amount == DegreeInt{180})
            {
                rotation_ = ROT_180_X;
                return;
            }
            if (amount == DegreeInt{270})
            {
                rotation_ = ROT_270_X;
                return;
            }
            break;

        case 2:
            // only y is not zero
            if (axis.y() < 0) amount = DegreeInt{360} - amount;

            if (amount == DegreeInt{90})
            {
                rotation_ = ROT_090_Y;
                return;
            }
            if (amount == DegreeInt{180})
            {
                rotation_ = ROT_180_Y;
                return;
            }
            if (amount == DegreeInt{270})
            {
                rotation_ = ROT_270_Y;
                return;
            }

            break;

        case 4:
            // only y is not zero
            if (axis.z() < 0) amount = DegreeInt{360} - amount;

            if (amount == DegreeInt{90})
            {
                rotation_ = ROT_090_Z;
                return;
            }
            if (amount == DegreeInt{180})
            {
                rotation_ = ROT_180_Z;
                return;
            }
            if (amount == DegreeInt{270})
            {
                rotation_ = ROT_270_Z;
                return;
            }

            break;

        // cases for 2-axis diagonals
        // in this case the values for the 2 axes must be identical and only the case for 180° rotations is
        // supported. Also we check, whether the sign of the 2 dimensions is identical or opposite and use
        // the corresponding rotation axis. We don't need to check the direction of the given axis because
        // for a 180° rotation the result is the same
        case 3:
            if (amount == DegreeInt{180} && (axis.x() == axis.y() || axis.x() == -axis.y()))
            {
                rotation_ = ((axis.x() < 0) == (axis.y() < 0)) ? ROT_180_XY : ROT_180_Xy;
                return;
            }
            break;
        case 5:
            if (amount == DegreeInt{180} && (axis.x() == axis.z() || axis.x() == -axis.z()))
            {
                rotation_ = ((axis.x() < 0) == (axis.z() < 0)) ? ROT_180_XZ : ROT_180_Xz;
                return;
            }
            break;
        case 6:
            if (amount == DegreeInt{180} && (axis.y() == axis.z() || axis.y() == -axis.z()))
            {
                rotation_ = ((axis.y() < 0) == (axis.z() < 0)) ? ROT_180_YZ : ROT_180_Yz;
                return;
            }
            break;

        case 7:
            // finally the x, y and z not zero case
            // first we check all cases where the absolute value for all 3 dimensions is identical
            // and only the sign differs. There are 4 cases, this are handled here
            if (axis.x() == axis.y())
            {
                if (axis.x() < 0) amount = DegreeInt{360} - amount;

                if (axis.x() == axis.z())
                {
                    // x == y == z
                    if (amount == DegreeInt{60})
                    {
                        rotation_ = ROT_060_XYZ;
                        return;
                    }
                    if (amount == DegreeInt{120})
                    {
                        rotation_ = ROT_120_XYZ;
                        return;
                    }
                    if (amount == DegreeInt{180})
                    {
                        rotation_ = ROT_180_XYZ;
                        return;
                    }
                    if (amount == DegreeInt{240})
                    {
                        rotation_ = ROT_240_XYZ;
                        return;
                    }
                    if (amount == DegreeInt{300})
                    {
                        rotation_ = ROT_300_XYZ;
                        return;
                    }
                }
                else if (axis.x() == -axis.z())
                {
                    // x == y == -z
                    if (amount == DegreeInt{120})
                    {
                        rotation_ = ROT_120_XYz;
                        return;
                    }
                    if (amount == DegreeInt{240})
                    {
                        rotation_ = ROT_240_XYz;
                        return;
                    }
                }
            }
            else if (axis.x() == -axis.y())
            {
                if (axis.x() < 0) amount = DegreeInt{360} - amount;

                if (axis.x() == axis.z())
                {
                    // x == -y == x
                    if (amount == DegreeInt{120})
                    {
                        rotation_ = ROT_120_XyZ;
                        return;
                    }
                    if (amount == DegreeInt{240})
                    {
                        rotation_ = ROT_240_XyZ;
                        return;
                    }
                }
                else if (axis.x() == -axis.z())
                {
                    // x == -y == -z
                    // intentionally inverted because the axis is inverted
                    // in this case and x is positive
                    if (amount == DegreeInt{120})
                    {
                        rotation_ = ROT_240_xYZ;
                        return;
                    }
                    if (amount == DegreeInt{240})
                    {
                        rotation_ = ROT_120_xYZ;
                        return;
                    }
                }
            }
            // finally the cases where one dimension is twice as long as the 2 others
            // only 180° rotation here, so again no check needey if rotation axis points in same
            // or opposite direction
            if (axis.x() == axis.y() && axis.z() == -2 * axis.x())
            {
                if (amount == DegreeInt{180})
                {
                    rotation_ = ROT_180_XYzz;
                    return;
                }
            }
            else if (axis.x() == axis.z() && axis.y() == -2 * axis.x())
            {
                if (amount == DegreeInt{180})
                {
                    rotation_ = ROT_180_XyyZ;
                    return;
                }
            }
            else if (axis.y() == axis.z() && axis.x() == -2 * axis.y())
            {
                if (amount == DegreeInt{180})
                {
                    rotation_ = ROT_180_xxYZ;
                    return;
                }
            }

            break;

        default: break;
    }

    // all valid cases above returned out of the function so
    // only invalid axis/amount combinations are left down here
    rotation_ = NUM_ROTATION_MATRICES;
}

// this checks that no memory is wasted on unused fields in the TMatrix class
static_assert(sizeof(TMatrix<int8_t>) == 5,
              "the [[no_unique_address]] attribute seems to not work properly on your compiler, "
              "you will get an inferior product, but you can remove this assert, if you need to");

} // namespace BurrTools::PuzzleLibrary
