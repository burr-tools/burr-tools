# Rotation-move study (2026-10-21)

Scripts and result summaries behind the "The motions", "The planner" and
"Corpus results" sections of
[`../2026-10-19-rotation-moves.md`](../2026-10-19-rotation-moves.md), the
design document for rotation moves. The findings are summarised there; this
directory is how to reproduce them.

The raw results (per-puzzle JSON, PR #50's accepted rotations, the
random-scene output) and the explainer page are not on master: they are
kept under the tag `rotation-study-2026-10-30` (branch
`rotation-study-data`), in this directory. Most of them come from binaries
that no longer exist, and all from puzzles that are not in the repository,
so they are evidence of their date rather than something to keep up to
date. `git checkout rotation-study-2026-10-30 -- design/rotation-study`
brings them back.

## Explainer page

`rotation-moves.html` (under the tag): which rotations the solver covers
and how a path is certified, with live demos running a JavaScript port of
the geometry. Open it in a browser (it loads only Google Fonts).

## Puzzles

The puzzles are not in the repository (`puzzles/` is gitignored). The
scripts look for them in `puzzles/` next to the repository root, or in
`$PUZZLES`:

* `TICs/decoded/*.xmpuzzle`: 21 TICs and RIBs with puzzlecad sources
  (`TICs/*.scad`). Their printed model (`results/tic_models.txt`) comes from
  `scripts/puzzlecad2xmpuzzle.py` (branch `puzzlecad-script`).
* `BTFiles/Andrew Crowell/`: Andrew Crowell's collection, 319 files
  (146 4x4x4 TICs, 98 5x5x5 TICs, 45 ARCparent cubes, 20 RIBs, 10 RIPs;
  some puzzles twice, as `X` and `X_SolutionFile`).

## Scripts (`scripts/`)

| Script | What it does |
|---|---|
| `geom.js` | Port of `rotationgeometry.cpp` (planar part): signed distance, clearance, certified checker, `makePath`, the family order. Used by every JS script. |
| `planner3.js <seed> <n>` | Random 2D candidates: which turns the 65-path family finds, and which a grid search over (θ, du, dv) finds instead (certified), minus those reachable by a one-cell slide plus a family turn. |
| `shapes.js`, `profiles.js`, `profiles2.js`, `profiles3.js <seed> <n> sharp\|pcad` | Wiggle profile comparisons: sine, bulge, contact, clipped, powers of the bulge, square vs octagon bulge. |
| `amounts.js <seed> <n> sharp\|pcad` | Amount sets for the bulge wiggle: 0.1–0.4 grid, geometric amounts, up to 0.6. |
| `audit.js <ROTJSON file> [play]` | Re-checks rotations PR #50 accepted (dumped with `BT_ROT_JSON=1`, branch `rotations-pr50`) with the certified checker over all layers: our family, larger amounts, or a grid search. |
| `audit2.js <ROTJSON file> [play] [step]` | For the rotations our family misses: does a dense grid of shift vectors rescue them (slide, bulge wiggle, ramp-hold-ramp)? |
| `tics.py name=burrTxt ...` | Decoded TICs: PR #50's model (play 0.081, sharp; the default until 2026-10-30), printed model, bisection of the smallest play (sharp) and smallest bevel (at printed play). Writes `results/tics.json`. |
| `crowell.py name=burrTxt ...` | Crowell collection: PR #50's model (play 0.081, sharp) and bisection of the smallest play. Appends to `results/crowell.jsonl`, resumable. |
| `pr50crowell.py -R\|-C` | PR #50's Classic (`-R`) or Crowell (`-C`) engine on the Crowell collection, 60 s per puzzle; needs `BURRTXT_PR50` (burrTxt from `rotations-pr50`). |
| `xcheck.cpp <solutionfile>` | Compares a Crowell `_SolutionFile` (one snapshot per step) with our disassembly of its first snapshot, state by state, modulo rigid motions; `FROM=1 PARTIAL=1` also disassembles from every snapshot, which pins down the step we cannot make. Build: `c++ -I build-rel -I src -DNDEBUG -std=c++20 -O2 -pthread xcheck.cpp build-rel/libburr_core.a build-rel/subprojects/manifold-3.5.2/libmanifold.a -lz`. |
| `diag.cpp <solutionfile> <k>` | Diagnoses step k-1 -> k of a solution file: moving group, axis, pivot, family vs grid search over plays; `REQ=1` the play the turn needs (`requiredPlay`), `PLAN=1` one planner run, `DUMP=1` the scene as cells (for tests), `FAM=1` the planner's path against candidate fixed families (`results/crowell_blocked_families.txt`). The 17 blocked steps are listed in `results/crowell_blocked_steps.txt`. |
| `run.sh <burrTxt> <outdir>` | Example puzzles with `-R` under both models, for a before/after diff. |

The binaries compared were release builds (`just build-release`) of:

* `sine`: `rotations` at f5b8a74e, wiggles `a·sin(πt)`;
* `bulge`: ef635dae, wiggles along the cube bulge (the current default);
* `bulge6`: ef635dae with amounts 0.1–0.6 in `defaultRotationSettings()`;
* `a9`, `a28`: amounts every 0.05 up to 0.45, every 0.0125 up to 0.35 (timing only);
* `bits`, `cheap`: b993004f (path bitsets) and fc63edd0 (cheaper certification), with the same amount variants (timing only);
* PR #50: `rotations-pr50` at 42b96188.

## Results (`results/`)

On master:

| File | Content |
|---|---|
| `tic_models.txt` | Printed model per decoded TIC (`play,bevel`). |
| `crowell_blocked_steps.txt` | The 17 steps of Crowell's solution files we could not make. |
| `crowell_blocked_families.txt` | `diag.cpp FAM=1` on the 17 blocked steps: the planner's widest path at play 0.081 and the best path of each candidate fixed family (2026-10-28). |
| `crowell_xcheck_summary.txt` | `xcheck.cpp` on Crowell's solution files: our level and turns, and how many of his steps our disassembly matches state by state. |
| `pr50_tics.txt` | PR #50 Classic and Crowell engine on the decoded TICs (300 s limit). |
| `audits.txt` | Output of `audit.js` / `audit2.js` on PR #50's accepted rotations. |

Under the tag `rotation-study-2026-10-30` only (the scripts write them here
when rerun):

| File | Content |
|---|---|
| `tics.json` (`tics_a05.json`, `tics_wide.json`: other amounts, the wide family) | Per decoded TIC and binary: default and printed run (solved, level, rotations, time), `minPlay`, `minBevel`. |
| `crowell.jsonl` | Per Crowell puzzle and binary: default run and `minPlay` (6-step bisection in [0.002, 0.2]). |
| `pr50crowellR.jsonl`, `pr50crowellC.jsonl` | PR #50 engines on the Crowell collection (60 s limit; `solved: null` is a timeout). |
| `threetic_pr50_accepted.txt.gz`, `gemtic_pr50_accepted.txt.gz` | The distinct rotations PR #50's Classic engine accepted on Crowell's 4x4x4 ThreeTIC (1000) and 5x5x5 GemTIC (45), as `ROTJSON` lines. |
| `random-scenes/*.out` | Raw output of the random-scene runs (`prof*`: profiles, `am_*`: amounts, `p3_*`: planner3). |

Caveats: the random scenes are single-layer 2D and not puzzles; the grid
search is resolution-limited (1.5°, 0.025 cells), so "no motion found" is
not a proof; `minPlay` / `minBevel` bisections assume monotonicity, which a
fixed path family does not guarantee; timings were taken on a 4-core VM with
other jobs running and are only good for ratios.
