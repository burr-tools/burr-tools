# uicore: reviewing a change (for an agent reviewing a hand-written PR)

Read the diff against `llm_docs/uicore_rules.md`. For each gap, name the
rule, point to the file that does it right, and suggest the smallest fix.

- A Qt or FLTK include or type in `src/uicore` → move that part to the
  toolkit layer, keep the decision here.
- Logic added to a Qt controller or QML that needs no window → suggest it
  belongs here, with a test.
- New behaviour without a `[ui]` test, or a test that only exercises the
  happy path where the GUI can pass edge values (empty, -1, every grid).
- A menu item or shortcut defined outside `commands.cpp`; a legacy command
  that disappears without an entry in `dropped`; a reused access key.
- A change to `puzzlehistory` or to `palette`'s colours without considering
  the legacy GUI.
- A raw owning pointer from the library, an assert guarding input, a
  function reachable from the GUI that can throw on reachable input.
- A change to the puzzle outside `DocumentSession::edit` /
  `editWithoutUndo` (a `const_cast`, a reference kept from inside `f`).
- A file written in place, or a write result ignored.
- A `switch` with `default:` over a project enum; a new voxel style or enum
  value not handled everywhere (`applyStyle`, settings, tests).
- A second text-colour rule, or colour logic duplicated from `palette`.
- Per-frame or per-triangle allocations added to `scenemesh` / `depthsort`;
  a hot path changed without benchmark numbers.
- A new file missing from `libburr_uicore`, its test missing from
  `uicore_test_src`, or `llm_docs/uicore_map.md` not updated.
