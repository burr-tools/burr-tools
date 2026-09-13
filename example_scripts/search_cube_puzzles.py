#!/usr/bin/env python3
# BurrTools
#
# Copyright (C) 2026 Arne Köhn <arne@chark.eu>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

"""
Search for puzzles with identical pieces in a cubic box using BurrTools.

The cubic target does not need to be completely filled (target voxels are variable).
Prunes candidate pieces that are a superset ("more") of an already-known unsolvable piece,
since if N copies of a piece cannot assemble without overlapping, N copies of any
superset piece cannot assemble either.
At least one bounding dimension of each piece must be strictly smaller than the cube size.
"""

import sys
import os
import argparse
from typing import Set, Tuple, List, Dict

# Ensure local build directory is in sys.path if running from repository
script_dir = os.path.dirname(os.path.abspath(__file__))
for candidate in [
    os.path.join(script_dir, "build"),
    os.path.join(os.path.dirname(script_dir), "build"),
    os.path.join(os.path.dirname(os.path.dirname(script_dir)), "build"),
]:
    if os.path.isdir(candidate) and candidate not in sys.path:
        sys.path.insert(0, candidate)

try:
    import burrtools
except ImportError:
    print("Error: Could not import 'burrtools'.", file=sys.stderr)
    print("Make sure to build the extension module first (e.g. 'just build') or set PYTHONPATH=build.", file=sys.stderr)
    sys.exit(1)

Coord = Tuple[int, int, int]
Piece = Tuple[Coord, ...]

def generate_cube_symmetries(include_reflections: bool = True):
    """
    Generate orthogonal symmetry matrices of the 3D cube.
    - If include_reflections=True: returns all 48 symmetries (full octahedral group O_h).
    - If include_reflections=False: returns 24 proper rotations (chiral octahedral group O, det = +1).
    """
    symmetries = []
    axes = [(1,0,0), (-1,0,0), (0,1,0), (0,-1,0), (0,0,1), (0,0,-1)]
    for x_axis in axes:
        for y_axis in axes:
            if x_axis[0]*y_axis[0] + x_axis[1]*y_axis[1] + x_axis[2]*y_axis[2] == 0:
                if include_reflections:
                    for z_axis in axes:
                        if (x_axis[0]*z_axis[0] + x_axis[1]*z_axis[1] + x_axis[2]*z_axis[2] == 0 and
                            y_axis[0]*z_axis[0] + y_axis[1]*z_axis[1] + y_axis[2]*z_axis[2] == 0):
                            symmetries.append((x_axis, y_axis, z_axis))
                else:
                    z_axis = (
                        x_axis[1]*y_axis[2] - x_axis[2]*y_axis[1],
                        x_axis[2]*y_axis[0] - x_axis[0]*y_axis[2],
                        x_axis[0]*y_axis[1] - x_axis[1]*y_axis[0]
                    )
                    symmetries.append((x_axis, y_axis, z_axis))
    return symmetries

ROTATIONS = generate_cube_symmetries(include_reflections=False)
ALL_SYMMETRIES = generate_cube_symmetries(include_reflections=True)

def transform_coord(c: Coord, mat) -> Coord:
    x_axis, y_axis, z_axis = mat
    x, y, z = c
    rx = x * x_axis[0] + y * x_axis[1] + z * x_axis[2]
    ry = x * y_axis[0] + y * y_axis[1] + z * y_axis[2]
    rz = x * z_axis[0] + y * z_axis[1] + z * z_axis[2]
    return (rx, ry, rz)

def normalize_piece(coords: Set[Coord]) -> Piece:
    """Translate piece so min(x)=0, min(y)=0, min(z)=0 and sort coordinates."""
    min_x = min(x for x, y, z in coords)
    min_y = min(y for x, y, z in coords)
    min_z = min(z for x, y, z in coords)
    translated = sorted((x - min_x, y - min_y, z - min_z) for x, y, z in coords)
    return tuple(translated)

def canonicalize_piece(coords: Set[Coord], symmetries=ALL_SYMMETRIES) -> Piece:
    """Return the lexicographically smallest orientation among the given symmetries."""
    return min(normalize_piece({transform_coord(c, mat) for c in coords}) for mat in symmetries)

def fits_in_box(piece: Piece, max_dim: int = 3, require_smaller_dim: bool = True) -> bool:
    """
    Check if at least one orientation of the piece fits within max_dim x max_dim x max_dim.
    If require_smaller_dim is True (and max_dim > 1), also requires that at least one
    dimension is strictly smaller than max_dim (so the piece does not span max_dim along all 3 axes).
    """
    for rot in ROTATIONS:
        rotated = [transform_coord(c, rot) for c in piece]
        min_x = min(x for x, y, z in rotated)
        min_y = min(y for x, y, z in rotated)
        min_z = min(z for x, y, z in rotated)
        max_x = max(x for x, y, z in rotated) - min_x + 1
        max_y = max(y for x, y, z in rotated) - min_y + 1
        max_z = max(z for x, y, z in rotated) - min_z + 1
        if max_x <= max_dim and max_y <= max_dim and max_z <= max_dim:
            if require_smaller_dim and max_dim > 1 and (max_x == max_dim and max_y == max_dim and max_z == max_dim):
                return False
            return True
    return False

def piece_to_bitmask(coords, cube_size: int = 3) -> int:
    mask = 0
    cs2 = cube_size * cube_size
    for x, y, z in coords:
        mask |= (1 << (x + cube_size * y + cs2 * z))
    return mask

def get_all_placements(piece: Piece, cube_size: int = 3, symmetries=ALL_SYMMETRIES) -> Set[int]:
    """Generate all bitmasks for all rotations/reflections and translations of piece within cube_size^3."""
    masks = set()
    for mat in symmetries:
        rotated = {transform_coord(c, mat) for c in piece}
        norm = normalize_piece(rotated)
        max_x = max(x for x, y, z in norm)
        max_y = max(y for x, y, z in norm)
        max_z = max(z for x, y, z in norm)
        if max_x < cube_size and max_y < cube_size and max_z < cube_size:
            for dx in range(cube_size - max_x):
                for dy in range(cube_size - max_y):
                    for dz in range(cube_size - max_z):
                        placed = [(x + dx, y + dy, z + dz) for x, y, z in norm]
                        masks.add(piece_to_bitmask(placed, cube_size))
    return masks

def generate_free_polycubes(
    max_voxels: int = 6,
    cube_size: int = 3,
    symmetries=ALL_SYMMETRIES,
    require_smaller_dim: bool = True,
) -> Dict[int, List[Piece]]:
    """Generate all unique free polycubes that fit in a cube_size^3 bounding box up to max_voxels."""
    polycubes_by_size = {1: [((0, 0, 0),)]}

    for k in range(1, max_voxels):
        next_set = set()
        for p in polycubes_by_size[k]:
            p_set = set(p)
            for x, y, z in p:
                for dx, dy, dz in [(-1,0,0), (1,0,0), (0,-1,0), (0,1,0), (0,0,-1), (0,0,1)]:
                    neighbor = (x + dx, y + dy, z + dz)
                    if neighbor not in p_set:
                        candidate = p_set | {neighbor}
                        if fits_in_box(candidate, cube_size, require_smaller_dim=require_smaller_dim):
                            next_set.add(canonicalize_piece(candidate, symmetries))
        polycubes_by_size[k + 1] = sorted(list(next_set))

    return polycubes_by_size

def solve_piece(piece: Piece, cube_size: int = 3, num_pieces: int = 4, count_all: bool = False, stop_at: int = 1) -> int:
    """
    Test whether num_pieces copies of the piece can assemble into a cube_size^3 cube with variable voxels.
    - If count_all is True: counts all assemblies.
    - If count_all is False: stops searching once stop_at assemblies are found.
    Returns the number of assemblies found.
    """
    puzzle = burrtools.Puzzle()
    
    # Target result shape with variable voxels (unfilled voxels allowed)
    target = puzzle.add_shape(cube_size, cube_size, cube_size, name=f"target_{cube_size}x{cube_size}x{cube_size}")
    for x in range(cube_size):
        for y in range(cube_size):
            for z in range(cube_size):
                target.set_state(x, y, z, burrtools.Voxel.VARIABLE)

    # Piece shape with bounding box
    max_x = max(x for x, y, z in piece) + 1
    max_y = max(y for x, y, z in piece) + 1
    max_z = max(z for x, y, z in piece) + 1
    shape = puzzle.add_shape(max_x, max_y, max_z, name="piece")
    shape.fill(piece)

    problem = puzzle.add_problem(name=f"assemble_{num_pieces}_pieces")
    problem.set_result(target)
    problem.set_piece_count(shape, num_pieces)

    assemblies = 0
    solver = problem.solve(disassemble=False)
    for _ in solver:
        assemblies += 1
        if not count_all and assemblies >= stop_at:
            solver.stop()
            break

    return assemblies

def is_superset_of_unsolvable(piece_mask: int, unsolvable_placements: List[Set[int]]) -> bool:
    """
    Check if the candidate piece contains any translated/rotated placement of an unsolvable piece.
    """
    for placement_set in unsolvable_placements:
        for u_mask in placement_set:
            if (piece_mask & u_mask) == u_mask:
                return True
    return False

def main():
    parser = argparse.ArgumentParser(
        description="Search for puzzles with identical pieces in a cubic box (assemblies only) with subset pruning."
    )
    parser.add_argument(
        "--cube-size", "-c",
        type=int,
        default=3,
        help="Target cube side length (default: 3 for a 3x3x3 cube)",
    )
    parser.add_argument(
        "--pieces", "-p",
        type=int,
        default=4,
        help="Number of identical pieces (default: 4)",
    )
    parser.add_argument(
        "--min-size",
        type=int,
        default=None,
        help="Minimum piece size in voxels (default: min(3, max-size))",
    )
    parser.add_argument(
        "--max-size",
        type=int,
        default=None,
        help="Maximum piece size in voxels (default: floor(cube_size^3 / piece_count))",
    )
    parser.add_argument(
        "--include-mirrors",
        action="store_true",
        help="Include chiral mirror duplicates (uses 24 rotations instead of 48 symmetries)",
    )
    parser.add_argument(
        "--allow-full-cube-pieces",
        action="store_true",
        help="Allow pieces that span the full cube size along all three axes (default: false, requiring at least one dimension smaller than the cube)",
    )
    parser.add_argument(
        "--single-assembly-only",
        "--unique-only",
        action="store_true",
        dest="single_assembly_only",
        help="Only save puzzles that have a single unique assembly (stops searching after 2 assemblies if not --count-all)",
    )
    parser.add_argument("--count-all", action="store_true", help="Count all assemblies instead of stopping at first")
    parser.add_argument("--save-dir", type=str, default=None, help="Directory to save solvable puzzles as .xmpuzzle")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose progress output")
    args = parser.parse_args()

    if args.cube_size < 1:
        parser.error("--cube-size must be at least 1")
    if args.pieces < 1:
        parser.error("--pieces must be at least 1")

    cube_vol = args.cube_size ** 3
    max_possible = cube_vol // args.pieces

    if max_possible < 1:
        print(f"Error: Cube volume {cube_vol} cannot accommodate {args.pieces} pieces (max size = 0).", file=sys.stderr)
        sys.exit(1)

    if args.max_size is None:
        args.max_size = max_possible
    elif args.max_size > max_possible:
        print(f"Note: {args.pieces} pieces * {args.max_size} voxels = {args.pieces * args.max_size} > {cube_vol} "
              f"(volume of {args.cube_size}x{args.cube_size}x{args.cube_size}). Capping max_size to {max_possible}.")
        args.max_size = max_possible

    if args.min_size is None:
        args.min_size = max(1, min(3, args.max_size))
    elif args.min_size > args.max_size:
        print(f"Note: min_size ({args.min_size}) cannot be larger than max_size ({args.max_size}). Setting min_size={args.max_size}.")
        args.min_size = args.max_size
    elif args.min_size < 1:
        args.min_size = 1

    if args.save_dir:
        os.makedirs(args.save_dir, exist_ok=True)

    active_symmetries = ROTATIONS if args.include_mirrors else ALL_SYMMETRIES
    sym_desc = "24 rotations (chiral mirrors included)" if args.include_mirrors else "48 symmetries (mirror duplicates prohibited)"

    print("=" * 70)
    print(f" {args.cube_size}x{args.cube_size}x{args.cube_size} Puzzle Search: {args.pieces} Identical Pieces (Assemblies Only)")
    print(f" Piece sizes: {args.min_size} to {args.max_size} voxels ({args.pieces} * size <= {cube_vol})")
    print(f" Target: {args.cube_size}x{args.cube_size}x{args.cube_size} bounding box with variable voxels")
    print(f" Symmetries: {sym_desc}")
    if not args.allow_full_cube_pieces and args.cube_size > 1:
        print(" Constraint: At least one dimension must be smaller than the cube")
    if args.single_assembly_only:
        print(" Filter: ONLY saving/highlighting puzzles with a single unique assembly")
    print(" Pruning: Supersets of known unsolvable pieces are automatically skipped")
    print("=" * 70)

    print("\nGenerating candidate polycubes...")
    polycubes = generate_free_polycubes(
        args.max_size,
        cube_size=args.cube_size,
        symmetries=active_symmetries,
        require_smaller_dim=not args.allow_full_cube_pieces,
    )

    total_candidates = sum(len(polycubes[s]) for s in range(args.min_size, args.max_size + 1))
    print(f"Total candidate polycubes (sizes {args.min_size}..{args.max_size}): {total_candidates}")

    # Unsolvable pieces tracking
    unsolvable_pieces: List[Piece] = []
    unsolvable_placements: List[Set[int]] = []

    tested_count = 0
    pruned_count = 0
    solvable_count = 0
    single_assembly_count = 0
    saved_count = 0

    solvable_results = []

    stop_at = 2 if args.single_assembly_only else 1

    for size in range(args.min_size, args.max_size + 1):
        candidates = polycubes[size]
        print(f"\n--- Testing size {size} ({len(candidates)} candidate polycubes) ---")

        size_solvable = 0
        size_single = 0
        size_pruned = 0
        size_unsolvable = 0

        for idx, piece in enumerate(candidates, 1):
            # Check if piece contains an unsolvable sub-piece (piece is at origin)
            norm_mask = piece_to_bitmask(piece, cube_size=args.cube_size)
            
            # A piece contains an unsolvable piece u if some placement of u is a subset of piece
            if is_superset_of_unsolvable(norm_mask, unsolvable_placements):
                pruned_count += 1
                size_pruned += 1
                if args.verbose:
                    print(f"  [Size {size} #{idx}] PRUNED (contains known unsolvable sub-piece)")
                continue

            tested_count += 1
            assemblies = solve_piece(
                piece,
                cube_size=args.cube_size,
                num_pieces=args.pieces,
                count_all=args.count_all,
                stop_at=stop_at,
            )

            if assemblies > 0:
                solvable_count += 1
                size_solvable += 1
                is_unique = (assemblies == 1)
                if is_unique:
                    single_assembly_count += 1
                    size_single += 1

                solvable_results.append((size, piece, assemblies))

                if args.single_assembly_only:
                    if is_unique:
                        print(f"  [Size {size} #{idx}] SOLVABLE (1 unique assembly)")
                    elif args.verbose:
                        print(f"  [Size {size} #{idx}] SOLVABLE (>1 assemblies - not saving)")
                else:
                    if args.verbose or not args.count_all:
                        label = f"{assemblies} assemblies" if (args.count_all or assemblies == 1) else f">={assemblies} assemblies"
                        print(f"  [Size {size} #{idx}] SOLVABLE ({label})")

                should_save = bool(args.save_dir) and (not args.single_assembly_only or is_unique)
                if should_save:
                    puz = burrtools.Puzzle()
                    target = puz.add_shape(args.cube_size, args.cube_size, args.cube_size, name=f"target_{args.cube_size}x{args.cube_size}x{args.cube_size}")
                    for x in range(args.cube_size):
                        for y in range(args.cube_size):
                            for z in range(args.cube_size):
                                target.set_state(x, y, z, burrtools.Voxel.VARIABLE)
                    max_x = max(x for x, y, z in piece) + 1
                    max_y = max(y for x, y, z in piece) + 1
                    max_z = max(z for x, y, z in piece) + 1
                    shape = puz.add_shape(max_x, max_y, max_z, name=f"piece_{size}v")
                    shape.fill(piece)
                    prob = puz.add_problem(name=f"{args.pieces}_pieces_size_{size}")
                    prob.set_result(target)
                    prob.set_piece_count(shape, args.pieces)
                    fname = os.path.join(args.save_dir, f"puzzle_c{args.cube_size}_p{args.pieces}_size{size}_{idx:03d}.xmpuzzle")
                    puz.save(fname)
                    saved_count += 1
            else:
                size_unsolvable += 1
                unsolvable_pieces.append(piece)
                unsolvable_placements.append(get_all_placements(piece, cube_size=args.cube_size, symmetries=active_symmetries))
                print(f"  [Size {size} #{idx}] UNSOLVABLE -> registered for pruning supersets")

        if args.single_assembly_only:
            print(f"Size {size} summary: {size_solvable} solvable ({size_single} unique), {size_unsolvable} unsolvable, {size_pruned} pruned.")
        else:
            print(f"Size {size} summary: {size_solvable} solvable, {size_unsolvable} unsolvable, {size_pruned} pruned.")

    print("\n" + "=" * 70)
    print(" Search Complete")
    print("=" * 70)
    print(f"Total candidates evaluated: {total_candidates}")
    print(f"Tested with solver:        {tested_count}")
    print(f"Pruned (skipped):          {pruned_count}")
    print(f"Solvable pieces found:     {solvable_count}")
    if args.single_assembly_only:
        print(f"Unique single-assembly:    {single_assembly_count}")
    if args.save_dir:
        print(f"Puzzles saved to disk:     {saved_count} (in {args.save_dir})")
    print(f"Unsolvable base pieces:    {len(unsolvable_pieces)}")
    print("=" * 70)

if __name__ == "__main__":
    main()
