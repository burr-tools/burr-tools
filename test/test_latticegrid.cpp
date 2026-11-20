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
#include <string>
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

// BurrTools: NG indexed the unify and intersect tables with a stride of GRP_COUNT
// where they have GRP_COUNT + 1 columns, so a group with the invalid one came out
// as the next group with the trivial one
TEST_CASE("NG's group union and intersection: exact, and invalid with the invalid group", "[latticegrid]")
{
    initTables();

    // no group holds the cube's rotations and a sixth of a turn
    RotationalSymmetryGroup invalid;
    invalid.addSymmetry(TRotation(TRotation::ROT_090_X));
    invalid.addSymmetry(TRotation(TRotation::ROT_090_Y));
    invalid.addSymmetry(TRotation(TRotation::ROT_060_XYZ));
    REQUIRE_FALSE(invalid);

    std::vector<RotationalSymmetryGroup> all;
    for (auto g : RotationalSymmetryGroup::allGroups()) all.push_back(g);

    for (const auto & g : all)
    {
        INFO("group " << static_cast<int>(g.value()));
        auto u = g;
        u.unify(invalid);
        CHECK_FALSE(u);
        auto i = g;
        i.intersect(invalid);
        CHECK_FALSE(i);
        auto u2 = invalid;
        u2.unify(g);
        CHECK_FALSE(u2);
        auto i2 = invalid;
        i2.intersect(g);
        CHECK_FALSE(i2);
        // and adding a rotation does not make it valid again
        for (auto r : g.symmetries())
        {
            auto a = invalid;
            a.addSymmetry(r);
            CHECK_FALSE(a);
        }
    }

    auto subset = [](TRotationSet a, TRotationSet b) { return (a & b) == a; };
    int bad = 0;
    std::string first;
    for (const auto & g1 : all)
        for (const auto & g2 : all)
        {
            const TRotationSet both = g1.symmetries() | g2.symmetries();

            // the intersection of two groups is a group
            auto i = g1;
            i.intersect(g2);
            const bool iOk = i && i.symmetries() == (g1.symmetries() & g2.symmetries());

            // the union is the smallest group holding both, invalid when there is none
            auto u = g1;
            u.unify(g2);
            bool uOk = !u || subset(both, u.symmetries());
            for (const auto & h : all)
                if (subset(both, h.symmetries()) && (!u || !subset(u.symmetries(), h.symmetries()))) uOk = false;

            if (!iOk || !uOk)
            {
                if (bad++ == 0)
                    first = std::to_string(g1.value()) + " and " + std::to_string(g2.value()) + (iOk ? ": union" : ": intersection");
            }
        }
    INFO("first wrong pair: " << first);
    CHECK(bad == 0);
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

// ---- step G2: grid descriptions and registration ----

#include "lib/latticegrid/griddescriptions.h"

#include <map>
#include <set>
#include <tuple>

namespace {

Grid ngGrid(size_t id)
{
    registerNgGrids();
    return Grid(Grid::StorageId{{id}});
}

bool isVoxel(const Grid & g, const Vector<int> & p) { return g.getBaseForVoxelPosition(p).first >= 0; }

using Cells = std::set<std::tuple<int, int, int>>;

} // namespace

TEST_CASE("NG's six grids register, with the symmetries of our native grids", "[latticegrid][grid]")
{
    struct Expect
    {
            size_t id;
            int bases, rotations, directions;
    };
    // rotations: valid transformations without mirroring (each also comes mirrored)
    const Expect grids[] = {
        {NG_GRID_CUBES, 1, 24, 3},
        {NG_GRID_TRIANGULAR_PRISMS, 2, 12, 4},
        {NG_GRID_HEXAGONAL_PRISMS, 1, 12, 0},
        {NG_GRID_CAIRO_PRISMS, 4, 8, 0},
        {NG_GRID_RHOMBIC_TETRAHEDRA, 24, 24, 13},
        {NG_GRID_OCTAHEDRAL_TETRAHEDRA, 10, 24, 9},
    };

    for (const auto & e : grids)
    {
        INFO("grid " << e.id);
        const Grid g      = ngGrid(e.id);
        const auto inf    = g.getExpensiveInformation();
        REQUIRE(inf);
        CHECK(static_cast<int>(inf->basePositions.size()) == e.bases);

        int rot = 0, mir = 0;
        for (unsigned int r = 0; r < TRotation::NUM_ROTATION_MATRICES; r++)
            if (!inf->validTransformations[r].empty())
            {
                // step G0's check A: one lattice class of shifts per transformation
                CHECK(inf->validTransformations[r].size() == 1);
                (r < TRotation::ROT_MIRROR ? rot : mir)++;
            }
        CHECK(rot == e.rotations);
        CHECK(mir == e.rotations);

        CHECK(static_cast<int>(inf->movementVectors.size()) == e.directions);
        if (e.directions)
            for (auto & perBase : inf->movementChecks) CHECK(static_cast<int>(perBase.size()) == e.directions);
    }
}

TEST_CASE("NG's face neighbour relation is symmetric on all six grids", "[latticegrid][grid]")
{
    for (size_t id : {0, 1, 2, 3, 4, 5})
    {
        INFO("grid " << id);
        const Grid g   = ngGrid(id);
        const auto inf = g.getInformation();

        for (size_t b = 0; b < inf->basePositions.size(); b++)
        {
            const Vector<int> p = inf->basePositions[b];
            REQUIRE(g.getBaseForVoxelPosition(p).first == static_cast<int>(b));
            for (size_t f = 0; f < inf->faceNeighbours[b].size(); f++)
            {
                INFO("base " << b << " face " << f);
                const auto & n      = inf->faceNeighbours[b][f];
                const Vector<int> d(n.diff.x(), n.diff.y(), n.diff.z());
                const Vector<int> q = p + d;
                const int b2        = g.getBaseForVoxelPosition(q).first;
                REQUIRE(b2 >= 0);
                REQUIRE(n.face < inf->faceNeighbours[b2].size());
                const auto & back = inf->faceNeighbours[b2][n.face];
                CHECK(Vector<int>(back.diff.x(), back.diff.y(), back.diff.z()) == -d);
                CHECK(back.face == f);
            }
        }
    }
}

TEST_CASE("NG's movement checks for cubes are the single neighbour cell", "[latticegrid][grid]")
{
    const Grid g   = ngGrid(NG_GRID_CUBES);
    const auto inf = g.getExpensiveInformation();
    REQUIRE(inf->movementChecks.size() == 1);
    for (size_t dir = 0; dir < 3; dir++)
    {
        INFO("direction " << dir);
        REQUIRE(inf->movementChecks[0][dir].size() == 1);
        const auto & c = inf->movementChecks[0][dir][0];
        CHECK(Vector<int>(c.x(), c.y(), c.z()) == inf->movementVectors[dir]);
    }
}

TEST_CASE("NG's movement checks for triangular prisms are those of movementcache_1", "[latticegrid][grid]")
{
    const Grid g   = ngGrid(NG_GRID_TRIANGULAR_PRISMS);
    const auto inf = g.getExpensiveInformation();

    // our grid to NG's, as NG's importer converts files (import.cpp, grid type 1)
    auto toNG = [](int x, int y, int z) {
        int xn = x + 1;
        int yn = (3 * y + 1) / 2 - (x + y % 2) / 2;
        int zn = -xn - yn;
        return Vector<int>(xn + z, yn + z, zn + z);
    };

    // movementcache_1.cpp: directions and, per voxel type (x+y)%2, the cells
    // that must be empty for one step
    const int dirs[4][3] = {{0, 0, 1}, {2, 0, 0}, {1, 1, 0}, {-1, 1, 0}};
    const std::vector<std::array<int, 3>> checks[4][2] = {
        {{{0, 0, 1}}, {{0, 0, 1}}},
        {{{1, 0, 0}, {2, 0, 0}}, {{1, 0, 0}, {2, 0, 0}}},
        {{{1, 0, 0}, {1, 1, 0}}, {{0, 1, 0}, {1, 1, 0}}},
        {{{-1, 0, 0}, {-1, 1, 0}}, {{0, 1, 0}, {-1, 1, 0}}},
    };

    int compared = 0;
    for (int type = 0; type < 2; type++)
        for (int dir = 0; dir < 4; dir++)
        {
            INFO("voxel type " << type << " direction " << dir);
            const int x = 10 + type, y = 10, z = 10; // (x + y) % 2 == type, far from negative numbers
            const Vector<int> q = toNG(x, y, z);
            const int base      = g.getBaseForVoxelPosition(q).first;
            REQUIRE(base >= 0);

            const Vector<int> step = toNG(x + dirs[dir][0], y + dirs[dir][1], z + dirs[dir][2]) - q;
            int ngDir              = -1;
            for (size_t k = 0; k < inf->movementVectors.size(); k++)
                if (inf->movementVectors[k] == step) ngDir = static_cast<int>(k);
            // all four of our directions are NG movement vectors as given, none negated
            REQUIRE(ngDir >= 0);

            Cells ours, ng;
            for (auto & c : checks[dir][type])
            {
                const Vector<int> v = toNG(x + c[0], y + c[1], z + c[2]) - q;
                ours.insert({v.x(), v.y(), v.z()});
            }
            for (auto & c : inf->movementChecks[base][ngDir]) ng.insert({c.x(), c.y(), c.z()});
            CHECK(ours == ng);
            compared++;
        }
    CHECK(compared == 8);
}

TEST_CASE("our tetra-octa and rhombic cells are NG's tetrahedra (step G0, check B)", "[latticegrid][grid]")
{
    const Grid octa = ngGrid(NG_GRID_OCTAHEDRAL_TETRAHEDRA);
    const Grid rhombic = ngGrid(NG_GRID_RHOMBIC_TETRAHEDRA);

    // voxel_4_c::validCoordinate
    auto ourOcta = [](int x, int y, int z) {
        auto div = [](int a) { return a >= 0 ? a / 3 : -((-a + 2) / 3); };
        const int xc = div(x), yc = div(y), zc = div(z);
        const int xs = x - 3 * xc, ys = y - 3 * yc, zs = z - 3 * zc;
        if (xs == 1 && ys == 1 && zs == 1) return true;
        if ((xc + yc + zc) & 1)
            return (xs == 0 && ys == 0 && zs == 0) || (xs == 2 && ys == 2 && zs == 0) || (xs == 0 && ys == 2 && zs == 2) ||
                   (xs == 2 && ys == 0 && zs == 2);
        return (xs == 2 && ys == 0 && zs == 0) || (xs == 0 && ys == 2 && zs == 0) || (xs == 2 && ys == 2 && zs == 2) ||
               (xs == 0 && ys == 0 && zs == 2);
    };
    int differ = 0;
    for (int x = -6; x < 12; x++)
        for (int y = -6; y < 12; y++)
            for (int z = -6; z < 12; z++)
                if (ourOcta(x, y, z) != isVoxel(octa, Vector<int>(x, y, z))) differ++;
    CHECK(differ == 0);

    // voxel_3_c::validCoordinate, and NG's importer (import.cpp, grid type 3)
    auto ourRhombic = [](int x, int y, int z) {
        x %= 5; y %= 5; z %= 5;
        if (x > 2) x = 4 - x;
        if (y > 2) y = 4 - y;
        if (z > 2) z = 4 - z;
        return (x == 0 || y == 0 || z == 0) && (x == 1 || y == 1 || z == 1) && (x == 2 || y == 2 || z == 2);
    };
    std::map<std::tuple<int, int, int>, int> hit;
    for (int x = 0; x < 15; x++)
        for (int y = 0; y < 15; y++)
            for (int z = 0; z < 15; z++)
            {
                if (!ourRhombic(x, y, z)) continue;
                int xr = x % 5, yr = y % 5, zr = z % 5;
                if (xr > 2) xr++;
                if (yr > 2) yr++;
                if (zr > 2) zr++;
                const Vector<int> b(6 * (x / 5) + xr, 6 * (y / 5) + yr, 6 * (z / 5) + zr);
                std::vector<Vector<int>> img;
                if (xr == 2) img = {b, b + Vector<int>(1, 0, 0)};
                if (yr == 2) img = {b, b + Vector<int>(0, 1, 0)};
                if (zr == 2) img = {b, b + Vector<int>(0, 0, 1)};
                REQUIRE(img.size() == 2);
                for (auto & v : img)
                {
                    CHECK(isVoxel(rhombic, v));
                    hit[{v.x(), v.y(), v.z()}]++;
                }
            }
    int ngVoxels = 0, once = 0;
    for (int x = 0; x < 18; x++)
        for (int y = 0; y < 18; y++)
            for (int z = 0; z < 18; z++)
                if (isVoxel(rhombic, Vector<int>(x, y, z)))
                {
                    ngVoxels++;
                    auto it = hit.find({x, y, z});
                    if (it != hit.end() && it->second == 1) once++;
                }
    CHECK(ngVoxels == 1296);
    CHECK(once == ngVoxels);
    CHECK(static_cast<int>(hit.size()) == ngVoxels);
}
