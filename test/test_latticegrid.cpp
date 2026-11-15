/* BurrTools
 *
 * Tests for the mathematics ported from BurrTools NG (src/lib/latticegrid/,
 * step G1 of design/2026-11-19-ng-merge-plan.md). The rotation-constructor
 * and MPR cases are NG's own tests (src/tests/test-rotation-constructor.cpp,
 * test-mpr.cpp, test-mpr-3d.cpp at fdae7951) in Catch2; the table cases check
 * NG's 60 transformations and 136 symmetry groups against each other and
 * against this repository's cube and prism rotation tables.
 *
 * NG is licensed under the GNU General Public License version 3, and so is
 * this file.
 */

#include "lib/latticegrid/algebra.h"
#include "lib/latticegrid/mpr.h"
#include "lib/latticegrid/symmetry.h"
#include "lib/latticegrid/transformation.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using namespace BurrTools::PuzzleLibrary;
using namespace BurrTools::Maths;

namespace {

void initTables()
{
    static const bool done = [] {
        TRotation::init_tables();
        RotationalSymmetryGroup::init_tables();
        return true;
    }();
    (void)done;
}

// d * matrix, as integers
std::array<int, 9> scaled(const TRotation r, int d)
{
    std::array<int, 9> m{};
    const auto & mat = r.matrix();
    for (int k = 0; k < 9; k++)
    {
        // entries are multiples of 1/denominator; d is a multiple of every denominator used here
        m[k] = mat[k] * (d / mat.denominator());
    }
    return m;
}

const std::vector<Vector<int>> probes = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 2, 3}, {-2, 5, 1}};

} // namespace

// NG: test-rotation-constructor.cpp
TEST_CASE("TRotation from axis and angle, every rotation", "[latticegrid]")
{
    initTables();

    for (unsigned int i = 0; i < TRotation::NUM_ROTATION_MATRICES / 2; i++)
    {
        INFO("rotation " << i);
        const auto info      = TRotation(i).matrixInfo();
        const auto & dir     = info.direction;
        const DegreeInt angle = info.angle;

        TRotation rot1(RVector<int>(dir), angle);
        CHECK(static_cast<bool>(rot1));
        CHECK(rot1.matrixInfo().direction == dir);
        CHECK(rot1.matrixInfo().angle == angle);

        for (int scale : {2, 3})
        {
            TRotation r(RVector<int>(dir.x() * scale, dir.y() * scale, dir.z() * scale), angle);
            CHECK(static_cast<bool>(r));
            CHECK(r.matrixInfo().direction == dir);
            CHECK(r.matrixInfo().angle == angle);
        }

        TRotation rot3(RVector<int>(-dir.x(), -dir.y(), -dir.z()), angle);
        CHECK(static_cast<bool>(rot3));
        CHECK(rot3.matrixInfo().direction == dir);
        CHECK(rot3.matrixInfo().angle == DegreeInt{(360 - angle.value) % 360});
    }

    RVector<int> axis(1, 0, 0);
    CHECK(TRotation(axis, DegreeInt{360}).value() == TRotation::ROT_IDENTITY);
    CHECK(TRotation(axis, DegreeInt{450}).value() == TRotation::ROT_090_X);
    CHECK(TRotation(axis, DegreeInt{-90}).value() == TRotation::ROT_270_X);
    CHECK_FALSE(static_cast<bool>(TRotation(RVector<int>(1, 2, 3), DegreeInt{90})));
}

// NG: test-rotation-constructor.cpp
TEST_CASE("TRotation from axis and angle, edge cases", "[latticegrid]")
{
    initTables();

    CHECK_FALSE(static_cast<bool>(TRotation(RVector<int>(0, 0, 0), DegreeInt{90})));

    RVector<int> axis(0, 0, 1);
    CHECK(TRotation(axis, DegreeInt{720}).value() == TRotation::ROT_IDENTITY);
    CHECK(TRotation(axis, DegreeInt{-720}).value() == TRotation::ROT_IDENTITY);
    CHECK_FALSE(static_cast<bool>(TRotation(RVector<int>(1, 2, 4), DegreeInt{180})));
}

TEST_CASE("TRotation product and inverse tables agree with the matrices", "[latticegrid]")
{
    initTables();

    int invalidProducts = 0;
    for (unsigned int a = 0; a < TRotation::NUM_ROTATION_MATRICES; a++)
    {
        const TRotation ra(a);
        const TRotation inv = ~ra;
        REQUIRE(static_cast<bool>(inv));
        CHECK((ra * inv).value() == TRotation::ROT_IDENTITY);

        for (unsigned int b = 0; b < TRotation::NUM_ROTATION_MATRICES; b++)
        {
            const TRotation rb(b);
            const TRotation p = ra * rb;
            if (!p)
            {
                invalidProducts++;
                continue;
            }
            for (auto & v : probes)
            {
                INFO("a " << a << " b " << b);
                CHECK(p * v == ra * (rb * v));
            }
        }
    }
    // the 60 are the union of two groups (cube and hexagonal), not a group
    CHECK(invalidProducts > 0);
}

TEST_CASE("Our cube rotation table is the cube part of NG's transformations", "[latticegrid]")
{
    initTables();

    static const int ours[48][9] = {
#include "lib/tabs_0/rotmatrix.inc"
    };

    std::vector<int> found(48, -1);
    for (int i = 0; i < 48; i++)
        for (unsigned int j = 0; j < TRotation::NUM_ROTATION_MATRICES; j++)
        {
            auto m = scaled(TRotation(j), 3);
            bool same = true;
            for (int k = 0; k < 9; k++)
                if (m[k] != ours[i][k] * 3) same = false;
            if (same)
            {
                INFO("our transformation " << i << " matches two of NG's");
                CHECK(found[i] == -1);
                found[i] = static_cast<int>(j);
            }
        }

    for (int i = 0; i < 48; i++)
    {
        INFO("our transformation " << i);
        CHECK(found[i] >= 0);
        // our first 24 are rotations, NG's rotations are 0..29
        if (found[i] >= 0) CHECK((i < 24) == (found[i] < TRotation::ROT_MIRROR));
    }
}

TEST_CASE("Our prism rotation table is the hexagonal part of NG's transformations", "[latticegrid]")
{
    initTables();

    static const double ours[24][9] = {
#include "lib/tabs_1/rotmatrix.inc"
    };

    // NG keeps the prism axis along (1,1,1); these take it to (0,0,1) and back
    // (NG: test-symmetry.cpp)
    const double fwd[9] = {std::sqrt(2.0 / 3), -1 / std::sqrt(6.0), -1 / std::sqrt(6.0),
                           0,                  1 / (3 * std::sqrt(2.0)) + std::sqrt(2.0) / 3, -1 / (3 * std::sqrt(2.0)) - std::sqrt(2.0) / 3,
                           1 / std::sqrt(3.0), 1 / std::sqrt(3.0), 1 / std::sqrt(3.0)};
    const double inv[9] = {2 / std::sqrt(6.0),  0, 1 / std::sqrt(3.0), -1 / std::sqrt(6.0), 1 / std::sqrt(2.0), 1 / std::sqrt(3.0),
                           -1 / std::sqrt(6.0), -1 / std::sqrt(2.0), 1 / std::sqrt(3.0)};

    std::vector<int> found(24, -1);
    for (int i = 0; i < 24; i++)
    {
        double b[9] = {0};
        for (int x = 0; x < 3; x++)
            for (int y = 0; y < 3; y++)
                for (int z = 0; z < 3; z++) b[y * 3 + x] += fwd[x + z * 3] * ours[i][y * 3 + z];
        double c[9] = {0};
        for (int x = 0; x < 3; x++)
            for (int y = 0; y < 3; y++)
                for (int z = 0; z < 3; z++) c[y * 3 + x] += b[x + z * 3] * inv[y * 3 + z];

        for (unsigned int j = 0; j < TRotation::NUM_ROTATION_MATRICES; j++)
        {
            const TRotation r(j);
            const auto & m = r.matrix();
            bool same = true;
            for (int k = 0; k < 9; k++)
                if (std::fabs(m.denominator() * c[k] - m[k]) > 1e-4) same = false;
            if (same)
            {
                INFO("our transformation " << i << " matches two of NG's");
                CHECK(found[i] == -1);
                found[i] = static_cast<int>(j);
            }
        }
    }

    for (int i = 0; i < 24; i++)
    {
        INFO("our transformation " << i);
        CHECK(found[i] >= 0);
        if (found[i] >= 0) CHECK((i < 12) == (found[i] < TRotation::ROT_MIRROR));
    }
}

TEST_CASE("NG's symmetry groups are closed and the full cube group has 24 rotations", "[latticegrid]")
{
    initTables();

    int groups = 0;
    for (auto g : RotationalSymmetryGroup::allGroups())
    {
        groups++;
        const TRotationSet s = g.symmetries();
        INFO("group " << static_cast<int>(g.value()));
        CHECK(s.contains(TRotation(TRotation::ROT_IDENTITY)));
        for (unsigned int a = 0; a < TRotation::NUM_ROTATION_MATRICES; a++)
            if (s.contains(TRotation(a)))
            {
                CHECK(s.contains(~TRotation(a)));
                for (unsigned int b = 0; b < TRotation::NUM_ROTATION_MATRICES; b++)
                    if (s.contains(TRotation(b))) CHECK(s.contains(TRotation(a) * TRotation(b)));
            }
    }
    CHECK(groups == 136); // RotationalSymmetryGroup::GRP_COUNT, private

    RotationalSymmetryGroup cube;
    cube.addSymmetry(TRotation(TRotation::ROT_090_X));
    cube.addSymmetry(TRotation(TRotation::ROT_090_Y));
    CHECK(cube.symmetries().size() == 24);
    CHECK_FALSE(cube.containsMirrorTransformations());

    RotationalSymmetryGroup hex;
    hex.addSymmetry(TRotation(TRotation::ROT_060_XYZ));
    hex.addSymmetry(TRotation(TRotation::ROT_180_XYzz));
    CHECK(hex.symmetries().size() == 12);
}

namespace {

// NG: test-mpr.cpp
struct MPR2dHelper
{
        typedef float CoordinateType;
        float bev = 0;

        Vector2d<float> support(const Vector2d<float> & d, const Vector2d<float> & center) const
        {
            float bestval = -10000000;
            Vector2d<float> bestvec;
            const Vector2d<float> corners[8] = {{1 - bev, 1},     {1, 1 - bev},     {1, -1 + bev},  {1 - bev, -1},
                                                {-1 + bev, -1},   {-1, -1 + bev},   {-1, 1 - bev},  {-1 + bev, 1}};
            for (auto & nv : corners)
            {
                const float t = (center + nv) * d;
                if (t > bestval)
                {
                    bestval = t;
                    bestvec = center + nv;
                }
            }
            return bestvec;
        }
        Vector2d<float> support1(const Vector2d<float> & d) const { return support(d, center1()); }
        Vector2d<float> support2(const Vector2d<float> & d) const { return support(d, center2()); }
        Vector2d<float> center1() const { return {0, 0}; }
        Vector2d<float> center2() const { return {1, 1}; }
};

// NG: test-mpr-3d.cpp
struct MPR3dHelper
{
        typedef float CoordinateType;
        Vector<float> pos1, size1, pos2, size2;

        static Vector<float> boxSupport(const Vector<float> & d, const Vector<float> & pos, const Vector<float> & size)
        {
            return Vector<float>((d.x() > 0) ? pos.x() + size.x() : pos.x() - size.x(), (d.y() > 0) ? pos.y() + size.y() : pos.y() - size.y(),
                                 (d.z() > 0) ? pos.z() + size.z() : pos.z() - size.z());
        }
        Vector<float> support1(const Vector<float> & d) const { return boxSupport(d, pos1, size1); }
        Vector<float> support2(const Vector<float> & d) const { return boxSupport(d, pos2, size2); }
        Vector<float> center1() const { return pos1; }
        Vector<float> center2() const { return pos2; }
};

} // namespace

TEST_CASE("MPR in 2d: bevelled squares", "[latticegrid]")
{
    MPR2dHelper c;
    c.bev = 0;
    CHECK(MPR::intersects2d(c));
    c.bev = 1;
    CHECK_FALSE(MPR::intersects2d(c));
    c.bev = 0.9f;
    CHECK(MPR::intersects2d(c));
}

TEST_CASE("MPR in 3d: boxes", "[latticegrid]")
{
    MPR3dHelper c;
    c.pos1  = {0, 0, 0};
    c.size1 = {1, 1, 1};
    c.size2 = {1, 1, 1};

    c.pos2 = {0.5f, 0.5f, 0.5f};
    CHECK(MPR::intersects3d(c));
    c.pos2 = {3, 3, 3};
    CHECK_FALSE(MPR::intersects3d(c));

    // touching faces do not intersect
    c.pos2 = {2, 0, 0};
    CHECK_FALSE(MPR::intersects3d(c));
    c.pos2 = {1.9999f, 0, 0};
    CHECK(MPR::intersects3d(c));
    c.pos2 = {2, 1.9999f, 1.9999f};
    CHECK(MPR::intersects3d(c));

    // one inside the other
    c.pos2  = {0, 0, 0};
    c.size2 = {0.5f, 0.5f, 0.5f};
    CHECK(MPR::intersects3d(c));
}
