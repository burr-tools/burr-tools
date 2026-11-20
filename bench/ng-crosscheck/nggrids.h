// NG's six grids (data/scripts/grid_*.lua at fdae7951) as RegisterInformation
// initialisers, for the drivers in this directory. Editor snap points are
// left out.
#pragma once

#include "grid.h"

#include <cmath>
#include <vector>

namespace nggrids {

using namespace BurrTools::PuzzleLibrary;
using BurrTools::Maths::Vector;

inline const float EQT = std::sqrt(0.75f);

inline Grid::Polyhedron tetrahedron(std::vector<Vector<float>> v)
{
    // the winding of grid.tetrahedron in NG's lua_functions.cpp
    return Grid::Polyhedron{std::move(v), {{0, 1, 2}, {1, 0, 3}, {2, 1, 3}, {0, 2, 3}}, {0, 0, 0}};
}

// The six descriptions of NG's data/scripts/grid_*.lua (fdae7951), without
// editor snap points.
inline std::vector<Grid::RegisterInformation> ngGrids()
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
        .movementVectors       = {},
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
            .movementVectors       = {},
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


// registers them all; returns an error text, empty when all went well
inline std::string registerAll()
{
    for (auto & g : ngGrids())
        if (auto e = Grid::registerGrid(std::move(g)); !e.empty()) return e;
    return {};
}

} // namespace nggrids
