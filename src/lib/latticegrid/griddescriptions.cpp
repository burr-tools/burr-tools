/* BurrTools
 *
 * The six grids of BurrTools NG by Andreas Röver
 * (https://gitlab.com/roever/burrtools, data/scripts/grid_*.lua at commit
 * fdae7951) as C++ initialisers, for grids as data
 * (design/2026-11-19-ng-merge-plan.md, step G2). NG is licensed under the
 * GNU General Public License version 3, and so is this file.
 *
 * Editor snap points are left out. Loading descriptions from files is step G7.
 * The movement vectors of the hexagonal and Cairo prisms are not NG's (it has
 * none for them) but this repository's (step C6).
 */
#include "griddescriptions.h"

#include <cmath>
#include <mutex>

namespace BurrTools::PuzzleLibrary {

using BurrTools::Maths::Vector;

namespace {

const float EQT = std::sqrt(0.75f);

Grid::Polyhedron tetrahedron(std::vector<Vector<float>> v)
{
    // the winding of grid.tetrahedron in NG's lua_functions.cpp
    return Grid::Polyhedron{std::move(v), {{0, 1, 2}, {1, 0, 3}, {2, 1, 3}, {0, 2, 3}}, {0, 0, 0}};
}

} // namespace

// The six descriptions of NG's data/scripts/grid_*.lua (fdae7951), without
// editor snap points.
std::vector<Grid::RegisterInformation> ngGridDescriptions()
{
    std::vector<Grid::RegisterInformation> g;

    g.push_back({
        .storageId             = {0},
        .name                  = "Cubes",
        .translationalSymmetry = {1, 0, 0, 0, 1, 0, 0, 0, 1},
        .physicalTranslations  = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0},
        .movementVectors       = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
        .physicalShapes        = {prism({{-0.5, -0.5, -0.5}, {-0.5, 0.5, -0.5}, {0.5, 0.5, -0.5}, {0.5, -0.5, -0.5}}, {0.0, 0.0, 1.0})},
        .basePositions         = {{0, 0, 0}},
        .gridSnaps             = {},
    });

    g.push_back({
        .storageId             = {4},
        .name                  = "Triangle prisms",
        .translationalSymmetry = {2, 1, 1, -1, 1, 1, -1, -2, 1},
        .physicalTranslations  = {1.0, 0.5, 0.0, 0.0, EQT, 0.0, 0.0, 0.0, 1.0},
        .movementVectors       = {{1, 1, 1}, {2, -1, -1}, {1, 1, -2}, {-1, 2, -1}},
        .physicalShapes        = {prism({{0.0, 0.0, -0.5}, {0.5, EQT, -0.5}, {1.0, 0.0, -0.5}}, {0.0, 0.0, 1.0}),
                                  prism({{1.0, 0.0, -0.5}, {0.5, EQT, -0.5}, {1.5, EQT, -0.5}}, {0.0, 0.0, 1.0})},
        .basePositions         = {{1, 0, -1}, {2, 0, -2}},
        .gridSnaps             = {},
    });

    g.push_back({
        .storageId             = {3},
        .name                  = "Hexagonal prisms",
        .translationalSymmetry = {1, 1, 1, -1, 0, 1, 0, -1, 1},
        .physicalTranslations  = {2 * EQT, EQT, 0.0, 0, 1.5, 0.0, 0, 0.0, 1.0},
        /* Not in NG ("no movement analysis"); added here (step C6). The
         * lattice columns are (1,-1,0) and (1,0,-1), physically (sqrt 3, 0, 0)
         * and (sqrt 3 / 2, 3/2, 0): the centres of the edge neighbours at 0
         * and 60 degrees (a hexagon of circumradius 1 has its neighbours
         * sqrt 3 away), and (1,1,1), physically (0, 0, 1), the prism axis.
         * So: the axis, and one vector per pair of opposite edge neighbours,
         * at 0, 60 and 120 degrees (the third is the difference of the
         * first two). Every cell is a lattice point, so all are lattice
         * vectors. As for cubes, longer lattice vectors (the 30 degree
         * directions between two neighbours) are left out. */
        .movementVectors       = {{1, 1, 1}, {1, -1, 0}, {1, 0, -1}, {0, 1, -1}},
        .physicalShapes        = {prism({{EQT, 0.5, -0.5}, {EQT, -0.5, -0.5}, {0.0, -1.0, -0.5}, {-EQT, -0.5, -0.5}, {-EQT, 0.5, -0.5}, {0.0, 1.0, -0.5}},
                                        {0.0, 0.0, 1.0})},
        .basePositions         = {{0, 0, 0}},
        .gridSnaps             = {},
    });

    {
        const float OFS = 4.0f / 3 - std::sqrt(7.0f) / 3;
        g.push_back({
            .storageId             = {5},
            .name                  = "Cairo prisms",
            .translationalSymmetry = {2, -2, 0, 2, 2, 0, 0, 0, 1},
            .physicalTranslations  = {2, -2, 0, 2, 2, 0, 0, 0, 1},
            /* Not in NG ("no movement analysis"); added here (step C6): the
             * prism axis (0,0,1) and the two shortest in-plane lattice
             * vectors, the lattice columns (2,2,0) and (-2,2,0) (cell and
             * physical coordinates agree on this grid), which the grid's
             * quarter turn maps onto each other. Moves must be lattice
             * vectors (moved pieces stay a lattice vector apart, so cells
             * keep their kind), and the four pentagons of a lattice cell
             * are differently oriented, so no shorter move takes a
             * pentagon onto a pentagon: the translations of the tiling are
             * the lattice. The next lattice vectors, (4,0,0) and (0,4,0),
             * are left out as the face diagonals are for cubes. */
            .movementVectors       = {{0, 0, 1}, {2, 2, 0}, {-2, 2, 0}},
            .physicalShapes =
                {
                    prism({{0, 0, -0.5}, {1, -OFS, -0.5}, {1, -2 + OFS, -0.5}, {0, -2, -0.5}, {-OFS, -1, -0.5}}, {0, 0, 1}),
                    prism({{0, 0, -0.5}, {OFS, 1, -0.5}, {2 - OFS, 1, -0.5}, {2, 0, -0.5}, {1, -OFS, -0.5}}, {0, 0, 1}),
                    prism({{0, 0, -0.5}, {-OFS, -1, -0.5}, {-2 + OFS, -1, -0.5}, {-2, 0, -0.5}, {-1, OFS, -0.5}}, {0, 0, 1}),
                    prism({{0, 0, -0.5}, {-1, OFS, -0.5}, {-1, 2 - OFS, -0.5}, {0, 2, -0.5}, {OFS, 1, -0.5}}, {0, 0, 1}),
                },
            .basePositions = {{0, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}},
            .gridSnaps     = {},
        });
    }

    g.push_back({
        .storageId             = {1},
        .name                  = "Rhombic Tetrahedra",
        .translationalSymmetry = {6, 0, 3, 0, 6, 3, 0, 0, 3},
        .physicalTranslations  = {2.0, 0.0, 1.0, 0.0, 2.0, 1.0, 0.0, 0.0, 1.0},
        .physicalOffset        = {0.5, 0.5, 0.5},
        .movementVectors       = {{6, 0, 0}, {0, 6, 0}, {0, 0, 6}, {6, 6, 0}, {6, 0, 6}, {0, 6, 6}, {6, -6, 0},
                                  {6, 0, -6}, {0, 6, -6}, {3, 3, 3}, {3, -3, 3}, {3, -3, -3}, {3, 3, -3}},
        .physicalShapes =
            {
                tetrahedron({{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 1, 1}}), tetrahedron({{0, 1, 0}, {0, 2, 0}, {1, 1, 0}, {1, 1, 1}}),
                tetrahedron({{1, 0, 0}, {0, 0, 0}, {1, 1, 0}, {1, 1, 1}}), tetrahedron({{0, 2, 0}, {1, 2, 0}, {1, 1, 0}, {1, 1, 1}}),
                tetrahedron({{2, 0, 0}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}}), tetrahedron({{1, 2, 0}, {2, 2, 0}, {1, 1, 0}, {1, 1, 1}}),
                tetrahedron({{2, 1, 0}, {2, 0, 0}, {1, 1, 0}, {1, 1, 1}}), tetrahedron({{2, 2, 0}, {2, 1, 0}, {1, 1, 0}, {1, 1, 1}}),

                tetrahedron({{0, 0, 1}, {0, 0, 0}, {1, 0, 1}, {1, 1, 1}}), tetrahedron({{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {1, 1, 1}}),
                tetrahedron({{1, 0, 0}, {2, 0, 0}, {1, 0, 1}, {1, 1, 1}}), tetrahedron({{2, 0, 0}, {2, 0, 1}, {1, 0, 1}, {1, 1, 1}}),

                tetrahedron({{0, 2, 0}, {0, 2, 1}, {1, 2, 1}, {1, 1, 1}}), tetrahedron({{1, 2, 0}, {0, 2, 0}, {1, 2, 1}, {1, 1, 1}}),
                tetrahedron({{2, 2, 0}, {1, 2, 0}, {1, 2, 1}, {1, 1, 1}}), tetrahedron({{2, 2, 1}, {2, 2, 0}, {1, 2, 1}, {1, 1, 1}}),

                tetrahedron({{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {1, 1, 1}}), tetrahedron({{0, 1, 0}, {0, 0, 0}, {0, 1, 1}, {1, 1, 1}}),
                tetrahedron({{0, 2, 0}, {0, 1, 0}, {0, 1, 1}, {1, 1, 1}}), tetrahedron({{0, 2, 1}, {0, 2, 0}, {0, 1, 1}, {1, 1, 1}}),

                tetrahedron({{2, 0, 1}, {2, 0, 0}, {2, 1, 1}, {1, 1, 1}}), tetrahedron({{2, 0, 0}, {2, 1, 0}, {2, 1, 1}, {1, 1, 1}}),
                tetrahedron({{2, 1, 0}, {2, 2, 0}, {2, 1, 1}, {1, 1, 1}}), tetrahedron({{2, 2, 0}, {2, 2, 1}, {2, 1, 1}, {1, 1, 1}}),
            },
        .basePositions = {{1, 2, 0}, {1, 3, 0}, {2, 1, 0}, {2, 4, 0}, {3, 1, 0}, {3, 4, 0}, {4, 2, 0}, {4, 3, 0},
                          {1, 0, 2}, {2, 0, 1}, {3, 0, 1}, {4, 0, 2}, {1, 5, 2}, {2, 5, 1}, {3, 5, 1}, {4, 5, 2},
                          {0, 1, 2}, {0, 2, 1}, {0, 3, 1}, {0, 4, 2}, {5, 1, 2}, {5, 2, 1}, {5, 3, 1}, {5, 4, 2}},
        .gridSnaps     = {},
    });

    g.push_back({
        .storageId             = {2},
        .name                  = "Octahedral tetrahedra",
        .translationalSymmetry = {3, 3, 3, -3, 3, 0, 0, 0, 3},
        .physicalTranslations  = {1.0, 1.0, 1.0, -1.0, 1.0, 0.0, 0.0, 0.0, 1.0},
        .physicalOffset        = {0.5, 0.5, 0.5},
        .movementVectors       = {{6, 0, 0}, {0, 6, 0}, {0, 0, 6}, {3, 3, 0}, {3, -3, 0}, {3, 0, 3}, {3, 0, -3}, {0, 3, 3}, {0, 3, -3}},
        .physicalShapes =
            {
                tetrahedron({{0.0, 1.0, 0.0}, {1.0, 1.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 1.0, 1.0}}),
                tetrahedron({{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 0.0, 1.0}, {1.0, 1.0, 0.0}}),
                tetrahedron({{0.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {1.0, 0.0, 1.0}, {0.0, 1.0, 1.0}}),
                tetrahedron({{0.0, 0.0, 1.0}, {1.0, 0.0, 1.0}, {0.0, 1.0, 1.0}, {0.0, 0.0, 0.0}}),
                tetrahedron({{1.0, 1.0, 1.0}, {0.0, 1.0, 1.0}, {1.0, 0.0, 1.0}, {1.0, 1.0, 0.0}}),
                tetrahedron({{1.0, 1.0, 1.0}, {1.0, 0.0, 1.0}, {2.0, 1.0, 1.0}, {1.0, 1.0, 0.0}}),
                tetrahedron({{2.0, 0.0, 1.0}, {1.0, 0.0, 1.0}, {2.0, 0.0, 0.0}, {2.0, 1.0, 1.0}}),
                tetrahedron({{2.0, 1.0, 1.0}, {1.0, 0.0, 1.0}, {2.0, 0.0, 0.0}, {1.0, 1.0, 0.0}}),
                tetrahedron({{1.0, 0.0, 0.0}, {2.0, 0.0, 0.0}, {1.0, 0.0, 1.0}, {1.0, 1.0, 0.0}}),
                tetrahedron({{2.0, 1.0, 0.0}, {2.0, 1.0, 1.0}, {2.0, 0.0, 0.0}, {1.0, 1.0, 0.0}}),
            },
        .basePositions = {{0, 2, 0}, {2, 0, 0}, {1, 1, 1}, {0, 0, 2}, {2, 2, 2}, {3, 2, 2}, {5, 0, 2}, {4, 1, 1}, {3, 0, 0}, {5, 2, 0}},
        .gridSnaps     = {},
    });

    return g;
}


void registerNgGrids()
{
    static std::once_flag once;
    std::call_once(once, [] {
        TRotation::init_tables();
        RotationalSymmetryGroup::init_tables();
        for (auto & g : ngGridDescriptions())
        {
            const std::string name = g.name;
            if (auto e = Grid::registerGrid(std::move(g)); !e.empty())
                throw std::runtime_error("registering grid " + name + ": " + e);
        }
    });
}

} // namespace BurrTools::PuzzleLibrary
