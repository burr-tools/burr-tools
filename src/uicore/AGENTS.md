# src/uicore — guide for AI agents and contributors

The user interface's logic without a toolkit: everything that decides *what*
the GUI does, kept free of Qt and FLTK so it is tested in plain C++ and could
serve another front end. `src/qtgui` (burrtools-qt) turns it into Qt objects
and pixels; the legacy FLTK GUI shares two parts of it, the undo history
and the piece colours.
Namespace `btui`, built as `libburr_uicore` (root `meson.build`).

`src/qtgui` uses it together with the library's classes, not instead of
them (see the architecture in [`../qtgui/README.md`](../qtgui/README.md)).
The API here is in-tree only and changes with the later phases (the Puzzle
and Solver workspaces, the voxel editor); do not treat it as stable.

Read with it: [`../qtgui/AGENTS.md`](../qtgui/AGENTS.md) (the layer above),
[`../qtgui/README.md`](../qtgui/README.md) (architecture and status), and the
spec ([`../../design-spec/BurrTools-SPEC-FULL-FINAL-v6.md`](../../design-spec/BurrTools-SPEC-FULL-FINAL-v6.md)),
whose component and acceptance ids (C06, AC-C13-03, …) the comments cite.

## Commands

Tests: `../../test/test_ui_<name>.cpp`, tagged `[ui]`, compiled into
`test_burrtools` (`uicore_test_src` in the root `meson.build`); benchmarks in
`../../test/bench_ui.cpp` (`[bench]`, hidden).

```sh
./build/test_burrtools "[ui]"          # all of uicore's tests
./build/test_burrtools "[ui][scenemesh]"
just bench-ui                          # the benchmarks, release build
```

## Rules

1. **No toolkit.** No `<Q…>` or `<FL/…>` include, no Qt types, no FLTK
   calls: standard C++20 and the library (`src/lib`, `src/halfedge`,
   `src/tools`).
2. **Every behaviour has a Catch2 test**, tagged `[ui][<area>]`.
3. **Add a file in three places:** the source in `libburr_uicore`, its test
   in `uicore_test_src` (root `meson.build`), and a line in
   `llm_docs/uicore_map.md`.
4. **The puzzle changes only through `DocumentSession::edit` /
   `editWithoutUndo` / `editVoxels`**; `puzzle()` is read-only.
5. **`puzzlehistory` and `palette` are shared with the legacy GUI**: a
   change there changes it too.

## Consult when the task touches...

- Which file decides what, where its test is -> `llm_docs/uicore_map.md`
- Writing code here: tests, ownership, determinism, the command table, the legacy GUI, puzzle edits, files, enums, text colour, performance -> `llm_docs/uicore_rules.md`
- Reviewing a hand-written PR -> `llm_docs/uicore_review.md`
