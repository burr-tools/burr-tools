/* BurrTools
 *
 * The six grids of BurrTools NG, as data (design/2026-11-19-ng-merge-plan.md,
 * step G2). GNU General Public License version 3, like NG.
 */
#pragma once

#include "grid.h"
#include "symmetry.h"
#include "transformation.h"

#include <vector>

namespace BurrTools::PuzzleLibrary {

// NG's storage ids; they are what NG writes into its files, so they never change
enum ngGridId : size_t {
    NG_GRID_CUBES                 = 0,
    NG_GRID_RHOMBIC_TETRAHEDRA    = 1,
    NG_GRID_OCTAHEDRAL_TETRAHEDRA = 2,
    NG_GRID_HEXAGONAL_PRISMS      = 3,
    NG_GRID_TRIANGULAR_PRISMS     = 4,
    NG_GRID_CAIRO_PRISMS          = 5,
};

// the descriptions of NG's data/scripts/grid_*.lua, without editor snap points,
// in the order cubes, triangular prisms, hexagonal prisms, Cairo prisms,
// rhombic tetrahedra, octahedral tetrahedra
std::vector<Grid::RegisterInformation> ngGridDescriptions();

// initialises the transformation and symmetry tables and registers the six
// grids, once per process; safe to call from several threads. Throws
// std::runtime_error if a description does not register.
void registerNgGrids();

} // namespace BurrTools::PuzzleLibrary
