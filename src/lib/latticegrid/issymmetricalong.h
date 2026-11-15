/* BurrTools
 *
 * Ported from BurrTools NG by Andreas Röver (https://gitlab.com/roever/burrtools),
 * src/lib/issymmetricalong.h at commit fdae7951, for grids as data
 * (design/2026-11-19-ng-merge-plan.md, step G1). NG is licensed under the
 * GNU General Public License version 3, and so is this file.
 *
 * Changes from NG's file: none.
 * Keep the rest identical to NG's file, so changes can be exchanged both ways.
 */
#pragma once

namespace BurrTools::PuzzleLibrary {

// check if two given voxels-arrays/sets/... are symmetric along a certain transformation,
// meaning that the shape is identical when transformed by this transformation
template <class V, class R>
[[nodiscard]] bool isSymmetricAlong(const V & v, const R & t)
{
    // iterate over the non-empty voxels
    // and check if we transform the voxel position if we
    // end up at a position that has the same value
    // if the contents at that position is different, the
    // contained shape is not symmetric along that transformation
    for (auto [pos, val] : v)
    {
        // calculate the transformed position, get the whole and fractional part
        // the fractional part should always be zero
        auto [whole, fraction] = (t * pos).getWholeFraction();
        assert(fraction.isZero());
        if (v.get(whole) != val) return false;
    }

    return true;
}

} // namespace BurrTools::PuzzleLibrary
