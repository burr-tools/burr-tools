// Grid plan step G0 (design/2026-11-19-ng-merge-plan.md): registers NG's six
// grids and checks the two assumptions the plan rests on.
//
// Check A: for every grid and every one of NG's 60 transformations, the
//          translations that make "rotate, then shift" a symmetry of the grid
//          form at most one lattice class. Printed twice: as NG computes it
//          (validTransformations, which also looks at the voxel polyhedra) and
//          by a brute-force search over voxel positions only (a superset).
// Check B: our tetra-octa cells (voxel_4_c::validCoordinate) are exactly NG's
//          octahedral-tetrahedra voxels, and every valid rhombic cell
//          (voxel_3_c::validCoordinate) becomes two NG rhombic-tetrahedra voxels
//          under NG's importer, one-to-one and onto.
//
// usage: nggriddump [-v]   (exit status 0 if both checks hold)
#include "grid.h"
#include "symmetry.h"
#include "transformation.h"
#include "nggrids.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <tuple>
#include <vector>

using namespace BurrTools::PuzzleLibrary;
using BurrTools::Maths::Vector;

namespace {

bool isVoxel(const Grid & g, const Vector<int> & p) { return g.getBaseForVoxelPosition(p).first >= 0; }

// R * p, scaled by the matrix denominator d: returns d * (R p).
Vector<int> rotScaled(const BurrTools::Maths::RMatrix<int> & R, const Vector<int> & p)
{
    auto r = R * p;
    // RVector: x/denominator
    const int d = R.denominator();
    // r.denominator() divides d after reduction; bring it to d
    return Vector<int>(r.x() * (d / r.denominator()), r.y() * (d / r.denominator()), r.z() * (d / r.denominator()));
}

// Brute force over voxel positions: all shifts t (scaled by d) such that
// p -> R p + t maps every voxel near the origin onto a voxel. Returns one
// representative per lattice class.
std::vector<Vector<int>> bruteShifts(const Grid & g, const Grid::Information & inf, int rot)
{
    const auto & R = TRotation(rot).matrix();
    const int d    = R.denominator();

    // sample voxels: every base position in the 27 cells around the origin cell
    std::vector<Vector<int>> sample;
    for (int a = -1; a <= 1; a++)
        for (int b = -1; b <= 1; b++)
            for (int c = -1; c <= 1; c++)
                for (auto & bp : inf.basePositions)
                    sample.push_back(bp + inf.translationalSymmetry * Vector<int>(a, b, c));

    const Vector<int> b0  = inf.basePositions[0];
    const Vector<int> rb0 = rotScaled(R, b0);

    std::vector<Vector<int>> classes;
    // candidate images of b0: every voxel in a box around R b0
    const int box = 14;
    const int cx = rb0.x() / d, cy = rb0.y() / d, cz = rb0.z() / d;
    for (int x = cx - box; x <= cx + box; x++)
        for (int y = cy - box; y <= cy + box; y++)
            for (int z = cz - box; z <= cz + box; z++)
            {
                Vector<int> v(x, y, z);
                if (!isVoxel(g, v)) continue;
                Vector<int> t = v * d - rb0; // scaled shift

                bool ok = true;
                for (auto & p : sample)
                {
                    Vector<int> q = rotScaled(R, p) + t;
                    if (q.x() % d || q.y() % d || q.z() % d) { ok = false; break; }
                    if (!isVoxel(g, Vector<int>(q.x() / d, q.y() / d, q.z() / d))) { ok = false; break; }
                }
                if (!ok) continue;

                bool known = false;
                for (auto & c : classes)
                {
                    Vector<int> diff = t - c;
                    if (diff.x() % d || diff.y() % d || diff.z() % d) continue;
                    if (g.isValidTranslation(Vector<int>(diff.x() / d, diff.y() / d, diff.z() / d))) { known = true; break; }
                }
                if (!known) classes.push_back(t);
            }
    return classes;
}

// voxel_3_c::validCoordinate, src/lib/voxel_3.cpp (for non-negative coordinates)
bool ourRhombic(int x, int y, int z)
{
    x %= 5;
    y %= 5;
    z %= 5;
    if (x > 2) x = 4 - x;
    if (y > 2) y = 4 - y;
    if (z > 2) z = 4 - z;
    if (x != 0 && y != 0 && z != 0) return false;
    if (x != 1 && y != 1 && z != 1) return false;
    if (x != 2 && y != 2 && z != 2) return false;
    return true;
}

int intdivInf(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

// voxel_4_c::validCoordinate, src/lib/voxel_4.cpp
bool ourTetraOcta(int x, int y, int z)
{
    int xc = intdivInf(x, 3), yc = intdivInf(y, 3), zc = intdivInf(z, 3);
    int xs = x - 3 * xc, ys = y - 3 * yc, zs = z - 3 * zc;
    if (xs == 1 && ys == 1 && zs == 1) return true;
    if ((xc + yc + zc) & 1)
        return (xs == 0 && ys == 0 && zs == 0) || (xs == 2 && ys == 2 && zs == 0) || (xs == 0 && ys == 2 && zs == 2) ||
               (xs == 2 && ys == 0 && zs == 2);
    return (xs == 2 && ys == 0 && zs == 0) || (xs == 0 && ys == 2 && zs == 0) || (xs == 2 && ys == 2 && zs == 2) ||
           (xs == 0 && ys == 0 && zs == 2);
}

// NG's importer for our rhombic grid (import.cpp, case 3)
std::vector<Vector<int>> importRhombic(int x, int y, int z)
{
    std::vector<Vector<int>> out;
    int xr = x % 5, xd = x / 5, yr = y % 5, yd = y / 5, zr = z % 5, zd = z / 5;
    if (xr > 2) xr++;
    if (yr > 2) yr++;
    if (zr > 2) zr++;
    Vector<int> b(6 * xd + xr, 6 * yd + yr, 6 * zd + zr);
    if (xr == 2) { out.push_back(b); out.push_back(b + Vector<int>(1, 0, 0)); }
    if (yr == 2) { out.push_back(b); out.push_back(b + Vector<int>(0, 1, 0)); }
    if (zr == 2) { out.push_back(b); out.push_back(b + Vector<int>(0, 0, 1)); }
    return out;
}

std::string str(const Vector<int> & v, int d = 1)
{
    char buf[64];
    if (d == 1) snprintf(buf, sizeof buf, "(%d,%d,%d)", v.x(), v.y(), v.z());
    else snprintf(buf, sizeof buf, "(%d,%d,%d)/%d", v.x(), v.y(), v.z(), d);
    return buf;
}

} // namespace

int main(int argc, char ** argv)
{
    const bool verbose = argc > 1 && !strcmp(argv[1], "-v");

    TRotation::init_tables();
    RotationalSymmetryGroup::init_tables();

    const std::vector<size_t> ids = {0, 4, 3, 5, 1, 2}; // the storage ids of ngGrids(), in order
    for (auto & gi : nggrids::ngGrids())
    {
        std::string name = gi.name;
        if (auto e = Grid::registerGrid(std::move(gi)); !e.empty())
        {
            fprintf(stderr, "registering %s: %s\n", name.c_str(), e.c_str());
            return 1;
        }
    }

    bool checkA = true;

    for (size_t id : ids)
    {
        Grid g{Grid::StorageId{{{id}}}};
        auto inf = g.getExpensiveInformation();

        int nRot = 0, nMir = 0, nMulti = 0, nBruteMulti = 0, nDisagree = 0;
        std::string rots, mirs;
        for (int r = 0; r < (int)TRotation::NUM_ROTATION_MATRICES; r++)
        {
            const size_t ng = inf->validTransformations[r].size();
            const auto brute = bruteShifts(g, *inf, r);
            if (ng) (r < 30 ? nRot : nMir)++;
            if (ng) (r < 30 ? rots : mirs) += " " + std::to_string(r);
            if (ng > 1) nMulti++;
            if (brute.size() > 1) nBruteMulti++;
            if ((ng > 0) != (brute.size() > 0)) nDisagree++;

            if (verbose && (ng || brute.size()))
            {
                printf("    rot %2d: NG %zu class(es):", r, ng);
                for (auto & v : inf->validTransformations[r])
                    printf(" (%d,%d,%d)/%d", v.x(), v.y(), v.z(), v.denominator());
                printf("; positions only %zu:", brute.size());
                for (auto & v : brute) printf(" %s", str(v, TRotation(r).matrix().denominator()).c_str());
                printf("\n");
            }
        }
        if (nMulti) checkA = false;

        size_t nChecks = 0, maxChecks = 0;
        for (auto & perBase : inf->movementChecks)
            for (auto & perDir : perBase)
            {
                nChecks += perDir.size();
                maxChecks = std::max(maxChecks, perDir.size());
            }

        printf("%-22s id %zu: %zu bases, rotations %d, mirrored %d, max group %d, axes %zu, mirror planes %zu, "
               "directions %zu, movement checks %zu (max %zu per base and direction)\n",
               inf->name.c_str(), id, inf->basePositions.size(), nRot, nMir, inf->maxSymmetryGroup, inf->rotationalSymmetries.size(),
               inf->mirrorSymmetries.size(), inf->movementChecks.empty() ? 0 : inf->movementChecks[0].size(), nChecks, maxChecks);
        printf("    rotations:%s\n    mirrored:%s\n", rots.c_str(), mirs.c_str());
        printf("    check A: transformations with more than one class of shifts: NG %d, by positions only %d; "
               "valid by NG but not by positions or the reverse: %d\n",
               nMulti, nBruteMulti, nDisagree);

        if (verbose || id == 0 || id == 4)
            for (size_t b = 0; b < inf->movementChecks.size(); b++)
                for (size_t dir = 0; dir < inf->movementChecks[b].size(); dir++)
                {
                    printf("    base %zu direction %zu:", b, dir);
                    for (auto & v : inf->movementChecks[b][dir]) printf(" (%d,%d,%d)", v.x(), v.y(), v.z());
                    printf("\n");
                }
    }

    // check B, tetra-octa: same cells over a box of 4x4x4 of our 3-cells, with negatives
    bool checkB = true;
    {
        Grid g{Grid::StorageId{{{2}}}};
        int same = 0, diff = 0;
        for (int x = -6; x < 12; x++)
            for (int y = -6; y < 12; y++)
                for (int z = -6; z < 12; z++)
                {
                    bool a = ourTetraOcta(x, y, z), b = isVoxel(g, Vector<int>(x, y, z));
                    if (a == b) same++;
                    else
                    {
                        if (diff < 10) printf("    tetra-octa differs at (%d,%d,%d): ours %d NG %d\n", x, y, z, a, b);
                        diff++;
                    }
                }
        printf("check B, tetra-octa: %d coordinates agree, %d differ\n", same, diff);
        if (diff) checkB = false;
    }
    // check B, rhombic: each of our cells to two NG voxels, injective, onto
    {
        Grid g{Grid::StorageId{{{1}}}};
        std::map<std::tuple<int, int, int>, int> hit;
        int ourCells = 0, notTwo = 0, invalid = 0;
        const int N = 15; // three periods of 5
        for (int x = 0; x < N; x++)
            for (int y = 0; y < N; y++)
                for (int z = 0; z < N; z++)
                {
                    if (!ourRhombic(x, y, z)) continue;
                    ourCells++;
                    auto img = importRhombic(x, y, z);
                    if (img.size() != 2) notTwo++;
                    for (auto & v : img)
                    {
                        if (!isVoxel(g, v)) invalid++;
                        hit[{v.x(), v.y(), v.z()}]++;
                    }
                }
        int twice = 0;
        for (auto & [k, n] : hit)
            if (n > 1) twice++;
        int ngVoxels = 0, missed = 0;
        const int M = 6 * N / 5; // the same three periods of 6
        for (int x = 0; x < M; x++)
            for (int y = 0; y < M; y++)
                for (int z = 0; z < M; z++)
                    if (isVoxel(g, Vector<int>(x, y, z)))
                    {
                        ngVoxels++;
                        if (!hit.count({x, y, z})) missed++;
                    }
        printf("check B, rhombic: %d of our cells, %d not mapped to exactly two, %d images not NG voxels, %d NG voxels hit twice; "
               "%d NG voxels in the box, %d not hit\n",
               ourCells, notTwo, invalid, twice, ngVoxels, missed);
        if (notTwo || invalid || twice || missed) checkB = false;
    }

    printf("check A %s, check B %s\n", checkA ? "holds" : "FAILS", checkB ? "holds" : "FAILS");
    return checkA && checkB ? 0 : 1;
}
