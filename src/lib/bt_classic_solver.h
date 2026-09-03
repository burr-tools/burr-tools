/* BurrTools
 *
 * AI / maintainer notes for BurrTools Classic (SOLVER_CLASSIC).
 *
 * Read this file when comparing the three solver types, or before changing
 * disassembler_0_c, rotationMoves_0_c, assembler_0_c, or assembler_1_c in a
 * way that would change BurrTools Classic numerical behaviour.
 *
 * Andrew Crowell heuristics must stay in rotationmoves_crowell.* /
 * disassembler_crowell.* (see crowell_solver.h). DLX tree-split must stay
 * behind SOLVER_BT2 (see bt2_solver.h). BurrTools Classic is the
 * complete-search baseline.
 *
 * ---------------------------------------------------------------------------
 * THE THREE SOLVER TYPES (comparison)
 * ---------------------------------------------------------------------------
 *
 * GUI: Solver tab → "Solver Type" (BurrTools Classic | Andrew Crowell | BurrTools 2).
 * CLI: ./build/burrTxt -dRr --solver "BurrTools Classic"|"Andrew Crowell"|"BurrTools 2" <file>
 *      short names: classic | crowell | bt2   (default: BurrTools Classic)
 *
 *                 BurrTools Classic    Andrew Crowell        BurrTools 2
 * Take-apart      complete BFS         incomplete 90°        same as Classic
 *                 Cutler slides +      heuristics cloned     (disassembler_0_c +
 *                 all 90° subsets      from SolveSlidingCube rotationMoves_0_c)
 * Assembly        assembler_0 / _1     same as Classic       assembler_bt2_c
 *                 single-thread DLX                          dancing cells +
 *                                                            split / steal
 * Completeness    yes                  no (can miss paths)   same take-apart as
 *                                                            Classic; assembly
 *                                                            covers the same tree
 * When it wins    baseline; puzzles    rotation-heavy        DLX/cells is the
 *                 that need a path     take-apart (e.g.      limiter and there
 *                 Crowell pruned       MagellanTIC ~4.5s     are many placements
 *                                      vs Classic ~100s)
 * MagellanTIC     ~100s, 1 assembly,   ~4.5s, 1 assembly     ~same as Classic:
 *                 1 solution                                 take-apart dominates
 *
 * Shared by all three:
 *   - Linear slides are Bill Cutler (movementAnalysator_c), not Fortran SimpleMove.
 *   - Rotation geometry (bevel arc, sandwich, perp, axis-cross) is rotationRules_c.
 *   - Check Rotations must be on or no 90° moves are generated.
 *   - assembler_1_c (ranges / multi-copies of a shape) is always serial.
 *
 * Docs per engine:
 *   BurrTools Classic  this file
 *   Andrew Crowell     crowell_solver.h
 *   BurrTools 2        bt2_solver.h
 *
 * ---------------------------------------------------------------------------
 * HOW CLASSIC IS SELECTED
 * ---------------------------------------------------------------------------
 *
 * Factory: createDisassembler(..., SOLVER_CLASSIC) → new disassembler_0_c
 *          → disassembler_a_c(..., SOLVER_CLASSIC)
 *          → movementAnalysator_c(..., SOLVER_CLASSIC)
 *          → new rotationMoves_0_c
 * Assembly: puzzle gridType→findAssembler (assembler_0_c or assembler_1_c),
 *           then assemble() on the solve thread. No BT2 dancing cells.
 *
 * ---------------------------------------------------------------------------
 * TAKE-APART (complete search)
 * ---------------------------------------------------------------------------
 *
 * disassembler_0_c::disassemble_rec is a 3-front BFS (new / old / old2) over
 * unit steps. movementAnalysator_c::find generates, in order:
 *   0. piece removals, 1. group removals, 2. 1-step slides, 3. 90° rotations.
 * First is_separation() node wins; checkSubproblems recurses on both parts.
 *
 * rotationMoves_0_c tries every non-empty proper subset of remaining pieces
 * (masks 1 .. 2^n-2), including the largest piece. Pivot cells are every
 * half-index in the in-plane bbox. Each (subset, axis, pivot, sense) is
 * checked with rotationRules_c::allowRotation. No DctMax, no Nkeep, no
 * reverse-skip, no axis-level prune that drops a whole axis before pivots.
 *
 * That is why Classic is slower than Fortran on rotation puzzles: it explores
 * a complete 90° tree. Do not add Fortran caps here.
 *
 * Disassembly workers (solveThread): 1 worker when Check Rotations is on
 * (rotation BFS is memory-bandwidth heavy); otherwise hw-2 capped at 16.
 * MagellanTIC has 1 assembly so the pool never runs in parallel anyway.
 *
 * ---------------------------------------------------------------------------
 * ASSEMBLY (single-thread dancing links)
 * ---------------------------------------------------------------------------
 *
 * assembler_0_c: Knuth DLX, one of each piece, iterativeMultiSearch.
 * assembler_1_c: ranges / multi-pieces, recursive/iterative covering.
 * Both run assemble() on one thread. createMatrix + reduce happen once.
 * Resume uses setPosition / save() (ASSEMBLER_VERSION "1.4").
 *
 * BurrTools 2 does not use these classes for assembly. Its assembler is
 * assembler_bt2_c (see bt2_solver.h).
 *
 * ---------------------------------------------------------------------------
 * FILES (do not put Fortran or BT2 policy in these)
 * ---------------------------------------------------------------------------
 *
 * disassembler_0.cpp      3-front BFS, no successor cap
 * rotationmoves_0.cpp     all subsets, dense pivots
 * assembler_0.cpp         DLX; no BT2 hooks
 * assembler_1.cpp         range assembler; no split
 * solvethread.cpp         Classic path: assm->assemble(this)
 */
#ifndef __BT_CLASSIC_SOLVER_H__
#define __BT_CLASSIC_SOLVER_H__

#endif
