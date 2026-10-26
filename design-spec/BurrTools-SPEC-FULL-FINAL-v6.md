# BurrTools Entities + Puzzle + Solver redesign — FULL SPEC (single-file edition, Rev 6.0 — final)

> Concatenation of every markdown document in the bundle, in reading order. Asset paths are relative to the bundle root (`burrtools-entities-redesign-spec/`). The modular files in the zip are authoritative; `foundations/density.md` is the authority for dimensions.


---
<!-- FILE: README.md -->

# BurrTools — Entities tab redesign: implementation spec bundle

**Audience:** coding agents / engineers migrating the legacy BurrTools Entities tab (the voxel/piece editor) to the new UI.
**Status:** design approved at mock level; this bundle is the source of truth for implementation.
**Target stack:** front-end framework **to be decided between Qt 6 Quick (QML views + C++ controllers — recommended) and RmlUi (RML/RCSS + C++ controllers)** by the spike in `migration/04-framework-spike-plan.md`; **all interaction logic in C++** in both cases. The spec is **framework-neutral**: it describes interactions, states, visuals and the view↔logic contract; the two implementation profiles are `foundations/qtquick-implementation-guide.md` and `foundations/rmlui-implementation-guide.md`.
**Scope:** the **Entities, Puzzle and Solver workspaces** (C00–C21) and the shared shell (workspace rail, top bar, status bar, Settings). Not rewritten: file format, solver algorithms, voxel data model; legacy menu-only dialogs keep their existing UI (OQ-34).

> The interactive mock (`reference/mock/entities-mock.html`) is a *visual and behavioural reference only* — open it in a browser to see and click through the design. Anything scripted inside it is prototype scaffolding that is **not part of the spec**. Build the real UI as RML/RCSS documents plus C++ controllers.

## 1. Start here

1. Read `AGENT-INSTRUCTIONS.md` (rules of engagement, definition of done, how to report), then `REVISIONS.md` (latest design decisions).
2. Read `overview/` (goals, layout, state model, input map, **UI contract (ids / data / events)**, **open questions you must resolve by reading the repo**).
3. Read `foundations/` (**the implementation guide for the chosen framework** — Qt Quick or RmlUi — plus tokens, glyphs, cursors, primitives). Phase P0 implements these; until the framework is decided, run the spike (`migration/04`).
4. Work through `migration/00-phase-plan.md` phase by phase. Each phase points to the `components/C##-*.md` files it implements.
5. Use `migration/01-legacy-to-new-map.md` for the 1:1 old→new control mapping, and `migration/02-test-plan.md` for automated/manual tests.

## 2. Bundle map

| Path | Contents |
|---|---|
| `AGENT-INSTRUCTIONS.md` | How to work, constraints, reporting template |
| `overview/01-goals-scope-principles.md` | Why, what, design principles |
| `overview/02-layout-and-information-architecture.md` | Canonical layout, zones, dimensions, ASCII diagrams |
| `overview/03-state-model-and-persistence.md` | Every UI state variable: owner, default, persistence, events |
| `overview/04-input-map-shortcuts-and-focus.md` | Master keyboard / mouse / modifier map, focus order, Esc ladder |
| `overview/05-open-questions-verify-in-repo.md` | **Things the designer could not see** (legacy behaviour / platform facts to confirm from source) |
| `overview/06-ui-contract.md` | **Markup↔C++ contract**: element ids, data-model variables, events, state classes, cursor decision table |
| `foundations/density.md` | **Interface density** — Standard (default) and Minimal geometry tokens, Minimal rules, auto-hiding toolbar; authority for all dimensions |
| `foundations/qtquick-implementation-guide.md` | **Qt 6 Quick profile (recommended):** QML/C++ split, contract → Qt mapping, theming, special surfaces (QQuickRhiItem 3D, scene-graph grid), build/deploy, testing |
| `foundations/rmlui-implementation-guide.md` | **RmlUi profile:** RmlUi/C++ responsibility split, RCSS constraints, special surfaces, runtime infrastructure, RML skeletons |
| `migration/04-framework-spike-plan.md` | Two-week spike: scope, measurements, hard budgets, weighted decision matrix |
| `foundations/cursors/` | Edit-cursor composition assets: badge/icon templates, fallback arrow, rendered examples (light/dark, default + red drawing colour), contact sheet |
| `foundations/design-tokens.md` + `.json` | Colours (light/dark), type, spacing, radii, motion, state styling |
| `foundations/primitives.md` | Reusable widgets (IconButton, GlyphButton, ActionButton, Segmented, Stepper, Switch, Chip, Accordion, Popup menu, Dialog, Tooltip…) |
| `foundations/glyph-catalog.md` | Every glyph/icon, meaning, usage, file names |
| `foundations/glyphs/{light,dark}/*.svg` | 45 coloured glyphs (viewBox 32, exported at 64/48) |
| `foundations/ui-icons/*.svg` | 23 monochrome UI icons (stroke=currentColor, 24 viewBox) |
| `foundations/glyph-contact-sheet-*.png` | Visual index of all glyphs/icons |
| `components/C00…C12` | One spec per UI component (C00 shell · C01 Shapes · **C02 Grid size** · **C03 Fit & scale** · C04 Transform · C05 Repair & surface · C06 3D viewport & Display menu · C07 Tools/Mirror/Span · C08 Plane/Layers/Grid · C09 Drawing colour · C10 Status · C11 Layout modes · C12 Settings · **C13 View cube** · **C14 Voxel space types** · **C15 Workspace rail** · **C16–C18 Puzzle tab** · **C19–C21 Solver tab** · **C22 Keyboard shortcuts**): anatomy, states, interactions, rules, acceptance criteria |
| `migration/00-phase-plan.md` | Ordered, independently testable migration phases |
| `migration/01-legacy-to-new-map.md` | L-id → N-id mapping with behaviour changes |
| `migration/02-test-plan.md` | Widget-ID convention, UI-test cases (Given/When/Then), manual scripts |
| `migration/03-user-journeys.md` | 8 end-to-end user journeys (use as integration tests) |
| `migration/RESOLUTIONS.md` | Template to log how each open question was resolved from source |
| `reference/mock/entities-mock.html` | Interactive prototype (open in a browser) |
| `REVISIONS.md` | Log of design revisions to the mock/spec and the reasoning (read second) |
| `reference/screenshots/*.png` | Mock states at 2560×1600 (`00-new-annotated.png` carries N-ids) |
| `reference/legacy/*.png` | Legacy screenshots (`*-annotated.png` carry L-ids) |

## 3. Conventions used in every document

* **dp** — device-independent pixel. All dimensions are stated for a **logical 1600 × 1000 dp window**. On the canonical **2560 × 1600** display the UI scale is **×1.6** (1 dp = 1.6 physical px). Implement one global `UI_SCALE` and multiply everywhere; never hard-code physical pixels.
* **Legacy ids `L##`** name controls in the old UI (see annotated legacy images). **New ids `N##`** name controls in the new UI (see `00-new-annotated.png`).
* **Widget ids** like `entities.shapes.new` are the stable automation names every widget must expose (see test plan).
* **Requirement language:** MUST / SHOULD / MAY as in RFC 2119.
* **`[VERIFY-IN-REPO OQ-n]`** marks behaviour the designer could not observe; resolve it by reading the legacy source (`overview/05-open-questions-verify-in-repo.md`). When legacy behaviour and this spec disagree on *functional semantics*, **legacy wins**; when they disagree on *look, layout or interaction pattern*, **this spec wins**.
* **Mock-only** marks items shown in the prototype that are not product features ("Show zones" button, the voxel-type and solver-result selectors in the top bar).
* **Spec-only addition** marks behaviour not visible in the mock but required here (e.g. Alt+↑/↓ reordering).
* **Front-end note:** in either framework the view layer renders structure/styling only; state and behaviour live in C++ controllers bound through the contract in `overview/06-ui-contract.md`. Capabilities of the pinned RmlUi version must be verified (OQ-13).


---
<!-- FILE: AGENT-INSTRUCTIONS.md -->

# Instructions for the implementing agent

## Mission
Build the redesigned **Entities, Puzzle and Solver workspaces** as a front-end **driven by C++ controllers** — **Qt 6 Quick (QML)** or **RmlUi (RML/RCSS)**, whichever the spike in `migration/04-framework-spike-plan.md` selects (default: Qt 6 Quick) — on top of the **existing BurrTools model and command layer**. Every legacy capability must still exist (see `migration/01-legacy-to-new-map.md`) except the controls explicitly marked **Dropped**. File format, solver algorithms and the voxel data model are **not** being rewritten.

## What the spec is (and is not)
* The spec describes **interactions, states, visuals, markup structure and the markup↔logic contract**. It contains no scripting. Interaction logic is **C++**.
* `reference/mock/entities-mock.html` is a browser prototype to *look at and click through*. Ignore whatever scripting it contains; never port it. Where the mock and the written spec differ, **the written spec wins** (the mock simplifies: e.g. size clipping, immediate colour add, axis-letter gizmo).
* All dimensions in the component specs are **Standard** density; `foundations/density.md` defines Standard and Minimal and wins on conflicts.
* Start with the implementation guide for the chosen framework (`foundations/qtquick-implementation-guide.md` or `foundations/rmlui-implementation-guide.md`) and `overview/06-ui-contract.md`. Build the framework-neutral controller layer first.

## Ground rules
1. **Phase by phase** (`migration/00-phase-plan.md`). Do not start phase *n+1* until phase *n* exit criteria pass. Each phase leaves the app buildable and runnable.
2. **Markup is declarative, logic is C++.** State is held by controllers (data model); markup reads it (data bindings if available, otherwise C++-set classes/text). Events are handled in C++. Visual states use RCSS pseudo-classes or state classes — never inline colour hacks.
3. **Reuse model logic.** Call existing, undo-aware commands for flip, rotate, nudge, prune, centre, origin, scale, minimize, fill holes, surface ops, colour ops, shape new/duplicate/delete/reorder, weight. Algorithms given in the spec describe *expected results* to test against.
4. **Undo/redo.** Every mutating interaction is one undo step per the table in `overview/03` (a paint stroke, a 3D click with mirror/span expansion = one step each). Honour Settings ▸ Undo history depth.
5. **Resolve open questions from source** (`overview/05`); log answers in `migration/RESOLUTIONS.md` with file/function references. Verify the framework capabilities you rely on (OQ-13).
6. **Stable ids.** Give every element the id from its component spec (scheme `entities.<area>.<name>`); tests rely on it.
7. **One state, one owner, one control.** The 2D grid and the 3D viewport read/write the *same* tool/colour/mirror/span state and share one `hover_preview`.
8. **Theme via generated stylesheets** from `foundations/design-tokens.json` (light + dark from P0). No literal colours/sizes in widget code.
9. **dp units** everywhere; assets rasterised/selected for the device scale; cursors from `foundations/cursors/`.
10. **Accessibility baseline:** tab order, visible focus ring, Enter/Space activation, tooltips with the exact texts in the specs.

## Definition of done (per phase)
* Acceptance criteria (`AC-*`) referenced by the phase pass (automated where feasible, else the manual script, result recorded).
* 2560×1600 screenshots (light + dark) of the affected area compared with `reference/screenshots/*`.
* No regression in untouched features; no new warnings; no leaks in create/delete loops.
* Open questions touched by the phase resolved and logged.

## Reporting template (per PR / phase)
```
Phase: P#  Components: C##…
Implemented: <bullets>
Legacy controls replaced/removed: <L-ids → N-ids / Dropped>
ACs passed: <list>   ACs deferred: <list + reason>
Open questions resolved: <OQ-n → decision + source ref>
Framework capabilities relied on / fallbacks used: <list>
Deviations from spec: <none | list with justification>
Screenshots: <paths>
```

## When in doubt
Preserve legacy *functional* behaviour; match the reference screenshots and the interaction tables. If something is impossible in the chosen framework, implement the closest equivalent and log a deviation — never silently drop a behaviour.


---
<!-- FILE: REVISIONS.md -->

# Revisions log

## Rev 6.0 — final: density, keyboard, help window
| # | Change | Where |
|---|---|---|
| 1 | **Interface density** setting: **Standard** (former Compact; the old Comfortable spacing is retired) and **Minimal**. Whole spec **re-baselined to Standard**: side cards **320 / 340** (Minimal 264 / 320), workspace rail 60 (44), collapsed rails 48 (40), top bar 36 (32), status bar 24 (22), card radius 8 (0), headers 36 (30), rows 34, toolbar buttons 48×44 (34×32), Puzzle tiles 96×50 (78×46), Solver player 112 (84) and piece display 104 (76); centre 864 (970) at 1600 dp | foundations/density.md, tokens (md + json), overview/02, C00, C01, C06, C08, C11, C15–C17, C19, C20, primitives |
| 2 | **Minimal rules:** icon-only controls, help text → tooltips (guaranteed), short inline labels in Fit & scale, first-line-only Surface/Repair rows, rotated rail captions, chrome-less flush panels, **auto-hiding viewport toolbar** (72 dp reveal zone, menu/focus aware, `O P E F` flash) | foundations/density.md |
| 3 | **2D grid right-click / right-drag = erase** regardless of tool and modifiers; context menu suppressed | C08, overview/04 |
| 4 | **Solver keyboard model** shared by list, solution slider and move seekbar (`Space`/`Enter` play, `←`/`→` moves, `Shift` start/end, `↑`/`↓` solutions, `Home`/`End`, `PgUp`/`PgDn`, `Del`, `Esc`) with **sticky focus** — viewport, view cube, toolbar and transport presses never steal keyboard focus; focus restored after re-render; piece chips keyboard-operable | C20, overview/04 §8, contract §11 |
| 5 | **New global keys:** `F1` help, `Ctrl+[`/`Ctrl+]` collapse cards, `Ctrl+Space` Focus 3D, `O`/`P` Orbit/Pan, Shapes list `↑ ↓ Alt+↑ Alt+↓ Enter`, Puzzle `+ − Del`; Entities-only keys scoped to Entities (they previously leaked into Puzzle/Solver); Esc ladder adds "pause playback" | overview/04, C01, C16, contract §12 |
| 6 | **C22 Keyboard shortcuts** help window (Help ▸ Keyboard shortcuts, `F1`; in the mock a Settings page), grouped by workspace, searchable, key-cap and mouse-token styling | C22, C12, C00 |
| 7 | Tests T-DEN-1…4, T-C08-RC, T-C01-K, T-KB-1/2, T-C20-7/8, T-C22-1; journeys J19–J20; phase P12; OQ-36; all reference screenshots regenerated at Standard density plus Minimal and help screenshots 67–75 | migration/*, reference/screenshots |

## Rev 5.3 — framework-neutral spec, Qt Quick profile, spike plan
| # | Change | Where |
|---|---|---|
| 1 | The spec is now **framework-neutral**: target stack = **Qt 6 Quick (recommended) or RmlUi**, decided by a two-week spike; README, agent instructions, contract intro, phase plan updated | README, AGENT-INSTRUCTIONS, overview/06, migration/00 |
| 2 | New **Qt 6 Quick implementation guide**: QML/C++ split, contract → Qt mapping (`objectName`, `Q_PROPERTY`, `QAbstractListModel`, `Q_INVOKABLE`), Theme singleton with Light/Dark/System, layout rules, 3D in `QQuickRhiItem` (QRhi port or forced OpenGL, optional underlay), scene-graph 2D grid, native dialogs, cursors, performance rules, Meson `qml_module` + C++20 moc note, Windows build routes, Qt Test / Qt Quick Test offscreen + screenshot diffs | foundations/qtquick-implementation-guide.md |
| 3 | New **framework spike plan**: identical scope in both frameworks, 12 measurements with hard budgets, weighted decision matrix, "controller layer first" | migration/04-framework-spike-plan.md, OQ-35 |
| 4 | Corrected stale scope text (README/agent instructions still said Entities-only); manual test minimum width now 960 dp with both cards collapsed | README, AGENT-INSTRUCTIONS, test plan |

## Rev 5.2 — collapsible everywhere, theme setting, weight, scope clarifications
| # | Change | Where |
|---|---|---|
| 1 | **Left and right side cards are collapsible in all workspaces** (Puzzle and Solver get the same 56 dp rails with expand button + vertical title); collapse state is **per workspace**; **no auto-collapse**; supported minimum window width **960 dp** (two windows side by side) | C11, C15, contract §1, tests, J17 |
| 2 | **Theme = Light \| Dark \| System** in Settings ▸ General (System follows the OS when available, else Light); the mock-only theme toggle button is removed | C12, state model, glyph catalog, J18 |
| 3 | **Shape weight** documented: in the Solver's disassembly animation heavier shapes stay in place and lighter ones move; new tooltips | C01, C21 |
| 4 | **Scale/efficiency is not a requirement** (virtualisation wording removed) | C20, tests, phase plan |
| 5 | **OQ-34 resolved:** legacy menu dialogs stay as they are; Config = Settings; **bulk range** added as *Set range for all pieces…* in the Puzzle ⋯ menu | C16, OQ |
| 6 | Bug fix in the mock: with both side cards collapsed the workspace band shrank vertically and floated mid-window (grid items were centred); now always full height | reference/mock |

## Rev 5.1
* **Manage solutions — Before / After are relative to the current sorted order of the list** (product owner), recorded in C20 and OQ-25 (the other Delete/Analyse semantics remain to be verified).
* New **OQ-34**: legacy features reachable only through menus (bulk range for all pieces, assembly import, convert grid type, image/vector/STL export, Status, Edit comment, Config) are not yet mapped.

## Rev 5.0 — Solver tab; one shell width for all workspaces
| # | Change | Where |
|---|---|---|
| 1 | **Solver tab designed** (read-only 3D; no solving logic in the mock). Left card: Problem · **What to check** radio cards · collapsible **Search options** (sort by id / pieces / level / moves) · **Run** state machine with progress and status grid · **Step-by-step explorer** (Placements / Movements / Step). | C19 |
| 2 | Right card: **List ⁄ Slider** solutions (columns #, Level, Moves-with-bar, Pcs, DA; header sorting with reverse; keyboard; **smooth scrubbing**), **fixed-height Disassembly player** (move counter above a full-width slider, speeds 0.5×–8×), **Manage solutions** menu (hover preview of rows to delete, confirmation, Analyse), Export to STL, **fixed-height Piece display** whose chip appearance (solid / dotted border / greyed) encodes visibility | C20 |
| 3 | 3D: whole-assembly scene at the move position, **wireframe with all hull edges + ≈ 6 % tint**, Display ▸ **"Show through solid pieces"** (default on), assembler-state mode, **docked explorer drawer** instead of separate windows | C21 |
| 4 | **Side-card widths are now identical in all workspaces: left 380, right 400** (Entities was 288/400, Puzzle 380/360, Solver 360/340); centre 720 dp at the reference size everywhere | overview/02, C11, C15, C17 |
| 5 | Legacy Solver mapping L120–L137 with annotated screenshot, primitives PR-27…32, transport icons, contract §11, state, input map, tests, journeys J14–J16, phase P11, open questions OQ-25…OQ-33 (notably **OQ-25: Delete/Analyse semantics could not be read from `mainwindow.cpp#L4333` — verify**) | migration/*, overview/* |
| 6 | Bug fix in the mock: Entities hotkeys and Display ▸ Show items were disabled on first load (initial workspace state unset) | reference/mock |

## Rev 4.0 — Workspace rail and the Puzzle tab
| # | Change | Where |
|---|---|---|
| 1 | The **Entities / Puzzle / Solver switch moves to a 68 dp vertical rail** on the left (icon + caption, selected indicator, per-workspace state, `Ctrl+1/2/3`); the top bar loses the tabs | C15, C00, overview/02 |
| 2 | **Puzzle tab designed** (Entities panels removed, no editing): problems with positional ids + editable labels; **Result card**; **In this puzzle** (fixed-size tiles with always-visible steppers, or **Chips ⁄ Table**); **All pieces** library with drag / double-click / + add and visible drop zones; fit **summary strip** with a **derived hole count** (the maximum-holes setting is dropped) | C16 |
| 3 | Right card: **fixed count by default**, **range** only behind a switch with **inline-label Min / Max steppers**; **one group per piece (or none)** via None / Gn / + New; **colour-rules matrix** replacing the pair list and sort buttons | C17 |
| 4 | **Puzzle 3D scene**: result, selected piece, then **one object per copy** (optional range copies translucent); **layout fixed on screen, each object rotates in place, pan/zoom move the whole layout**; all voxel space types supported; no edit affordances | C18 |
| 5 | Legacy Puzzle-tab mapping (L100–L112) with annotated screenshot, new primitives PR-21…26, icons, contract §10, state, input map, tests, journeys J11–J13, phases P9–P10, open questions OQ-20…OQ-24 | migration/*, overview/* |

## Rev 3.11 — non-brick voxel types are interactive; spheres closely packed
* **Spheres are closely packed**: sites (x+y+z even), radius √2/2 of the step → neighbouring circles touch in the 2D layer view (diagonally) and neighbouring spheres touch in 3D (12 neighbours). → C14
* The mock's **Triangular Prism, Spheres, Rhombic Tetrahedra and Tetrahedra-Octahedra** are now **interactive in 2D and 3D**: per-type cell shapes, painting, layers, 3D rendering, hover ghosts, quick tools, Edit mode, slab and dim-other-layers. Geometry rules and 3D picking rules for non-cubic voxels are specified. Both tetrahedral types share a **placeholder geometry** until their assignment is confirmed (OQ-19). → C14

## Rev 3.10
The whole **view-cube setup shifted up/right into the corner with equal margins**: the visible content (Home icon top, roll-icon right edge) is now **16 dp from both the top and right edges** of the viewport (was ≈ 103 dp from the top and 26 dp from the right). Below 640 dp centre-card width it drops under the toolbar. → C13 (AC-C13-15)

## Rev 3.9
The four **90° rotate arrows (▲▼◀▶)** moved outward: centres now **20 dp beyond the cube edge** (were 10) so they no longer crowd the cube. → C13

## Rev 3.8 — upright double-click, roll icon tuning
* **Double-click a view-cube face centre** → snaps to that face *and* rolls the cube so the face label reads upright (e.g. `+X` turns 90° clockwise). → C13 (AC-C13-13)
* **Roll icons smaller (radius 9 dp) with larger, more prominent arrowheads (8.5 × 12 dp)**; **roll icons and Home moved slightly outward** from the cube. → C13 (AC-C13-14)

## Rev 3.7 — roll icon redesign, smooth zoom
* **View-cube roll icons** redrawn to match `reference/legacy/target-view-cube-roll-icons.png`: a counter-clockwise half-circle arch over the top (head down at its left end) and a clockwise “)” arc down the right side (head at the bottom pointing left), wrapped around the cube's upper-right corner; 90° arrows moved to 10 dp from the cube edge; Home slightly closer (36,52). → C13
* **Zoom is now smooth**: wheel input sets a target zoom and the camera eases toward it (≈ 90 ms time constant) instead of jumping per wheel notch. → C06 (AC-C06-13)

## Rev 3.6 — cube refinements, official voxel types, cursor plus
| # | Change | Where |
|---|---|---|
| 1 | **View cube in perspective** per Display ▸ Projection (orthographic otherwise); **visible per-face shading**; **corner hover highlights all 3 faces** (edge hover both faces); **roll icons are a mirror-symmetric opposing pair**; **Home moved closer** | C13 |
| 2 | **Bug fix** — after an arrow/roll click, a mouse-drag orbit no longer jumps back to an older orientation: running animations are cancelled on manual input, the roll is kept and orbit drags are interpreted in the rolled frame | C13 |
| 3 | Cursor **“+” now has the same colour as the square's outline** in both themes (Variable with a colour: dotted boundary and “+” in that colour) | glyph catalog ▸ Cursors |
| 4 | **Official voxel types** from File ▸ New "Select space grid": **Brick, Triangular Prism, Spheres, Rhombic Tetrahedra, Tetrahedra-Octahedra** (legacy label typo "Octahera" corrected). Clarifications: nudges follow the axes the type exposes (prism: 6 in-plane + 2 perpendicular); **Triangular Prism supports scaling** | C14, C03, C04 |

## Rev 3.5 — cursor badge, view cube, voxel space types
| # | Change | Where |
|---|---|---|
| 1 | Fixed/Variable cursor badge: **white inside for the Default colour in the light theme** (near-black in dark, mirrored), **the selected colour inside** otherwise; both carry a small **“+”** (contrast-adjusted) | glyph catalog ▸ Cursors, C06 |
| 2 | **View cube (C13)** documented from the legacy screenshots and implemented in the mock: hover regions (face/edge/corner), click-to-snap, **Home**, 90° arrows and roll arrows when face-aligned, **smooth 360 ms animation (incl. Home)**. The toolbar's **Views and Fit buttons are removed** (the cube + keys `Home`/`F` replace them) → toolbar is Orbit, Pan, Edit, Display, Focus | C13, C06, input map |
| 3 | **Voxel space types (C14)** from the legacy screenshots (Spheres, Triangular prisms, Tetrahedra besides Cubes): type fixed at **File ▸ New**, shown read-only in the Voxel-editor header; Transform (different rotate/nudge sets), Fit & scale (Spheres hide Scale) and the 2D grid cell shape are **descriptor-driven**. The mock has a **mock-only** type selector with adapted panels and static grid previews | C14, C03, C04, C08, C00 |
| 4 | New open questions OQ-18 (File ▸ New, type names, cube details) and OQ-19 (per-type behaviour not in the screenshots) | overview/05 |

## Rev 3.4 — cursor tweaks
The Fixed/Variable badge moved **closer to the pointer (≈ 3 px further left, 1 px higher: left edge at the pointer's width, top at y = 0)**. **Erase and Paint no longer show the arrow**: the eraser and the paint-bucket(+drop) icons *are* the cursor (hotspot at the glyph centre; monochrome outline with an opposite-tone body fill so they read on any voxel colour). The Edit-button icon follows the same split (pointer+badge for 1/2, bare eraser/bucket for 3/4).

## Rev 3.3 — simple edit cursors
Edit cursors simplified: **the regular OS default pointer + a tiny badge at its top-right** (hands removed). Fixed = square filled with the **drawing colour**; Variable = **dotted-boundary** square in the drawing colour; Erase = sleek **monochrome eraser**; Paint = **paint bucket** (monochrome) with a **drop in the drawing colour**. When the drawing colour is Default the square/dotted square/drop use the **theme contrast colour**. The cursor is composed at runtime (colour-dependent); badge templates, a fallback arrow and rendered examples are in `foundations/cursors/`. The **Edit button icon** uses the same pointer+badge language (with the 1–4 number badge) so button and cursor match — say if you would rather keep hands on the button.

## Rev 3.2 — cursor fill
Cursors now have a **filled hand**: white fill + dark outline in the light theme, near-black fill + light outline in the dark theme (the outline-only version did not read on every voxel colour). The faint glow was removed; `cursor-sheet.png` now shows each cursor over red, green, blue, yellow, white and near-black voxels.

## Rev 3.1 — cursor refinement
Tool cursors redesigned: **slim** hand outline (1.5 dp stroke) with only a faint opposite-tone glow instead of a hard white/black border, and **separate light-theme (dark ink) and dark-theme (light ink) sets**. Size is now **24 px at 100 % OS cursor scale** (36 / 48 for 150 / 200 %), chosen by the OS cursor scale rather than the UI dp ratio (the previous 32 dp asset scaled up by the UI ratio looked oversized). Hotspot (10,3) at 24 px. Files: `foundations/cursors/`; mock updated.

## Rev 3 — edit-mode refinements, RmlUi/C++ framing
| # | Change | Decision | Where |
|---|---|---|---|
| 1 | **Layer slab gets a volumetric highlight** (all six faces tinted + 12 edges) so boundaries are not lost | Accepted | C06 |
| 2 | **Slab and dim-other-layers only while the 2D editor is visible** (not in Focus-3D, not with the right sidebar collapsed; Display rows disabled with a hint) | Accepted; applies to both, and the saved preference is kept | C06, C11 |
| 3 | **Edit button** shows the active tool as a per-tool **hand glyph** replaced in place, with a **1–4 badge**; **cursor** changes per tool (4 cursors with hotspot) | Accepted | C06, glyph catalog, `foundations/cursors/` |
| 4 | **Quick tools (Shift=1 Fixed, Alt/⌘=2 Variable, Ctrl=3 Erase) alongside a latched Edit mode** | **Recommended — keep both**: one rule, not two features. Edit mode = latched tool; quick tool = the *same* tool held momentarily. Same numbering, cursor, ghost and Edit-button feedback (dashed momentary state), instant on modifier press. Paint has no modifier. Also applied to the 2D grid for consistency, and `E` toggles Edit mode | C06, overview/04, C07, C08 |
| 5 | **Hovering the 2D grid previews the edit in 3D** (same ghost, same tool in effect, incl. mirror/span) via one shared `hover_preview` | Accepted | C06, C08 |
| 6 | **Grid scale removed** | Accepted (legacy L54 dropped; OQ-2 narrowed) | C03, legacy map |
| 7 | **Spec reframed for RmlUi + C++**: no scripting content; new `foundations/rmlui-implementation-guide.md` and `overview/06-ui-contract.md` (ids, data-model variables, events, state classes, cursor table); primitives, phase plan, agent instructions and test plan rewritten accordingly | Done | whole bundle |

## Rev 2 — design review changes (all reflected in the mock, screenshots and every affected spec file)
| # | Change | Decision | Where |
|---|---|---|---|
| 1 | **Grid size moves to the Voxel editor** (under the 2D grid, above Drawing colour). Always visible, no accordion. "Apply to all shapes" switch sits in its header. Dimensions no longer repeated in the inspector head | Accepted. It is a property of the grid, the right card has the vertical room (~76 dp), and it keeps size editing next to the grid it changes. Fixed parts of the right card ≈ 414 dp of 916 | C02, overview/02 |
| 2 | **Active-layer indicator** becomes a **one-voxel-thick slab** covering the whole layer (translucent faces + 12 edges) instead of a single mid-layer plane. Optional **Dim other layers** (off by default) | Accepted. The plane sliced through voxels and read as a line from side views; the slab shows the real extent. Dim-other-layers was an addition (cheap, strong cue) | C06, C08 |
| 3 | **Grid operations merged with scale ops** as **Fit & scale** (FIT TO GRID: Prune, Center, To origin · SCALE: Minimize, ×2, ×3, Grid scale…). Inspector is now Transform → Fit & scale → Repair & surface | Accepted. None of these is a size edit. **Grid Scale**: purpose unknown from screenshots (see OQ-2); placeholder lives in Fit & scale | C03, C05 |
| 4 | **Zoom mode removed** from the toolbar | Accepted: the wheel zooms everywhere | C06 |
| 5 | **Orbit + pan together**: toolbar keeps a 2-button **Orbit / Pan** pair that sets what plain left-drag does; **Shift+drag does the other**; **middle-drag always pans**; wheel zooms | Recommended design; three-button-mouse users never touch the toolbar, trackpad users have a mode switch or Shift | C06, overview/04 |
| 6 | **Projection and Piece/Voxel colour move into the Display menu** (sections Show · Projection · Voxel colour); status bar is text-only | Accepted: they are viewport display options, used rarely | C06, C10 |
| 7 | **Shape ids are positional**: S1…SN = list order; chip colour also by position; reorder/delete renumber; New/Duplicate append | Accepted, with consequences documented (flash message, cross-tab references OQ-16) | C01, state model |
| 8 | Per-axis "opt" checkboxes **dropped** | Accepted (OQ-3 closed) | C02 |
| 9 | **Undo history depth** is a dropdown: 25 · 50 · 100 · 200 · 500 (default 25) | Accepted; new PR-19 Dropdown; legacy values map to nearest option | C12, primitives |

## Rev 1 — initial bundle
Flip is one button per axis; weight W−/W+ stays on the shape row; spec bundle created.


---
<!-- FILE: overview/01-goals-scope-principles.md -->

# 01 — Goals, scope, principles

## Problem (legacy UI)
The Entities tab is powerful but hard to learn and slow to use:
* Editing is fragmented across tabs (Size / Transform / Tools) and panels (Shapes, Colours); the 2D grid — the most used surface — sits buried below them.
* ~60 small, ambiguous icon buttons; rare commands (centre, origin, scale) occupy prime space.
* Shape actions (New/Delete/Copy/Label/W±/←→) are detached from the shapes they act on.
* The 3D viewport has no toolbar or visible affordances; the status bar holds unlabeled glyphs.
* Duplicate/ambiguous state (layer slider vs plane vs tool icons; tabs that hide state).

## Goals
1. Make **precise voxel editing** the primary workflow; drawing needs no tab switching.
2. Give the **3D viewport** a central, generous canvas *and* make it directly editable (modifier+click).
3. One consistent visual language (tokens, glyph vocabulary, control sizes) with light + dark themes.
4. **Preserve all functionality**; relocate rare operations into grouped, collapsible inspector sections.
5. Every icon-only legacy control gets a **caption** (user recollection matters more than compactness).
6. Work naturally at **2560 × 1600 (16:10)**.

## Non-goals
* No changes to file format, solver, Puzzle/Solver tab content, or voxel data model.
* No new editing features beyond: 3D modifier editing with ghost preview, settings search, focus modes, theme.
* Legacy select-region / select-cells tools (L61) are **intentionally dropped** ("not useful" — product owner).

## Design principles
| # | Principle | Consequence |
|---|---|---|
| 1 | **Scope by zone** | Left = *which shape & what happens to the whole shape*; Centre = *understand the piece (3D)*; Right = *draw voxels*. |
| 2 | **One state, one owner, one control** | Tool, colour, mirror, span, plane, layer each exist once and drive both 2D and 3D. |
| 3 | **Draw without switching** | Tool strip, mirror/span, plane, layer strip, grid and colour live in one card. |
| 4 | **Recognise, don't recall** | Glyph + caption on every icon control; tooltips state the exact effect; shortcut badges on tools. |
| 5 | **Inherit the legacy icon vocabulary** | Axis frame (blue Z, red X, green Y), pink/green/cyan axis bars, gold cubes, red/lime pencils — so existing users recognise controls. |
| 6 | **Rare ≠ hidden, just grouped; near what they affect** | Inspector accordion: Transform, Fit & scale, Repair & surface. Grid size sits under the 2D grid. All display options live in the viewport's Display menu. Open/closed state persists. |
| 7 | **Non-destructive by default** | Blocked operations explain why in the status bar; nothing is silently deleted (see OQ-5). |
| 8 | **Identity is position** | Shape ids `S1…SN` are list order; reordering renumbers. |
| 9 | **Progressive collapse** | Sidebars collapse to 48 dp rails (40 in Minimal, `Ctrl+[` / `Ctrl+]`); focus modes maximise 2D or 3D; `Esc` always steps back. |


---
<!-- FILE: overview/02-layout-and-information-architecture.md -->

> **Density (Rev 6.0):** dimensions here are **Standard** density values; Minimal values and any later corrections are in `foundations/density.md`, which wins on conflicts.

# 02 — Layout and information architecture

Reference images: `reference/screenshots/01-main-light.png`, `02-main-dark.png`, `08-zones-overlay.png`, `00-new-annotated.png`.

## 1. Canonical window (logical 1600 × 1000 dp; ×1.6 on 2560×1600)

```
┌──────────────────────────────────────────────────────────────────────────────────────────────┐ 40
│ ◧ BurrTools  File Toggle3D Export Status Editcomment Help About    [Entities|Puzzle|Solver]  file.xmpuzzle ⚙ │  TOP BAR
├───────────────┬──────────────────────────────────────────────────────┬───────────────────────┤
│ SHAPES  (320) │                3D VIEWPORT  (flex)                   │   VOXEL EDITOR (340)  │
│ header 44     │  ┌────── floating toolbar (top-centre) ──────┐       │ header 44             │
│ ─────────     │  │Orbit Pan│Edit│Views Fit│Display│Focus     │       │ ┌Fixed┐┌Var┐┌Erase┐┌Paint┐│ tools 58
│ ▮S1 row 42    │  └──────────────────────────────────────────┘ ⌂ [cube]│ MIRROR  X Y Z  SPAN X Y Z│ 54
│ ▮S2 row 42    │                                                      │ [XY][XZ][YZ]  Layer 1/4  │ 56
│ ▮S3           │         (canvas: voxels, axes, bounds,               │ ┌────┬───────────────┐   │
│ ▮S4           │          active-layer SLAB)                          │ │ Z  │               │   │
│ ─────────     │                                                      │ │lay-│   2D GRID     │   │
│ EDIT SHAPE 44 │                                                      │ │ers │               │   │
│ ▾ Transform   │                                                      │ └────┴───────────────┘   │
│ ▸ Fit & scale │  hint (bottom-left)     │ GRID SIZE  X Y Z  [all]  │ 76
│ ▸ Repair&surf │                                                      │ ■ Drawing colour ●●● + ⋯ │ 66
├───────────────┴──────────────────────────────────────────────────────┴───────────────────────┤ 28
│ Shape S2 has 14 voxels (13 fixed, 1 variable)                                       cursor    │ STATUS
└──────────────────────────────────────────────────────────────────────────────────────────────┘
```

| Region | Size (dp) | Notes |
|---|---|---|
| Top bar | height 40 | panel background, 1 dp bottom border |
| Workspace padding / gap | 8 / 8 | three **cards** |
| Left card (Shapes + inspector) | width **320** (rail **48**) — **the same in every workspace** (Minimal: 264 / rail 40) | |
| Centre card (3D) | flexible (`1fr`) | never below 360 (focus-2D) |
| Right card (Voxel editor) | width **400** (rail **56**) | |
| Status bar | height 28 | **text only** (status + cursor readout) |
| Card | radius 8, 1 dp border, elevation shadow (Minimal: flush, radius 0, no shadow) | overflow clipped |
| Card header | height 44, padding 0 8 0 14, semibold, 1 dp bottom border | |

Right-card vertical budget (fixed parts): header 44 + tool strip 74 + mirror/span 82 + plane row 72 + **grid size 76** + colour footer 66 ≈ 414 dp; the 2D grid takes the rest (≈ 500 dp at 1000 dp window height, ≈ 300 dp at the 800 dp minimum).
Minimum supported logical window: **1280 × 800 dp** (enforce as window minimum).

## 2. Zones and what lives where (the information architecture)

| Zone | Scope of its commands | Contents (component) |
|---|---|---|
| **Left — Object scope** | which piece; operations on the *whole* piece | Shapes list (C01) · Edit-shape inspector: Transform (C04), Fit & scale (C03), Repair & surface (C05) |
| **Centre — Spatial view** | understanding and (modifier-)editing in 3D; all *display* options; view snapping | Viewport + floating toolbar (Orbit, Pan, Edit, Display, Focus) + Display menu + hint (C06) · **view cube with Home / snaps / 90° steps (C13)** |
| **Right — Voxel scope** | painting voxels and the grid they live in | Tool strip & Mirror/Span (C07) · Plane / Layer strip / Grid (C08) · **Grid size (C02)** · Drawing colour (C09) |
| **Bottom — Status** | feedback only | Status text, cursor readout (C10) |
| **Global** | | Top bar/menu/tabs/gear (C00) · Layout modes (C11) · Settings dialog (C12) |

Inspector order (top→bottom, fixed): **Transform** (open by default) → **Fit & scale** (closed) → **Repair & surface** (closed).
Rationale: placement of the piece first; then fitting it to its grid and changing resolution; repair / clean-up last. **Grid size** is not in the inspector: it is a property of the 2D grid and lives directly under it.

## 3. Layout modes (details in C11)
| Mode | Left | Centre | Right |
|---|---|---|---|
| Default | 320 | flex | 340 |
| Left collapsed | **56 rail** | flex | 400 |
| Right collapsed | 320 | flex | **48 rail** |
| Both collapsed | 56 | flex | 56 |
| **Focus 2D** | 56 rail | **360 fixed** (compact toolbar, no captions) | flex (grid gets all remaining width) |
| **Focus 3D** | 56 rail | flex | 56 rail |
Transitions: column widths animate 200 ms ease.

## 4. Z-order (top to bottom)
Settings dialog scrim+dialog (30) > popup menus / context menus (20) > zone overlay (mock-only) (9) > floating viewport toolbar / hint (3) > viewport canvas.

## 5. Visual themes
Light and dark share identical geometry; only token values change (see `foundations/design-tokens.md`). Theme source: Settings ▸ General ▸ Appearance (System/Light/Dark) — **spec-only addition** (the mock has a mock-only toggle button).

## Workspace rail and the Puzzle layout (added Rev 4.0)
* A **60 dp vertical workspace rail** (C15; Minimal 44) sits on the far left under the top bar and hosts **Entities · Puzzle · Solver**; the top bar no longer holds the workspace tabs. The 1600 × 1000 dp reference includes the rail.
* **Entities** keeps the three-card layout (Shapes 320 · 3D · Voxel editor 340).
* **Puzzle** uses the same 3-column shell with **different side cards and a read-only 3D card**: **left 320 dp = Problems · Result · In this puzzle · All pieces · Summary (C16)**; **right 340 dp = Selected piece · Exclusive groups · Colour rules (C17)**; **centre = the shared viewport card showing the Puzzle scene (C18)**. The Entities panels do not appear in Puzzle and no editing is possible there. Collapse modes of the side cards are not defined for Puzzle in this revision; **Focus 3D** hides both cards.
* Priority: the 3D scene stays the dominant surface; the left card is the primary working surface (add pieces, set counts), the right card holds the rarely used options (ranges, groups, colour rules).

## One shell, three workspaces (Rev 5.0)
**Side-card widths are identical in every workspace: left 320 dp, right 340 dp** in Standard density (264 / 320 in Minimal; collapsed rails 48 / 40); the workspace rail is 60 dp (44 in Minimal); the centre viewport therefore has the same width everywhere — **864 dp** at the 1600 dp reference (970 in Minimal). Only the *content* of the side cards changes:
| Workspace | Left card (320) | Centre | Right card (340) |
|---|---|---|---|
| Entities | Shapes + inspector (C01–C05) | 3D editing view (C06) | Voxel editor (C02, C07–C09) |
| Puzzle | Problems · Result · In this puzzle · All pieces · Summary (C16) | Puzzle scene (C18) | Selected piece · Groups · Colour rules (C17) |
| **Solver** | **Problem · What to check · Search options · Run · Step-by-step explorer (C19)** | **Solver scene (C21) + docked explorer drawer** | **Solutions (List/Slider) · Disassembly player · Manage · Piece display (C20)** |
The Solver is a *browse and verify* workspace: left = set up and run, right = inspect and manage results, centre = read-only 3D. Focus 3D hides both side cards in every workspace.


---
<!-- FILE: overview/03-state-model-and-persistence.md -->

# 03 — State model and persistence

Rule: **each state variable has exactly one owner**; all views derive from it. Changing it raises the listed event; every listed subscriber MUST refresh.

## 1. UI state (new)
| State | Type / default | Owner | Persist | Changed by | Subscribers to refresh |
|---|---|---|---|---|---|
| `selectedShape` | index, default 0 (clamped) | Entities model | session | list click/keys, new/copy/delete | shapes list, inspector head & all sections, status text, editor grid/layers, 3D, colour swatches |
| `tool` | `fixed`\|`variable`\|`erase`\|`paint`, default `fixed` | Voxel editor | session | tool strip click, keys `1`–`4` | tool strip (selected), 3D Edit mode (plain click), ghost preview |
| `mirror` | `{x,y,z}` bool, all false | Voxel editor | session | C07 Mirror toggles | toggle visuals, 2D/3D placement expansion, 3D ghost preview |
| `span` | `{x,y,z}` bool, all false | Voxel editor | session | C07 Span toggles | same as mirror |
| `plane` | `XY`\|`XZ`\|`YZ`, default `XY` | Voxel editor | session | plane buttons | layer strip, grid, 3D active layer slab |
| `layer` | int 0-based, default 0, clamped to `[0, layers-1]` | Voxel editor | session | layer chip, `PgUp/PgDn`, plane/size change (clamp) | layer strip, grid, label, 3D plane |
| `drawColour` | colour index, default 0 (**Default** = piece colour) | Colour model | session | swatch click/right-click, add/remove | colour footer, rail swatch, tool preview |
| `edit3d` (`edit_mode`) | bool, false | Viewport | session | toolbar **Edit** button, key `E` | Edit button (`on`), cursor, ghost on plain hover |
| `quick_tool` (derived) | none\|fixed\|variable\|erase from held modifiers (Ctrl→erase, Alt/⌘→variable, Shift→fixed) | Input bridge | — | modifier keys | cursor, hover ghost (3D **and** 2D), Edit-button icon/badge (momentary) |
| `hover_preview` (derived) | source + action + target cells + validity | Viewport | — | 3D pointer **or** 2D grid hover | 3D ghost overlay |
| `editor_visible` (derived) | focus==2d ∨ (focus==none ∧ ¬rightCollapsed) | Layout | — | layout changes | slab / dim-other-layers drawing |
| `navMode` | `orbit`\|`pan`, `orbit` | Viewport | session | toolbar Orbit/Pan | toolbar, plain left-drag behaviour, hint line 2 (Shift swaps; middle-drag always pans) |
| `camera` | yaw/pitch/zoom/pan | Viewport | session (per shape optional) | drag, wheel, Views, Fit | 3D |
| `voxel_type` | id (brick\|prism\|spheres\|rhombic\|tetoct) | Model (file property) | with file | **File ▸ New only** — read-only everywhere else | type chip, Transform/Fit&scale sections, grid cell shape, renderer |
| `camera` (+ `camera_animation`, `cube_hover`) | azimuth/elevation/roll/zoom/pan; 360 ms eased animation | Viewport | session | orbit/pan/zoom, view cube, `Home`/`F` | 3D, view cube |
| `display.axes`/`bounds`/`layerSlab` | bools, **true** | Viewport | **persist** | Display menu | 3D |
| `display.dimOtherLayers` | bool, **false** | Viewport | **persist** | Display menu | 3D |
| `colourView` | `piece`\|`voxel`, default `piece` | View prefs | **persist** | **Display menu ▸ Voxel colour** | 3D |
| `projection` | `perspective`\|`orthographic`, default `perspective` | View prefs | **persist** | **Display menu ▸ Projection** | 3D |
| `applyToAllShapes` | bool, false | Voxel editor (Grid size) | session | Grid-size switch | Grid-size edits |
| `inspector.open.{transform,fit,repair}` | bool; transform=T, fit=F, repair=F | Inspector | **persist** | accordion headers | accordion |
| `layout.leftCollapsed` / `rightCollapsed` | bool, false | Layout | **persist** | collapse/expand buttons | layout |
| `layout.focus` | `none`\|`2d`\|`3d`, none | Layout | session (never restored) | Focus buttons, Esc | layout, toolbar compact mode |
| `settings.*` | see C12 | Config | **persist** (config file) | Settings dialog | 3D, tooltips, undo depth, threads… |
| `theme` | `light`, `dark` or `system` (follows the OS when available, else light); default system | Config | **persist** | Settings ▸ General | all widgets |
| `status.flash` | string + timer 2400 ms | Status bar | transient | blocked/complete messages | status text |

Persist keys SHOULD go through the app's existing config mechanism (same store as legacy Config). Names: `ui.entities.<state>`.

## 2. Model data touched by the UI (existing legacy model — do not redesign)
**Shape id (`S#`) and chip colour are derived from list position** (`S{index+1}`; colour by index) — never stored or edited; reordering/deleting renumbers (C01). Shape: label, weight (W, int ≥ 0), grid dims (x,y,z), voxels (position → state `fixed|variable`, colour index). Colours: list with index 0 = **Default** (legacy label "Default", chip `C1`), then user colours.
Shape *order* is user-visible and persisted with the puzzle (legacy ←/→ reordered it).

## 3. Events (logical)
`ShapeSelected(i)`, `ShapeListChanged`, `VoxelsChanged(shape)`, `GridResized(shape)`, `ColourListChanged`, `ToolChanged`, `ModifiersChanged` (mirror/span), `PlaneChanged`, `LayerChanged`, `LayoutChanged`, `SettingChanged(key)`.
`VoxelsChanged` MUST trigger: 2D grid redraw, layer-strip dots, 3D redraw, status text. `GridResized` additionally clamps `layer`, rebuilds rulers, refreshes Size summary.

## 4. Undo granularity
| Interaction | Undo steps |
|---|---|
| Click-drag paint in 2D grid (mouse-down → up) | 1 |
| 3D modifier click | 1 |
| Mirror/Span expanded placement | 1 (with its originating click) |
| Transform / scale / prune / fill / constrain / colour ops | 1 each |
| Grid size ±1 | 1 per click (typed value: 1 per commit) |
| Shape new / duplicate / delete / reorder (incl. the implied renumbering) / rename / weight ± | 1 each |
| Purely view state (camera, layout, tool, edit mode, plane, layer, sections, hover) | none |

## Puzzle tab state (Rev 4.0)
| State | Type | Owner | Persistence | Written by | Read by |
|---|---|---|---|---|---|
| `workspace` | entities \| puzzle \| solver | Layout | session | rail | shell, viewport |
| `problems[]` | list of Problem {label, result_shape, pieces{shape → {min, max, range?, group?}}, groups[], colour_rules[piece_colour][result_colour]} | Model (file) | **with file** | Puzzle controllers | Puzzle UI, Solver |
| `problem_index` | int | Puzzle | session (per file) | problems list | everything in the tab |
| `puzzle_selected_piece` | shape ref | Puzzle | session | tiles, chips, table, 3D click | selected-piece card, 3D "Selected" slot, frames |
| `pieces_view` | chips \| table | Puzzle | session | segmented control | In this puzzle |
| `all_pieces_open` | bool | Puzzle | session | header | library |
| `drag` | none \| {shape, from: library\|used} | Puzzle | transient | drag events | drop-zone highlight |
Derived: `piece_in_puzzle = max > 0`; `piece_is_range = range flag (min ≠ max possible)`; `copies[]` = every copy of every used piece (max copies, those ≥ min are *optional*); `result_range` = fixed … fixed+variable voxels; `pieces_voxels` = Σ min·v … Σ max·v; `fits`; `holes` (derived, see C16). Camera: unchanged (C06/C13) — the same camera object drives both workspaces.

## Solver tab state (Rev 5.0)
| State | Persistence | Notes |
|---|---|---|
| `solver_options` (per problem) | with file | the search options of C19 |
| `solutions[]` (per problem) | with file | assemblies found (+ stored disassemblies / levels) |
| `solver_state`, progress, counters, `explorer`, `assembler_step` | session | transient |
| `solutions_view`, `solutions_sort`, `selected_solution`, `move_position`, `play_speed`, `piece_visibility`, `display_xray` | session | view state; `display_xray` default **on** |
Undo/redo does not cover solution deletion or analysis (they mark the document modified only).

## Density and help (Rev 6.0)
| State | Persistence | Notes |
|---|---|---|
| `ui_density` | Config | `standard` (default) or `minimal` |
| auto-hiding toolbar visibility | session | Minimal only, transient |
| Keyboard-shortcuts window open/search | session | |


---
<!-- FILE: overview/04-input-map-shortcuts-and-focus.md -->

# 04 — Input map, shortcuts, focus

## 1. Keyboard (when focus is not in a text field) — authoritative; C22 is the user-facing copy
**Everywhere**
| Key | Action | Component |
|---|---|---|
| `Ctrl+1` / `Ctrl+2` / `Ctrl+3` | Switch workspace: Entities / Puzzle / Solver | C15 |
| `F1` | Keyboard shortcuts window (Help ▸ Keyboard shortcuts) | C22 |
| `Ctrl+,` | Open Settings | C12 |
| `Ctrl+[` / `Ctrl+]` | Collapse ⁄ expand the left / right card (leaves a focus mode first) | C11 |
| `Ctrl+Space` | Focus 3D view (toggle) | C11 |
| `Esc` | Step back — ladder below | global |
| `Tab` / `Shift+Tab` | Move keyboard focus (order §5) | all |
| `Enter` / `Space` on a focused button, switch or chip | Activate | all |
**3D view (every workspace)**
| `O` / `P` | Orbit mode / Pan mode (toolbar) | C06 |
| `Home` | Home view (default orientation, framed, animated) | C13 |
| `F` | Fit to view (zoom/pan only, orientation kept, animated) | C06/C13 |
**Entities**
| `1` `2` `3` `4` | Tool: Fixed / Variable / Erase / Paint | C07 |
| `E` | Toggle 3D **Edit mode** | C06 |
| `PgUp` / `PgDn` | Next / previous layer (clamped) | C08 |
| `F2` (or `Enter` in the focused Shapes list) | Rename selected shape (inline) | C01 |
| `Del` | Delete selected shape (disabled if only one shape) | C01 |
| `Ctrl+D` | Duplicate selected shape | C01 |
| `↑` / `↓` in the Shapes list | Select previous / next shape | C01 |
| `Alt+↑` / `Alt+↓` in the Shapes list | Move shape earlier / later (renumbers) | C01 |
| `Enter` / `Esc` in inline rename | Commit / cancel | C01 |
**Puzzle** (with a piece selected)
| `+` (or `=`) / `−` | One more / one fewer copy | C16 |
| `Del` | Remove the piece from the puzzle | C16 |
**Solver** — see §8 (keyboard model with sticky focus).
**Scoping rule:** Entities-only keys (`1–4`, `E`, `F2`, `Del` for shapes, `Ctrl+D`, `PgUp`/`PgDn` for layers) **do nothing in Puzzle and Solver** unless those workspaces define their own meaning (Puzzle `Del`, Solver list `Del`/`PgUp`/`PgDn`).
Legacy menu accelerators (File, Edit/Undo/Redo, …) MUST continue to work unchanged and are shown in the menus `[VERIFY-IN-REPO OQ-9]`.

### Esc ladder (first match wins, one step per press)
1. Close Settings dialog  2. Close open popup/context menu  3. Cancel inline rename  4. **Pause Solver playback**  5. Exit focus mode (2D/3D)  6. (nothing). In Minimal density `Esc` also releases keyboard focus from the auto-hiding toolbar so it can hide.

## 2. Mouse — 3D viewport
| Input | Effect |
|---|---|
| Left-drag (no modifier) | Per toolbar mode: **Orbit** (default) or **Pan**. Rotation style per Settings ▸ Rotation method |
| `Shift` + left-drag | The *other* navigation action (Orbit mode → Pan; Pan mode → Orbit) |
| **Middle-drag** | **Pan** (always) |
| Right-drag | Reserved (no action) |
| Wheel / trackpad scroll | **Zoom** in every mode (direction per Settings ▸ Reverse scroll zoom). There is no Zoom toolbar mode |
| Left **click** (press-release ≤ 4 dp) | Performs the *viewport action* below |

### Edit mode and quick tools — one rule
There are **four edit tools**, numbered like the tool strip: **1 Fixed · 2 Variable · 3 Erase · 4 Paint**. The *viewport action* is resolved on every pointer move **and** every modifier press/release:
1. If a modifier is held it selects a **quick tool** (momentary override): `Shift` = **1 Fixed**, `Alt` (macOS `⌘`) = **2 Variable**, `Ctrl` = **3 Erase** (precedence Ctrl > Alt/⌘ > Shift when several are held).
2. Otherwise, if **Edit mode** is on (toolbar **Edit** button or key `E`), the action is the **active tool** (set by the tool strip or keys `1`–`4`).
3. Otherwise there is **no action**: clicks do nothing and no preview is shown.
Paint (4) has no modifier; it is available through Edit mode only.
Feedback is identical for edit mode and quick tools (learn once): the **cursor** changes to the tool cursor (pointer + coloured / dotted square badge for Fixed / Variable; eraser or paint-bucket icon alone for Erase / Paint — `edit-fixed|variable|erase|paint`), the **ghost preview** appears under the pointer, and the toolbar **Edit** button shows the tool glyph and number badge of the tool in effect — as a *dashed momentary highlight* while a quick tool is held, as the normal *on* state when Edit mode is on. The cursor/ghost/button update the instant a modifier is pressed or released, even if the pointer does not move.
Click commits one undo step; press-drag > 4 dp navigates and never edits; middle/right buttons never edit.

### View cube (top-right of the viewport, C13)
| Input | Effect |
|---|---|
| Hover face centre / edge / corner | Highlight that region |
| Click face / edge / corner | Animate to that face / edge (45°) / corner view |
| Click Home | Animate to the default view and frame the shape |
| Click ▲▼◀▶ (when face-aligned) | Animate 90° to the neighbouring face |
| Click ↶ ↷ (when face-aligned) | Animate ±90° roll |

## 3. Mouse — 2D grid
| Input | Effect |
|---|---|
| Hover a cell | Cell outline (accent, 2 dp inset); tool **cursor** (`edit-<tool>`); status cursor readout; **the same ghost preview appears in the 3D view** at that cell of the active layer, styled for the tool in effect (incl. Mirror/Span copies) |
| Press on cell | Apply the tool in effect (captured at press) to that cell |
| Drag across cells | Apply to each newly entered cell (single undo step) |
| `Shift` / `Alt`·`⌘` / `Ctrl` held | **Quick tool** override (Fixed / Variable / Erase), same as in 3D; cursor, outline and 3D ghost update immediately |
| Press layer chip | Go to layer |
| **Right button** (press / drag) | **Erase**, whatever tool is active and whatever modifier is held; context menu suppressed on the grid |

## 4. Mouse — lists and menus
| Input | Effect |
|---|---|
| Click row | Select shape |
| Double-click row | Inline rename |
| Drag row | Reorder — **renumbers** shape ids |
| Drag grip / row | Reorder (drop indicator line above target) |
| Right-click row | Shape context menu (C01) |
| Right-click colour swatch | Colour menu for that colour (C09) |
| Hover row | Reveal row actions |

## 5. Tab / focus order
Top bar (menu → tabs → gear) → Shapes header (New, collapse) → Shapes list (roving: one tab stop; arrows move) → Inspector section headers and their controls (top→bottom, left→right) → 3D toolbar (roving) → 3D canvas → Voxel-editor header buttons → tool strip (roving) → Mirror → Span → Plane → layer strip (roving) → grid → colour footer → status toggles.
Focus ring: 2 dp `accent` outline, 1 dp offset, on every focusable control. Canvas focus shows ring on the card edge only.

## 6. Modifier matrix summary
| | none | Shift | Alt / ⌘ | Ctrl |
|---|---|---|---|---|
| 3D click | edit off: nothing · edit on: active tool | **1** Fixed | **2** Variable | **3** Erase |
| 2D grid press | active tool (right button: **erase**) | **1** Fixed | **2** Variable | **3** Erase |
| 3D left-drag | mode action (orbit/pan) | the other action | mode action | mode action |
| 3D middle-drag / wheel | pan / zoom | pan / zoom | pan / zoom | pan / zoom |
Note (platform): on macOS `Ctrl+click` is conventionally a secondary click; if a macOS build is ever made, map Erase to a different modifier or require confirmation of the mapping.

## Puzzle tab input (Rev 4.0)
| Input | Effect |
|---|---|
| `Ctrl+1 / 2 / 3` | Switch workspace (Entities / Puzzle / Solver) |
| Click problem row / double-click / `F2` | Select / rename the problem label |
| Drag a problem's grip | Reorder problems (ids renumber) |
| Click a piece (tile, chip, table row, 3D copy) | Select it everywhere |
| `+` button / double-click a chip / drag a chip into *In this puzzle* | Add one copy |
| Tile `−` `+` | Count −1 / +1 (at 0 the piece leaves the puzzle) |
| Drag a tile onto *All pieces* | Remove the piece |
| Right-click a tile or chip | One more · One fewer · Remove from puzzle · Set as result |
| 3D: click a piece | Select it; **orbit/snap rotate every object in place**; pan/zoom move/scale the whole layout |
| `1–4`, `E`, Shift/Alt/Ctrl voxel modifiers | **Ignored** in the Puzzle workspace |

## Solver tab input (Rev 5.0)
| Input | Effect |
|---|---|
| Click a list row / `↑ ↓ Home End PageUp PageDown` (list focused) | Select a solution |
| Double-click a row | Select and play its disassembly (if stored) |
| Click a list header / Sort-by segment (again) | Sort / reverse |
| Drag the Solution slider | Scrub through all solutions smoothly |
| Player buttons / drag the Move slider | Start · back · play⁄pause · forward · end / seek |
| Click a piece chip | Cycle solid → wireframe → hidden |
| Hover a *Remove* item in Manage solutions | Preview the rows that would be deleted |
| 3D view | Orbit / pan / zoom / view cube / Home / `F` as everywhere; **no editing keys** (`1–4`, `E`, Shift/Alt/Ctrl voxel modifiers are ignored) |

## 8. Solver keyboard model and sticky focus (Rev 6.0)
Applies when keyboard focus is on the **solutions list**, the **solution slider** or the **move seekbar** (the slider/seekbar native arrow behaviour is overridden):
| Key | Action |
|---|---|
| `Space` / `Enter` | Play / pause the disassembly of the selected solution |
| `←` / `→` | One move back / forward (stops playback) |
| `Shift+←` / `Shift+→` | Assembled / fully disassembled |
| `↑` / `↓` | Previous / next solution |
| `Home` / `End` | First / last solution (move seekbar: start / end of the moves) |
| `PgUp` / `PgDn` | 8 solutions back / forward |
| `Del` / `Backspace` (list) | Remove this solution (confirmation) |
| `Esc` | Pause playback |
Piece chips: `Space`/`Enter` cycle solid → wireframe → hidden; `←`/`→` move between chips.
**Sticky focus:** mouse interaction with the 3D viewport (orbit, pan, zoom, view cube, toolbar) and with the player's transport buttons **does not move keyboard focus** away from the Solutions card; any re-render restores focus to the same control.

## 9. Focus order additions
Solver: problem selector → radio cards → Search-options header/controls → Run buttons → explorer buttons → List/Slider toggle → solutions list (one tab stop) or slider → player transport → speed → move seekbar → Manage / Export → piece chips (one tab stop each) → Show all. Puzzle: problems list → Result Change… → Chips/Table → tiles → All pieces chips → right-card controls.


---
<!-- FILE: overview/05-open-questions-verify-in-repo.md -->

# 05 — Open questions: verify in the repository

The designer worked from screenshots and product-owner answers. The items below are **unknown legacy behaviours or platform facts**. Resolve each by reading the BurrTools source (search by legacy label text / tooltip text / fluid widget names), then log the answer in `migration/RESOLUTIONS.md`. Until resolved, implement the **default stated here** and keep the code isolated so it can change.

| ID | Question | Where it bites | Default until resolved |
|---|---|---|---|
| **OQ-1** | Exact semantics of legacy **Flip** (L40): mirror about the *grid* centre plane or the *piece* centre? (Product owner: "mirror the piece around the planes"; one button per axis.) | C04 | Mirror across the plane through the **grid** centre: `c[i] → dim[i]-1-c[i]` |
| **OQ-2** | Confirm the **icon-to-function order** of the Size-tab icons (L34–L39: prune/centre/origin, minimize/×2/×3) and whether the left/right icon of each Constrain pair (L50–L52) is Inner/Outer. *(Legacy **Grid Scale** (L54) is dropped by product decision — nothing to resolve; if it exposed a capability with no other path, report it.)* | C03, C05 | As in the specs |
| ~~OQ-3~~ | ~~Per-axis checkboxes (L33)~~ — **Closed: dropped by product decision.** If removing them removes a capability that has no other path in the app, report it. | C02 | — |
| **OQ-4** | How does legacy select the **2D editing plane** (XY/XZ/YZ)? (Not visible in screenshots: only a Z layer slider, green Y and red X edge lines.) Also legacy tooltips text and exact **shape-list context menu** entries | C07/C08/C01 | New XY/XZ/YZ segmented buttons are the *only* plane selector; context menu per C01 plus any extra legacy entries |
| **OQ-5** | **Shrinking a grid dimension** below occupied voxels: clip, refuse, or confirm? | C02 | **Refuse and explain** ("Blocked: voxels exist beyond this size — prune or move the piece first"); the mock clips for demo only |
| **OQ-6** | Legal **min/max grid size**, and voxel/size limits for ×2/×3 | C02/C03 | min 1; max from legacy; ×n blocked when any dim·n exceeds max (mock: 32 size, 64 scale) |
| **OQ-7** | Behaviour when **nudge/rotate/flip** would leave the grid: blocked, grow grid, or wrap? | C04 | Blocked with message "Blocked: the piece would leave the grid" |
| **OQ-8** | **Colour Add/Edit/Remove**: does Add open a chooser? What happens to voxels of a removed colour? Colour naming (legacy shows `Default [C1]`) | C09 | `+` opens the legacy colour chooser; removal reassigns voxels to Default; names: Default(=C1), C2, C3… |
| **OQ-9** | Legacy **keyboard shortcuts** (undo/redo, etc.) and any legacy modifier-click behaviour in the 3D view that the new Shift/Alt/Ctrl mapping replaces | 04 | Keep global shortcuts; new modifier mapping replaces legacy modifier-click |
| **OQ-10** | What exactly do menu commands **Toggle 3D** and **Status** do; does **Config** hold anything beyond the Settings dialog; is **About** best under Help | C00 | Keep all legacy menu entries in place except Config → gear/Settings |
| **OQ-11** | Config **keys, ranges, defaults, storage file** for every Settings row (e.g. max worker threads) | C12 | Use legacy keys; ranges in C12 are from the screenshot |
| **OQ-12** | Picking: how legacy resolves the face/voxel under the pointer and which neighbour an added voxel attaches to | C06 | Top-most face under cursor; add at `voxel + faceNormal`; erase/paint the owning voxel |
| **OQ-13** | **Framework integration facts** — *Qt profile:* Qt version (≥ 6.8), graphics API on Windows (QRhi port vs forced OpenGL), Meson `qml_module` setup, Windows build route (native kit vs MinGW cross), deployment. *RmlUi profile:* **RmlUi integration facts**: pinned RmlUi version; render/system/file interfaces and host window backend; whether **data bindings**, **SVG plugin**, **flexbox**, `box-shadow`, `transition`, gradient decorators and `dp` units are available; how the new RmlUi front-end co-exists with (or replaces) the legacy GUI toolkit for the Entities tab; how the existing GL renderer can draw into a rect behind the RmlUi overlay; availability of custom cursors via the system interface | foundations | Follow `foundations/rmlui-implementation-guide.md`; record deviations |
| **OQ-14** | Minimum/default **dimensions of a new shape**; whether Delete confirms; whether legacy **Copy appends at the end** (spec assumes yes) | C01 | Legacy defaults; Duplicate appends |
| **OQ-16** | **Positional ids**: how do legacy ←/→, Delete and Copy keep references from the Puzzle/Solver tabs (problem definitions, results) consistent when ids change? Is the chip colour a function of position (palette cycle)? What is the palette beyond S4? | C01 | Ids/colours derive from position; references follow the shape object exactly as legacy does |
| **OQ-15** | Do **W+/W−** have a maximum; is weight 0 legal? | C01 | min 0, no max |
| **OQ-17** | How to obtain the **OS default pointer bitmap and hotspot** for the Fixed/Variable cursors' runtime composition (e.g. Windows `LoadCursor(IDC_ARROW)` → icon info; Erase/Paint cursors are icon-only and do not need it) and whether the host/RmlUi system interface can install a runtime-built colour cursor with alpha at the OS cursor scale | C06, guide | Use the bundled `arrow-fallback.svg` (classic black arrow, white edge) and compose the badge on it |
| **OQ-18** | **File ▸ New**: what the legacy flow asks besides the "Select space grid" choice; whether "Tetrahedra-Octahera" is a typo for **Tetrahedra-Octahedra**; any further types; **view-cube details**: legacy semantics of the 90° and roll arrows, animation duration/easing, what Home frames, whether dragging the cube orbits, whether edge hover highlights one face or both | C00, C13, C14 | Names as in the legacy dialog (typo fixed); behaviour as specified in C13 |
| **OQ-19** | **Per-type behaviour** still not visible: which of **Rhombic Tetrahedra / Tetrahedra-Octahedra** the X-split-squares screenshot shows, and the look/behaviour of the other; Triangular Prism rotate angles (X, Y single buttons; Z pair); the tetrahedral types' transform sets; whether scale ops exist for Rhombic Tetrahedra; semantics of the sphere nudge rows ("u"/"d"); mirror/span semantics, valid 2D planes, layer meaning and rulers for each non-cubic lattice; 2D cell hit-testing rules. *(Resolved: official names; prism nudges = 6 in-plane + 2 perpendicular; prism supports scale.)* | C14, C04, C07, C08 | Use the legacy commands exactly; mock labels are placeholders |
| **OQ-20** | **Groups:** the Puzzle tab treats a group as a **logical grouping only** (a piece is in at most one group); the real meaning is in the Solver (pieces of a group are not disassembled against each other). Confirm the stored representation (legacy dialog shows a number per piece per group column) and any constraint a group places on counts | C17 | Store membership as group id; weight 1 |
| **OQ-21** | Legacy Puzzle-tab **← / → buttons** next to the piece list (L106) and next to Problems (L101): reorder? Confirm their command; the redesign drops piece reordering (order = shape order) and reorders problems by dragging | C16 | Preserve problem reorder only |
| **OQ-22** | **Colour rules:** exact legacy semantics of the Colour Assignment chips + piece→result pair list (default pairs, whether the white "Default" row is special, what *Sort by Piece / Sort by Result* persists) | C17 | Matrix of allowed (piece colour → result colour) pairs; default identity |
| **OQ-23** | Where ranges (min–max) and fixed counts are stored (legacy Problem Details has Min / Max); how the Solver uses ranges; whether **count 0 with min 0** (optional) is distinct from "not in the puzzle" | C16/C17 | max > 0 = in the puzzle |
| **OQ-24** | **Result colour range** in the status text ("can contain 192 – 352 voxels"): confirm it is fixed-voxel count … total-voxel count of the result shape | C16 | As implemented in the mock |
| **OQ-25** | **Manage solutions semantics** — labels derived from the button names and the BurrTools user guide (`mainwindow.cpp#L4333` could not be read). **Resolved:** *Before / After* are relative to the **current sorted order of the list**. **Still to verify:** *All* and *At* (assumed: all / the selected solution), *w/o DA*, and the five disassembly buttons *D DA, D A DA, A DA, A A DA, A M DA* (assumed: delete current / delete all disassemblies; analyse current / all / missing) | C20 | As specified in C20; confirm before cut-over |
| **OQ-26** | Legacy **Drop** field — assumed "drop solutions below this disassembly level" (min 0, default 1) | C19 | Label "Drop below level" |
| **OQ-27** | Solver-side **Sort by** list: confirm all options (legacy shows *Moves for Complete Disassembly*; product owner: also *original id* and *piece count*, both valid with and without disassembly) and how it interacts with **Limit**; relation to the display sort (C20) | C19, C20 | Four options as in C19 |
| **OQ-28** | Contents and commands of the legacy **Placement browser** and **Movement browser** windows (what each list shows, step/back/run semantics, what the 3D view shows) | C21 | Layout-only drawer |
| **OQ-29** | Exact semantics of **Prepare / Start / Continue / Stop / Step** (does Start discard earlier results? does Prepare clear them? what does *Step* do beyond one assembler step?) | C19 | As in the C19 state table |
| **OQ-30** | Meaning of the legacy **"Move 19 (14.2)"** label and the Move slider's value box (e.g. 3.14): total moves vs current position; how fractional positions map to piece motion for several pieces/groups moving together | C20 | "Move current / total" |
| **OQ-31** | **Groups in the Solver:** members of a group are removed together and may stay together at the end — confirm what the player/move list should display for group moves (e.g. a group badge on piece chips) | C19, C20 | No extra UI |
| **OQ-32** | Behaviour of the UI with **Just count** (solutions dropped): list empty with an explanatory message? counters only? | C19, C20 | Counters only |
| **OQ-33** | **Export Solution to STL** dialog content and whether export applies to the selected solution at the current move position | C20 | Opens the legacy dialog |
| **OQ-34** | *Resolved by the product owner:* the legacy menu-only features (assembly import, convert grid type, image/vector/STL export, Status window, Edit comment) **keep their existing dialogs — no major UI to redesign**; **Config = the redesigned Settings window (C12)**; the **bulk range** command is exposed as *Set range for all pieces…* in the Puzzle "In this puzzle" ⋯ menu (C16) | C00, C16 | Closed |
| **OQ-35** | **Front-end framework decision** (Qt 6 Quick vs RmlUi) — run `migration/04-framework-spike-plan.md`; default Qt 6 Quick unless it misses a hard budget that RmlUi meets | all | Qt 6 Quick (recommended) |
| **OQ-36** | **Legacy menu accelerators** (open/save/undo/redo/export …) must be listed in the menus and must not collide with the new keys (`Ctrl+1/2/3`, `Ctrl+[`/`]`, `Ctrl+Space`, `F1`, `O`, `P`, Puzzle `+`/`−`) — verify against the legacy key table | C22, overview/04 | New keys as specified; resolve any collision in favour of the legacy accelerator and log it |


---
<!-- FILE: overview/06-ui-contract.md -->

# 06 — UI contract: element ids, data-model variables, events, state classes

This is the **interface between the view layer and the C++ logic** — framework-neutral. Mapping: **Qt Quick** → `foundations/qtquick-implementation-guide.md` §2 (ids → `objectName`, variables → `Q_PROPERTY`, lists → `QAbstractListModel`, events → `Q_INVOKABLE`); **RmlUi** → `foundations/rmlui-implementation-guide.md` §2 (ids → element ids, variables → data bindings/classes, events → listeners). Component specs (`components/C##`) define behaviour; this file lists the names both sides must agree on. Variable names are `snake_case` data-model fields (types in brackets). "Owner" = the C++ controller that holds the truth. Markup only *reads* variables (via bindings or C++-set classes/text) and *raises events*; controllers *handle* events and update variables. Event names are logical — map them to RmlUi event listeners (`click`, `mousedown`, `keydown`, …) as needed.

## 0. Input facts every controller can read (from the input bridge)
`modifier_shift`, `modifier_alt_or_cmd`, `modifier_ctrl` [bool], `mouse_buttons` (left/middle/right), pointer position, `wheel_delta`. Controllers re-evaluate hover feedback on **every pointer move and on every modifier key press/release**.

## 1. Layout (C11) — owner `LayoutController`
| Variable | Type | Default | Persist |
|---|---|---|---|
| `layout_left_collapsed` | bool | false | yes |
| `layout_right_collapsed` | bool | false | yes |
| `layout_focus` | `none`\|`2d`\|`3d` | none | no |
| `editor_visible` *(derived)* | bool = `focus==2d` ∨ (`focus==none` ∧ ¬right_collapsed) | — | — |
Events: `collapse_left`, `expand_left`, `collapse_right`, `expand_right`, `toggle_focus_2d`, `toggle_focus_3d`, `escape`. **The collapse/expand state is kept per workspace** (`layout_left_collapsed[workspace]`, `layout_right_collapsed[workspace]`); every side card of every workspace has a `[data-collapse=L|R]` header button and, when collapsed, a `.crail` with `[data-expand=L|R]` and a vertical title.
State classes on `#workspace`: `left-collapsed`, `right-collapsed`, `focus-2d`, `focus-3d` (mutually consistent per C11 table). `editor_visible` also gates the 3D **layer slab / dim-other-layers** drawing (C06).

## 2. Shapes (C01) — owner `ShapesController` (backed by the model's shape list)
| Variable | Type |
|---|---|
| `shapes[]` | list of `{ index, id_text ("S"+(index+1)), colour (by position), label, weight, voxel_count }` |
| `selected_shape` | int |
| `renaming_index` | int or −1 |
| `drag_source`, `drag_target` | int or −1 |
Events: `shape_select(i)`, `shape_new`, `shape_duplicate(i)`, `shape_delete(i)`, `shape_rename_begin(i)`, `shape_rename_commit(i,text)`, `shape_rename_cancel`, `shape_weight(i,±1)`, `shape_reorder(from,to)`, `shape_move(i,±1)`, `shape_context_menu(i,x,y)`.
Classes on a row: `selected`, `hovered`, `renaming`, `dragging`, `drop-target`. Weight badge present only when `weight != 1`. After any reorder/delete the controller recomputes `id_text` and `colour` for **every** row (position is identity).

## 3. Voxel editor (C07, C08, C02, C09) — owner `EditorController`
| Variable | Type | Default | Persist |
|---|---|---|---|
| `tool` | `fixed`\|`variable`\|`erase`\|`paint` | fixed | no |
| `mirror_x/y/z`, `span_x/y/z` | bool | false | no |
| `plane` | `XY`\|`XZ`\|`YZ` | XY | no |
| `layer`, `layer_count` | int | 0 | no |
| `layer_has_voxels[]` | bool list | — | — |
| `grid_cols`, `grid_rows` | int (derived from plane/size) | — | — |
| `size_x/y/z` | int (selected shape) | — | — |
| `apply_size_to_all` | bool | false | no |
| `draw_colour` | int index (0 = Default) | 0 | no |
| `colours[]` | `{ index, name, rgb, is_default }` | — | with puzzle |
| `quick_tool` *(derived)* | `none`\|`fixed`\|`variable`\|`erase` — from modifiers: Ctrl→erase, Alt/⌘→variable, Shift→fixed (precedence in that order) | none | — |
| `effective_tool_2d` *(derived)* | `quick_tool` if set else `tool` | — | — |
| `cursor_readout` | string | "" | — |
Events: `tool_select(id)`, `mirror_toggle(axis)`, `span_toggle(axis)`, `plane_select(p)`, `layer_select(k)`, `layer_step(±1)`, `grid_hover(cell,mods)`, `grid_leave`, `grid_press(cell,mods)`, `grid_drag_enter(cell)`, `grid_release`, `size_step(axis,±1)`, `size_commit(axis,text)`, `size_apply_all_toggle`, `colour_select(i)`, `colour_add`, `colour_menu_open(i)`, `colour_edit`, `colour_remove`.
Classes: tool/mirror/span/plane buttons `on`; layer chips `on`, `has-voxels`; colour swatches `on`, `is-default`; grid cells (if DOM) `fixed`, `variable`, `hovered`; stepper `invalid`.
Grid **stroke rule:** the effective tool is captured on `grid_press` (from modifiers at that moment) and used for the whole stroke; all voxels changed by the stroke are one undo step.

## 4. 3D viewport (C06) — owner `ViewportController`
| Variable | Type | Default | Persist |
|---|---|---|---|
| `nav_mode` | `orbit`\|`pan` | orbit | no |
| `edit_mode` | bool | false | no |
| `display_axes`, `display_bounds`, `display_layer_slab` | bool | true | yes |
| `display_dim_other_layers` | bool | false | yes |
| `projection` | `perspective`\|`orthographic` | perspective | yes |
| `colour_view` | `piece`\|`voxel` | piece | yes |
| `viewport_action` *(derived, per pointer move)* | `none`\|`fixed`\|`variable`\|`erase`\|`paint` = `quick_tool` if a modifier is held, else (`edit_mode` ? `tool` : none) | — | — |
| `edit_button_tool` *(derived)* | tool whose glyph and number badge the Edit button shows: `quick_tool` while a modifier is held, else `tool` | — | — |
| `hover_preview` | `{ source: viewport\|grid, action, target_cells[], valid }` or none | none | — |
Events: `nav_mode_select(m)`, `edit_toggle`, `views_open`, `view_select(name)`, `fit`, `display_open`, `display_toggle(key)`, `projection_select(p)`, `colour_view_select(v)`, `viewport_pointer(button, x, y, mods)`, `viewport_wheel(dy)`, `viewport_click(x,y,mods)` (press-release ≤ 4 dp, left button), `viewport_drag(dx,dy,button,mods)`.
Classes: toolbar buttons `on` (nav mode, edit mode, focus), Edit button additionally `override` (momentary quick tool; dashed accent outline), display menu rows `checked`, `disabled` (slab and dim rows disabled when `editor_visible` is false, with hint "needs Voxel editor").
**Hover preview source rule:** the controller owns one `hover_preview`. It is filled by the 3D pointer (target from picking) **or** by `grid_hover` from the 2D grid (target = the hovered cell at the current layer); mirror/span expansion applies in both cases; it is cleared on leave, on drag, or when no action applies.

### Cursor decision table (set by the controller that owns the pointer)
| Pointer over | Condition | Cursor |
|---|---|---|
| 3D surface | `viewport_action` ∈ fixed/variable | OS default pointer + square badge with a “+” (solid / dotted boundary; inside = `draw_colour`, Default → white in light theme / near-black in dark); hotspot = pointer's |
| 3D surface | `viewport_action` ∈ erase/paint | tool icon alone (eraser / bucket with drop), hotspot = glyph centre |
| 3D surface | no action, dragging with orbit/pan | `grabbing` / `move`; idle: `default` |
| 2D grid cell | always | same as above for `effective_tool_2d` |
| elsewhere | — | OS default / `pointer` on buttons |
The cursor is recomposed whenever the tool in effect, `draw_colour` or the theme changes (see `foundations/glyph-catalog.md` ▸ Cursors). The Edit/quick-tool cursors depend on `viewport_action`, `draw_colour` and the theme. Cursor and ghost change **as soon as a modifier key is pressed or released**, without requiring a pointer move.

### View cube & camera (C13) — owner `ViewportController`
| Variable | Type | Notes |
|---|---|---|
| `camera` | azimuth, elevation, roll, zoom, pan | roll only changes via the cube's roll arrows |
| `camera_animation` | none \| `{from,to,start,duration_ms=360,easing}` | cancelled by any manual orbit/pan/zoom |
| `cube_hover` | none \| home \| face(k,sign,region) \| arrow(dir) \| roll(sign) | drives highlight, tooltip, cursor |
| `cube_aligned_face` *(derived)* | face or none | true when the view direction equals a face normal → show 90° and roll arrows |
Events: `cube_hover(region)`, `cube_click(region)`, `view_home`, `view_fit` (key `F`), `view_step(dir)`, `view_roll(sign)`. Classes/state: widget hidden when `view_cube` setting is off or `layout_focus==2d`.

## 5. Inspector (C03, C04, C05) — owner `InspectorController`
| Variable | Type | Default | Persist |
|---|---|---|---|
| `inspector_open_transform/fit/repair` | bool | true / false / false | yes |
| `minimize_factor` *(derived)* | int (≥2) or 0 | — | — |
| `can_prune`, `can_center`, `can_origin` *(derived)* | bool (shape non-empty) | — | — |
| `can_scale_2`, `can_scale_3` *(derived)* | bool | — | — |
Events: `section_toggle(name)`, `transform(op∈flip|rotate|nudge, axis, sign)`, `fit_prune`, `fit_center`, `fit_origin`, `scale(n)`, `minimize`, `fill_holes`, `surface(op∈fixed|variable|clear, part∈inner|outer)`.
Classes: section `open`/`closed`; buttons `disabled` with the explanatory hint text.

## 6. Status bar (C10) — owner `StatusController`
`status_text` [markup string], `status_cursor` [string], `flash_text` [string], `flash_remaining_ms` [int]. Event `flash(text)` raised by any controller; it replaces `status_text` for 2400 ms then the live text is recomputed.

## 7. Settings (C12) — owner `SettingsController`
Variables: one per catalogue row (`undo_depth`, `tooltips`, `theme`, `view_cube`, `reverse_scroll`, `rotation_method`, `lighting`, `fade_pieces`, `worker_threads`, `gl_display_lists`), plus `settings_open`, `settings_page`, `settings_query`.
Events: `settings_open`, `settings_close`, `settings_page_select(p)`, `settings_search(text)`, `setting_set(key,value)`, `settings_reset_section(p)`, `settings_restore_all`.
Classes: nav item `on`; switch `on`; segmented option `on`; row `hidden` (search filter).

## 7b. Voxel space type (C14) — owner `Model` (read-only for the UI)
`voxel_type` [id] — property of the loaded file, **never written by the Entities UI**; `voxel_descriptor` *(derived)* = `{ display_name, cell_shape_2d, supports_scale, transform_set, planes, … }`. Consumers: Voxel-editor header chip, `InspectorController` (transform buttons, scale visibility), `voxel-grid` element (cell shape), renderer. Only File ▸ New can set it (`shell.newfile.type.*`).

## 8. App shell (C00)
Variables: `workspace` (entities|puzzle|solver; set by the **workspace rail**, C15), `file_name`, `file_path`. Events: `menu_command(name)`, `workspace_select(name)` (also `Ctrl+1/2/3`), `settings_open`. Ids: `#app-rail`, `#rail-entities`, `#rail-puzzle`, `#rail-solver` (state class `selected`). State class on `#window`: `workspace-entities|puzzle|solver` selects which side cards, toolbar buttons and Display items are shown.

## 10. Puzzle tab (C16, C17, C18) — owner `PuzzleController` (backed by the model's problems)
| Variable | Type | Notes |
|---|---|---|
| `problems[]` | `{ index, id_text ("P"+(index+1)), label, result_shape, pieces{ shape → {min, max, range, group} }, groups[], colour_rules[][] }` | positional ids, editable labels |
| `problem_index` | int | selected problem |
| `puzzle_selected_piece` | shape ref | single selection across all views |
| `pieces_view` | `chips`\|`table` | |
| `all_pieces_open` | bool | |
| `drag` | none\|`{shape, from}` | drives `drop-ok` classes |
| derived: `result_range`, `pieces_voxels`, `fits`, `holes`, `copies[]` | see overview/03 | `holes` is derived, never edited |
Ids: `#pz-problems` (rows `.row[data-i]`, `#pz-new`), `#pz-result` (`#pz-change`), `#pz-used` (tiles `.ut[data-u]`, table `tr[data-u]`), `#pz-view-seg`, `#pz-more`, `#pz-lib-head`, `#pz-lib` (chips `.lc[data-u]`), `#pz-summary`, `#pz-sel` (`#pz-range` switch, steppers `[data-sf=min|max]`, group buttons `[data-grp]`), `#pz-groups`, `#pz-rules` (cells `.mxc[data-i][data-j]`, helpers `[data-rule]`).
Events: `problem_select(i)`, `problem_new`, `problem_duplicate(i)`, `problem_delete(i)`, `problem_rename_begin|commit(i,text)`, `problem_reorder(from,to)`, `result_change(shape)`, `piece_select(shape)`, `piece_add(shape)`, `piece_count_step(shape, ±1)`, `piece_count_set(shape, field, value)`, `piece_range_toggle(shape)`, `piece_group_set(shape, group|none|new)`, `group_delete(g)`, `pieces_view_set(v)`, `pieces_add_all`, `pieces_clear`, `colour_rule_toggle(i,j)`, `colour_rule_preset(identity|all|none)`, `piece_drag_start|end(shape, from)`, `piece_drop(zone)`, `scene_pick(piece)`.
State classes: tiles/chips/rows `selected`, `hovered`, `dragging`; zones `drop-ok`; summary `ok`|`warn`; stepper `inline-label`. The Puzzle scene reuses the viewport events of §4 except edit-related ones, which are disabled when `workspace != entities`.
Scene rule: object anchors are laid out in screen space; the camera rotation applies per object about its own centre; pan/zoom apply to the whole layout (C18).

## 11. Solver tab (C19, C20, C21) — owner `SolverController` (wraps the existing solver threads and the problem's saved solutions)
| Variable | Type | Notes |
|---|---|---|
| `solver_problem_index` | int | which problem is solved / browsed |
| `solver_options` | `{ disassemble, drop_disassemblies, just_count, keep_mirror, keep_rotated, thorough_rotation_check, sort_key (id\|pieces\|level\|moves), drop_below_level, limit }` | defaults: false…, `moves`, 1, 100; `drop_disassemblies ⇒ disassemble` |
| `solver_state` | idle \| preparing \| ready \| running \| paused \| finished | drives button enabling (C19 table) |
| `solver_progress`, `solver_activity`, `time_used`, `time_left`, `assemblies_found` | number / text | status grid |
| `solutions[]` | per found assembly `{ id, pieces, has_disassembly, level_only, cannot_disassemble, level[], moves }` | persisted with the file |
| `solutions_shown` *(derived)* | list | `solutions` filtered by disassemblable when `disassemble ∨ drop_disassemblies`, ordered by display sort |
| `solutions_view` | list \| slider | persists per session |
| `solutions_sort` | `{ key (number\|level\|moves\|pieces), direction }` | click again reverses; default directions: number asc, others desc |
| `selected_solution` | index into `solutions_shown` | |
| `move_position` | float 0…total_moves | player/scene |
| `playing`, `play_speed` | bool, {0.5,1,2,4,8} | |
| `piece_visibility` | map piece id (S2, S5.1…) → solid \| wireframe \| hidden | |
| `display_xray` | bool (default true) | Display ▸ Show through solid pieces |
| `explorer` | none \| placements \| movements | docked drawer |
| `assembler_step` | none \| k | assembler-state mode |
Ids: `#slv-problem`, `#slv-mode` (radio cards `[data-m=asm|dis]`, `#slv-drop`), `#slv-opts-head`, `#slv-opts`, `#slv-run` (`[data-r=prep|start|cont|stop]`), `#slv-bar`, `#slv-pct`, `#slv-stat`, `#slv-adv` (`[data-x=placements|movements|step]`), `#slv-view-seg`, `#slv-sel`, `#slv-list` (rows `.lrow[data-i][data-id]`, header `[data-sort]`), `#slv-slider`, `#slv-meta`, `#slv-player` (`[data-t=start|back|play|fwd|end]`, `#slv-speed`, `#slv-move`), `#slv-sort`, `#slv-act` (`[data-a=manage|stl]`), `#slv-pieces` (chips `.pvc[data-p]`), `#slv-show-all`, `#slv-drawer`.
Keyboard (Solver): `solver_key(context: list|slider|seek, key, shift)` implements the §8 map of `overview/04`; the view must **not** let viewport/toolbar presses take keyboard focus while a Solutions control is focused (`sticky_focus`), and must restore focus after re-rendering.
Events: `solver_problem_select(i)`, `solver_mode_set(assemblies|disassembly)`, `solver_option_set(name,value)`, `solver_prepare`, `solver_start`, `solver_continue`, `solver_stop`, `solver_step`, `explorer_toggle(kind)`, `explorer_step(back|step|run|reset)`, `solutions_view_set(v)`, `solutions_sort(key)`, `solution_select(i)`, **`solution_scrub(i)`** (continuous while dragging; must not recreate the slider element), `player_transport(start|back|play|fwd|end)`, `player_speed(v)`, `player_seek(position)` (continuous), `solutions_manage(at|before|after|without_disassembly|all|delete_da_current|delete_da_all|analyse_current|analyse_all|analyse_missing)`, `solutions_manage_preview(action|none)` (menu item hover), `solutions_export_stl`, `piece_visibility_cycle(piece)`, `piece_visibility_reset`, `display_xray_toggle`.
State classes: radio card `selected`; rows `selected`, `will-delete`; chips `wireframe` / `hidden`; drawer `open`; run buttons `disabled`. The Solver scene reuses the viewport events of §4 except edit-related ones (disabled when `workspace != entities`).
Rules: scene geometry/positions come from the model's assembly + disassembly data (no mock logic in production); destructive removals ask for confirmation and are **not undoable** (they only mark the document modified).

## 9. Undo/redo coupling
Any controller (including `PuzzleController`: counts, ranges, groups, result, colour rules, problem list changes) that mutates the model does so through the existing command layer inside one undo transaction (granularity table in `overview/03`). After undo/redo the model raises change notifications; controllers refresh all derived variables (`id_text`, `size_*`, `layer_count`, `layer_has_voxels`, `minimize_factor`, `can_*`, status text).

## 12. Density, help window and global keys (Rev 6.0)
| Variable / event | Notes |
|---|---|
| `ui_density` | `standard` \| `minimal` (Settings ▸ General); drives every geometry token in `foundations/density.md`; changing it re-lays out all workspaces and resets the auto-hiding toolbar |
| `toolbar_autohide_visible` | Minimal only; true while the pointer is in the 72 dp reveal zone, the toolbar is hovered / keyboard-focused / its menu is open, or for 1.4 s after `O P E F` |
| `help_shortcuts_open` | C22 window (Help ▸ Keyboard shortcuts, `F1`) |
| events | `workspace_switch(n)` (`Ctrl+1/2/3`), `card_toggle(left\|right)` (`Ctrl+[`/`Ctrl+]`), `focus3d_toggle` (`Ctrl+Space`), `nav_mode(orbit\|pan)` (`O`/`P`), `shapes_list_key(up\|down\|alt_up\|alt_down\|enter)`, `puzzle_piece_key(plus\|minus\|delete)`, `grid_right_stroke(cells)` (always erase) |
Ids: `#help-shortcuts` (window), `.kbgrp`, `.kbrow`; in the mock `#setPg` page `keys`. Minimal tooltips: every control whose caption or help text is hidden in Minimal carries that text in its tooltip (`title` / `ToolTip.text`).


---
<!-- FILE: foundations/density.md -->

# Interface density — Standard (default) and Minimal

**Setting:** Settings ▸ General ▸ **Interface density** = **Standard | Minimal** (default **Standard**; persisted with the other settings). The earlier *Comfortable* spacing is **retired**: Standard (formerly "Compact") is the baseline every component spec refers to. **Every function is available in both densities** — Minimal only changes geometry, captions and where help text appears.
**Images:** Standard — `01-main-light.png`, `41-puzzle-tab-chips.png`, `51-solver-list-view.png`; Minimal — `67-minimal-entities.png`, `68-minimal-entities-fit-repair.png`, `69-minimal-puzzle.png`, `70-minimal-solver.png`, `71-minimal-toolbar-hidden.png`, `72-minimal-toolbar-shown.png`, `73-minimal-dark-collapsed.png`.

## 1. Geometry tokens (dp) — authoritative
Component specs quote **Standard** values. Where an older number in C00–C21 disagrees with this table, **this table wins**.
| Token / element | Standard | Minimal |
|---|---|---|
| Top bar height | 36 | 32 (wordmark hidden, logo only) |
| Status bar height / font | 24 / 11.5 | 22 / 11 |
| Workspace rail width (C15) · button | 60 · 52 × 50, radius 10, icon over caption | 44 · 36 wide, icon over a **caption rotated 90°** (reads top-to-bottom) |
| Side cards left / right (every workspace) | **320 / 340** | **264 / 320** |
| Collapsed card rail (C11) | 48 | 40 |
| Centre viewport at the 1600 dp reference | 864 | 970 |
| Body padding / gap between cards | 4 / 4 | 0 / 1 (cards flush, 1 dp `line` divider) |
| Card radius / shadow | 8 / token shadow | 0 / none (no card border) |
| Card header (`.ch`) / section header in cards | 36 | 30 |
| List row (shapes, problems) | 34 | 30 |
| Inspector accordion header / body padding | 32 / 2 10 10 | 28 / 2 8 8 |
| Button / stepper / segmented button | 28 / 28 / 22 | 26 / 26 / 22 |
| Tool strip button (C07) | h48 | h36, **icon only** |
| Mirror / Span button | h44 | h32, icon only, group captions hidden |
| Plane button | 54 × 46 | 38 × 34, icon only |
| Transform matrix button (C03) | h42 | h30, icon only (row/column headers kept) |
| Fit & scale / Repair action button (C04, C05) | min-h 40, glyph 30, title + description | h32, **glyph 24 + short inline label** (Prune, Center, Origin, Minimize, Fill holes), no description; Scale buttons show glyph + caption side by side |
| Surface-voxel rows (C05) | title + second line ("solid", "optional", "remove paint") | **title only** |
| 3D toolbar button (C06) | 48 × 44 (icon + caption), toolbar top 8, padding 3, radius 10 | 34 × 32 **icon only**; toolbar **auto-hides** (§3) |
| Layer strip / chip (C08) | 48 / 32 × 24 | 40 / 30 × 22, "Z layer" caption hidden |
| Drawing-colour footer (C09) | big swatch 32 + caption + palette on one row | big swatch 26 + palette (caption → swatch tooltip) |
| Puzzle tile (C16) / library chip | 96 × 50, 3 per row / h26 | 78 × 46, 3 per row / h24 |
| Solver section padding (C19) | 9 × 12 | 7 × 10 |
| Solver list row (C20) | 28; columns 44 · 1fr · 96 · 40 · 30 | 26; columns 34 · 1fr · 76 · 30 · 24 |
| Disassembly player (fixed) | 112 | 84 (title hidden, move counter kept) |
| Piece display list (fixed) | 104, chips h26 | 76, chips h24 |

## 2. Minimal — what changes (and where the information goes)
1. **Icon + caption → icon only:** viewport toolbar, tool strip, planes, mirror/span, transform matrix, "+ New" (→ "+"), shortcut badges. The caption becomes (or already is) the control's **tooltip**.
2. **Help / descriptive text → tooltip:** accordion summaries, action-button descriptions, 3D hint lines, Solver radio-card descriptions, option help lines, explorer note, list footer, Puzzle hints ("drag, double-click or + to add", group note, colour-rules caption), "Drawing colour" caption, Problem/Result sub-lines. Each hidden text is attached as the tooltip of its control or section (the implementation must guarantee that no control is left without its name — AC-DEN-03).
3. **Short inline labels where width allows** (Fit & scale), **first line only** where rows have two (Surface voxels, Repair).
4. **Chrome-less panels:** cards flush, hairline dividers, no radius, no shadow.
5. **Narrower everything:** side cards 264/320, rails 44/40, bars 32/22, denser rows and fixed blocks (table above).
6. **Auto-hiding viewport toolbar** (§3).
Nothing is removed: menus, buttons, lists, keyboard shortcuts (C22) and tooltips stay identical.

## 3. Auto-hiding viewport toolbar (Minimal only, every workspace)
* Hidden state: opacity 0, translated up 10 dp, not hit-testable; a **40 × 4 dp handle** (muted, 45 %) marks its position at the top centre of the viewport.
* **Shows** (160 ms fade/slide) when: the pointer is within **72 dp of the viewport's top edge**; keyboard focus enters the toolbar (`Tab`, focus-visible); its **Display** menu is open; a mode key is pressed (`O`, `P`, `E`, `F` — shown for 1.4 s).
* **Hides** 700 ms after the pointer leaves that zone (500 ms after leaving the viewport) unless hovered, keyboard-focused or its menu is open. `Esc` or a click in the 3D view releases toolbar focus so it can hide; a mouse click on a toolbar button does not keep it pinned.
* Switching density resets it.

## Acceptance criteria
* AC-DEN-01 Settings ▸ General offers exactly *Standard* and *Minimal* (default Standard); switching applies immediately in every workspace and persists.
* AC-DEN-02 All geometry matches §1 in both densities; at 1600 × 1000 no card content overflows, no button label or header wraps, Puzzle tiles stay 3 per row.
* AC-DEN-03 In Minimal every icon-only control and every hidden help text is reachable as a tooltip; no function, menu entry or shortcut is missing compared with Standard.
* AC-DEN-04 Minimal shows the rotated rail captions, short Fit & scale labels, first-line-only Surface/Repair rows and chrome-less panels.
* AC-DEN-05 The Minimal toolbar shows/hides exactly as §3 describes, including keyboard focus and open-menu cases.


---
<!-- FILE: foundations/qtquick-implementation-guide.md -->

# Implementation guide: Qt 6 Quick (QML) views + C++ controllers

**Status:** framework decision pending between **Qt 6 Quick (recommended)** and **RmlUi** (see `migration/04-framework-spike-plan.md`). The behaviour spec (components C00–C21, contract, journeys, tests) is **framework-neutral**; this guide and `rmlui-implementation-guide.md` are the two implementation profiles. Use the one for the chosen framework.
**Baseline:** Qt **6.8 LTS or newer** (track the latest 6.x minor; 6.12 LTS when available), **C++20** codebase, **Meson** with the `qt6` module (`compile_moc`, `compile_resources`, `qml_module`), Qt Quick + Qt Quick Controls (**Basic** style as the base, fully re-skinned), Qt Quick Shapes, Qt SVG, Qt Quick Dialogs, Qt Test / Qt Quick Test. **Not used:** Qt Widgets for the main UI, Qt Quick 3D (the 3D view is the app's own renderer), native platform Controls styles.

## 1. Responsibility split
| Concern | Lives in | Notes |
|---|---|---|
| Static structure, layout, look, hover/focus/disabled visuals, transitions | **QML** components | One QML module `BurrTools.Ui`; one file per primitive (PR-xx) and per component (Cxx) |
| State (selection, tool, plane, layer, toggles, layout mode, solver state, …) | **C++ controllers** (`QObject`) | One controller per contract section (`overview/06`), registered with `QML_ELEMENT`/`QML_SINGLETON` |
| Interaction rules (Trigger → Effect tables, Esc ladder, modifiers, previews) | **C++ controllers** | QML only forwards intent (`controller.shapeSelect(i)`); **no business logic or JS state in QML** beyond trivial view glue |
| Model mutations (flip, rotate, fill, scale, reorder, solve, delete solutions, …) | Existing **BurrTools model/commands** (undo-aware) | Never reimplemented in QML |
| Lists (shapes, problems, pieces, solutions, colours, layers) | **`QAbstractListModel`** subclasses in C++ | Roles mirror the contract's list fields |
| 3D scene, view cube, ghosts, slab | App renderer in a custom **`QQuickRhiItem`** (or a window underlay) | §5 |
| 2D voxel grid, hatching, dashed outlines | Custom **`QQuickItem` with scene-graph geometry** | §5 |
| Cursors, tooltips, popups, dialogs | `QCursor` (C++), `ToolTip`, `Popup`/`Menu`, `Dialog`, `FileDialog`/`ColorDialog` | §6 |
| Automation/test hooks | `objectName` = spec id; Qt Test / Qt Quick Test | `migration/02-test-plan.md` |

## 2. Mapping the UI contract to Qt
| Contract concept (`overview/06`) | Qt construct |
|---|---|
| **Element id** (`entities.shapes.row.<i>.rename`, `#slv-list`, …) | `objectName` with the exact spec id (dots allowed); delegates set it from `index` |
| **Variable** (`selected_shape`, `solver_state`, `display_xray`, …) | `Q_PROPERTY(T name READ … WRITE … NOTIFY nameChanged)` on the owning controller; enums via `Q_ENUM` |
| **List variable** (`shapes[]`, `solutions[]`, …) | `QAbstractListModel` with named roles; derived lists (`solutions_shown`) via `QSortFilterProxyModel` or a dedicated model |
| **Event** (`shape_select(i)`, `solutions_manage(action)`, `solution_scrub(i)`) | `Q_INVOKABLE void shapeSelect(int)` etc.; continuous events (scrub, seek) are throttled to one update per frame in C++ |
| **State class** (`selected`, `on`, `open`, `will-delete`, `wireframe`, `hidden`) | Boolean/enum roles or properties driving QML bindings and `states`/`transitions`; never colour-only |
| **Tooltip text** | `ToolTip.text` with the exact spec string; respects Settings ▸ Show tooltips (one global property) |
| **Per-workspace memory** (collapse, selection, scroll) | Stored in the controllers keyed by workspace; QML restores from properties on switch |

## 3. Theming (Light / Dark / System)
* A `Theme` **QML singleton** exposes every token from `foundations/design-tokens.json` as properties (colours, radii, spacing, durations, type scale). Generate it at build time from the JSON (small script) so tokens stay single-source.
* Two palettes; the active one is chosen from Settings `theme` (`light` | `dark` | `system`). **System** follows `Qt.styleHints.colorScheme` (updates live when the OS changes) and falls back to Light when the platform reports *Unknown*.
* Glyphs with theme variants (`glyphs/light|dark`) switch by binding the `source` to the theme; monochrome UI icons are tinted (`MultiEffect` colorization or `Image` + `ColorOverlay` equivalent, or render the SVG with the current colour in C++ once per theme).

## 4. Units, layout, styling features
* **Units:** design **dp = QML logical pixels**. High-DPI scaling is automatic (set the rounding policy to *PassThrough* for fractional scales); the canonical 2560×1600 display at 1.6× matches the reference.
* **Layout:** `RowLayout`/`ColumnLayout`/`GridLayout` (grids in the spec — tool strip, transform matrix, list rows, detail tiles, status grid — are **real `GridLayout`s**); card widths left **380** / right **400** / rail **56** / workspace rail **68** from the Theme.
* **Cards:** `Rectangle` (radius, 1 dp border) + a single cached shadow (`MultiEffect` shadow on the card container, **not** per row).
* **Dashed/dotted outlines** (variable voxels, wireframe piece chips, drop zones): `Shape` + `ShapePath { strokeStyle: ShapePath.DashLine; dashPattern: [...] }`; hatch tiles via a 12 × 12 dp tiled `Image` (`fillMode: Image.Tile`).
* **Vertical rail titles:** `Text { rotation: 90 }` inside a rotated-size container.
* **Text:** `elide: Text.ElideRight` for labels/levels; tabular numerals via `font.features: { "tnum": 1 }`.
* **Transitions:** sidebar collapse = `Behavior on width { NumberAnimation { duration: Theme.durCollapse } }`; hover/press colours via `states`. Keep animations on properties that do not relayout large subtrees.
* **Lists:** `ListView` (built-in delegate recycling) for shapes, problems, solutions; sticky headers via `ListView.headerPositioning`.

## 5. Special surfaces
1. **3D viewport (C06, C13, C18, C21).** A custom **`QQuickRhiItem`** (Qt ≥ 6.7) hosts the renderer. Choose one: **(a)** port the renderer to **QRhi** (portable: D3D11/12, Vulkan, Metal, OpenGL) — keep it behind a thin adapter because QRhi has limited compatibility guarantees across releases; or **(b)** force the **OpenGL** graphics API (`QQuickWindow::setGraphicsApi`) and render with modern GL inside the item. If profiling shows the item's texture pass matters, render as an **underlay** (`beforeRenderPassRecording`) directly into the window. Toolbar, view tag, hint and the view cube's hit-testing overlay are QML items above it; pointer events on the item go to the 3D interaction controller (orbit/pan/zoom/pick/edit, modifiers).
2. **2D voxel grid (C08).** Custom `QQuickItem` building one `QSGGeometryNode` per layer (cells, hatch, ghost, rulers) and handling mouse/hover/drag-stroke with modifiers in C++. Do **not** use `Canvas` (JS rasterisation) or a `Repeater` of 1 024 `Rectangle`s.
3. **Solver List/Slider (C20).** `ListView` + `QSortFilterProxyModel`; the Slider view binds `value` one-way and calls `solutionScrub(i)` — the slider item is never recreated (AC-C20-02).
4. **Piece chips, tiles, colour matrix (C16, C17, C20).** `Flow`/`GridLayout` + delegates; chip appearance from roles (`solid` / `wireframe` dotted `Shape` border / `hidden` greyed).
5. **Drag-and-drop (C01 reorder, C16 add/remove pieces).** `DragHandler` + `DropArea` with `Drag.keys`; the drop indicator is a QML item positioned from the controller's computed index.

## 6. Cross-cutting runtime pieces
* **Popups/menus (PR-11), Manage solutions, context menus:** `Menu`/`Popup` (closePolicy: outside press, Esc); the Manage-menu hover preview calls `solutionsManagePreview(action|none)` from each item's `hoveredChanged`.
* **Modal dialogs (PR-12, PR-32, Settings C12, Set range for all pieces):** `Dialog { modal: true }` with the Theme scrim; focus trapped by default.
* **Native dialogs:** `FileDialog` (open/save/export), `ColorDialog` (colour edit) from Qt Quick Dialogs — native where the platform offers them.
* **Tooltips (PR-14):** `ToolTip.delay: 500`, global enable flag.
* **Cursors:** the composed edit cursors (`foundations/cursors/`) are built at runtime as `QPixmap` → `QCursor(pixmap, hotX, hotY)` (recompose on tool/colour/theme change) and applied to the viewport/grid items (`setCursor`).
* **Status flash (PR-13):** `QTimer` (2400 ms) in the status controller.
* **Input:** modifier state is read on every pointer event *and* on key press/release (quick-tool cursor/ghost); middle button and wheel handled in the 3D item.
* **Assets:** SVG glyphs via `Image { source: "...svg"; sourceSize: ... }` (Qt SVG) — set `sourceSize` to the device-pixel size to stay crisp.
* **Window menu / top bar:** the redesigned top bar (C00) is QML; on macOS optionally mirror it into the native `MenuBar`.

## 7. Performance rules
* Build QML with the **Qt Quick Compiler** (`qmlsc`/`qmlcachegen` via `qml_module`): typed properties, no `var` in hot bindings, no JS loops in delegates.
* Load rarely used surfaces lazily (`Loader { active: … }`): Settings, explorer drawer, dialogs, Puzzle/Solver workspaces until first visited.
* One shadow per card, cached; avoid `layer.enabled` on large or frequently changing subtrees.
* Throttle continuous controller updates (scrub, seek, hover pick) to the frame (`QQuickWindow::frameSwapped` / `update()` coalescing).
* Budgets: `migration/02-test-plan.md` §6 apply unchanged.

## 8. Build, deploy, testing
* **Meson:** ``qt6 = import('qt6')``; ``qt6.qml_module('BurrTools.Ui', qml_sources: …, moc_headers: …)``; `compile_resources` for icons/glyphs/cursors. **C++20 note:** keep classes with `Q_OBJECT`/`QML_ELEMENT` in conventional headers (no C++20 module `import`s there); everything else may use concepts, ranges, `std::span`, `std::format`.
* **Windows:** either build natively on Windows CI with an MSVC or MinGW Qt kit, or cross-compile with a MinGW Qt build (self-built or distro `mingw64-qt6` packages); deploy with `windeployqt` (Quick, Controls, Shapes, Svg, Dialogs, platform plugin).
* **Tests:** C++ controller/model tests with **Qt Test** (no GUI); QML behaviour tests with **Qt Quick Test** (`TestCase`, `SignalSpy`, `mouseClick`, `keyClick`), run with `-platform offscreen` in CI; visual regression via `grabToImage()` compared with references (tolerant diff) for the screenshots in `reference/screenshots/`; stable lookups by `objectName` (spec ids). Optional external automation through the accessibility tree (UI Automation on Windows).
* **Accessibility:** set `Accessible.name` from the tooltip/label text; this also makes external UI automation reliable.

## 9. What the implementing agent must produce (per phase)
QML components for the primitives (PR-01 … PR-32) and components (C00 … C21) with `objectName`s from the specs; C++ controllers and models exposing exactly the contract's variables/events; the 3D `QQuickRhiItem` and the 2D grid item; the Theme singleton generated from tokens; Qt Quick Test cases for every T-Cxx test that is UI-observable, Qt Test cases for the rest; a log of deviations in `migration/RESOLUTIONS.md`.


---
<!-- FILE: foundations/rmlui-implementation-guide.md -->

# Implementation guide: RmlUi markup/styling + C++ logic

**Target stack:** the front-end is **RmlUi** documents (RML = HTML-like markup, RCSS = CSS-like styling). **All interaction logic is C++.** There is no scripting layer. This spec therefore describes *what the UI looks like, which states it has and what each interaction must do* — never how to script it. Anything in `reference/mock/` that is script is prototype scaffolding: ignore it, use the mock only to *see* and *click through* the intended behaviour.

> RmlUi capability notes below are written from general knowledge of RmlUi and **must be verified against the RmlUi version pinned in the repo** (log the result in `migration/RESOLUTIONS.md`, OQ-13). Where a capability is missing, use the stated fallback and record a deviation.

## 1. Responsibility split
| Concern | Lives in | Notes |
|---|---|---|
| Static structure (cards, rows, buttons, labels, section order) | **RML** documents/templates | One document per top-level surface; reusable pieces as templates |
| Look: colours, sizes, borders, radii, shadows, typography, hover/focus/disabled styling, transitions | **RCSS** | One flat stylesheet per theme (see §3) |
| State (selected shape, tool, plane, layer, toggles, layout mode, …) | **C++ controllers** owning a data model | Exposed to markup via RmlUi **data bindings** if available (preferred), otherwise by C++ setting classes/attributes/inner text |
| Interaction (clicks, drags, keys, modifiers, hover previews, Esc ladder) | **C++ event listeners** | Spec tables "Trigger → Effect" are the behavioural contract |
| Model mutations (flip, rotate, fill, scale, reorder, …) | Existing **BurrTools model/commands** (undo-aware) | Never reimplemented in the UI layer |
| 3D scene (voxels, axes, bounds, slab, ghosts, gizmo) | Existing **GL renderer** + C++ overlay drawing | The viewport is a placeholder element; see §5 |
| 2D grid drawing | **Custom element** (C++) or a DOM of cells | See §5 |
| Cursors, tooltips, popups, modal dialogs | C++ (system interface / layer management) | See §6 |
| Automation/test hooks | Element `id`s + C++ input injection | See `migration/02-test-plan.md` |

## 2. The contract between markup and logic
`overview/06-ui-contract.md` lists, for every component: **element ids**, **data-model variables**, **events** (with the C++ responsibility), and **state classes**. Rules:
* A visual state is expressed as **(a)** an RCSS pseudo-class (`:hover`, `:active`, `:focus`, `:disabled`, `:checked` where applicable) or **(b)** a state class set by C++ / bound via data model (`selected`, `on`, `open`, `disabled`, `override`, …). Never encode state only in the colour of an inline style.
* Markup never contains logic. Event wiring is done in C++ by id/class (or `data-event-*` if the bindings are used).
* Every focusable/clickable element has a stable `id` from the component specs and a `title`/tooltip attribute with the exact tooltip text.

## 3. Theming without variables
Do not assume RCSS supports CSS custom properties. Generate **two flat stylesheets** — `theme-light.rcss`, `theme-dark.rcss` — from `foundations/design-tokens.json` at build time (a small generator script, not a runtime feature), or keep one `base.rcss` for geometry plus one `colors-<theme>.rcss` for colours only. Theme switch = swap the colour sheet (and reload documents) — see C12 Appearance. Geometry (sizes, radii, padding) is theme-independent and uses `dp`.

## 4. Units, layout, styling features
* Units: **`dp`** everywhere (density-independent px; ×1.6 on the canonical 2560×1600 display via the context's dp ratio). No physical `px` except hairlines (1 dp borders).
* Layout: use **flexbox** (`display:flex`, `flex-direction`, `flex:1`, fixed `width/height`); avoid CSS grid (not available in RmlUi). Where the spec says "grid of N columns" (tool strip, transform matrix, surface table, scale buttons) build it from flex rows with equal-width children, or from `display:block`/`inline-block` children with computed widths.
* Round corners, borders, `box-shadow`, opacity, `transition` (e.g. `width 0.2s`), `@keyframes` are used by the design and are available in RmlUi (verify `box-shadow` blur support and `transition` on `width`).
* Gradients/hatch: the *variable-voxel hatch* and the logo gradient SHOULD be implemented with an image/tile or gradient **decorator**; fall back to a pre-rendered tileable bitmap (provide a 12×12 dp tile at 1×/2×).
* Text: system UI font stack from the tokens; supply one bundled font file (e.g. Segoe-equivalent open font) if the platform font isn't available to RmlUi's font engine. Tabular numerals desirable.
* Overflow/scroll: lists and the inspector use `overflow-y:auto`.
* `:hover` on a parent revealing children (shape-row actions) is done with `.row:hover .actions {display:flex}` (RCSS descendant selectors) or with a C++-managed `hovered` class.

## 5. Special surfaces
1. **3D viewport (C06).** Reserve an element `entities.viewport.surface` filling the centre card. The existing GL renderer draws the scene into that element's absolute rect *before* RmlUi renders the overlay layer; the toolbar, hint, view tag and Display menu are normal RML elements positioned over it. Pointer events over the surface (not over toolbar/hint) go to the 3D interaction controller (navigation + 3D editing). Scene overlays — **active layer slab, dim-other-layers, ghost previews** — and the **view cube (C13, including its hover regions, arrows and animation)** are drawn/handled by C++ (GL) using the colours in the tokens, not by RML.
2. **2D grid (C08).** Preferred: a custom RmlUi `Element` subclass (`<voxel-grid>`) that renders cells with the RenderInterface (one geometry batch) and handles mouse input itself (hover cell, press, drag-stroke, modifiers). Acceptable fallback: a DOM of `div` cells (≤ 32×32 = 1 024) with classes `fixed`, `variable`, `hovered`; verify the performance budget in the test plan. Rulers, axis lines and the layer strip are ordinary RML.
3. **Layer strip, palette, shape list** — ordinary RML lists generated/updated by C++ (`data-for` loops if bindings are available).
4. **Numeric stepper input** — RML `<input>` (text/number) styled by RCSS; commit on Enter/blur handled in C++.
5. **Drag-and-drop reorder (C01)** — implemented in C++ from mouse events (pointer capture on the grip/row, drop indicator element positioned by C++); no HTML5 DnD is assumed.

## 6. Cross-cutting runtime pieces (C++)
* **Popup menus / context menus (PR-11):** a top-level "popup layer" document (or an absolutely positioned element at the end of the main document with the highest z-order) shown/hidden by C++; positioned from the anchor/pointer rect, flipped/clamped inside the window; closes on outside press, Esc, or item activation.
* **Modal dialog (PR-12):** a separate document shown modally (scrim element + dialog); input outside is blocked; focus is trapped.
* **Tooltips (PR-14):** C++ shows a small tooltip element after 500 ms of hover, using the element's `title` (or `data-tooltip`) text; respects Settings ▸ Show tooltips.
* **Cursors:** the four edit cursors are **composed at runtime** (Fixed/Variable: OS default pointer + upper-right badge; Erase/Paint: the tool icon alone — colour-aware) and installed through the SystemInterface; the rule and assets are in `foundations/glyph-catalog.md` ▸ Cursors and `foundations/cursors/`. Static RCSS `cursor:` values cover only the OS defaults (`default`, `pointer`, `grab`, `grabbing`, `move`, `text`). The controllers (not RCSS) choose the edit cursor — see the cursor decision table in `overview/06-ui-contract.md` §4.
* **Status flash (PR-13):** C++ timer (2400 ms).
* **Animation:** column-width transitions via RCSS `transition`; the renderer/grid resize after the transition ends (and optionally each frame).
* **Input bridge:** keyboard, mouse buttons (left, middle, right), wheel and modifier states (Shift, Alt, Ctrl, Meta/⌘) are forwarded from the host window to the RmlUi Context; the interaction controllers read modifier state on every pointer event *and* on modifier key press/release (needed for the quick-tool cursor/ghost).
* **Assets:** glyph and icon SVGs (`foundations/glyphs`, `foundations/ui-icons`) are loaded with RmlUi's SVG support if available; otherwise pre-render to bitmaps (1×, 1.5×, 2×) at build time and reference them as `<img>`/decorators. Theme variants of glyphs are separate files (`glyphs/light`, `glyphs/dark`); monochrome UI icons are tinted (use mask/colour-tinted decorator, or ship per-state bitmaps).

## 7. Indicative RML skeletons (structure + ids + classes only)
```html
<body class="app theme-light">
  <header id="topbar">…</header>
  <main id="workspace">
    <section id="card-shapes" class="card"> … </section>
    <section id="card-viewport" class="card">
      <div id="entities.viewport.surface"></div>
      <div id="entities.viewport.tag"></div>
      <nav id="entities.viewport.toolbar"> … </nav>
      <div id="entities.viewport.hint"></div>
    </section>
    <section id="card-editor" class="card"> … </section>
  </main>
  <footer id="statusbar"><span id="status.text"></span><span id="status.cursor"></span></footer>
  <div id="popup-layer"></div>
</body>
```
```html
<!-- glyph button (toggle): C++ toggles class "on"; "disabled" via attribute -->
<button id="entities.editor.tool.fixed" class="gbtn on" title="Place a fixed (solid) voxel (1)">
  <img class="glyph" src="glyphs/light/tool-fixed.svg"/><span class="cap">Fixed</span><i class="kb">1</i>
</button>
```
```html
<!-- shape row: classes "selected", "hovered", "drop-target"; actions revealed on :hover or .selected -->
<div id="entities.shapes.row.1" class="row selected">
  <span class="grip"></span><span class="chip">S2</span><span class="label">Arm piece</span>
  <span class="wbadge">W2</span>
  <span class="actions"><button class="ib rename"/><button class="ib duplicate"/><button class="tbtn weightDown">W−</button><button class="tbtn weightUp">W+</button><button class="ib delete"/></span>
</div>
```
(Element `id`s with dots are for documentation clarity; if the RmlUi build disallows dots in ids, use `-` or `_` consistently and update the test hooks — the *scheme*, not the punctuation, is the contract.)

## 8. What the implementing agent must produce
1. RML documents/templates per component, one RCSS base sheet + two colour sheets.
2. C++ controllers: `ShapesController`, `GridSizeController`, `InspectorController` (transform/fit/repair), `ViewportController` (navigation, 3D editing, hover preview, display options), `EditorController` (tool, mirror/span, plane/layer, grid input, colour), `LayoutController` (collapse/focus), `SettingsController`, `StatusController`, all talking to the BurrTools model through its existing command layer and publishing UI state per `overview/03` and `overview/06`.
3. Custom elements (`voxel-grid`; optionally `viewport-surface`).
4. Cursor and glyph asset loading; tooltip/popup/dialog infrastructure.
5. Tests per `migration/02-test-plan.md`.


---
<!-- FILE: foundations/design-tokens.md -->

# Design tokens

Machine-readable copy: `design-tokens.json`. All sizes in **dp** (× the dp ratio; 1.6 at 2560×1600). RCSS is not assumed to support custom properties: generate flat per-theme stylesheets from this table (`rmlui-implementation-guide.md §3`).

## 1. Colour tokens
| Token | Light | Dark | Used for |
|---|---|---|---|
| `outer` | `#dde1e8` | `#08090b` | area outside window content (mock only) |
| `bg` | `#eceff4` | `#111318` | workspace background between cards |
| `panel` | `#ffffff` | `#1a1d24` | cards, top/status bars, dialogs, popups, inputs |
| `panel2` | `#f3f5f8` | `#222630` | secondary surface: button fill, segmented track, grid area bg, hover fill |
| `line` | `#dde1e8` | `#2b303b` | 1 dp dividers, card borders, button borders |
| `line2` | `#c6ccd7` | `#3a4150` | stronger borders (inputs, steppers, grid lines, dashed guides) |
| `text` | `#1a1f29` | `#e7eaf0` | primary text |
| `muted` | `#667085` | `#8d96a6` | secondary text, captions, inactive glyph strokes |
| `accent` | `#2f6df6` | `#5b9bff` | selection, focus ring, primary button, active states |
| `accentSoft` | `rgba(47,109,246,.12)` | `rgba(91,155,255,.16)` | selected/hover fill |
| `canvas` | `#e4e8ef` | `#0f1115` | 3D viewport background |
| `axisX` | `#e5484d` | `#ff6b70` | X axis (red) |
| `axisY` | `#2fa85a` | `#4bd17f` | Y axis (green) |
| `axisZ` | `#2f6df6` | `#5b9bff` | Z axis (blue) |
| `danger` | `#d92d20` | `#ff7066` | destructive text, delete ghost, invalid placement |
| `scrim` | `rgba(10,12,16,.5)` | same | modal backdrop |

### Fixed (theme-independent) palette
| Token | Value | Use |
|---|---|---|
| glyph `fixedRed` | `#ff3b3b` | fixed voxel glyphs (legacy red pencil) |
| glyph `variableLime` | `#8be000` | variable/optional voxel glyphs (legacy green outline) |
| glyph `gold` | `#ffc400` | colour/scale/grid glyph blocks |
| glyph `eraserPink` / stroke | `#ff9ec4` / `#b04f7c` | erase glyph |
| glyph axis bars | X `#e0245e`, Y `#2fb463`, Z `#1aa7d6` | mirror/span/flip/rotate/nudge bars (legacy pink/green/cyan) |
| Shape colours | the app's existing per-shape colours (legacy S1 blue, S2 green, S3 red, S4 cyan …) | shape chips, *piece colour* view |
| Chip text | `#ffffff` + 1 dp text-shadow `rgba(0,0,0,.25)` | |

## 2. Typography
Font stack: `-apple-system, "Segoe UI", Inter, Roboto, "Helvetica Neue", Arial, sans-serif` (Windows → **Segoe UI**). Numbers use **tabular figures** where the toolkit allows. Mono (key badges): `ui-monospace, Menlo, Consolas`.
| Role | Size / weight |
|---|---|
| Dialog title | 15 / 700 |
| Body, list rows, menu items, inputs | 13 / 400 (600 for emphasised values) |
| Card & section titles | 13 / 600 |
| Action-button title | 12.5 / 600 |
| Secondary text, status bar, sums, hints | 12 / 400 |
| Micro labels (UPPERCASE, letter-spacing .4–.5 dp) | 11 / 600 |
| Glyph-button captions | 10.5 / 600 |
| Key badge on tools | 9.5 / 700 |
| `kbd` chips | 11 mono |
Line height 1.35.

## 3. Spacing, radii, elevation
* Spacing scale (dp): 2, 4, 6, 8, 10, 12, 14, 16. Card gap/padding 8. Section body padding `2 14 14`. Row/list padding 6.
* Radii: 6 (chips, small buttons, menu items, layer chips 7), 8 (buttons, steppers, inputs), 10 (popups, big swatch), 12 (cards, toolbar), 14 (dialog).
* Elevation: card `0 1 2 rgba(16,24,40,.06), 0 4 14 rgba(16,24,40,.06)` (dark: `0 1 2 rgba(0,0,0,.4), 0 4 14 rgba(0,0,0,.35)`); popup `0 10 30 rgba(0,0,0,.25)`; dialog `0 24 60 rgba(0,0,0,.4)`.

## 4. Control sizes (dp)
| Control | Size |
|---|---|
| Icon button | 28 (toolbar 32, row action 24, row action text `W±` 30×24) |
| Glyph button (`gbtn`) | tool strip h48 · mirror/span h44 · plane 54×46 · transform h42 · surface h46 · scale h46; glyph 26–32 (Minimal: `density.md`) |
| Action button (`abtn`) | min-h 40, padding 3 8, gap 10, glyph 30 (Minimal: h32, glyph 24 + short label) |
| 3D toolbar button | 48×44 (Minimal 34×32 icon only), separator 1×28 |
| Stepper | h28, axis label 18, ± buttons 22×26 |
| Segmented | h22 buttons, track padding 2 |
| Dropdown | h30, min-w 120 |
| Switch | 32×18, knob 14 |
| Layer chip | 32×24; strip width 48 (Minimal 30×22 / 40) |
| Grid-size row | padding 8 12 10; stepper h30 |
| Shape row | h42; chip min-w 34 × h26; grip 14 |
| Section header | h40 |
| Big colour swatch | 40×40; palette swatch 26×26 |
| Popup item | h30 |

## 5. Interaction-state styling (all controls)
| State | Style |
|---|---|
| Hover (icon button) | fill `panel2`, icon `text` |
| Hover (glyph/action button) | border `accent`, fill `accentSoft`, text `text` |
| Selected / on | fill `accentSoft`, border `accent` 1 dp + inset `accent` 1 dp, label `text` |
| Pressed (spec-only) | fill darken 6 % |
| Focus | 2 dp `accent` outline, offset 1 |
| Disabled | 40 % opacity, no hover, `not-allowed`-style no feedback; tooltip still shows *why* |
| Drag-over target | 2 dp `accent` line on the top edge of the row |
| Destructive menu item | text `danger` |

## 6. Motion
Column width transitions 200 ms ease; accordion chevron rotate 150 ms; switch knob 150 ms; status flash visible **2400 ms**; tooltip delay 500 ms (gated by Settings ▸ Show tooltips). Respect OS "reduce motion" by setting durations to 0.

## 7. Tooltip style
`panel` fill, 1 dp `line2` border, radius 6, padding 4×8, 12 dp text, max width 280. Text formula: *Effect sentence* + optional *(shortcut)*. Exact strings are given in component specs.

## Density
All sizes above are **Standard** density. The complete Standard ↔ Minimal geometry table (bars, rails, card widths, rows, fixed blocks) is `foundations/density.md` §1 and is the authority on conflicts.


---
<!-- FILE: foundations/primitives.md -->

> **Density (Rev 6.0):** dimensions here are **Standard** density values; Minimal values and any later corrections are in `foundations/density.md`, which wins on conflicts.

# UI primitives (Phase P0)

Each primitive is an **RML element/template + RCSS class set + C++ behaviour**. All expose: element `id`, tooltip text (`title`), and the state classes/pseudo-classes listed. Visual states follow `design-tokens.md §5`. How they are built in RmlUi: `rmlui-implementation-guide.md`.

## PR-01 IconButton
28×28 (toolbar 32, row action 24). Monochrome icon (`ui-icons/*.svg`): `muted` → `text` on `:hover`, `accent` when class `on`. Variants: plain, **on** (toggle; accentSoft background), **danger-hover** (trash icon turns `danger` on hover). Tooltip mandatory.

## PR-02 GlyphButton ("gbtn") — glyph + caption
Vertical stack: glyph (26–32 dp) above a caption (10.5/600, `muted`; `text` on hover/selected), centred; 1 dp `line` border, `panel2` fill, radius 8. Optional **key badge** (top-right, 9.5/700, `muted`). Optional axis-coloured caption (Mirror/Span X/Y/Z). States: normal, `:hover`, **`on`**, `disabled`, `:focus`. Used by: tool strip, mirror/span, plane selector, transform matrix, surface table, ×2/×3.

## PR-03 ActionButton ("abtn") — glyph + title + hint
Horizontal: glyph (30) · text block (title 12.5/600 `text`; hint 11 `muted`, ≤ 2 lines). Min height 48, padding 4×10, radius 8, `panel2` fill. Disabled shows a hint explaining why (e.g. "Already at minimum resolution"). Used by: Prune, Center, To origin, Minimize (C03), Fill interior holes (C05).

## PR-04 Button
Height 30 (small 26), padding 0×12, radius 8, 1 dp `line2` border, `panel` fill. Variants: *secondary*, *primary* (accent fill, white text, hover +8 % brightness). Used by: New shape, Restore all defaults, Done, Reset section.

## PR-05 Segmented
Track `panel2`, 1 dp `line`, radius 8, padding 2; options h24, padding 0×12, 600 weight; selected = `panel` fill + subtle shadow + `text`; others `muted`. Exactly one `on`. Left/Right keys move selection when focused. Used by: app tabs (Entities/Puzzle/Solver), Settings rotation method and appearance.

## PR-06 Stepper
`[axis] [−] [value] [＋]` in a 30 dp-high 1 dp `line2` bordered radius-8 box. The value is an **editable numeric field**: click to edit, **Enter commits**, **Esc reverts**, ↑/↓ ±1, wheel ±1 while focused; clamp to min/max; invalid text reverts and flashes class `invalid`. Buttons repeat while held (400 ms delay, 80 ms interval). Axis letter coloured with the axis token. Used by Grid size (C02).

## PR-07 Switch
32×18 track, 14 dp knob. Off: `line2` track; on (class `on`): `accent` track, knob right, 150 ms. The whole row label is clickable; `Space` toggles; role "switch".

## PR-08 Chip
Shape chip: min-w 34 (list) / 28 (header), h26, radius 6, fill = shape colour, white bold 11 text `S#`, 1 dp text-shadow. Count badge: h20 `muted` fill. Weight badge: `W{n}` 11/700 `muted` on `panel2`, 1 dp `line`, radius 5, padding 0×5.

## PR-09 AccordionSection
Header h40: chevron (rotated −90° when `closed`) · title 13/600 · right-aligned summary text 12 `muted`. Click or Enter/Space toggles; body padding `2 14 14`. Open/closed persists. Sections separated by 1 dp `line`.

## PR-10 Card
Rounded container (radius 8, border, elevation; Minimal: flush, radius 0) with optional header (h36; Minimal h30). Content clipped.

## PR-11 PopupMenu (popover + context menu)
`panel` fill, 1 dp `line2`, radius 10, padding 6, popup shadow, min-w 190. Items h30, padding 0×10, radius 6, `:hover` `panel2`; optional left check/radio column (14 dp ✓); optional right-aligned hint/shortcut (11 `muted`); section headers 11/600 uppercase `muted`; separators 1 dp `line`; `disabled` items 40 %. Opens below the anchor (or at the pointer for context menus), flips above if there is no room, clamps inside the window. Closes on outside press, Esc, or after activating an item — except items flagged **keep-open** (check/radio rows of the Display menu). Arrow keys navigate, Enter activates. Hosted in the popup layer (guide §6).

## PR-12 Dialog (modal)
Centred on a scrim, radius 14, dialog shadow; layout per C12. Esc and scrim click close; focus trapped; initial focus on the first interactive element (search box). Hosted as a modal document.

## PR-13 StatusFlash
Temporarily replaces the status-bar text for 2400 ms (C++ timer), then the live text is recomputed.

## PR-14 Tooltip
Per `design-tokens.md §7`; shown by C++ after 500 ms hover from the element's `title`; suppressed when Settings ▸ Show tooltips is off.

## PR-15 KeyBadge (`kbd`)
11 mono, 1 dp `line2` border (bottom 2 dp), radius 4, `panel2` fill, padding 1×4.

## PR-16 Check / radio menu rows
Rows inside PopupMenu use the 14 dp check column (✓). No standalone checkbox is needed.

## PR-17 GlyphRenderer (infrastructure)
Loads glyph/icon assets by name for the current theme and device scale (SVG support if the pinned RmlUi has it; otherwise pre-rendered bitmaps at 1×/1.5×/2×). Glyph theme variants live in `glyphs/light` and `glyphs/dark`; monochrome UI icons are tinted per state.

## PR-18 Element ids (test/automation hooks)
Stable element ids from the component specs. Tests locate elements by id, inject input events through the RmlUi Context (mouse move/press, key down/up with modifiers, wheel) and assert on element classes, text, visibility and bounds.

## PR-19 Dropdown
h30, min-w 120, radius 8, 1 dp `line2`, `panel` fill, value text 13, chevron at the right; opens a PopupMenu (PR-11) listing the options with the current one checked. ↑/↓ change value when focused (Space/Enter opens). Used by Settings ▸ Undo history depth (25 · 50 · 100 · 200 · 500).

## PR-20 Cursor set
Four tool cursors (`edit-fixed|variable|erase|paint`): **OS default pointer + a small square badge at its upper right** for Fixed (coloured square) and Variable (dotted square); **the tool icon alone (no pointer)** for Erase (eraser) and Paint (bucket + coloured drop; hotspot at glyph centre). Composed at runtime from the tool in effect, the drawing colour (Default → theme contrast), the theme and the OS cursor scale. Rule: `glyph-catalog.md` ▸ Cursors.

## PR-21 Workspace rail button (C15)
52 × 50 dp, radius 10, 22 dp icon over a 10.5/600 caption (Minimal: 36 wide, caption rotated 90°); states rest / hover (`panel2`) / selected (`accentSoft` + `accent` + 3 dp left indicator bar) / focus ring.

## PR-22 Piece tile (C16, "In this puzzle")
**Fixed 96 × 50 dp** (Minimal 78 × 46), radius 8, 1 dp `line` border on `panel2`, 6 dp shape-colour stripe at the left; row 1 (18 dp): grip dots, **id** (700), label (muted 10.5, ellipsis), optional **G-badge** (accent, 9.5/700, radius 5); row 2 (22 dp): 22 × 22 `−` button · count text (700 12, min-w 34, centred) · 22 × 22 `+` button. Selected = accent border + 1 dp accent ring + `accentSoft` fill. **No hover resizing; controls are always visible.** Draggable (`grab`).

## PR-23 Library chip (C16, "All pieces")
28 dp high, radius 8, 1 dp `line` on `panel2`; grip dots · 10 dp colour square (radius 3) · id (700) · label (muted 10.5, max 60 dp) · **fixed 34 dp action button** (reads "+" or "×N +", width constant). Hover = accent border + `accentSoft` fill (no size change); selected = accent ring; used pieces use the `panel` fill.

## PR-24 Inline-label stepper (C17)
A stepper box (− field +) whose **label (e.g. "Min", "Max") sits inside the box** between the minus button and the value; the value is right-aligned and the field has no inner border. Used for the range editor.

## PR-25 Summary strip (C16)
Full-width row at the bottom of a card: 18 dp status icon + bold line + muted detail line; `ok` (green icon) / `warn` (amber icon, amber title); 1 dp top border; padding 10 × 14.

## PR-26 Matrix toggle (C17)
34 × 28 dp, radius 7, 1 dp `line2`; ticked = `accent` fill with a white check; row/column headers are 24 dp colour swatches (radius 7, 1 dp dark hairline).

## PR-27 Radio card (C19)
Full-width card (radius 10, 1 dp `line`, padding 9 × 10): 16 dp radio dot (2 dp ring; selected = accent ring + inner dot) · bold title · muted description (12 dp, line-height 1.35). Selected = accent border + 1 dp accent ring + `accentSoft` fill.

## PR-28 Solution list row (C20)
Grid row **32 dp** (radius 8, 1 dp transparent border): columns `44 | 1fr | 96 | 40 | 30`; hover `panel2`; selected = accent border + `accentSoft`; `will-delete` = red-tinted fill + border with struck-through text. **Moves** cell carries a proportional bar (`accentSoft`, 20 dp high, behind the number). Header row (sticky, 11/600 uppercase `muted`): sort buttons with an **inline** ▲/▼; the active column uses `accent`.

## PR-29 Disassembly player (C20)
Fixed **112 dp** block (Minimal 84): title row (label left, **move counter right**), transport row (five 30 dp buttons, primary play, speed select right), full-width range slider. Alternate states (message ± button) occupy the same height.

## PR-30 Piece-display chip (C20)
30 dp chip, 1.5 dp border, radius 8, 16 dp swatch in the piece colour. **solid** = filled swatch; **wireframe** = dotted border in the piece colour + hollow dotted swatch; **hidden** = 42 % opacity, greyscale, hollow dashed swatch. No state words in the chip.

## PR-31 Docked drawer (C21)
Panel docked over the bottom of a viewport: 12 dp inset, radius 14, 1 dp `line`, shadow; header (segmented tabs, caption, close) + two-pane body (list 230 dp / controls).

## PR-32 Confirm dialog (C20)
Centred modal over the window (scrim 38 % dark), 400 dp wide, radius 14: title (15/600), muted message, right-aligned **Cancel** and a **danger-coloured** confirm button.


---
<!-- FILE: foundations/glyph-catalog.md -->

# Glyph & icon catalog

Two asset families:
* **Glyphs** (`glyphs/light|dark/*.svg`) — *coloured, semantic* pictograms inherited from the legacy icon vocabulary. viewBox 32×32, exported at 64. Theme variants differ only in axis/accent/muted strokes. Always shown **with a caption** (glyph button) or a title+hint (action button).
* **UI icons** (`ui-icons/*.svg`) — *monochrome* 24×24 outline icons, `stroke=currentColor`, 1.8 dp stroke, round caps/joins; tinted with `muted`/`text`/`accent`.
Visual index: `glyph-contact-sheet-light.png`, `glyph-contact-sheet-dark.png`. Cursors: `cursors/` (see below).

## Visual vocabulary (keep consistent when adding glyphs)
* **Axis frame**: blue vertical = Z, red horizontal = X, green diagonal = Y (origin bottom-left). Appears in every axis-related glyph.
* **Axis bars/cubes**: X pink-red `#e0245e`, Y green `#2fb463`, Z cyan `#1aa7d6`; small 3-face shaded cubes (front, lighter top, darker side).
* **Ghost**: dashed outline cube (dash 2/2, 55–60 % opacity) = "where it was / will be".
* **Dashed line** = mirror plane.
* **Fixed voxel** = solid red `#ff3b3b` square (+ pencil). **Variable/optional voxel** = lime outline square `#8be000` (+ pencil). **Colour** = gold `#ffc400` (+ brush). **Erase** = pink eraser.
* **Gold blocks** = the piece in grid/scale operations; blue arrows/crosshair = grid operations.

## Glyph inventory (41 + 4 edit-tool glyphs)
| File (without `.svg`) | Meaning | Used in | Caption | Legacy source |
|---|---|---|---|---|
| `tool-fixed` | Place fixed (solid) voxel | C07 tool strip | Fixed | L60 icon 1 (red pencil) |
| `tool-variable` | Place variable (optional) voxel | C07 | Variable | L60 icon 2 (green-outline pencil) |
| `tool-erase` | Erase voxel | C07 | Erase | L60 icon 3 (eraser) |
| `tool-paint` | Apply current colour to existing voxel | C07 | Paint | L60 icon 4 (brush) |
| `mirror-x` `mirror-y` `mirror-z` | Also place mirrored voxel across that axis' centre plane | C07 Mirror row | X / Y / Z | L62 icons 1–3 (paired cubes along axis) |
| `span-x` `span-y` `span-z` | Fill the whole grid along that axis per click | C07 Span row | X / Y / Z | L63 icons 1–3 (long bars) |
| `plane-xy` `plane-xz` `plane-yz` | Editing plane (shaded plane in the axis frame) | C08 | XY / XZ / YZ | new |
| `flip-x` `flip-y` `flip-z` | Mirror the piece across that axis' plane (solid cube + ghost across dashed plane) | C04 | Flip | L40 (one button per axis) |
| `rotate-{x,y,z}-neg` / `-pos` | Rotate ∓/±90° about axis (bar with arc arrow) | C04 | −90° / +90° | L42 |
| `nudge-{x,y,z}-neg` / `-pos` | Move piece −1/+1 along axis (solid + ghost destination) | C04 | −1 / +1 | L41 |
| `surface-fixed-inner` `-outer` | Make inner / outer surface voxels fixed (red blocks + pencil) | C05 | Inner / Outer | L50 pair |
| `surface-variable-inner` `-outer` | … variable (lime outlines + pencil) | C05 | Inner / Outer | L51 pair |
| `surface-clear-inner` `-outer` | … clear colour (gold blocks + brush) | C05 | Inner / Outer | L52 pair |
| `fill-holes` | Fill interior holes (red ring, gold filled centre) | C05 | (title) Fill interior holes | L53 (text-only legacy) |
| `prune-grid` | Shrink grid to piece (blue arrows in) | C03 | (title) Prune grid to fit piece | L34 |
| `center-on-grid` | Centre piece on grid (crosshair) | C03 | (title) Center | L35 |
| `move-to-origin` | Move piece to origin (cube at axis origin) | C03 | (title) To origin | L36 |
| `minimize` | Reduce resolution keeping shape (grid → small block) | C03 | (title) Minimize | L37 |
| `multiply-2` `multiply-3` | Multiply voxels ×2 / ×3 (block → n×n grid) | C03 | Double ×2 / Triple ×3 | L38 / L39 |

| `edit-tool-fixed` `-variable` | **3D Edit button** icon for Fixed / Variable: the default pointer arrow with the same square badge as the cursor (square / dotted square with a “+”, white inside for the Default colour) at its upper right | C06 Edit button | Edit (+ number badge) | new |
| `edit-tool-erase` `-paint` | **3D Edit button** icon for Erase / Paint: the **eraser** / **paint bucket with drop** alone, **no arrow** (same drawing as the cursor). Swapped in place with the tool in effect; the badge colour follows the drawing colour (Default → theme contrast) | C06 Edit button | Edit (+ number badge) | new |

Rendering notes: flip/nudge/mirror use slots along the axis (X: two cubes left/right; Y: two along the diagonal; Z: two stacked). Rotate draws an axis bar (along X, diagonal for Y, vertical for Z) with a 220° arc arrow; arrowhead flips for ±.

## UI icon inventory (`ui-icons/`)
| File | Used for |
|---|---|
| `orbit` `pan` | 3D navigation modes (toolbar) — there is no zoom icon (wheel zooms) |
| `ptplus` | 3D **Edit** mode toggle (pointer + plus) |
| `cube` | Standard **Views** |
| `fit` | **Fit** to view |
| `eye` | **Display** menu (Show / Projection / Voxel colour) |
| `focus` / `unfocus` | Enter / exit focus mode (3D toolbar, Voxel editor header) |
| `panelL` / `panelR` | Collapse/expand left / right sidebar |
| `plus` / `minus` | New shape, Grid-size stepper buttons, add colour, rail new |
| `edit` | Rename shape |
| `copy` | Duplicate shape |
| `trash` | Delete shape |
| `grip` | Drag handle |
| `gear` | Settings |
| `close` | Close dialog |
| `chevD` | Accordion chevron (rotated −90° when closed) |
| `check` | Menu check mark |
| `more` | Colour options (⋯) |
| `rail-entities` `rail-puzzle` `rail-solver` | **Workspace rail** icons (cube, puzzle piece, play) |
| `alert-triangle` / `check-circle` | Puzzle summary strip: pieces cannot / can fill the result |
| `grip-dots` | Drag affordance on piece tiles and chips (same glyph as `grip`, 12 dp) |
| `chevron-down` / `more-horizontal` | Library collapse chevron; piece menu (⋯) — aliases of `chevD` / `more` |
| `transport-skip-start` `-step-back` `-play` `-pause` `-step-forward` `-skip-end` | **Disassembly player** transport buttons (filled 16 dp glyphs) |
| `sun` | *(retired)* former mock-only theme toggle — the theme is now chosen in Settings ▸ General |

## Asset rules for the implementation
* Rasterise at `size_dp × UI_SCALE`; cache per theme. Do not stretch bitmaps.
* Glyph button glyph sizes: tool strip 30, mirror/span 28, plane 30, transform 28 (mock uses 26–28), surface 30, scale 32, action buttons 30.
* Tint rules: UI icons use `muted` (normal), `text` (hover/selected), `accent` (toolbar `on`); glyph `currentColor` strokes use `muted`; disabled = 40 % opacity.
* If the toolkit cannot render SVG, pre-render PNGs at 1×/1.5×/2× from the provided SVGs during the build and load by scale.

## Cursors (`foundations/cursors/`)
**Cursor composition rule.** Two families, chosen by the tool in effect:
* **Fixed (1) and Variable (2)** — the **regular OS default pointer** with a **small square badge snug against its upper right** (the badge's left edge at the pointer's width ≈ 12 px at 100 % scale, top aligned with the pointer's top), carrying a small **“+” in the square's outline colour**. Hotspot = the pointer's own.
* **Erase (3) and Paint (4)** — **no pointer**: the tool icon alone is the cursor (≈ 18 px glyph centred on a 32 px canvas at 100 % scale). **Hotspot = the centre of the glyph** (16,16 at 32 px).
| Tool in effect | Drawing | Colour rule |
|---|---|---|
| **Fixed** | Rounded square (12×12 px, radius 2, 1 px outline) with a **“+”** (1.4 px strokes, 6 px) inside | **Inside = the current drawing colour.** If the drawing colour is **Default**, the inside is **white in the light theme** (near-black `#14171c` in the dark theme) with a theme-ink outline (`#1a1f29` light / `#f2f4f8` dark). The **“+” is drawn in the same colour as the square's outline in both themes** (theme ink: `#1a1f29` light / `#f2f4f8` dark); outline @ 55 % when a colour is selected |
| **Variable** | The same square with a **dotted boundary** (1.6 px stroke, dash 1.9/1.6) and a “+” inside | Default: white inside (near-black in dark theme) with a dotted theme-ink boundary and a theme-ink “+”. Specific colour: dotted boundary **and “+” in that colour**, the colour inside at 32 % |
| **Erase** | Sleek **eraser** (1.3 px outline with a short bevel line) | **Monochrome**: theme-ink outline, body filled with the theme's opposite tone (white light / `#14171c` dark) |
| **Paint** | Photoshop-style **paint bucket** (tipped bucket outline, handle arc) with a **drop** at its lower right | Bucket monochrome like the eraser; the **drop is filled with the drawing colour**; Default → monochrome (theme ink) drop |
Assets: `badge-fixed.template.svg`, `badge-variable.template.svg` (12×12; placeholders `%COLOUR%` — omit/transparent for Default —, `%INK%`, `%OPP%`); `icon-cursor-erase.template.svg`, `icon-cursor-paint.template.svg` (32×32 icon cursors; `%COLOUR%`, `%INK%`, `%OPP%`); `arrow-fallback.svg`; rendered examples `cursor-edit-<tool>-<theme>-<default|example-red>.svg|png` (32/48/64 px); `cursor-sheet.png` (all of them over six voxel colours).
**Runtime composition (C++):** the colour-dependent parts mean the cursor bitmap is **built at runtime**. Fixed/Variable: obtain the OS default arrow (platform API; fall back to `arrow-fallback.svg` — classic black arrow with white edge, hotspot (1,1) at 32 px), draw the badge at its upper right, install with the arrow's hotspot. Erase/Paint: render the icon from the templates, hotspot at the glyph centre. Re-compose when the tool in effect, the drawing colour, the app theme or the OS cursor scale changes; cache per combination. Scale follows the **OS cursor size**, not the UI dp ratio (reference canvas 32 px at 100 %, 48 px at 150 %, 64 px at 200 %).


---
<!-- FILE: components/C00-app-shell-menu-tabs-settings-entry.md -->

# C00 — App shell: top bar, menu, app tabs, settings entry

**Phase:** P1. **Images:** `01-main-light.png`, `00-new-annotated.png` (N01–N03). **Legacy:** L01 menu bar, L02 tabs (`legacy-tools-tab-annotated.png`).

## Purpose
Restyle the application chrome shared by all tabs. Content of the Puzzle and Solver tabs is **not** changed.

## Anatomy (height 36 dp — 32 in Minimal, logo without wordmark — `panel` fill, 1 dp bottom `line`)
Left → right: **Brand** (18×18 rounded-5 gradient logo, "BurrTools" 13/700) · **Menu buttons** (`File`, `Toggle 3D`, `Export`, `Status`, `Edit comment`, `Help`, `About`) · *(centred, absolute)* **App tabs** segmented `Entities | Puzzle | Solver` · right: **file name** (12 dp, ellipsis ≤ 330 dp, tooltip = full path) · **Settings** gear icon button · *(mock-only: "Show zones" button, theme toggle — do not implement)*.

| Widget id | Element |
|---|---|
| `shell.menu.<name>` | each menu button (`file`,`toggle3d`,`export`,`status`,`editcomment`,`help`,`about`) |
| `shell.tabs` / `shell.tabs.entities` `.puzzle` `.solver` | segmented tabs |
| `shell.filename` | file name label |
| `shell.settings` | gear button |

## Legacy → new
| Legacy | New |
|---|---|
| Menu: File, Toggle 3D, Export, Status, Edit Comment, Help, About | Same entries, same order, same commands, new look (menu buttons h28, padding 0×9, radius 6, hover `panel2`). Labels use sentence case ("Edit comment"). `[OQ-10]` |
| Menu: **Config** | **Removed** from the menu. Replaced by the gear button and `Ctrl+,` opening the Settings dialog (C12). |
| Tabs (Entities, Puzzle, Solver) | Centred segmented control, same switching behaviour |
| Window title shows full path | OS title bar unchanged; the top bar additionally shows file name (path in tooltip) |

## Interactions
| Trigger | Effect |
|---|---|
| Click menu button | Opens the **existing** legacy drop-down/command, unchanged (restyling the drop-down popups is optional and uses PR-11 if done) |
| Click tab | Switch tab as before; selected tab = `panel` fill, `text` colour; other tabs `muted` |
| Click gear / `Ctrl+,` | Open Settings dialog (C12) |
| Hover gear | tooltip "Settings (Ctrl+,)" |
| Hover file name | tooltip with full path |

## Rules
* Menu items that are checkable in legacy keep their check state semantics.
* Tab order: menu buttons → tabs → gear.
* The bar is identical on all tabs (shell-level), only the highlighted tab changes.

## Acceptance criteria
* AC-C00-01 Every legacy menu entry except Config exists and fires the same command.
* AC-C00-02 Gear and `Ctrl+,` open Settings; `Esc` closes it.
* AC-C00-03 Tabs switch; Entities content shows the new three-card layout when the v2 flag is on.
* AC-C00-04 Light/dark switch recolours the bar from tokens with no residual legacy colours.
* AC-C00-05 At 1280 dp width the file name truncates with ellipsis and nothing overlaps the centred tabs.

## File ▸ New and the voxel type
File ▸ New includes the one-time **voxel type** choice (Brick preselected; the five official types listed in C14); the type is fixed for the file's life — see **C14**.

## Workspace switching (Rev 4.0)
The **Entities / Puzzle / Solver switch moves from the top bar to the vertical workspace rail (C15)**. The top bar keeps the menu (File, Toggle 3D, Export, Status, Edit comment, Config, Help, About), the file name and the global buttons (Settings, theme).


---
<!-- FILE: components/C01-shapes-sidebar.md -->

# C01 — Shapes sidebar (list, row actions, weight, reorder, context menu, rail)

**Phase:** P2. **Images:** `01-main-light.png`, `21-shape-row-hover.png`, `20-shape-row-context-menu.png`, `05-left-collapsed.png`. **Legacy:** L10 New, L11 Delete, L12 Copy, L13 Label, L14 W+, L15 W−, L16 ←, L17 →, L18 shape list. **New ids:** N10–N15.

## Purpose
Select, create and manage the puzzle's shapes (pieces). Every action that used to live in a separate button bar now sits **on the shape row** (hover/selected) or in the row's context menu.

## Anatomy
**Card header** (h44): title **"Shapes"** · count badge (`muted` chip, h20) · **`+ New`** primary button (h26) · collapse-left icon button (`panelL`).
**List** (padding 6, scrolls vertically; max height 250 dp then scroll; min 60): one **row** per shape, h42, radius 8, gap 8:

`[grip 14] [Chip S# 34×26] [label (muted, ellipsis, flex)] [weight badge Wn if n≠1] [row actions]`

* **Chip**: text `S#` and fill colour are both **derived from the shape's position in the list** (see *Position is identity* below).
* **Row actions** (visible on **hover** and on the **selected** row; 24 dp buttons): `Rename` (edit icon), `Duplicate` (copy icon), `W−`, `W+` (text buttons 30×24), `Delete` (trash, hover → `danger`).
* **Selected row**: fill `accentSoft` + 1 dp border `accent` @ 35 %.
* **Hover row**: fill `panel2`.
* **Grip** colour `line2`, `muted` on row hover; cursor grab.

**Collapsed rail** (48 dp wide, 40 in Minimal, see C11): expand button (`panelL`) · one chip per shape (selected has 2 dp `panel` + 4 dp `accent` ring; tooltip "S# label") · `+` new button at the bottom.

| Widget id | Element |
|---|---|
| `entities.shapes.header` / `.count` / `.new` / `.collapse` | header parts |
| `entities.shapes.list` | list container |
| `entities.shapes.row.<i>` (+ `.chip`, `.label`, `.weightBadge`, `.grip`) | row i (0-based) |
| `entities.shapes.row.<i>.rename` `.duplicate` `.weightDown` `.weightUp` `.delete` | row actions |
| `entities.shapes.rail`, `entities.shapes.rail.expand`, `.rail.chip.<i>`, `.rail.new` | rail |
| `entities.shapes.contextMenu` | context menu |

## Position is identity (IMPORTANT)
The shape id is its **order in the list**: the first shape is `S1`, the second `S2` … the Nth is `SN`. Ids are **not stored or editable**; they are recomputed from position whenever the list changes. The chip colour is likewise a function of the position (legacy: S1 blue, S2 green, S3 red, S4 cyan, then the app's palette cycles) `[OQ-16]`.
Consequences (all MUST hold):
* **Reordering renumbers**: moving a shape to another position changes its id (and chip colour) to match its new position; every shape whose position changed is renumbered. Label, weight and voxels travel with the shape. The status text, view tag, inspector head, context-menu header and rail chips all show the new id immediately.
* **Delete** renumbers everything after the removed shape (S4 becomes S3 …).
* **New** and **Duplicate** *append* to the end (new id `S{N+1}`), so they never renumber existing shapes. (Duplicate used to be modelled as "insert after source" — that is wrong under positional ids.)
* After any reorder or delete show a status flash: "Order changed — shapes renumbered S1…S{n}".
* References from other tabs (Puzzle: problems/result pieces; Solver) must follow the *shape object*, not the old id string — check how the legacy ←/→ and Delete keep those references consistent and keep exactly that behaviour `[OQ-16]`.

## Legacy → new
| Legacy | New | Change |
|---|---|---|
| New (L10) | Header `+ New` and rail `+` | Same command; selects the new shape |
| Delete (L11) | Row trash, context menu, `Del` | Per-row; disabled when only one shape exists |
| Copy (L12) | Row copy, context menu, `Ctrl+D` | Copy is **appended as the last shape** and selected |
| Label (L13) | Row pencil, context menu, `F2`, double-click | **Inline** rename (no dialog); `[OQ-4]` legacy may use a dialog — behaviour (set label) identical |
| W+ / W− (L14/L15) | Row `W+` / `W−`, context menu | Shape **weight**: in the **Solver's disassembly animation heavier shapes stay in place and lighter shapes move** (so a big piece such as a tray stays put while the pieces move, instead of everything flying back and forth). Badge `Wn` shown when n≠1 |
| ← / → (L16/L17) | Drag grip, context menu "Move earlier/later", `Alt+↑/↓` | Reorder changes shape order **and therefore ids** (persisted with puzzle) |
| Horizontal chip strip (L18) | Vertical list | Chip identical meaning |

## Interactions
| Trigger | Effect | State / undo |
|---|---|---|
| Click row | Select shape | `selectedShape`; no undo |
| `↑`/`↓` with list focused | Select previous/next | same |
| Double-click row, `F2`, Rename button, menu "Rename" | Row label becomes a text field (placeholder "Label", focus + select all). **Enter** or blur commits, **Esc** cancels (restores old label). Empty string = no label | 1 undo step on commit if changed |
| Duplicate / `Ctrl+D` | Deep copy (voxels, dims, weight, label) **appended at the end** (id `S{N+1}`, colour by position); becomes selected and scrolled into view | 1 undo |
| Delete / `Del` | Remove the shape; shapes after it are **renumbered**; selection moves to the nearest remaining (same index, clamped). Disabled if it is the only shape. Confirm dialog only if legacy confirms `[OQ-14]` | 1 undo |
| `W−` / `W+` | weight −1 (min 0) / +1; badge appears/disappears when ≠1; tooltips "Weight −1 — in the Solver, heavier shapes stay in place and lighter ones move" / "Weight +1 — in the Solver, heavier shapes stay in place and lighter ones move"; `W−` disabled at 0 | 1 undo |
| Drag grip or row | Row follows pointer (opacity .6); drop target row shows 2 dp `accent` line above it; drop reorders and **renumbers**; selection follows the moved shape. Esc during drag cancels | 1 undo |
| Right-click row | Selects the row, opens **context menu** at the pointer | — |
| Hover row | Reveal actions (also keyboard focus within the row reveals them) | — |
| `+ New` | Creates empty shape (legacy default dims `[OQ-14]`; mock: 3×3×3), unique id, colour cycles; selected | 1 undo |
| Collapse button (`Ctrl+[`) | Left sidebar → 48 dp rail (C11) | `layout.leftCollapsed` |

### Context menu (right-click a row)
Header (non-clickable, 11/600 uppercase): `Shape S#`. Items:
1. **Rename…** `F2` 2. **Duplicate** `Ctrl+D` — separator — 3. **Move earlier** 4. **Move later** (each disabled at the list end) — separator — 5. **Weight +1** 6. **Weight −1** (disabled at 0) — separator — 7. **Delete** `Del` (text `danger`, disabled when single shape).
Any additional entries the legacy shape list context menu already has MUST also be present `[OQ-4]`.

## Edge cases
* Long labels truncate with ellipsis; tooltip shows full label.
* 0 shapes cannot occur (delete disabled at 1). If the legacy file can contain 0 shapes, show an empty-state row "No shapes — click + New".
* Weight badge never hides the label's first 6 characters (label shrinks first).
* Reordering while a rename field is open: commit the rename first.

## Acceptance criteria
* AC-C01-01 Row shows grip, chip, label, badge (n≠1), and actions on hover/selected; actions hidden otherwise.
* AC-C01-02 Click selects; selection updates inspector head, status text, grid, 3D, colour footer.
* AC-C01-03 Rename: Enter commits, Esc reverts, empty clears; `F2` and double-click start it.
* AC-C01-04 Duplicate copies voxels/dims/weight/label, appends as the last shape (`S{N+1}`), selects it; existing ids unchanged; one undo removes it.
* AC-C01-05 Delete disabled with one shape; with ≥2 removes it, renumbers later shapes and selects the neighbour.
* AC-C01-06 `W+`/`W−` change weight; `W−` stops at 0; badge visible iff weight≠1; one undo each.
* AC-C01-07 Drag reorder shows drop line, reorders, **renumbers ids and colours by position**, selection follows the shape, single undo; order persists after save/load; the status flash appears.
* AC-C01-08 Context menu has exactly the items above (plus legacy extras), correct disabled states, closes on Esc/outside click.
* AC-C01-09 Rail shows chips for all shapes, selected ring, clicking selects, `+` creates, expand restores.
* AC-C01-11 Ids are always `S1…SN` in list order (no gaps or duplicates) after any new/duplicate/delete/reorder/undo/redo/load.
* AC-C01-10 Row action buttons have tooltips; list is keyboard operable (arrows, F2, Del, Ctrl+D, Alt+↑/↓).

## Keyboard (Rev 6.0)
The list is a single tab stop. With the list focused: `↑`/`↓` select the previous/next shape (scrolls into view), `Alt+↑`/`Alt+↓` move the shape earlier/later (ids renumber, status flash), `Enter` or `F2` start the inline rename, `Del` deletes, `Ctrl+D` duplicates. Clicking a row gives the list focus (2 dp `accentSoft` inner ring, selected row gets an accent outline).
* AC-C01-K The keys above work after clicking a row; `Alt+↑/↓` reorders and renumbers exactly like the context-menu *Move earlier / later*.


---
<!-- FILE: components/C02-voxel-editor-grid-size.md -->

# C02 — Voxel editor ▸ Grid size

**Phase:** P4. **Images:** `01-main-light.png`, `23-voxel-editor-card.png`, `00-new-annotated.png` (N58), `legacy-size-tab-annotated.png` (L30–L33). **Legacy:** *Edit ▸ Size* tab (dimension fields + sliders + checkboxes + "Apply to All Shapes").
*Why here:* grid size is a property of the 2D grid, and the right card has the vertical room. It sits **between the grid and the Drawing-colour footer** and is always visible (no accordion). Grid-fitting operations (prune, centre, origin) are not size edits and live in C03.

## Anatomy (`entities.editor.size`, `flex: none`, 1 dp top border, padding 8 12 10)
1. **Header row**: micro-label **GRID SIZE** (11/600 uppercase `muted`) · spacer · PR-07 switch with label **"Apply to all shapes"** (12 dp `muted`; tooltip "Size changes apply to every shape"). id `entities.editor.size.applyAll`.
2. **Three PR-06 steppers** in one row, equal width, gap 6: X (red), Y (green), Z (blue). Each: `[axis letter] [−] [value] [＋]`, value editable. ids `entities.editor.size.x|y|z` (+ `.dec` `.inc` `.input`).
The legacy **per-axis checkboxes (L33) are dropped** (product decision) — nothing replaces them.

## Legacy → new
| Legacy | New |
|---|---|
| X/Y/Z numeric fields (L31) | Editable value in each stepper |
| X/Y/Z sliders (L32) | Removed; stepper ± (hold to repeat), typed value, wheel and ↑/↓ cover the range. Optional: drag-scrub on the axis letter `[MAY]` |
| Per-axis checkboxes (L33) | **Dropped** |
| Apply to All Shapes ☐ (L30) | Switch "Apply to all shapes" |
(Grid ▸ prune/centre/origin icons L34–L36 → C03.)

## Interactions
| Trigger | Effect | Undo |
|---|---|---|
| `−`/`＋` (hold repeats) | Resize that axis by 1 on the selected shape — or on **every** shape when Apply-to-all is on (each clamped to limits). Then: clamp `layer`, rebuild rulers/grid/layer strip, 3D bounds | 1 per click |
| Type value + Enter / blur | Set axis size; invalid text reverts; clamp to `[1, max]` `[OQ-6]` | 1 per commit |
| Shrink below occupied voxels | **Blocked**: status flash "Blocked: voxels exist beyond this size — prune or move the piece first"; no change `[OQ-5]` (the mock clips; do not copy) | none |
| Apply-to-all toggle | Affects subsequent size edits only (not retroactive) | none |
| Selection change | Steppers show the newly selected shape's size | — |
Dimensions are no longer repeated in the inspector head; the steppers are the single place that shows them.

## Acceptance criteria
* AC-C02-01 Grid-size row sits directly above the colour footer, always visible, in default and Focus-2D layouts; no per-axis checkboxes exist.
* AC-C02-02 Steppers resize the selected shape; typed values commit on Enter/blur; wheel/arrow keys work when focused.
* AC-C02-03 Apply-to-all: one edit changes every shape's axis by the same step (where legal); off = only selected.
* AC-C02-04 Shrinking into occupied voxels is blocked with the specified message and leaves data unchanged.
* AC-C02-05 After any size change `layer` is clamped, grid rulers, layer strip, 3D bounds and layer-slab update.
* AC-C02-06 Each edit is one undo step.
* AC-C02-07 Values shown always equal the selected shape's real dimensions after undo/redo and selection change.


---
<!-- FILE: components/C03-inspector-fit-and-scale.md -->

# C03 — Inspector ▸ Fit & scale

**Phase:** P6. **Images:** `00-new-annotated.png` N22–N26, `legacy-size-tab-annotated.png` (L34–L39). **Legacy:** Size-tab icon columns "Grid" and "Shape". (The legacy Tools-tab **Grid Scale** button is **dropped** by product decision.) Summary text `Prune · Center · Scale`. Default **closed**.
This section merges all operations that *adapt a piece to its grid or change its resolution* (previously split between the "Grid" and "Shape" icon columns of the Size tab). None of them is a plain size edit (that is C02).

## Anatomy
**Sub-heading FIT TO GRID** (11/600 uppercase `muted`)
1. **Prune grid to fit piece** — full-width PR-03; glyph `prune-grid`; hint "Shrink the grid to the piece's bounding box". Disabled for an empty shape. id `entities.fit.prune`.
2. Two PR-03 side by side: **Center** (`center-on-grid`, hint "Centre on the grid") id `entities.fit.center`; **To origin** (`move-to-origin`, hint "Corner at 0, 0, 0") id `entities.fit.origin`.

**Sub-heading SCALE**
3. **Minimize** — full-width PR-03; glyph `minimize`; hint "Reduce resolution ÷n, same shape" (n = factor that will be applied) or, when unavailable, "Already at minimum resolution" (disabled). id `entities.fit.minimize`.
4. Two PR-02 glyph buttons, 2 columns: **Double ×2** (`multiply-2`) id `entities.fit.x2`; **Triple ×3** (`multiply-3`) id `entities.fit.x3`. Tooltip "Multiply every voxel n× per axis (piece grows)". Disabled when a resulting dimension would exceed the grid limit `[OQ-6]` (mock: 64).

## Legacy → new
| Legacy | New |
|---|---|
| Size ▸ Grid icon 1 — arrows-in (L34) | Prune grid to fit piece |
| Size ▸ Grid icon 2 — piece centred (L35) | Center |
| Size ▸ Grid icon 3 — piece at corner (L36) | To origin |
| Size ▸ Shape icon 1 — "1:1" (L37) | Minimize |
| Size ▸ Shape icon 2 — "×2" (L38) | Double ×2 |
| Size ▸ Shape icon 3 — "×3" (L39) | Triple ×3 |
| Tools ▸ Grid Scale (L54) | **Dropped** (product decision) |
Icon-to-function order for L34–L39 was inferred from the artwork and the owner's description — verify `[OQ-2]`.

## Semantics (expected results; reuse legacy code)
* **Prune**: crop the grid to the voxels' bounding box and translate so the box starts at the origin; grid dims = box size.
* **Center**: translate the piece so its bounding box is centred in the *current* grid: offset `floor((dim − size)/2) − min` per axis.
* **To origin**: translate so the bounding-box minimum is `(0,0,0)`; grid unchanged.
* **×n**: every voxel becomes an n×n×n block of identical voxels (same state and colour); each grid dim becomes `dim·n`.
* **Minimize**: find the **largest** `k ≥ 2` (k ≤ 8 suffices) such that every dimension is divisible by `k` and every k×k×k block is uniformly empty or uniformly filled with identical state/colour; replace each block by one voxel and divide dims by `k`. If none exists → disabled.
* All act on the selected shape only; each is **one undo step**. Blocked cases flash: "Blocked: grid would exceed {max}" / "Already at minimum resolution".

## Interactions
| Trigger | Effect |
|---|---|
| Click any button | Apply op, refresh grid, layer strip (clamp), 3D, status, steppers (C02) |
| Selection/voxel change | Recompute enabled state (Prune/Center/Origin need voxels; Minimize needs divisible blocks; ×n needs room) |
| Hover disabled button | Tooltip/hint explains why |

## Acceptance criteria
* AC-C03-01 Section order and names as above; summary text; closed by default; open state persists.
* AC-C03-02 Prune/Center/To origin produce the stated results on fixtures (FX-D) and are single undo steps; disabled for empty shapes.
* AC-C03-03 ×2 then Minimize returns the original shape and dims; likewise ×3 (round trip).
* AC-C03-04 Minimize is disabled with the "Already at minimum resolution" hint when no uniform k-blocks exist (FX-F).
* AC-C03-05 ×n is disabled/blocked at the size limit with explanatory text; voxel count after ×n = `old·n³`.
* AC-C03-06 There is no Grid-scale control anywhere in the UI.
* AC-C03-07 After every op the Grid-size steppers (C02) show the new dimensions.
* AC-C03-08 For voxel types without scale support (Spheres, C14) the **Scale sub-section is hidden**; Fit-to-grid stays.


---
<!-- FILE: components/C04-inspector-transform.md -->

# C04 — Inspector ▸ Transform (Flip · Rotate · Nudge)

**Phase:** P6. **Images:** `00-new-annotated.png` N29, `legacy-transform-tab-annotated.png` (L40–L42). **Legacy:** Edit ▸ Transform tab. Summary text `Flip · Rotate · Nudge`. Default **open** — it is the first inspector section.

## Anatomy
A grid: **1 axis-label column (22 dp) + 5 equal columns**, gap 4. Column headers (11/600 uppercase `muted`): **FLIP** (1 column) · **ROTATE** (2 columns) · **NUDGE** (2 columns). Three rows: **X** (red), **Y** (green), **Z** (blue) — axis letter 13/700 coloured.
Each cell is a PR-02 glyph button, h50, glyph 26–28, caption below:

| Column | Glyph (file) | Caption | Tooltip | id |
|---|---|---|---|---|
| Flip | `flip-{x,y,z}` | Flip | "Mirror the piece across the X plane" (Y/Z likewise) | `entities.transform.flip.x` … |
| Rotate − | `rotate-{x,y,z}-neg` | −90° | "Rotate about X by −90°" | `entities.transform.rotate.x.neg` |
| Rotate + | `rotate-{x,y,z}-pos` | +90° | "Rotate about X by +90°" | `entities.transform.rotate.x.pos` |
| Nudge − | `nudge-{x,y,z}-neg` | −1 | "Nudge along X by −1" | `entities.transform.nudge.x.neg` |
| Nudge + | `nudge-{x,y,z}-pos` | +1 | "Nudge along X by +1" | `entities.transform.nudge.x.pos` |
**Flip is ONE button per axis** (product-owner confirmed; the legacy pair of flip icons per axis is a single operation).

## Legacy → new
Legacy Flip (3×2 icons), Nudge (3×2), Rotate (3×2) with unlabeled icons → one matrix: Flip 3 buttons, Rotate 6, Nudge 6, all captioned. Position commands (centre/origin) live in C03 *Fit & scale*. Legacy rotate/nudge "left/right" icon order is preserved as −/+ (verify direction signs against legacy so that "+" matches the legacy right-hand icon `[OQ-1]`).

## Semantics (expected results)
Let `D=[Dx,Dy,Dz]`, axis index `i` (X=0,Y=1,Z=2), voxel coordinate `c`.
* **Flip i**: `c[i] → D[i]−1−c[i]` (grid-centre plane) `[OQ-1]`.
* **Rotate about i by ±90°**: with `A=(i+1)%3`, `B=(i+2)%3`; new dims swap `D[A]`,`D[B]`. `+90°`: `c'[A]=c[B]`, `c'[B]=D[A]−1−c[A]`. `−90°`: `c'[A]=D[B]−1−c[B]`, `c'[B]=c[A]`. (Right-handed about the axis; verify handedness against legacy.)
* **Nudge i by ±1**: `c[i] → c[i]±1`. If any voxel would leave the grid the operation is **blocked**: no change, status flash "Blocked: the piece would leave the grid" `[OQ-7]`.
Flip and rotate that would leave the grid (rotate of a non-cubic grid fits by swapping dims, so it never leaves) use the same blocked behaviour. All ops act on the selected shape only, preserve voxel state/colour, and are single undo steps.

## Interactions
| Trigger | Effect |
|---|---|
| Click button | Apply op, refresh grid/3D/status, single undo |
| Hover | Hover style + tooltip |
| Hold (optional) | Nudge buttons MAY auto-repeat (400 ms delay, 100 ms interval; each repeat = its own undo step) |
| Empty shape | Buttons enabled but no-op (no flash) |

## Acceptance criteria
* AC-C04-01 15 buttons present (3 flip + 6 rotate + 6 nudge), each with glyph, caption, tooltip and id.
* AC-C04-02 Flip twice = identity. Rotate +90 ×4 = identity; +90 then −90 = identity (fixture shapes).
* AC-C04-03 Nudge to the boundary succeeds; one more is blocked with the message and no data change.
* AC-C04-04 Rotating a 4×3×5 shape about Y yields 5×3×4 (X and Z swapped); about X yields 4×5×3; about Z yields 3×4×5.
* AC-C04-05 Each op is one undo step.
* AC-C04-06 Direction (±) matches legacy icons' behaviour.

## Voxel-type variants
The matrix above is the **Brick** layout. For Triangular Prism, Spheres and the tetrahedral types the section is generated from the voxel type's transform set (different rotate counts, a 12-way sphere nudge pad, an 8-direction prism nudge pad (6 in-plane + 2 perpendicular)) — see **C14**. Flip stays one button per axis in every type.


---
<!-- FILE: components/C05-inspector-repair-and-surface.md -->

# C05 — Inspector ▸ Repair & surface (Fill holes, Surface voxels)

**Phase:** P6. **Images:** `00-new-annotated.png` N28–N29, `legacy-tools-tab-annotated.png` (L50–L53). **Legacy:** Edit ▸ Tools tab ("Constrain" icon pairs, Fill Holes). *The legacy Grid Scale button is dropped.* Summary text `Fill · Inner / Outer`. Default **closed**.

## Anatomy
1. **Fill interior holes** — PR-03 action button, glyph `fill-holes`; title "Fill interior holes"; hint "Fill empty voxels not visible from outside with fixed voxels". id `entities.repair.fillHoles`.
2. Sub-heading **SURFACE VOXELS**, then a 3-row table, grid `1fr · 66 · 66`, gap 6:

| Row label (12.5/600 + small hint 11 `muted`) | Inner button | Outer button |
|---|---|---|
| **Make fixed** / "solid" | `surface-fixed-inner`, caption "Inner" — id `entities.repair.fixed.inner` | `surface-fixed-outer`, "Outer" — `…fixed.outer` |
| **Make variable** / "optional" | `surface-variable-inner` — `…variable.inner` | `surface-variable-outer` — `…variable.outer` |
| **Clear colour** / "remove paint" | `surface-clear-inner` — `…clear.inner` | `surface-clear-outer` — `…clear.outer` |
Glyph buttons h56. Tooltips: Inner "Voxels not visible from outside", Outer "Voxels on the outer surface".

## Legacy → new
| Legacy | New |
|---|---|
| Constrain row A (L50) two icons | "Make fixed": Inner/Outer |
| Constrain row B (L51) | "Make variable": Inner/Outer |
| Constrain row C (L52) | "Clear colour": Inner/Outer |
| Fill Holes (L53) | Fill interior holes |
| Grid Scale (L54) | **Dropped** (product decision) |
Product-owner definitions: A = convert inner/outer surface voxels to **fixed**; B = to **optional (variable)**; C = **remove colour** from inner/outer voxels. Left icon of each legacy pair is assumed **Inner** (small block in dashed grid), right icon **Outer** (full ring) — **verify** against legacy code `[OQ-2b]`.

## Semantics
* **Exterior space** = empty cells reachable from outside the grid by 6-connected flood fill (pad the grid by 1 cell on every side).
* **Outer voxel** = filled voxel with ≥1 of its 6 neighbours in exterior space. **Inner voxel** = any other filled voxel (including those bordering interior holes).
* **Fill interior holes**: every empty cell *not* in exterior space becomes a **fixed** voxel with the **Default** colour.
* **Make fixed / variable**: set state of the chosen set. **Clear colour**: set colour index to Default (0).
* All act on the selected shape; each is one undo step. Status flash: "Filled N interior voxel(s)" / "No interior holes found" / "N inner|outer voxel(s) updated".

## Acceptance criteria
* AC-C05-01 Hollow 5×5×5 cube shell + Fill → 125 voxels; second Fill reports "No interior holes found".
* AC-C05-02 On a solid 3×3×3 cube: Outer set = 26 voxels, Inner set = 1 voxel; "Make variable ▸ Inner" changes only the centre voxel.
* AC-C05-03 Clear colour ▸ Outer sets coloured outer voxels to Default and leaves inner colours.
* AC-C05-04 Voxels adjacent to an enclosed cavity count as **inner**, not outer.
* AC-C05-05 Table labels, hints, captions, ids, tooltips present.


---
<!-- FILE: components/C06-viewport-3d.md -->

> **Density (Rev 6.0):** dimensions here are **Standard** density values; Minimal values and any later corrections are in `foundations/density.md`, which wins on conflicts.

# C06 — 3D viewport (canvas, toolbar, navigation, Display menu, 3D editing, overlays)

**Phase:** P5. **Images:** `01-main-light.png`, `15-display-menu.png`, `26-display-menu-dim-layers.png`, `16…19-3d-*-ghost.png`, `27…28-2d-hover-*.png`, `29-edit-button-*.png`, `30-focus-3d-no-slab.png`, `04-focus-3d.png`. **Legacy:** L95 (canvas; legacy modifier-click editing), L91/L92 (status glyph pairs → now in the Display menu). **New ids:** N40–N44. Variables/events: `overview/06-ui-contract.md §4`.

## Anatomy (centre card, no header)
* **Canvas surface** `entities.viewport.surface` — fills the card, background `canvas`. The existing GL renderer draws the scene into this rect; overlays (slab, ghosts, view cube) are drawn by C++ after the scene (guide §5).
* **View tag** (top-left, 14 dp inset): selected-shape chip + label (12 `muted`); hidden in Focus-2D. id `entities.viewport.tag`.
* **Floating toolbar** `entities.viewport.toolbar`: top 12 dp, horizontally centred, `panel` fill, 1 dp `line`, radius 10, elevation, padding 3. Buttons 48×44 (icon 20–24 + caption 10.5/600), separators 1×28 — Minimal: 34×32 icon-only and **auto-hiding** (`foundations/density.md` §3). Keys `O` / `P` select Orbit / Pan mode. Order:

| # | Button | Icon | Caption | id | Behaviour |
|---|---|---|---|---|---|
| 1 | Orbit | `orbit` | Orbit | `…toolbar.orbit` | navigation mode (default, `on`): plain left-drag orbits |
| 2 | Pan | `pan` | Pan | `…pan` | navigation mode: plain left-drag pans |
| — | separator | | | | |
| 3 | **Edit** | **glyph of the tool in effect** | Edit + number badge | `…edit` | toggles **Edit mode** (also key `E`) — see below |
| — | separator | | | | |
| — | separator | | | | |
| 4 | Display | `eye` | Display | `…display` | opens Display menu |
| — | separator | | | | |
| 5 | Focus | `focus`/`unfocus` | Focus / Exit | `…focus` | toggle Focus-3D (C11) |
There is **no Zoom button** (the wheel zooms in every mode) and **no Views / Fit buttons**: the **view cube (C13)** provides face/edge/corner snapping, 90° steps and **Home** (default view + frame the shape); key `F` fits without changing orientation, key `Home` = Home. Orbit/Pan is a radio pair. In **Focus-2D** the toolbar is compact: 34×34 icon-only buttons (tooltips keep captions), 20 dp separators.

### The Edit button (icon, badge, states)
* **Icon** — the same drawing as the cursor, **replaced in place** for the tool in effect: Fixed = default pointer with a square badge carrying a “+”; Variable = pointer with a dotted-square “+” badge; **Erase = the eraser icon alone; Paint = the paint-bucket icon alone (no arrow)** (`edit-tool-fixed | variable | erase | paint`, see glyph catalog). The badge/drop colour follows the drawing colour (Default → theme contrast), exactly like the cursor.
* **Badge** — the tool number **1–4** at the button's top-right corner (9.5/700; `accent` while `on`/`override`, otherwise `muted`).
* **States** — *off* (normal), **`on`** (Edit mode active: accent-soft fill, accent icon/caption/badge), **`override`** (a quick-tool modifier is held while Edit mode is off *or* on: dashed 1.5 dp accent outline inset 3 dp, accent-soft fill; icon and badge show the **quick tool**, not the active tool). When no modifier is held the icon/badge show the active tool (`tool`).
* **Tooltip** — "3D edit mode — {Tool name} ({n}). Press E to turn edit mode on; or hold Shift/Alt/Ctrl while clicking." (when on: "Click edits with the active tool. Press E to leave edit mode."); while overridden: "Quick tool — {Tool name} ({n})."

### Hint (bottom-left, 14 dp inset, 12 `muted`, KeyBadges, two lines)
Line 1 (fixed): `Shift click 1 fixed · Alt/⌘ click 2 variable · Ctrl click 3 erase`. Line 2 follows the navigation mode — Orbit: `Drag orbit · Shift/middle-drag pan · Wheel zoom`; Pan: `Drag pan · Shift-drag orbit · Middle-drag pan · Wheel zoom`. Hidden in Focus-2D. id `entities.viewport.hint`.
* **View cube** (top-right under the toolbar): see **C13** (Settings ▸ Show view cube). **Empty-shape message** (centre, `muted`): "Empty shape — draw in the Voxel editor".

## Navigation (mouse mapping — independent of the editing rules)
| Input | Orbit mode (default) | Pan mode |
|---|---|---|
| Left-drag | **Orbit** | **Pan** |
| `Shift` + left-drag | **Pan** | **Orbit** |
| Middle-drag | **Pan** | **Pan** |
| Right-drag | reserved | reserved |
| Wheel / trackpad scroll | **Zoom** (toward the cursor if supported) | **Zoom** |
The toolbar mode only chooses what a *plain* left-drag does; the other action is one modifier away and middle-drag always pans. Orbit 0.5°/dp, pitch ±89° (honour Settings ▸ Rotation method); **zoom is smooth, never stepped**: each wheel/scroll event multiplies a *target* zoom by `exp(−Δ·0.0016)` (line-mode deltas ×33) and the camera **eases toward the target** every frame with a ~90 ms time constant (`zoom += (target − zoom)·(1 − e^(−dt/90 ms))`, frame-rate independent, stops within 0.15 %), clamp 0.3×–4× of fit, direction per Settings ▸ Reverse scroll zoom; a Home/Fit animation or a new wheel input retargets the zoom without a jump. A **click** (press-release ≤ 4 dp) never navigates; a drag > 4 dp never edits.
**Home / Fit / standard views** are handled by the **view cube (C13)** and keys (`Home`, `F`); all camera snaps animate smoothly (360 ms).

## 3D editing: Edit mode + quick tools (one rule)
Four tools numbered as in the tool strip: **1 Fixed · 2 Variable · 3 Erase · 4 Paint**. The *viewport action* is re-resolved on every pointer move and every modifier press/release:
1. **Quick tool (momentary):** a held modifier selects it — `Shift` = 1 Fixed, `Alt`/`⌘` = 2 Variable, `Ctrl` = 3 Erase (precedence Ctrl > Alt/⌘ > Shift). Works with Edit mode **off or on**.
2. else, **Edit mode on** → the active tool (`1`–`4`, tool strip).
3. else **no action** (no cursor change, no ghost, clicks do nothing).
**Design note (why both exist):** Edit mode is a *latched* tool for sustained work; quick tools are the same tools *held* for a one-off change without leaving navigation. They share numbering, cursor, ghost and Edit-button feedback, so there is one mental model: "the number of the tool in effect is shown on the Edit button; the cursor and the ghost agree with it". Paint has no modifier by design (Edit mode only).
**Picking** `[OQ-12]`: front-most voxel face under the pointer. *Fixed/Variable* target = `voxel + faceNormal` (inside the grid). *Erase/Paint* target = the voxel itself.
**Cursor:** **Fixed** = OS default pointer + a square badge at its upper right with a small **“+”** inside; **Variable** = the same with a **dotted boundary**. The badge is **white inside for the Default drawing colour in the light theme** (near-black in the dark theme) and **filled with the drawing colour** when a specific colour is selected. **Erase** = the sleek monochrome eraser icon alone; **Paint** = the paint-bucket icon alone with a drop in the drawing colour (Default → monochrome drop). Icon-only cursors use their glyph centre as hotspot. Full rule and assets: glyph catalog ▸ Cursors. It changes instantly when a modifier is pressed/released (no pointer move needed); default arrow/grab otherwise.
**Ghost preview** — drawn for every cell produced by Mirror and Span (C07) applied to the target:
| Tool in effect | Ghost style |
|---|---|
| Fixed | cube(s) in the drawing colour @ 55 %, 2 dp `accent` outline |
| Variable | same, dashed (5/4) outline |
| Erase | over existing voxel(s): `danger` @ 50 %, `danger` outline |
| Paint | drawing colour @ 50 %, `accent` outline, existing voxels only |
| Fixed/Variable where a voxel exists but would change (state or colour differs) | faint (22 %) dashed "replace" overlay on that voxel |
| Fixed/Variable, target outside the grid | **no ghost**; the hit face is outlined dashed `danger` 2.5 dp; click does nothing |
**Commit:** left-button press-release ≤ 4 dp applies the tool; one undo step per click (including mirror/span copies). After commit refresh grid, layer dots, status; recompute the ghost. Middle/right buttons never edit. 3D edits do not change plane or layer.
**Same ghost from the 2D grid:** hovering a cell in the Voxel editor (C08) produces the same preview in this view at that cell of the active layer, for the tool in effect (including quick-tool modifiers). Both sources share one `hover_preview`; whichever pointer is active owns it; it clears on leave.

## Display menu (viewport display options; popup that **stays open** while toggling; all persisted)
| Section | Items | Type | Default | Notes |
|---|---|---|---|---|
| **Show** | ✓ Axes · ✓ Grid boundary · ✓ **Active layer slab** · ☐ **Dim other layers** | check | on, on, on, **off** | slab and dim rows are **disabled** (greyed, hint "needs Voxel editor") while the Voxel editor is not visible |
| **Projection** | Perspective · Orthographic | radio | Perspective | replaces legacy glyph pair L92 |
| **Voxel colour** | Piece colour · Voxel colour | radio | Piece colour | replaces legacy glyph pair L91 |
ids `entities.viewport.display.axes|bounds|layer|dim|persp|ortho|colourPiece|colourVoxel`. **Piece colour:** every voxel uses the shape colour. **Voxel colour:** each voxel uses its own colour index (Default = shape colour).

## Overlays drawn in the scene
| Overlay | Style | Toggle / visibility |
|---|---|---|
| Axes from origin | X/Y/Z tokens, 2.5 dp, extend 0.7 cell beyond the grid | Axes |
| Grid boundary box | dashed (4/4) 1 dp `line2` | Grid boundary |
| **Active layer slab** | The **whole one-voxel-thick layer volume** (full grid extent in the two in-plane axes, exactly one cell along the layer axis, for the current plane/layer). Drawn as a **volumetric highlight**: translucent `accent` fill on **all six faces** (≈ 11 % each, so the volume reads as a tinted block and edges are never lost) **plus** all 12 box edges in `accent` 1.8 dp | Active layer slab **and** `editor_visible` |
| **Dim other layers** | Voxels outside the active layer render at 28 % opacity; picking still works | Dim other layers **and** `editor_visible` |
| Variable voxels | 50 % alpha, dashed edges | — |
| Fixed voxels | opaque; edges `rgba(0,0,0,.32)` | — |
**Visibility rule:** the slab and dim-other-layers exist only to show the layer being edited in the 2D grid, so they are drawn **only while the Voxel editor is visible** (`editor_visible`: default layout, or Focus-2D). They are **hidden in Focus-3D and when the right sidebar is collapsed**, regardless of the saved Display preference (which is retained and applies again when the editor returns). Lighting per Settings ▸ Lighting (off = flat, unlit).

## Acceptance criteria
* AC-C06-01 Toolbar has 5 buttons in order — Orbit, Pan, Edit, Display, Focus (no Zoom, Views or Fit) with captions, tooltips, ids; compact icon-only in Focus-2D.
* AC-C06-02 Orbit mode: left-drag orbits, Shift-drag pans, middle-drag pans; Pan mode: left-drag pans, Shift-drag orbits, middle-drag pans; wheel zooms in both; hint line 2 follows the mode.
* AC-C06-03 Reverse-scroll flips wheel direction; `F` fits (orientation kept) and `Home` goes home, both animated; view snapping is covered by C13.
* AC-C06-04 Display menu has the three sections, defaults and keep-open behaviour; Projection and Voxel colour apply immediately and persist; the status bar has no toggles.
* AC-C06-05 Slab: full in-plane extent, one cell thick, all six faces tinted + 12 edges; follows plane/layer/size changes; **not drawn in Focus-3D or with the right sidebar collapsed**; its Display row is disabled with the hint in those layouts. Dim-other-layers follows the same visibility rule.
* AC-C06-06 Quick tools: with Edit mode off, `Shift`/`Alt·⌘`/`Ctrl` + click apply Fixed/Variable/Erase; precedence Ctrl > Alt/⌘ > Shift; without a modifier a click does nothing.
* AC-C06-07 Edit mode (button or `E`): plain click applies the active tool; modifiers still override momentarily.
* AC-C06-08 The cursor changes to the tool cursor and the ghost appears **immediately when a modifier is pressed** (pointer stationary) and reverts when released.
* AC-C06-09 Edit button: glyph (pointer+badge for 1/2, eraser/bucket alone for 3/4) and number badge 1–4 match the active tool and change in place on tool change; shows `on` in Edit mode; shows `override` with the quick tool's glyph/badge while a modifier is held.
* AC-C06-10 Ghost previews follow the table (incl. mirror/span copies, replace overlay, no ghost outside grid).
* AC-C06-11 Hovering a 2D grid cell shows the same ghost in 3D at that cell of the active layer; clears on leave; honours quick-tool modifiers.
* AC-C06-12 Click-vs-drag threshold 4 dp; middle/right never edit; each edit is one undo step; grid, layer dots, status update.
* AC-C06-13 Wheel zoom is smooth: one wheel notch produces an eased sequence of intermediate zoom levels over ≈ 0.3 s (no single-frame jump), consecutive notches accumulate into one continuous glide, trackpad/small deltas feel continuous, and Home/Fit or manual orbit during a zoom glide never snap.


---
<!-- FILE: components/C07-voxel-editor-tools-mirror-span.md -->

# C07 — Voxel editor: header, tool strip, Mirror & Span

**Phase:** P4. **Images:** `23-voxel-editor-card.png`, `19-3d-mirror-span-preview.png`, `legacy-tools-tab-annotated.png` (L60–L63). **New ids:** N50–N54.

## Card structure (right card, width 400; rail 56)
Top→bottom: **Header** (h44) → **Tool strip** → **Mirror/Span row** → **Plane row** (C08) → **Grid body with layer strip** (C08) → **Grid size** (C02) → **Colour footer** (C09).
**Header**: title "Voxel editor" (13/600) · `Focus` icon button (`focus`/`unfocus`, tooltip "Focus 2D editor (Esc to exit)", id `entities.editor.focus2d`) · collapse-right icon button (`panelR`, tooltip "Collapse sidebar", id `entities.editor.collapse`).

## Tool strip (`entities.editor.tools`)
Grid of **4 equal columns**, gap 6, padding 8×10, 1 dp bottom border. Each is a PR-02 glyph button, h58, glyph 30, caption, key badge top-right. Exactly one is selected (radio).

| Key | Tool | Glyph | Caption | Tooltip | Applies to a cell/voxel | id |
|---|---|---|---|---|---|---|
| `1` | Fixed voxel | `tool-fixed` | Fixed | "Place a fixed (solid) voxel (1)" | set voxel = fixed, colour = drawing colour | `entities.editor.tool.fixed` |
| `2` | Variable voxel | `tool-variable` | Variable | "Place a variable (optional) voxel (2)" | set voxel = variable, colour = drawing colour | `…tool.variable` |
| `3` | Erase | `tool-erase` | Erase | "Erase the voxel (3)" | remove voxel | `…tool.erase` |
| `4` | Paint | `tool-paint` | Paint | "Apply the drawing colour to an existing voxel (4)" | change colour only, state unchanged; **no effect on empty cells** | `…tool.paint` |
The numbers 1–4 are the **tool numbers used everywhere**: keys `1`–`4`, key badges, the 3D Edit-button badge, and the quick-tool modifiers (**Shift = 1 Fixed, Alt/⌘ = 2 Variable, Ctrl = 3 Erase**, momentary, in both the 2D grid and the 3D view — C06/C08). Legacy L60 (4 icons) maps 1:1. Legacy L61 (select-box / select-cell icons) is **dropped** (product decision). Default tool: Fixed. `tool` is shared by 2D grid and 3D Edit mode.

## Mirror & Span (`entities.editor.modifiers`)
A 2-column row (gap 14, padding 8×12). Each column: micro-label (**MIRROR** / **SPAN**, 11/600 uppercase `muted`) over three PR-02 glyph toggle buttons (h54, glyph 28, caption axis letter **X/Y/Z** coloured by axis). Each button is an independent **toggle** (aria-pressed); several can be on. Default all off.

| Control | Glyph | Tooltip | ids |
|---|---|---|---|
| Mirror X/Y/Z | `mirror-x/y/z` | "Also place a mirrored voxel across the X centre plane" | `entities.editor.mirror.x|y|z` |
| Span X/Y/Z | `span-x/y/z` | "Fill the whole grid along X on each click" | `entities.editor.span.x|y|z` |
Legacy: L62 (3 icons) = Mirror; L63 (3 icons) = Span "place voxel across the entire grid space in each of the 3 directions on click".

### Expansion algorithm (apply to every placement from 2D or 3D, for all four tools)
Input: one target voxel coordinate `t`; grid dims `D`.
1. `S = {t}`. For each axis `a` with **Span a** on: `S = ⋃_{s∈S} { s with coordinate a = v : v ∈ 0..D[a]-1 }` (whole line along a; multiple spans make a plane/volume).
2. For each axis `a` with **Mirror a** on: `S = S ∪ { s with coordinate a → D[a]-1-s[a] : s ∈ S }` (mirror about the grid centre plane; a centre cell on an odd dimension maps to itself).
3. Remove duplicates; apply the tool to every cell in `S` (Erase removes, Paint recolours existing, Fixed/Variable set). Span is applied **before** Mirror.
The same `S` is used for the 3D ghost preview (C06). The whole expansion is part of **one** undo step.

## Interactions
| Trigger | Effect |
|---|---|
| Click tool / press `1`–`4` | Select tool (tool strip, 3D Edit button glyph + badge, cursor and ghost style follow) |
| Click mirror/span axis | Toggle that flag; subsequent placements expand; no data change |
| Hover tool | tooltip incl. shortcut; selected tool keeps accent style |
| Keyboard | strip is a roving-tabindex group (←/→ move, Enter/Space select); mirror/span buttons are individual tab stops |

## Acceptance criteria
* AC-C07-01 Tool strip shows 4 captioned glyph buttons with key badges; keys 1–4 select; one selected at a time.
* AC-C07-02 Paint changes colour of an existing voxel without changing fixed/variable and does nothing on empty cells.
* AC-C07-03 With Mirror X on in a 4-wide grid, placing at x=0 also places x=3 (same y,z); on a 3-wide grid placing x=1 places only x=1.
* AC-C07-04 With Span Z on in a 4-layer grid, one click at (x,y) creates 4 voxels (z=0..3). Span X+Z creates a 4×4 plane.
* AC-C07-05 Mirror X + Span Y: result equals expansion order (span then mirror) — verify cell count on fixture.
* AC-C07-06 Mirror/Span apply identically from 2D clicks, 2D drags and 3D clicks, and to Erase/Paint.
* AC-C07-07 No selection tools present; legacy L61 not reachable.
* AC-C07-08 Header Focus and Collapse buttons work (C11).
* AC-C07-09 Selecting a tool updates the 3D Edit button's glyph/badge in place and the cursor over the 2D grid and (in Edit mode) over the 3D view.


---
<!-- FILE: components/C08-voxel-editor-plane-layer-grid.md -->

> **Density (Rev 6.0):** dimensions here are **Standard** density values; Minimal values and any later corrections are in `foundations/density.md`, which wins on conflicts.

# C08 — Voxel editor: plane selector, layer strip, 2D grid

**Phase:** P4. **Images:** `23-voxel-editor-card.png`, `03-focus-2d.png`, legacy L70 (layer slider), L71 (grid). **New ids:** N55–N57.

## Plane row (`entities.editor.plane`)
`subbar` (padding 8×12, bottom border): three PR-02 glyph buttons, 62×56, glyph 30, captions **XY**, **XZ**, **YZ** (glyphs `plane-xy/xz/yz`; tooltip "Edit the XY plane") — radio, default XY — then right-aligned text **"Layer k of n"** (12 `muted`, k 1-based). ids `entities.editor.plane.xy|xz|yz`, `entities.editor.layerLabel`.
Mapping of plane → grid axes: **XY**: columns=X, rows=Y, layers=Z · **XZ**: columns=X, rows=Z, layers=Y · **YZ**: columns=Z, rows=Y, layers=X. `n` = size along the layer axis.
Changing plane: keep `layer` but clamp to new `n−1`; redraw grid, strip, 3D layer slab. `[OQ-4]` legacy plane selection method is unknown; this control is the single selector.

## Layer strip (`entities.editor.layers`)
Left of the grid, width 48 (40 in Minimal), `panel` fill, 1 dp right border, padding 10×0, vertical stack gap 4, scrolls if long.
* Header text `"{axis} layer"` (11/700, coloured by the layer-axis colour).
* One **chip** per layer (36×28, radius 7, 600): label `1…n`, **highest layer at the top**. Chip shows a trailing small **dot** if that layer contains ≥1 voxel. Selected chip: `accent` fill, white text. Hover: border `accent`. Tooltip "Layer k".
* Click chip → `layer = k−1`. `PgUp` = layer+1 (up), `PgDn` = layer−1, clamped. (Replaces legacy vertical light-blue slider L70.)
ids `entities.editor.layer.<k>`.

## 2D grid (`entities.editor.grid`)
Background `panel2`; grid centred in the available area (both axes).
* **Cell size** = `floor(min((areaW−70)/cols, (areaH−60)/rows))` clamped to **16…120 dp**; recomputed on resize/collapse/focus/size change.
* **Cells**: `panel` fill; 1 dp gaps coloured `line2` (so grid lines are `line2`); outer border 1 dp `line2`; elevation shadow.
* **Fixed voxel**: solid fill of its colour. **Variable voxel**: diagonal hatch (45°, 6 dp stripes, colour @ 33 % / 13 %) + 2 dp dashed border (white @ 75 % over the fill, i.e. visibly dashed) in its colour. Colour = voxel colour index; **Default (index 0) renders as the shape colour**. (Grid always shows *voxel* colours, independent of the status-bar Piece/Voxel colour toggle.)
* **Rulers**: row numbers to the left (`muted` 11, right-aligned, top = highest row index), column numbers below; a 3 dp line along the left edge in the **row-axis colour** and along the bottom edge in the **column-axis colour** (e.g. XY: green Y at left, red X at bottom — matches legacy L71 edge lines).
* **Hover**: 2 dp `accent` inset outline on the cell; **tool cursor** (`edit-<tool in effect>`); the **same ghost preview appears in the 3D view** at that cell of the active layer (C06), for the tool in effect; status bar cursor readout "Cursor X1 Y2 · Z0" (column axis letter+index, row axis letter+index, `·`, layer-axis letter+layer index 0-based). Cleared on mouse leave.

### Drawing interactions
| Trigger | Effect |
|---|---|
| Mouse down on cell | begin stroke; apply the **tool in effect** (captured now: quick-tool modifier if held — Shift=Fixed, Alt/⌘=Variable, Ctrl=Erase — else the active tool) to that cell via expansion (C07) |
| Move with button down | apply to each **newly entered** cell (also if skipping cells, interpolate along the segment so no gaps `[SHOULD]`) |
| Mouse up / leave window | end stroke → **one undo step** for the whole stroke |
| **Right button** (press or drag) | **Erase**, regardless of the active tool or held modifiers (captured at press, one undo step per stroke); the OS context menu is suppressed on the grid; the hover preview returns to the tool in effect on release. Grid tooltip: "Click or drag: active tool (Shift fixed · Alt variable · Ctrl erase) · Right-click or right-drag: erase" |
| Tool = Erase/Paint | per C07 semantics |
Each application triggers `VoxelsChanged`: grid redraw, layer-strip dots, 3D redraw (≤ 16 ms), status text counts.
Cell → voxel coordinate by plane mapping above, at `layer`.

## Cell shapes by voxel type
The grid draws the shape of the file's voxel type (square, **circle on an offset lattice with touching circles**, triangle, or square split into four triangles — C14); hover outline, stroke painting, hatching and ghosts follow that shape. Everything below is described for square cells.

## Link to the 3D view
The current plane + layer are shown in 3D as the **active layer slab** (a one-voxel-thick box covering the entire layer, C06) and, optionally, by **Dim other layers**. Both update on every plane/layer/size change.

## Focus-2D behaviour
In Focus-2D the card gets all remaining width; cell size grows (up to 120 dp). Layer strip, plane row, **Grid size** row and colour footer remain.

## Acceptance criteria
* AC-C08-01 Plane buttons switch mapping; layer clamps; grid/3D plane update; label "Layer k of n" correct for each plane on a 4×3×5 shape (n=5 for XY, 3 for XZ, 4 for YZ).
* AC-C08-02 Layer strip lists n chips, top = highest; dot iff layer non-empty; click/PgUp/PgDn change layer within bounds.
* AC-C08-03 Cell size formula and clamping hold on resize and focus toggle; grid stays centred; rulers/axis lines use the correct axis colours per plane.
* AC-C08-04 Fixed and variable cells are visually distinguishable (solid vs hatched+dashed) in light and dark themes.
* AC-C08-05 Drag-painting a row of cells is one undo step; undo restores all.
* AC-C08-06 Hover readout correct for all three planes.
* AC-C08-07 Default-colour voxels render in the shape colour; recolouring the shape in the app updates the grid.
* AC-C08-09 3D shows the active layer slab for the current plane/layer (one cell thick, full in-plane extent) and it moves with `layer`.
* AC-C08-10 Hovering a cell shows the tool cursor and the matching ghost in 3D (incl. mirror/span copies); pressing/releasing Shift, Alt/⌘ or Ctrl while hovering changes cursor and ghost immediately; the tool captured at press is used for the whole stroke.
* AC-C08-08 Grid redraw for a 32×32 plane < 8 ms; 3D refresh after a stroke segment < 16 ms.
* AC-C08-RC Right-click or right-drag on grid cells erases with any tool selected (Fixed, Variable, Erase, Paint) and any modifier; no context menu appears.


---
<!-- FILE: components/C09-drawing-colour.md -->

# C09 — Drawing colour (footer, palette, colour menu, rail swatch)

**Phase:** P3. **Images:** `23-voxel-editor-card.png`, `24-colour-menu.png`, legacy L80–L83 (`legacy-tools-tab-annotated.png`). **New id:** N58.

## Concept
Colour is **drawing state** (the colour new/painted voxels receive), not a separate panel. The legacy "Colours" group (Add / Remove / Edit buttons + colour list) becomes a compact footer in the Voxel editor.
Colour index **0 = Default**: "use the piece's own colour" (legacy label `Default`, chip `C1`). Colours 1… are user colours (names `C2`, `C3`, … — `[OQ-8]` confirm legacy naming).

## Anatomy (`entities.editor.colour`, bottom of Voxel editor card)
Footer: padding 10×12, 1 dp top border, gap 12, aligned centre.
1. **Big swatch** 40×40, radius 10, 2 dp `panel` ring + 1 dp `line2`, filled with the current drawing colour (Default shows the selected shape's colour).
2. **Meta** (min-w 78): micro-label "DRAWING COLOUR" (11/600 uppercase `muted`) + name (13/600): "Default" or "C2"…
3. **Palette**: swatches 26×26, radius 7, 1 dp `rgba(0,0,0,.25)` border; selected = 2 dp `panel` + 4 dp `accent` ring. The Default swatch shows the shape colour and a small centred marker (inner ring) to distinguish it. Tooltip: "Default (piece colour)" / "C2". ids `entities.editor.colour.swatch.<i>`.
4. **`＋` Add colour** icon button (tooltip "Add colour") id `entities.editor.colour.add`; **`⋯` Colour options** icon button (tooltip "Colour options") id `entities.editor.colour.more`.
Wraps to a second line if there are many colours (palette flex-wrap); the footer grows up to 3 rows then the palette scrolls.

### Colour menu (PR-11) — from `⋯` for the current colour, or right-click on any swatch (for that swatch)
Items: **Edit colour…** · **Remove colour**. Both **disabled for Default**. Edit opens the toolkit/legacy colour chooser and updates the colour everywhere live. Remove deletes the colour; voxels using it are reassigned to Default; drawing colour becomes Default `[OQ-8]`.

### Add (`＋`)
Opens the legacy colour chooser; on OK appends a new colour, selects it as drawing colour. (The mock appends a preset immediately — simplification.)

### Collapsed rail
When the right sidebar is collapsed (C11) the rail shows the expand button, a 28×28 swatch of the drawing colour, and vertical text "Voxel editor".

## Legacy → new
| Legacy | New |
|---|---|
| Colours ▸ Add (L80) | `＋` |
| Colours ▸ Remove (L81) | Colour menu ▸ Remove colour |
| Colours ▸ Edit (L82) | Colour menu ▸ Edit colour… |
| Colour list with `Default [C1]` chip (L83) | Palette swatches + big current swatch + name |
Legacy greyed Remove/Edit when Default selected → same via disabled menu items.

## Interactions
| Trigger | Effect |
|---|---|
| Click swatch | `drawColour = i`; big swatch/name/rail update; 3D ghost colour updates |
| Right-click swatch / `⋯` | Colour menu |
| `＋` | add colour flow |
| Edit/Remove | as above; `ColourListChanged` ⇒ grid, 3D, palette refresh |

## Acceptance criteria
* AC-C09-01 Footer shows big swatch, "DRAWING COLOUR" + name, palette, `＋`, `⋯`; Default selected initially.
* AC-C09-02 Selecting a swatch changes the colour of subsequent Fixed/Variable/Paint operations (2D and 3D) and the ghost preview.
* AC-C09-03 Default voxels follow the shape colour; user-colour voxels keep their colour.
* AC-C09-04 Edit/Remove disabled for Default; removing a used colour reassigns voxels to Default and updates 3D (Voxel-colour view) and grid.
* AC-C09-05 Right-click on a swatch opens the menu for that swatch.
* AC-C09-06 Rail shows the current drawing colour.


---
<!-- FILE: components/C10-status-bar.md -->

# C10 — Status bar

**Phase:** P1. **Images:** `25-status-bar.png`, legacy L90 (text), L91/L92 (glyph pairs — **moved**). **New ids:** N60.

## Anatomy (height 28, `panel` fill, 1 dp top `line`, padding 0×14, 12 dp text)
Left→right: **Status text** (flex) · **Cursor readout** (`muted`, right-aligned). **Nothing else** — the legacy view-toggle glyphs have moved to the 3D viewport's Display menu (C06).

| Part | Content | id |
|---|---|---|
| Status text | `Shape **S2** has **14** voxels (13 fixed, 1 variable)` — shape id and count in `text` 600, rest `muted`. Recomputed on selection / voxel change | `status.text` |
| Cursor readout | Empty by default; while hovering the 2D grid: `Cursor X1 Y2 · Z0` | `status.cursor` |

## Legacy → new
| Legacy | New |
|---|---|
| Text "Shape S2 has 14 voxels (14 fixed, 0 variable)" (L90) | Same text/format |
| Two blue-square glyphs — piece colour vs voxel colour (L91) | Display menu ▸ **Voxel colour**: Piece colour / Voxel colour (C06) |
| Two red/cyan circle glyphs — perspective on/off (L92) | Display menu ▸ **Projection**: Perspective / Orthographic (C06) |
Verify which legacy glyph is which state and map accordingly `[OQ-9]`.

## Flash messages (PR-13)
Operation feedback temporarily replaces the status text for **2400 ms**, then it reverts to the live text. Exact strings: "Blocked: the piece would leave the grid" · "Blocked: voxels exist beyond this size — prune or move the piece first" · "Blocked: grid would exceed {max}" · "Already at minimum resolution" · "Filled N interior voxel(s)" · "No interior holes found" · "N inner|outer voxel(s) updated" · "Order changed — shapes renumbered S1…S{n}". A new flash replaces an active one and restarts the timer.

## Acceptance criteria
* AC-C10-01 Text format matches exactly and refreshes after any voxel/selection change (counts split fixed/variable).
* AC-C10-02 Cursor readout appears only while hovering the grid and shows the correct axes for each plane.
* AC-C10-03 The status bar contains no toggles or glyph buttons.
* AC-C10-04 Flash text shows for 2.4 s then reverts to live text.


---
<!-- FILE: components/C11-layout-modes-collapse-focus.md -->

> **Density (Rev 6.0):** dimensions here are **Standard** density values; Minimal values and any later corrections are in `foundations/density.md`, which wins on conflicts.

# C11 — Layout modes: collapse and focus

**Phase:** P1 (skeleton) with hooks in P2/P4/P5. **Images:** `05-left-collapsed.png`, `06-right-collapsed.png`, `07-both-collapsed.png`, `03-focus-2d.png`, `04-focus-3d.png`, `62-puzzle-right-card-collapsed.png`, `63-puzzle-both-cards-collapsed.png`, `64-solver-right-card-collapsed.png`.

## State (see state model)
`leftCollapsed` (persist), `rightCollapsed` (persist), `focus ∈ {none, 2d, 3d}` (session, resets to none on launch). Left and right collapse are **independent**; focus overrides them while active. **Every workspace (Entities, Puzzle, Solver) has both a collapsible left and a collapsible right card**, with identical behaviour; each workspace **remembers its own collapse state** (switching workspaces restores it; focus modes reset).

## Effective layout
| Condition | Left | Centre | Right |
|---|---|---|---|
| focus=none | 320 or **rail 48** if `leftCollapsed` | flex | 400 or **rail 56** if `rightCollapsed` |
| focus=**3d** | rail 56 | flex (max) | rail 56 |
| focus=**2d** | rail 56 | **360** fixed, toolbar compact, view tag/hint hidden | flex (all remaining) |
Width changes animate 200 ms ease; content redraws (grid cell size, 3D viewport) on the final size and, if cheap, per frame.

## Controls
| Control | Where | Effect |
|---|---|---|
| Collapse-left (`panelL`) | left-card header of **every workspace** (Shapes · Problems · Solver) | `leftCollapsed = true` |
| Expand-left (`panelL`) | left rail top | `leftCollapsed = false`; if focus active, exits focus |
| Collapse-right (`panelR`) | right-card header of **every workspace** (Voxel editor · Selected piece · Solutions) | `rightCollapsed = true` |
| Expand-right (`panelR`) | right rail top | `rightCollapsed = false`; exits focus |
| **Focus 2D** (`focus`/`unfocus`) | Voxel editor header | toggle `focus=2d` (icon becomes `unfocus`, button `on`) |
| **Focus 3D** (`focus`/`unfocus`, caption Focus/Exit) | 3D toolbar | toggle `focus=3d` |
| `Esc` | anywhere (after dialogs/popups) | exits focus (see Esc ladder) |
Focus buttons are mutually exclusive: entering one replaces the other. Exiting focus restores the previous `leftCollapsed/rightCollapsed` values.

## Rails in the Puzzle and Solver workspaces
A collapsed Puzzle/Solver side card becomes the same **48 dp rail** (40 in Minimal): the expand button (`panelL`/`panelR`) at the top and the card's title written **vertically** below it ("Problems", "Selected piece", "Solver", "Solutions"). Card state (selection, scroll, running search, player position) is kept while collapsed. The collapsed card hides all of its content; nothing else changes.

## Narrow windows (two BurrTools windows side by side)
**Collapsing is the narrow-window strategy; the layout never auto-collapses.** Supported minimum window width **960 dp** (e.g. two windows side by side on a 1920 dp or 2560 dp screen). The centre viewport width is `window − overhead − left − right`, with overhead = workspace rail + body padding/gaps (**Standard 60 + 16 = 76**, **Minimal 44 + 2 = 46**) and left/right = expanded widths (Standard 320/340, Minimal 264/320) or collapsed rails (48 / 40):
| Window width | Standard: both open | right collapsed | both collapsed | Minimal: both open | right collapsed | both collapsed |
|---|---|---|---|---|---|---|
| 1600 dp | **864** | 1156 | ≈ 1410 | **970** | 1250 | ≈ 1460 |
| 1280 dp | 544 | 836 | ≈ 1090 | 650 | 930 | ≈ 1140 |
| 960 dp | *(too narrow)* | 516 | ≈ 770 | 330 | 610 | ≈ 834 |
`Ctrl+[` / `Ctrl+]` toggle the left / right card from the keyboard (C22). At 960 dp collapse at least the right card; the toolbar, hint, status bar and view cube remain usable (the view cube drops under the toolbar when the viewport is narrower than 640 dp, C13).

## Rails (Entities)
Left rail contents: expand button, one chip per shape, `+`. Right rail: expand button, drawing-colour swatch, vertical label "Voxel editor". Rails 48 dp wide (40 in Minimal), same card style, content centred, gap 8, padding 8×0. In Focus-2D the right card is fully expanded (no right rail); in Focus-3D both rails show.

## Rules
* `editor_visible` = Focus-2D, or no focus with the right sidebar expanded. The 3D **active layer slab** and **dim-other-layers** are drawn only when `editor_visible` (C06); entering Focus-3D or collapsing the right sidebar hides them immediately and re-showing the editor restores them.
* Inspector, shapes list, grid-size row etc. keep their state while hidden (no teardown).
* Keyboard shortcuts (`1–4`, `PgUp/PgDn`, `F`) work in every layout mode.
* A shape selected in the rail is the same selection as in the list.

## Acceptance criteria
* AC-C11-01 Each of the six layouts produces the widths in the table at 1600 dp (±1).
* AC-C11-02 Left and right collapse are independent, available in **all three workspaces**, remembered **per workspace** and across restart; focus never persists.
* AC-C11-04 A collapsed Puzzle/Solver side card shows a 48 dp rail (40 in Minimal) with the expand button and its vertical title; the centre viewport grows to the freed width; at 960 dp window width with both cards collapsed the workspace is fully usable (no overlap, toolbar and view cube intact).
* AC-C11-03 Entering Focus-2D collapses left to rail, centre to 360, gives right the rest; the grid cell size increases; toolbar becomes compact icon-only.
* AC-C11-04 Entering Focus-3D shows both rails and a maximised canvas; Exit restores prior collapse flags.
* AC-C11-05 `Esc` exits focus only when no dialog/popup is open.
* AC-C11-06 Rail expand buttons restore the full sidebar and exit focus.
* AC-C11-08 Focus-3D and right-collapsed layouts do not draw the layer slab / dim-other-layers; default and Focus-2D layouts do.
* AC-C11-07 Transition is 200 ms and does not leave stale drawing (grid/3D redraw after the animation).


---
<!-- FILE: components/C12-settings-dialog.md -->

# C12 — Settings dialog

**Phase:** P7. **Images:** `09-settings-general.png`, `66-settings-general-theme.png`, `10-settings-3dview.png`, `11-settings-performance.png`, `12-settings-shortcuts.png`, `13-settings-search.png`; legacy `legacy-settings-dialog.png`. **New id:** N70 (`settings.*`).

## Purpose
Replace the flat legacy Settings window (9 settings + Restore Defaults + Close) with a grouped, searchable dialog. Opens from the gear (C00) or `Ctrl+,`. **Changes apply immediately**; there is no Apply/OK.

## Anatomy (modal, 780×580 dp, radius 14, centred over a `scrim`)
* **Header** (h56): title "Settings" (15/700) · **search field** (260 wide, h32, magnifier icon, placeholder "Search settings") · spacer · close button (`close`, tooltip "Close (Esc)").
* **Left nav** (200 wide, `panel2`, padding 10): page buttons **General · 3D view · Performance · Shortcuts** (h34, radius 8; selected = `panel` fill, `text`, 600, subtle shadow).
* **Page area** (padding 4×26×16, scrolls): page title row (11/600 uppercase `muted`) with right-aligned **Reset section** button (small); then **setting rows** (padding 14×0, 1 dp bottom border, gap 28): left text block (title 13/600 + description 12 `muted`, ≤ 3 lines) and right control.
* **Footer** (h56): left text "Changes apply immediately" (`muted`) · **Restore all defaults** (secondary) · **Done** (primary).
ids: `settings.dialog`, `.search`, `.nav.<page>`, `.row.<key>`, `.reset.<page>`, `.restoreAll`, `.done`, `.close`.

## Settings catalogue (legacy → new)
| Page | Key (config: use legacy key `[OQ-11]`) | Legacy label | New title | Control | Default | Range |
|---|---|---|---|---|---|---|
| General | `undoDepth` | Undo History Depth | Undo history depth | **Dropdown** with exactly **25 · 50 · 100 · 200 · 500** (PR-19) | **25** | those five values only |
| General | `tooltips` | Use Tooltips | Show tooltips | Switch | on | |
| General | `density` | — | **Interface density** | Segmented **Standard \| Minimal** | **Standard** | `foundations/density.md` |
| General | `theme` | — | **Theme** | Segmented **Light \| Dark \| System** | **System** | |
| 3D view | `viewCube` | Show View Cube | Show view cube | Switch | on | |
| 3D view | `reverseScroll` | Reverse scroll zoom direction | Reverse scroll zoom direction | Switch | off | |
| 3D view | `rotationMethod` | Use new rotation method | **Rotation method** | Segmented **Drag \| Arc-ball** (Drag = legacy "new method" on) | Drag | |
| 3D view | `lights` | Use Lights in 3D View | Lighting | Switch | on | |
| 3D view | `fadePieces` | Fade Out Pieces | Fade out removed pieces | Switch | on | |
| Performance | `workerThreads` | Worker Threads | Worker threads | Slider with value readout (min..max, width 230) | 12 *(legacy default)* | 1–32 `[OQ-11]` (legacy max may be core-count) |
| Performance | `glDisplayLists` | Use openGL display lists | Use OpenGL display lists | Switch | off | |
| (footer) | — | Restore Defaults | Restore all defaults | Button | | |
| (footer) | — | Close | Done | Button | | |
Descriptions: reuse the legacy sentences (visible in `reference/legacy/legacy-settings-dialog.png`), lightly edited for the new titles; the screenshots `09…13-settings-*.png` show the intended text.
*Menu consolidation:* the legacy **Config** menu entry is replaced by this dialog. Any additional preferences the legacy Config/Status menu items expose `[OQ-10]` SHOULD be added as rows on the most fitting page rather than as menu entries.

## Shortcuts page (read-only reference)
Rows `description … KeyBadges`: `1 2 3 4` Fixed · Variable · Erase · Paint; `E` Toggle 3D edit mode; `Shift Click` Quick tool 1 — fixed voxel (3D and 2D); `Alt / ⌘ Click` Quick tool 2 — variable voxel; `Ctrl Click` Quick tool 3 — erase voxel; `PgUp PgDn` Next/previous layer; `F` Fit; `Ctrl ,` Open Settings; `Esc` Close dialog · exit focus mode. (Extend with F2, Del, Ctrl+D, Alt+↑/↓ from `overview/04`.)

## Behaviour
| Trigger | Effect |
|---|---|
| Open | Scrim fades in; focus to search field; Shortcuts/pages remember last page within the session (default General) |
| Type in search | Live filter across **all pages** by title + description (case-insensitive substring); results grouped under page names; nav highlight cleared; empty result: "No settings match “query”" |
| Clear search | Back to selected page |
| Click nav item | Show page, clear search |
| Toggle / dropdown / slider / segmented | Value written to config immediately and applied live (3D redraw, tooltips on/off, scroll direction, lighting, view cube, undo depth trimmed on reduce, worker threads for next solve) |
| Reset section | Restore defaults of rows on that page only |
| Restore all defaults | Restore every setting (confirm not required) |
| Done / close / scrim click / `Esc` | Close (Esc from the search field closes the dialog) |
| `Space`/`Enter` on focused switch | Toggle |
Dialog traps focus; Tab order: search → nav → rows (top→bottom) → footer buttons.

## Acceptance criteria
* AC-C12-01 Every legacy setting is present once with the mapped control, default and live effect; Restore all defaults restores them.
* AC-C12-08 Undo history depth is a dropdown offering only 25, 50, 100, 200, 500 (default 25); choosing a smaller value trims the stored history; a legacy config value not in the list is mapped to the nearest option on load.
* AC-C12-02 Search filters across pages and groups results; clearing restores the page.
* AC-C12-03 Changes apply immediately and persist after restart (config file).
* AC-C12-04 Rotation method Drag/Arc-ball switches 3D rotation behaviour; view cube, lighting, reverse scroll, tooltips settings take effect live.
* AC-C12-05 `Esc`, scrim click, ✕, Done close; `Ctrl+,` and gear open; focus returns to the invoking control.
* AC-C12-06 Reset section affects only its page.
* AC-C12-07 Dark and light themes both render correctly (both ship).
* AC-C12-08 **Theme** offers exactly Light, Dark and System (default System); it applies immediately; **System follows the operating system's light/dark choice when the OS provides one and re-applies when it changes, otherwise it uses Light**. There is no theme toggle button in the top bar.

## Keyboard shortcuts page (mock only)
The mock's Settings dialog carries the **Keyboard shortcuts** list as its last page (`F1` opens it). In the application that list is the **Help ▸ Keyboard shortcuts** window (C22), not part of Settings; the Settings search may still return shortcut rows.
* AC-C12-09 *Interface density* offers Standard | Minimal (default Standard) and applies immediately.


---
<!-- FILE: components/C13-view-cube.md -->

# C13 — View cube (snap to face / edge / corner, Home, 90° steps)

**Phase:** P5. **Legacy references:** `legacy-view-cube-face-on.png`, `legacy-view-cube-iso-face-hover.png`, `legacy-view-cube-edge-hover.png` (L96). **Images (new):** `31-view-cube-face-hover.png`, `31-view-cube-edge-or-corner-hover.png`, `32-view-cube-snapped-face-with-arrows.png`, `01-main-light.png`. Settings switch: ▸ 3D view ▸ Show view cube. Variables/events: `overview/06-ui-contract.md §4`.

## What the legacy cube does (observed in the current build)
* A small **cube whose orientation mirrors the camera**, with **axis-coloured face labels** (`+X −X +Y −Y +Z −Z`: X red, Y green, Z blue) and a small **axis indicator** at its bottom-left.
* It is **snappable**: hovering highlights a **face centre**, an **edge strip** or a **corner** in blue (the highlighted patch is only that region, e.g. a strip along one edge of a face); clicking snaps the camera to that **face view**, **edge view** (between two faces) or **corner view** (three faces, isometric-style).
* A **Home** (house) icon at its top-left returns to the default view.
* When the camera is **exactly face-on** the cube is drawn flat (a square carrying the face label) and **four triangle arrows (▲▼◀▶)** appear around it for **90° steps** to the neighbouring face, plus **two curved arrows** for **90° roll** (counter-clockwise / clockwise) at its top-right.
* **All snaps animate smoothly** — including Home, which is also the "fit shape to view" action.

## New design (behaviour kept, presentation modernised)
**Placement:** in the **top-right corner of the viewport with equal margins to the top and right edges**: the widget's visible content — the Home icon's top edge and the roll icons' right edge (the extreme points when face-aligned) — sits **16 dp from the viewport's top edge and 16 dp from its right edge**. (In the 156 × 170 dp widget canvas this corresponds to a canvas offset of right 2 dp, top −11 dp; the transparent canvas may extend past the card edge, where it is clipped.) Because that puts the cube in the same band as the floating toolbar, when the centre card is **narrower than 640 dp** the widget moves **below the toolbar (canvas top 76 dp)** so they never overlap. Hidden when Settings ▸ Show view cube is off and in Focus-2D (the centre card is too narrow). Drawn by C++ as an overlay (not RML) or as a small custom element; it never covers the pointer events of the toolbar.
**Anatomy** (coordinates in the 156×170 widget):
* **Cube**: centre (78, 100), half-size 30 dp, orientation = current camera, **projected exactly like the main view: perspective when Display ▸ Projection = Perspective (camera distance 5.5 cube half-sizes; a face is visible when its normal faces the eye point, not merely the view axis), orthographic otherwise** — the cube redraws immediately when the projection option changes. Faces get a **visible per-face shading difference**: base tone (light `#d9dde4`, dark `#4a5262`) × `0.72 + 0.42·max(0, n·L)` with a fixed light direction L = (−0.35, 0.75, 0.55) in view space, so the faces always differ in brightness; 1.2 dp edge (`#3a3f4a` light / `#aab1bf` dark). Labels in axis colours (bold, drawn in the face's plane so they skew with it).
* **Hover region**: face centre = inner square (|u|,|v| ≤ 0.56 of the face), edge = strip outside that on one axis, corner = outside on both. The highlight (`#4f7df0`) covers the **whole region on every visible face that touches it**: a face centre highlights one patch, an **edge highlights its strip on both adjoining faces, and a corner highlights the corner patch on all three faces** (those that are visible). A highlighted face-centre label turns white.
* **Home icon**: house glyph, centred at (26, 40) — **moved slightly outward** from the cube, still near it (hit area 30×30); hover = soft accent disc.
* **90° arrows** (only when face-aligned): ▲ ▼ ◀ ▶ triangles (12 dp) centred **20 dp beyond the cube edge** (cube half-size 30 → centres 50 dp from the cube centre; ≈ 14–17 dp of clear space between the cube and each triangle's tip, in both projections); hover = `#4f7df0`.
* **Roll arrows** (only when face-aligned; target look: `target-view-cube-roll-icons.png`): **two small half-circle arrows around the cube's upper-right corner, set slightly outward from the cube** — (a) an **arch over the top**, centred at (103, 44), **radius 9 dp**, running right → left (**counter-clockwise**) with its **arrowhead at the left end pointing down**; (b) a **“)” down the right side**, centred at (132, 72), **radius 9 dp**, running top → bottom (**clockwise**) with its **arrowhead at the bottom end pointing left**. The two are mirror images across the corner's diagonal, so their heads oppose each other. 2.2 dp round-cap stroke; **arrowheads are prominent: 8.5 dp long, 12 dp wide, filled** (clearly larger than the stroke); hit radius 16 dp at (103,40) and (134,72); hover = `#4f7df0`.
* **Axis indicator**: X/Y/Z lines (24 dp, axis colours, letters at 33 dp) at bottom-left (18,152), following the camera including roll.
**Tooltips:** Home — "Home — default view, fitted"; face — "Snap to face view — double-click to stand the label upright"; edge/corner — "Snap to edge view" / "Snap to corner view"; arrows — "Rotate 90°"; roll — "Roll 90°". Cursor `pointer` over any hit region.

## Behaviour
| Trigger | Effect |
|---|---|
| Hover face centre / edge / corner | Highlight exactly that region (see above); tooltip |
| Click **face** | Animate to the view looking at that face (view direction = the face normal) |
| Click **edge** | Animate to the 45° view between the two faces (direction = sum of the two normals) |
| Click **corner** | Animate to the corner view (sum of the three normals) |
| **Double-click** a **face centre** (inner square) | Animate to that face view **and roll the cube so the face's label reads upright** (baseline horizontal, left→right): the roll is the multiple of 90° that brings the label's baseline axis to screen-horizontal. Example: double-clicking the `+X` face turns it 90° **clockwise** so "+X" reads normally. Works whether or not the view is already aligned to that face (if the first click's snap is still running it is retargeted). Double-click on edges, corners, Home or arrows has no extra effect |
| Click **Home** | Animate to the **default view** (isometric: azimuth −32°, elevation 26°, no roll) **and** frame the whole shape (zoom 1, pan 0) — the former "Fit" action |
| Click **▲ ▼ ◀ ▶** (face-aligned only) | Animate 90° so the **neighbouring face** in that screen direction becomes the front face (▶ = the face currently at the right) |
| Click **roll ↶ / ↷** (face-aligned only) | Animate ±90° about the view axis (roll CCW / CW) |
| Key `Home` | same as Home; key `F` = **fit only** (zoom/pan reset, orientation kept, animated) |
| User orbit/pan/zoom (or wheel) while animating | The animation is **cancelled at the current frame** and the user's input continues from that exact orientation — **no jump back to an earlier orientation** |
| Manual orbit after a roll | The **roll is preserved** and the drag is interpreted in the rolled screen frame (a horizontal drag still moves the model horizontally on screen); nothing resets or jumps |
**Animation:** 360 ms, ease-in-out (cubic); yaw/azimuth and roll take the **shortest** angular path; elevation clamped to ±89° (the ±Y faces use the nearest multiple of 90° of the current azimuth so the top/bottom views come out axis-aligned); Home also interpolates zoom and pan. With OS "reduce motion" set, snap instantly.
**Direction → camera:** from the view direction `d` (sum of 1–3 face normals) the camera azimuth is `atan2(−dx, dz)` and elevation `atan2(dy, hypot(dx,dz))` in the renderer's convention (adapt signs to the app's camera). The toolbar's former **Views** menu and **Fit** button are **removed**; the cube plus `Home`/`F` cover them (if the cube is hidden the keys still work).
**Voxel types:** the cube is unchanged for all voxel space types (it orients the world axes, not the voxel lattice).

## Acceptance criteria
* AC-C13-01 Cube shows `+X −X +Y −Y +Z −Z` labels in axis colours on the visible faces, follows the camera (incl. roll), with the axis indicator and Home icon; hidden when the setting is off or in Focus-2D.
* AC-C13-02 Hovering highlights exactly one region (face centre / edge strip / corner) per the thresholds; tooltips and pointer cursor as specified.
* AC-C13-03 Clicking a face/edge/corner animates (360 ms, eased) to the correct face/edge/corner view; the top/bottom faces end axis-aligned.
* AC-C13-04 Home animates to the default view **and** frames the shape (zoom 1, pan 0), smoothly.
* AC-C13-05 When exactly face-aligned the cube is face-on (flat in orthographic; its front face centred in perspective) and the four 90° arrows and two roll arrows appear; they disappear when not aligned.
* AC-C13-06 ▲▼◀▶ bring the neighbouring face in that screen direction to the front; roll arrows rotate ±90° about the view axis; sequences of four return to the start.
* AC-C13-07 Starting a manual orbit/pan/zoom cancels a running animation at its current frame; the next drag continues from the displayed orientation with **no jump to an older orientation**. After clicking an arrow or a roll button, a mouse-drag orbit starts from the arrived orientation and keeps the roll.
* AC-C13-08 `Home` and `F` keys work with the cube hidden; the toolbar has no Views/Fit buttons.
* AC-C13-09 The cube is drawn in perspective when the projection option is Perspective and orthographically otherwise, and updates immediately on change; hover/hit-testing stays correct in both.
* AC-C13-10 Faces show a clearly visible shading difference at every orientation.
* AC-C13-11 A corner hover highlights the corner patch on all visible faces of that corner; an edge hover highlights its strip on both adjoining faces.
* AC-C13-12 Roll icons match the target look: a counter-clockwise half-circle arch above the cube (head down on its left end) and a clockwise “)” arc on its right side (head at the bottom pointing left), mirror images across the corner diagonal; Home sits close to the cube; the 90° arrows sit 20 dp beyond the cube edge (clear of the cube silhouette in both projections).
* AC-C13-13 Double-clicking a face centre animates to that face and rolls the view so the face label is upright (baseline horizontal): for views entered at roll 0, `+X` ends at roll −90° (clockwise), `−X` −90°, `+Z` 0°, `−Z` 180°, `+Y` +90°, `−Y` −90° (with the renderer's current face-axis conventions; the invariant is "label upright"); it works from any starting orientation and does nothing on edges/corners.
* AC-C13-14 Roll icons have radius 9 dp, prominent filled arrowheads (≥ 8 dp long), and sit slightly outward from the cube; Home likewise; none of them touches or overlaps the cube silhouette in perspective or orthographic projection.
* AC-C13-15 The cube setup's visible content is 16 dp (±1) from both the top and the right edge of the viewport (equidistant) in the default layout; below 640 dp centre-card width it sits under the toolbar; it never overlaps the toolbar.


---
<!-- FILE: components/C14-voxel-space-types.md -->

# C14 — Voxel space types (adaptation rules) and File ▸ New choice

**Phase:** P1b (File ▸ New), P4 (grid), P6 (inspector). **Legacy references:** `legacy-new-file-select-space-grid.png` (the official list), `legacy-voxeltype-spheres-transform-tab.png`, `legacy-voxeltype-spheres-size-tab.png`, `legacy-voxeltype-triangles-transform-tab.png`, `legacy-voxeltype-tetrahedra-size-tab.png`, plus the brick screenshots `legacy-size-tab.png`, `legacy-transform-tab.png`. **New images:** `33-voxel-type-spheres.png`, `33-voxel-type-prism.png`, `33-voxel-type-tetoct.png` (the mock switches type with a **mock-only** selector, which stands in for choosing the type in File ▸ New and resets the document to demo shapes of that type; **2D painting, layers, 3D editing, ghosts, slab and dimming are interactive for every type**; transforms, fit & scale operations, mirror/span and non-XY planes are specified but not interactive for the non-brick types).

## Principle
BurrTools supports several **voxel space types** (cell/voxel geometry). The type is a property of the **puzzle file**: it is chosen **once, in File ▸ New** ("Select space grid"), and **can never be changed afterwards** (opening a file loads its type). Therefore:
* The Entities UI has **no control to switch type**. It shows the type **read-only**: a small chip with a lock icon and the type name in the Voxel editor header (`entities.editor.voxelType`; tooltip "Voxel type: {name} — fixed when the file is created (File ▸ New); it cannot be changed afterwards").
* Everything that depends on the type is **derived from a type descriptor** (below), never hard-coded per widget. Adding a type = adding a descriptor (the model/renderer already own the geometry).

## The official types (File ▸ New ▸ "Select space grid", in this order)
1. **Brick** (default, selected in the legacy dialog) · 2. **Triangular Prism** · 3. **Spheres** · 4. **Rhombic Tetrahedra** · 5. **Tetrahedra-Octahedra**
(The legacy dialog's last label reads "Tetrahedra-Octahera" — an apparent typo; the new UI uses **Tetrahedra-Octahedra**. Confirm the intended spelling `[OQ-18]`.)

## File ▸ New — voxel type choice (C00 shell)
File ▸ New presents the existing "Select space grid" step as a restyled **radio-card group** with the five names above in the same order (small cell-shape pictogram + name + one-line description per card; **Brick preselected**). After confirming, the type is stored in the file and is not editable. Anything else the legacy File ▸ New asks is preserved `[OQ-18]`. Ids: `shell.newfile.dialog`, `shell.newfile.type.brick|prism|spheres|rhombic|tetoct`, `.ok`, `.cancel`.

## Type descriptor (C++ data the UI is built from)
`{ id, display_name, cell_shape_2d (square | offset-circle | triangle | split-square | …), supports_scale (bool), transform_set { flip[], rotate[], nudge[] (the directions the type exposes) }, planes[] (valid 2D editing planes), layer_semantics, mirror_span_axes }`.

## Per-type reference (from the legacy screenshots and the product owner's notes)
| Type | 2D grid cell | 3D voxel | Size-tab "Grid" ops (prune / centre / origin) | "Shape" ops (minimize, ×2, ×3) | Flip | Rotate | Nudge |
|---|---|---|---|---|---|---|---|
| **Brick** (default) | square | cube | yes | **yes** | 3 (X, Y, Z) | 6 (X, Y, Z × ±90°) | 6 (±X, ±Y, ±Z) |
| **Triangular Prism** | **triangles** (alternating ▲▽ rows) | triangular prism | yes | **yes** — *scaling is possible: 3 triangles form one bigger triangle* (owner) | 3 | **X: 1, Y: 1, Z: 2** (X/Y single buttons, Z a −/+ pair; angles to be read from source — the mock labels 180° / 180° / ±60° as placeholders) | **8**: a hexagonal set of **6 in-plane directions along the triangle edges** (3 axes × 2: ↖ ↗ ← → ↙ ↘) **+ 2 perpendicular to the plane** (layer down / up) |
| **Spheres** | **circle** on an offset (checkerboard) lattice: cells where x+y is even | sphere-like faceted voxel | yes | **no — the whole "Shape" column is absent** | 3 | 6 (X, Y, Z × ±) | **12**: *up layer* ×4 (→ ↑ ← ↓), *in layer* ×4 diagonals (↗ ↖ ↙ ↘), *down layer* ×4 (→ ↑ ← ↓) |
| **Rhombic Tetrahedra** | no screenshot available `[OQ-19]` | tetrahedral (rhombic lattice) | assume yes | unknown `[OQ-19]` | unknown | unknown | unknown |
| **Tetrahedra-Octahedra** | no screenshot assigned `[OQ-19]` (one of the two tetrahedral types is shown in the references: **square cells each split by both diagonals into 4 triangles, every triangle individually editable**; 3D pieces are tetrahedra; Size tab has both Grid and Shape columns) | tetrahedra / octahedra | yes | **yes** (in the pictured tetrahedral type) | not shown | not shown | not shown |
**Rule for nudges (owner):** nudge buttons follow **the axes the type exposes**; where a type has no orthogonal 3-axis structure the nudge set is the type's own directions (e.g. the prism's 6 in-plane + 2 perpendicular; the sphere lattice's 4 diagonal in-layer moves plus 4 up and 4 down).
### Geometry rules (implemented in the mock; the renderer must match)
* **Spheres — closely packed.** Sites are the grid points with **(x + y + z) even** (face-centred-cubic packing); a sphere's radius is **half the nearest-neighbour distance (√2/2 of the grid step)**, so **neighbouring spheres touch** — in 3D, and in the 2D layer view where circles on the offset lattice touch **diagonally** (radius = 0.7071 × cell pitch). Each layer is the previous layer offset (parity alternates), every sphere has **12 neighbours** (the 12 nudge directions). The 2D cell is the circle; there is no gap between touching circles.
* **Triangular Prism.** Layers are a triangle lattice (alternating ▲▽ cells, every other row offset by half a triangle) extruded one unit along Z; neighbours: the 3 triangles sharing an edge in the layer plus the prisms above and below.
* **Tetrahedral types (placeholder geometry for both until the assignment is confirmed `[OQ-19]`).** Each square cell of a layer is split by both diagonals into 4 triangles; each triangle is the base of a tetrahedron whose apex is above (even layers) or below (odd layers) the square's centre; neighbours: adjacent triangles of the same square, the matching triangle of the adjacent square across the outer face, and the stacked layer across the base.
### 3D editing for non-cubic voxels
* **Polyhedral voxels (prism, tetrahedral):** the picked *face* decides the target — *add* places the voxel across the clicked face (the type's face-adjacency), *erase/paint* act on the voxel itself; only exposed faces are drawn/pickable.
* **Spheres:** a click picks the front-most sphere; *add* chooses, among the sphere's **free** neighbours, the one whose direction best matches the **surface normal at the pointer** (so clicking the upper-right of a sphere adds the neighbour to its upper right); if no free neighbour exists the placement is invalid (red dashed outline, no change).
* The **ghost preview** is drawn with the type's own voxel shape (circle/sphere or polyhedron); the **layer slab** spans the type's world extent and one layer thickness (spheres 1, prisms 1, tetrahedral types 0.6 of a square); Dim-other-layers, quick tools (Shift/Alt/Ctrl), Edit mode, the 2D-hover→3D preview and cursors work as for Brick.
Everything not listed (tool strip, mirror/span toggles, colour, layer strip, grid-size steppers, repair & surface) keeps the same UI for all types; semantics that may differ per type (mirror/span across a non-cubic lattice, which 2D planes are valid, what a "layer" is in an offset lattice) are listed in `[OQ-19]`.

## Adaptation rules
1. **Transform (C04)** is generated from `transform_set`. Brick uses the matrix layout in C04 (per axis: Flip | Rotate −/+ | Nudge −/+). Other types use the **two-block layout**: a *Flip & Rotate* matrix (rows X, Y, Z; Flip column, then the rotate buttons that type offers — 2, 1 or 2 per row) followed by a **Nudge pad** built from the type's directions: Spheres = three labelled rows *UP LAYER / IN LAYER / DOWN LAYER* of 4 arrow buttons; Triangular Prism = rows `↖ ↗`, `← →`, `↙ ↘` plus a `Z` row with ↓ ↑. Arrow buttons are captionless arrow glyphs with explicit tooltips (e.g. "Nudge up one layer, towards +X"). Keep the legacy button→command mapping exactly.
2. **Fit & scale (C03):** *Fit to grid* is always shown. The **Scale sub-section (Minimize, ×2, ×3) is hidden entirely** (not disabled) when `supports_scale` is false (Spheres). Brick, Triangular Prism and the pictured tetrahedral type support it.
3. **2D grid (C08):** the `voxel-grid` element draws and hit-tests the type's cell shape (circle, triangle, split-square triangle) with the same hover outline (the outline follows the shape), stroke painting and ghost behaviour; rulers/axes lines as for Brick unless the lattice requires otherwise `[OQ-19]`. Fixed/variable styling (solid vs hatched + dashed) applies to the cell shape.
4. **3D (C06):** voxel geometry, picking and ghosts come from the renderer/model for the type; ghost previews use the type's voxel shape.
5. **Voxel type chip** is always visible in the Voxel editor header, including Focus-2D.
6. No UI path converts shapes between types.

## Acceptance criteria
* AC-C14-01 The Voxel editor header shows a lock chip with the type's official name; no control anywhere changes the type; hovering shows the explanatory tooltip.
* AC-C14-02 File ▸ New offers the five official types in the legacy order (Brick preselected); the chosen type is stored in the file and displayed after reopening.
* AC-C14-03 Transform section content matches the per-type table (Brick 15 buttons; Spheres 3 flip + 6 rotate + 12 nudge; Triangular Prism 3 flip + 4 rotate + 8 nudge) and each button invokes the legacy command.
* AC-C14-04 Spheres show no Scale sub-section; Brick and Triangular Prism show it; other types follow `supports_scale`.
* AC-C14-05 The 2D grid draws/hit-tests circles, triangles and split squares correctly; hover outline follows the cell shape; a stroke paints the cells it crosses.
* AC-C14-06 Adding a new type requires only a descriptor, not new widget code.
* AC-C14-07 Spheres: neighbouring spheres touch in 3D and neighbouring circles touch (diagonally) in the 2D layer view, with no gaps; 12 neighbours per interior sphere.
* AC-C14-08 For every voxel type: clicking a 2D cell toggles that cell per the active tool; hovering a cell previews it in 3D; Shift/Alt/Ctrl + click in 3D adds Fixed/adds Variable/erases the voxel under the pointer (sphere add = best-matching free neighbour; polyhedron add = across the clicked face); undo granularity as for Brick.


---
<!-- FILE: components/C15-workspace-rail.md -->

# C15 — Workspace rail (Entities · Puzzle · Solver)

**Phase:** P1 (shell). **Replaces:** the centred Entities / Puzzle / Solver segmented control in the top bar (legacy L100). **Images:** `01-main-light.png`, `41-puzzle-tab-chips.png`, `48-puzzle-tab-dark.png`. Contract: `overview/06-ui-contract.md §9`.

## Anatomy
* A **vertical rail on the far left of the window, 60 dp wide** (44 in Minimal), running from under the top bar to the bottom of the window (the status bar sits to its right). Background `panel`, 1 dp `line` right border; the window body (cards) starts right of it. The 1600 × 1000 dp reference layout **includes** the rail (cards: **left 320 / right 340 in every workspace**, Minimal 264 / 320).
* **Three buttons**, stacked from the top with 6 dp gaps and 10 dp top padding, each **52 × 50 dp**, radius 10: a **22 dp icon over a 10.5 dp / 600 caption** (Minimal: 36 dp wide, icon over the caption **rotated 90°**, reading top-to-bottom). Order: **Entities** (`rail-entities.svg`, cube), **Puzzle** (`rail-puzzle.svg`, puzzle piece), **Solver** (`rail-solver.svg`, play).
* **States:** rest = `muted` icon + caption, transparent; hover = `panel2` background, `text` colour; **selected** = `accentSoft` background, `accent` icon + caption **and** a 3 dp `accent` indicator bar on the rail's left edge (6 dp outside the button, 13 dp inset top/bottom) — selection never relies on colour alone; focus = 2 dp focus ring.
* Tooltips: "Entities — create and edit pieces", "Puzzle — choose the result, the pieces and the rules", "Solver — run and browse solutions".

## Behaviour
* Click (or `Ctrl+1 / Ctrl+2 / Ctrl+3`) switches workspace. Each workspace **remembers its own layout state** (selected shape/problem, **side-card collapse**, scroll positions); switching resets Focus modes (Focus 3D / Focus 2D) to the normal layout.
* The 3D viewport card (C06) is the **same component** in Entities and Puzzle; the workspace decides which side cards, toolbar buttons (Edit hidden in Puzzle) and Display items it shows, and what the scene contains (C18).
* The rail stays visible in every layout mode except true full-screen viewport (Ctrl+Space "maximum viewport", which hides all chrome and restores it on exit).
* **Solver** opens the verification workspace specified in **C19–C21** (the rail entry is the same component in every workspace).
* Keyboard: the rail is a toolbar with roving tab-index (↑/↓ move, Enter/Space activate).

## Acceptance criteria
* AC-C15-01 The top bar contains the menu, file name and global buttons but **no workspace tabs**; the rail shows the three workspaces with icon + caption.
* AC-C15-02 The selected workspace is shown with the accent background **and** the left indicator bar; hover/focus states as specified.
* AC-C15-03 Switching workspace is instant, preserves each workspace's own state and resets Focus modes; the 3D viewport, toolbar and view cube remain the same components.
* AC-C15-04 The rail is 60 dp wide (44 in Minimal) at every window size and never overlaps the cards.


---
<!-- FILE: components/C16-puzzle-problems-result-pieces.md -->

# C16 — Puzzle tab · left card: Problems, Result, Pieces

**Phase:** P10b. **Legacy references:** `legacy-puzzle-tab-18-pieces.png`, `legacy-puzzle-tab-7-shapes.png`, `legacy-puzzle-tab-annotated.png` (L100–L111), `legacy-puzzle-problem-details-dialog.png` (L112). **Images:** `41-puzzle-tab-chips.png`, `42-puzzle-repeated-copies-shown-separately.png`, `44-puzzle-table-view.png`, `46-puzzle-problem-2-warning.png`, `65-puzzle-set-range-for-all-dialog.png`. Contract: `overview/06 §10`.

## Model (what the Puzzle tab edits — no voxel editing happens here)
A **file has one or more Problems**. A Problem = **one Result shape** (the shape the pieces must assemble into) + **the pieces** + **colour rules** (C17). A shape is "in the puzzle" when its count is > 0; the **same shape can be used several times** (count N). By default a piece has a **fixed count** (the common case); an optional **range** (min N … max M, N ≥ 0, M ≥ 0) is the exception (C17). A piece belongs to **at most one group or none** (C17). The Result shape is never also a piece of the same problem.
Ids are **positional**: problems are **P1…PN** in list order (like shapes S1…SN); each has an **editable custom label**.

## Layout (320 dp wide card in Standard, 264 in Minimal — the same width as the left card of every workspace, top to bottom)
| # | Section | Content |
|---|---|---|
| 1 | **Problems** (header 36 dp: title, count chip, **+ New** primary) | List of problem rows (34 dp, max ≈ 140 dp then scrolls) |
| 2 | **Result** (36 dp header) | The **Result card** |
| 3 | **In this puzzle** (36 dp header, never wraps: title, count chip, **Chips ⁄ Table** segmented, **⋯** menu) | The pieces used by this problem; flexible height (≥ 96 dp), scrolls |
| 4 | **All pieces** (36 dp collapsible header: title, count chip, hint "drag, double-click or + to add", chevron) | Every shape except the result, as chips; max ≈ 210 dp, scrolls; collapsible |
| 5 | **Summary strip** (fixed at the bottom) | Fit status + totals |

## 1. Problems
Row = **grip** (drag handle, tooltip "Drag to reorder") · **P-chip** (accent-blue chip "P1") · **label** · hover actions **Rename, Duplicate, Delete** (Delete disabled when only one problem). Click selects (the whole tab follows); **double-click** or the pencil renames inline (Enter commits, Esc cancels); **drag the grip** to reorder (ids renumber); `F2` renames; `Delete` removes (undoable, confirm only if it would leave no problem). **+ New** adds an empty problem (result = first shape, no pieces) and selects it; **Duplicate** copies result, counts, groups and colour rules.
*Replaces legacy:* New / Delete / Copy / Label / ←→ (L101, L102).

## 2. Result card
Shape chip (shape colour, id) · **name** (shape label or "Result piece") · "N voxels · can hold A–B" (fixed count … fixed + variable count) · **Change…** button → popover list of all shapes (chip, label, voxel count); choosing one makes it the result. Also available as "Set as result" in any piece's context menu. A shape chosen as result is removed from this problem's pieces. *Replaces legacy:* the "Result: S1 – Target" bar + **Set Result** (L103).

## 3. In this puzzle (the used pieces)
**Chips view** (default): **fixed-size tiles 96 × 50 dp** (78 × 46 in Minimal; 3 per row), no hover resizing. Tile = 6 dp shape-colour stripe · grip dots · **id** · label (ellipsised) · **G-badge** (e.g. "G1") when the piece is in a group · a stepper row **[−] count [+]** that is **always visible**: the count reads **×N** for a fixed count or **min–max** for a range; **+/−** change the count (for a range: the maximum; the minimum follows if needed); at **0 the piece leaves the puzzle**. Click selects the piece (drives the selected-piece card and the 3D "Selected" slot); right-click menu: *One more, One fewer, Remove from puzzle, Set as result*. Tiles are draggable (see below).
**Table view** (the power-edit view; replaces the legacy Problem Details dialog): columns **Shape** (shape-coloured cell, "S6 – A") · **Count** (number input; for a range two inputs "min – max") · **Range** (checkbox) · **Group** (select: –, G1…, + New). Inputs commit on change/Tab; clicking a row selects the piece. The view choice persists per session.
**⋯ menu:** *Add one of each piece* (+1 for every shape, adding the missing ones) · **Set range for all pieces…** (opens a small dialog with two inline-label steppers **Min** (≥ 0) and **Max** (≥ 1, raised to Min if lower); **Apply** gives every piece currently in the puzzle that minimum and maximum — legacy *bulk range* window; Cancel closes) · *Remove all pieces*.
**Empty state:** "No pieces yet. Add them from All pieces below — drag, double-click, or press +."

## 4. All pieces (the library)
Wrapping **chips** (28 dp high): grip dots · 10 dp shape-colour square · **id** · label · a fixed-width (34 dp) action button that reads **+** when the piece is not in the puzzle and **×N +** when it is (the width never changes, so nothing jumps). **Click** selects the piece (without adding); **+**, **double-click**, or **dragging the chip into "In this puzzle"** adds one copy (a piece already present gains +1). Pieces already used are visually quieter (panel background) but stay listed. Right-click menu as for tiles. The header toggles collapse/expand.
**Drag & drop (visible affordances):** every draggable item shows **grip dots** and the `grab` cursor with a descriptive tooltip; while dragging, the **valid drop zone gets a dashed accent outline and `accentSoft` fill** — *In this puzzle* when dragging from All pieces; *All pieces* (list or its header) when dragging a tile out. Dropping a tile on All pieces **removes** the piece (count 0, group cleared). Every drag action has a non-drag equivalent (+, double-click, − at 1, context menu); the problem rows reorder by dragging their grip.

## 5. Summary strip
Icon + two lines: **"Pieces can fill the result"** (✓ circle, `ok` colour) or **"Pieces are too large for the result"** / **"Pieces cannot reach the result"** (⚠ triangle, amber), then "Result A–B voxels · Pieces n = a–b → p–q voxels · **Holes: H**". *Holes* is **derived, never configured**: when every count is fixed and the result has only fixed voxels, **Holes = result voxels − piece voxels** (or "Pieces exceed the result by k"); otherwise "Holes depend on the counts". Feasibility = pieces' max voxels ≥ result's fixed voxels **and** pieces' min voxels ≤ result's total voxels. **There is no "maximum number of holes" setting** (legacy field dropped — it is determined by the counts).
The **status bar** (C10) shows the legacy sentence: *"Problem P1 result can contain 19 voxels, pieces (n = 4 - 5) contain 18 - 22 voxels"* (ranges as "a - b").

## Voxel types
Identical for every voxel space type; counts and voxel totals use the type's voxels (C14).

## Acceptance criteria
* AC-C16-01 Problems list: positional P-ids with editable labels (double-click / pencil / F2), drag-reorder renumbers, New / Duplicate / Delete work and are undoable.
* AC-C16-02 Result card shows the result and its voxel range; Change… (or a piece's "Set as result") swaps it and removes it from the pieces.
* AC-C16-03 Used pieces appear as fixed-size tiles with an always-visible stepper; changing counts never moves other tiles; count 0 removes the piece.
* AC-C16-04 A piece can be added from All pieces by +, double-click, or drag; removal by − at 1, context menu, or dragging onto All pieces; drop zones are visibly highlighted while dragging.
* AC-C16-05 Chips ⁄ Table toggle switches representation of the same data; table edits (count, range, group) are reflected in chips, scene and summary immediately.
* AC-C16-06 Summary strip and status bar report voxel totals, fit status and the derived hole count; no hole-count input exists.
* AC-C16-07 Selecting a piece anywhere (tile, chip, row, 3D copy) highlights it everywhere and updates the selected-piece card and 3D "Selected" slot.
* AC-C16-08 *Set range for all pieces…* sets min/max on every piece in the puzzle (Max never below Min, Max ≥ 1) and the tiles, table, scene copies (optional copies) and summary update.


---
<!-- FILE: components/C17-puzzle-piece-settings-groups-colours.md -->

# C17 — Puzzle tab · right card: Selected piece, Groups, Colour rules

**Phase:** P10c. **Legacy:** L105–L109, L112. **Images:** `43-puzzle-selected-piece-range.png`, `41-puzzle-tab-chips.png`. Contract: `overview/06 §10`.
Card width **340 dp** in Standard, 320 in Minimal (same as every workspace), header "Selected piece", content scrolls.

## 1. Selected piece
Header: shape chip · name (label or "Piece") · "N voxels · F fixed · V variable". If nothing (or the result) is selected: hint "Select a piece in the lists or click it in the 3D view…".
* **Fixed count (default):** label "Copies in the puzzle", a **stepper (150 dp: − value +)**, subtitle "Fixed count" (or "Not in this puzzle" at 0).
* **Range (exception):** switch **"Allow a range (min–max) instead of a fixed count"**. When on, the stepper is replaced by **two steppers side by side, each with its label inline inside the box** — *[ − Min  0 + ]  [ − Max  1 + ]* — the label is part of the control (not a caption above it); the field is borderless inside the stepper box. Rules: min ≤ max, both ≥ 0; turning the switch off sets min = max = max. A range of 0–N makes the piece optional (replaces legacy **min=0**).
* **Exclusive group:** a segmented row **[None] [G1] [G2]… [+ New]** — a piece is in **at most one** group; **+ New** creates the next group and puts the piece in it. Selecting None leaves the group.
*Replaces legacy:* +1 / −1 / min=0 / all+1 / Clr / Detail and the "Gr" columns (L105, L106, L112); ← → (order) are dropped — pieces are listed in shape order.

## 2. Exclusive groups (section hidden until a group exists)
One card per group: "Group n — pieces in a group are exclusive", a delete button, and the member pieces as mini tiles with their counts; empty groups say "No members — pick this group on a piece". Deleting a group clears the membership. **A group is only a logical grouping in the Puzzle tab** (it is meaningful in the Solver, where pieces of one group are not disassembled against each other — Solver design is out of scope now). Typically a puzzle has zero or one group covering a subset of pieces; groups never affect the voxel totals.

## 3. Colour rules
A **matrix**: **columns = result colours, rows = piece colours** (swatches 24 dp; colours = the document's colour list, starting with Default); each **cell is a toggle (34 × 28 dp)** — ticked = a piece voxel of the row colour **may sit on** a result voxel of the column colour. Default = **each colour on itself** only. Buttons: **Each colour on itself**, **Allow all**, **Clear**. Tooltips "Piece C2 on result C3: allowed / not allowed". *Replaces legacy:* the colour chips + piece→result pair list + **Sort by Piece / Sort by Result** (sorting is unnecessary in a matrix) (L108, L109). Colour rules are per problem.

## Acceptance criteria
* AC-C17-01 The default editor is a single fixed-count stepper; ranges appear only after enabling the switch.
* AC-C17-02 In range mode the labels "Min" and "Max" are inside their stepper boxes (inline), min ≤ max is enforced, and turning the switch off collapses to a fixed count.
* AC-C17-03 A piece can be in none or one group; assignment via None / Gn / + New; the Groups section appears only when a group exists and lists members; deleting a group clears members.
* AC-C17-04 The colour matrix toggles individual piece→result colour permissions; the three helper buttons set identity / all / none; rules are stored per problem.
* AC-C17-05 There is no "maximum holes" control anywhere.


---
<!-- FILE: components/C18-puzzle-3d-scene.md -->

# C18 — Puzzle tab · 3D scene (read-only)

**Phase:** P10d. **Legacy:** L110 (the result, the selected piece and all pieces shown together). **Images:** `41-puzzle-tab-chips.png`, `42-puzzle-repeated-copies-shown-separately.png`, `45-puzzle-rotation-keeps-layout.png`, `47-puzzle-voxel-type-spheres.png`, `47-puzzle-voxel-type-prism.png`, `47-puzzle-voxel-type-tetoct.png`. The viewport card, floating toolbar, view cube (C13) and camera are the **same components as C06**.

## Content and order
1. **Result** — the problem's result shape, top-left, captioned "RESULT · S5".
2. **Selected** — the piece selected in the lists, to the right of the result, captioned "SELECTED · S10", framed with a solid accent rounded rectangle (omitted if nothing/the result is selected).
3. **All pieces** — **every copy of every piece used by the problem, each as its own object**: a shape with count 3 appears **three times** (captions "S1 · 1/3", "S1 · 2/3", "S1 · 3/3"; a single copy is captioned just "S3"). For a **range**, `max` copies are shown; copies beyond `min` are **optional copies** drawn translucent with dashed outlines and the caption suffix "(optional)". Pieces are laid out in a grid below (5 per row, ≈ 5.9 × 5.6 grid units apart) in shape order, then copy order.

## Camera behaviour (important)
* **The arrangement is fixed on screen.** Each object has a fixed anchor position; **orbiting (and view-cube snaps, roll, Home) rotates every object about its own centre, in place** — the layout is *not* rotated as one rigid group. **Pan and zoom move/scale the whole layout as a group.**
* Object frames (hover/selection outlines and hit boxes) are **centred on the anchor** so they don't jitter while the object rotates.
* Perspective/orthographic follows Display ▸ Projection; all objects use the same camera rotation, so every piece is seen from the same direction.

## Interaction
* **Hover** a non-result object: dashed accent frame + pointer cursor; **click** selects that piece (all of its copies get a solid frame; list, tile, table row, "Selected" slot and the selected-piece card follow). Clicking the result or empty space does nothing.
* **No editing:** the Edit button, edit cursors, Shift/Alt/Ctrl voxel modifiers, layer slab, dim-other-layers, axes and grid-boundary overlays are **absent** in this workspace (keys `1–4`, `E` are ignored).
* **Toolbar:** Orbit, Pan, Display, Focus (no Edit). **Display menu:** Projection and Voxel colour only (Show ▸ items are hidden). The viewport tag shows "P1 Small · Result S5".
* **Hint:** "Click a piece to select it · drag to rotate every piece in place" + the navigation line.
* **Voxel types:** every voxel space type is supported — Brick cubes, **closely packed spheres**, triangular prisms and tetrahedral voxels are drawn with the same per-type geometry as the Entities 3D view (C14), each object centred on its own bounding box; the scene works identically.
* **Scale:** the whole layout fits the viewport at zoom 1 and re-fits as the number of copies grows.

## Acceptance criteria
* AC-C18-01 The scene shows the result, the selected piece, then one separate object per copy of every used piece; copies are captioned "k/N"; optional range copies are translucent/dashed and captioned "(optional)".
* AC-C18-02 Orbiting rotates each object about its own centre while anchors stay fixed on screen; panning and zooming move/scale the layout as a group; frames stay centred on their anchors during rotation.
* AC-C18-03 Hover frames, click-to-select and list/scene selection sync work for every copy; the result is not selectable.
* AC-C18-04 No editing affordance exists in the Puzzle viewport (no Edit button, modifiers, slab, dim, axes/bounds items).
* AC-C18-05 The scene renders correctly for all voxel space types (spheres touching; prisms; tetrahedral voxels).


---
<!-- FILE: components/C19-solver-left-card.md -->

# C19 — Solver tab · left card: Problem, What to check, Search options, Run, Step-by-step explorer

**Phase:** P11b. **Legacy references:** `legacy-solver-tab.png`, `legacy-solver-tab-annotated.png` (L120–L128). **Images:** `51-solver-list-view.png`, `58-solver-running.png`, `61-solver-dark.png`. Contract: `overview/06 §11`. The Solver **verifies** the puzzles built in the Puzzle tab; **nothing is edited** here. Card width **320 dp** in Standard, 264 in Minimal (same as every workspace). The card is a single scrolling column of sections separated by 1 dp lines (padding 12 × 14, section label 11/600 uppercase `muted`).

## 1. Problem
A full-width **selector button** (10 dp radius, `panel2`): accent-blue **P-chip** ("P1"), the problem **label** (bold) and "Result S5 · N piece shapes", chevron. Click → popover "Solve problem" listing every problem (chip + label). Switching problem loads that problem's own results (each problem keeps its own solution list). *Replaces legacy:* the "Parameters" problem list (L121).

## 2. What to check (radio cards)
Two **radio cards** (PR-27): **Assemblies** — "Find every way the pieces fit the result." · **Assemblies and disassembly** — "Also check that each assembly can be taken apart, moving pieces along the voxel axes." (the axes are those of the file's voxel space type, C14). Selected = accent border + `accentSoft` fill + filled radio dot.
When *Assemblies and disassembly* is selected a sub-switch appears: **"Don't keep the disassemblies"** (help: "Check disassemblies but keep only their level, not the moves. Recompute them later with “Analyse”."). Turning it on implies disassembly checking.
*Replaces legacy:* the **Disassemble** and **Drop Disassemblies** check boxes (L122).
**Meaning of the checks** (from the BurrTools user guide): *Assemblies* = every placement of the pieces (respecting their count ranges and colour rules — **groups are not considered**) that exactly fills the result. *Disassembly* = whether an assembly can be taken apart piece by piece by moves along the voxel axes; only assemblies that can be taken apart are listed as solutions (so 10 assemblies may yield 0 solutions). With **groups**, the members of a group are removed together, and the pieces of a group need not be separable from each other at the end `[OQ-31]`.

## 3. Search options (collapsible, collapsed by default)
Header "Search options" with chevron and, when collapsed and not at defaults, "· n changed". Rows (13 dp title + 11 dp muted help; switches right-aligned):
| Row | Control | Default | Help text | Legacy |
|---|---|---|---|---|
| Just count | switch | off | "Only count solutions; they are dropped as soon as they are found." | Just Count |
| Keep mirror solutions | switch | off | "Do not remove solutions that are mirror images of others (useful with piece ranges)." | Keep Mirror Solutions |
| Keep rotated solutions | switch | off | "Do not remove solutions that are only rotations of others." | Keep Rotated Solutions |
| Thorough rotation check | switch | off | "More thorough, slower check for duplicate (rotated) solutions." | Expnsv Rot Check |
| **Sort found solutions by** | select | *Moves for complete disassembly* | "Order the solver keeps them in" (+ "— level and moves need the disassembly check" in Assemblies mode) | Sort by |
| Drop below level | stepper (min 0, step 1) | 1 | "Solutions with a lower level are dropped" `[OQ-26]` | Drop |
| Limit | stepper (min 1, step 10) | 100 | "Keep at most this many solutions" | Limit |
**Sort-by options:** *Search order (original id)* · *Number of pieces in the assembly* · *Level* · *Moves for complete disassembly*. The first two work in **both** modes; **Level** and **Moves** need the disassembly check — in Assemblies mode they are shown **disabled with the suffix "(needs disassembly)"** and the effective choice falls back to *Search order*. (This option decides how the solver ranks and which solutions it keeps when the limit is reached; it is separate from the display sort in C20.)

## 4. Run
Four buttons in one row (34 dp, grid 1 : 1.4 : 1 : 1): **Prepare**, **Start** (primary), **Continue**, **Stop**; below: a **progress bar** (8 dp) with the percentage (one decimal, "0.0 %"), and a **status grid**: *Activity* · *Assemblies* · *Solutions* · *Time used* · *Time left*.
**State machine and enabling:**
| State | Activity text | Prepare | Start | Continue | Stop |
|---|---|---|---|---|---|
| idle / finished | "finished" (or empty before the first run) | ✓ | ✓ | – | – |
| preparing | "preparing" | – | – | – | – |
| ready | "ready (placements prepared)" | ✓ | ✓ | – | – |
| running | "assembling" → "disassembling" (or "counting" with Just count) | – | – | – | ✓ |
| paused (after Stop) | "stopped" | ✓ | – | ✓ | – |
Assemblies/Solutions counters rise while running; **Solutions** reads "—" in Assemblies mode. *Time left* is an estimate shown after ≈ 6 % progress. Results appear in the right card **as they are found**. **Prepare** builds the search structures only; **Start** runs from the beginning (clearing earlier results); **Continue** resumes a stopped search `[OQ-29]`. Buttons have tooltips ("Build the search structures without solving", "Run the search from the start", "Resume a stopped search", "Pause the search").

## 5. Step-by-step explorer (advanced)
Section label "Step-by-step explorer — advanced"; three equal buttons + a one-line note ("Placements browses the assembly search; Movements browses the disassembly search; Step advances the assembler one step."):
* **Placements** — enabled in *ready* or *paused*; toggles the **explorer drawer** (C21) in Placements mode.
* **Movements** — enabled when the selected solution has a **stored disassembly**; toggles the drawer in Movements mode. It closes automatically if the selection loses its disassembly.
* **Step** — enabled in *ready*/*paused*; advances the assembler one step; the 3D view enters **assembler-state mode** (C21) showing the pieces placed so far (cycles 0…n).
*Replaces legacy:* Placements / Movements / Step (L126), which opened separate windows.

## Acceptance criteria
* AC-C19-01 The card shows Problem, What to check, Search options, Run and Step-by-step explorer in that order at 320 dp (264 in Minimal); switching problem loads that problem's results.
* AC-C19-02 The two radio cards map to the legacy Disassemble check box; "Don't keep the disassemblies" appears only with disassembly and implies it.
* AC-C19-03 Search options are collapsed by default with a "n changed" hint; defaults and help texts as tabulated; Level/Moves sort options are disabled with "(needs disassembly)" in Assemblies mode and fall back to Search order.
* AC-C19-04 Run buttons are enabled exactly as the state table says; the progress bar, counters, activity text and time estimates update while running; results appear incrementally.
* AC-C19-05 Placements/Movements/Step have the stated enabling rules; Movements closes itself when the selection has no stored disassembly.


---
<!-- FILE: components/C20-solver-right-card.md -->

# C20 — Solver tab · right card: Solutions, Disassembly player, Manage, Piece display

**Phase:** P11c. **Legacy:** L129–L136. **Images:** `51-solver-list-view.png`, `52-solver-slider-view.png`, `54-solver-100-solutions-list.png`, `55-solver-none-disassemble.png`, `57-solver-manage-menu-delete-preview.png`, `53-solver-wireframe-xray-chips.png`, `60-solver-no-assemblies.png`. Card width **340 dp** (320 in Minimal). All **browse / play / manage** interaction lives in this card so the user moves back and forth within one screen region; the 3D view only shows the result.
Vertical layout (fixed sections do not change height with content): header (36) · **selection area** (flexible, ≥ 150, scrolls) · **Disassembly player (fixed 112; Minimal 84)** · sort row (Slider view only) · **actions row** · **Piece display header (36) + list (fixed 104, scrolls; Minimal 76)**.

## Header
"Solutions" + count chip (number of **shown** solutions) + a segmented **List | Slider** control (List default; the choice persists). Tooltips: "All solutions at a glance" / "One solution at a time with a slider".

## What is listed
`shown = found assemblies`, **filtered to those that can be taken apart when disassembly checking (or "don't keep") is on**; sorted by the display sort below. Selecting a solution never changes the list; deleting removes entries. With **Assemblies** only, every assembly is listed.

## Selection area — List view (default)
A table: sticky header row + rows (**28 dp**; 26 in Minimal), keyboard-focusable listbox:
| Column | Content |
|---|---|
| **#** (44) | assembly id (bold) — the original search order |
| **Level** (flex) | disassembly level "X.y.z…" in a monospace face (moves to remove the first piece, then the second, …), ellipsised with full text in the tooltip; "—" when unknown |
| **Moves** (96) | total moves + a **proportional bar** behind the number (relative to the largest in the list); "—" when unknown |
| **Pcs** (40) | number of pieces used by the assembly |
| **DA** (30) | status icon: ✓ *stored* · ◐ *level only* · ✕ *cannot be disassembled* · · *not checked* (tooltips) |
**Sorting:** the column headers (#, Level, Moves, Pcs) are the sort control — click to sort (arrow ▲/▼ on the active column), click again to **reverse**; default direction is ascending for # and **descending ("best first") for Level, Moves, Pcs**; Level/Moves are disabled until some solution has disassembly information. **Selection:** click a row (this also gives the list keyboard focus); the **Solver keyboard model** below applies; **double-click** selects and **plays** the disassembly (if stored). The selected row has an accent border and `accentSoft` fill. Footer: "N solutions · ↑↓ solution · ←→ move · Space play" (hidden in Minimal).
**Delete preview:** while a *Remove assemblies* item of the Manage menu is hovered, the rows it would delete are tinted red and struck through (see below).

## Selection area — Slider view
Title row: **"Solution 3"** and "of N" (adds "assemblies" in Assemblies mode); a control row: **previous** button, a **range slider** (1…N), **next** button; then six **detail tiles** (2 × 3): Assembly #, Level, Moves, Pieces, Disassembly (*stored / level only / none exists / not checked*), Position (i / N). **Scrubbing must be smooth:** dragging the slider updates the 3D view, title and tiles **continuously (at most once per frame) without rebuilding or replacing the slider control**; selecting a solution resets the move position to 0 and stops playback. Below the area a **Sort by** segmented control (Number · Level · Moves · Pieces) shares the same state as the List headers (active segment shows an **inline** ▲/▼ that never wraps; label and arrow stay on one line).

## Empty states (replace the list/slider)
| Situation | Message |
|---|---|
| Searching | "**Searching…** Solutions appear here as they are found." |
| No assemblies | "**No assemblies found** The pieces cannot fill the result with the current ranges and colour rules. Review the Puzzle tab." |
| Assemblies exist but none can be disassembled | "**No solutions to show** N assemblies were found, but none can be disassembled. Switch to “Assemblies” to list them anyway." + button *Show the assemblies* |
| Never run | "**No results yet** Press Start to search for assemblies." |

## Disassembly player (fixed height 112 dp; 84 in Minimal)
Title row: "DISASSEMBLY PLAYER" (left) and **"Move 14.2 / 19"** (right, above the controls). Controls row: five **transport buttons** (30 dp): *start* (assembled position) · *back one move* · **play/pause** (primary) · *forward one move* · *end* (fully disassembled), then a **speed select (0.5×, 1×, 2×, 4×, 8×)** at the right. Below, a **full-width move slider** (0…total moves, step 0.05). Playback runs at ≈ 1.2 moves/s × speed and stops at the end; dragging the slider pauses. Moves are the sequence of piece/group translations along the voxel axes; the 3D view shows the pieces at the fractional position. *Replaces the legacy Move slider (L130, "Move 19 (14.2)").*
**Alternate states (same height):** no stored disassembly → message + button **Analyse disassembly** ("Disassembly was not checked for this assembly." / "No disassembly stored." / "Only the level was kept for this solution — the moves were not stored."); cannot be disassembled → "This assembly cannot be taken apart." (no button); nothing to play → "Nothing to play yet — run the solver to find assemblies."

## Actions row
**Manage solutions ▾** (opens the menu below; disabled with no solutions or while analysing) and **Export to STL** (opens the existing legacy *Export Solution to STL* dialog; disabled with no solutions).
**Manage menu** (popup above the button; each item shows its **legacy abbreviation** at the right) — *the order rule for Before/After is confirmed; the remaining semantics (w/o DA and the five disassembly actions) are still to be verified against `mainwindow.cpp` `[OQ-25]`*:
* **Remove assemblies** (*Before* and *After* are relative to the **current sorted order of the list** — confirmed by the product owner — i.e. the rows above / below the selected row as displayed, whatever the display sort is): *This solution only* (At) · *All before this one* (Before) · *All after this one* (After) · *All without a disassembly* (w/o DA) · *All solutions* (All). Every removal asks for **confirmation** (modal: "Delete n solutions? … Deleted solutions cannot be restored." — Cancel / **Delete** in the danger colour) because solution deletion is **not undoable**; a toast confirms "n removed".
* **Disassemblies:** *Delete the disassembly of this solution* (D DA) · *Delete all disassemblies* (D A DA) — keep the assemblies · *Analyse this assembly's disassembly* (A DA) · *Analyse the disassembly of every assembly* (A A DA) · *Analyse only the missing disassemblies* (A M DA). Analysing shows "Analysing n assembl(y/ies)…", disables Manage, then marks each assembly *stored* or *cannot be disassembled* (and, in disassembly mode, drops the ones that cannot).
**Hover preview:** in List view, hovering a *Remove* item tints and strikes through exactly the rows that would be deleted.

## Piece display (fixed height 104 dp, scrolls; 76 in Minimal)
Header "Piece display" + **Show all** button (resets every piece to solid). Wrapping **chips** (30 dp) for the pieces **present in the current assembly** (copies of a ranged piece are named **S5.1, S5.2, …**; an assembly that uses fewer copies simply lacks the higher ones). **Click cycles solid → wireframe → hidden**; the state is shown **by the chip's appearance, not by words**:
* **solid** — filled colour swatch, normal chip;
* **wireframe** — **dotted chip border in the piece colour** and a hollow dotted swatch;
* **hidden** — **greyed-out chip** (low opacity, grey) with a hollow dashed swatch.
Tooltip: "S3 — wireframe. Click to cycle: solid → wireframe → hidden". Visibility is per piece id and persists while browsing solutions.

## Solver keyboard model (list, solution slider and move seekbar share one key map)
| Key | Action |
|---|---|
| `Space` / `Enter` | Play / pause the selected solution's disassembly (message if none is stored) |
| `←` / `→` | One move back / forward (stops playback; snaps to whole moves) |
| `Shift+←` / `Shift+→` | Jump to assembled / fully disassembled |
| `↑` / `↓` | Previous / next solution (resets the move position) |
| `Home` / `End` | First / last solution — on the move seekbar: start / end of the moves |
| `PgUp` / `PgDn` | 8 solutions back / forward |
| `Del` / `Backspace` (list) | *Remove this solution* (Manage ▸ At), with confirmation |
| `Esc` | Pause playback |
The slider and seekbar **override their native arrow behaviour** with this map. Piece chips are focusable (`Space`/`Enter` cycle the state, `←`/`→` move between chips).
**Sticky keyboard focus:** while the list, slider, seekbar or a chip has focus, **pressing, dragging or clicking anywhere in the 3D viewport — canvas, view cube, toolbar — never moves keyboard focus**, and the player's transport buttons do not take focus from the list/slider either; re-rendering (sorting, playback start/stop, chip changes) restores focus to the same control. Focus styling: list = 2 dp `accentSoft` inner ring + accent outline on the selected row; slider/seekbar/chips = 2 dp `accent` focus-visible outline.

## Acceptance criteria
* AC-C20-01 The List shows id, level, moves (with bar), pieces and disassembly status for every shown solution; header clicks sort and reverse; keyboard/double-click behave as specified; large lists scroll smoothly (as the mock does).
* AC-C20-02 The Slider view scrubs through all solutions smoothly without recreating the slider; title/tiles/3D update continuously.
* AC-C20-03 With disassembly checking on, only disassemblable assemblies are listed (possibly 0, with the stated empty state and shortcut); with Assemblies only, all are listed.
* AC-C20-04 The player has a fixed height in all states; the move counter is above a full-width slider; speeds 0.5×–8×; Analyse appears only when a disassembly could exist.
* AC-C20-05 Manage actions behave as listed, confirm destructive removals, show the hover preview, and Analyse updates the DA status.
* AC-C20-07 The keyboard map works identically on the list, the solution slider and the move seekbar; after orbiting, panning or clicking toolbar/view cube with the mouse, the same keys still act on the focused Solutions control; focus survives re-renders.
* AC-C20-08 Heights of the player and piece display are fixed per density (112/104 Standard, 84/76 Minimal).
* AC-C20-06 Piece display height is fixed; chips show solid / dotted-border / greyed-out appearances with no state words; ranged copies are S5.1, S5.2…; Show all resets.


---
<!-- FILE: components/C21-solver-3d-scene-and-explorer.md -->

# C21 — Solver tab · 3D scene (read-only), Display options, explorer drawer

**Phase:** P11d. **Legacy:** L137 (3D viewer with view cube; the screenshot is mid-move). **Images:** `51-solver-list-view.png`, `53-solver-wireframe-xray-chips.png`, `56-solver-display-menu-xray.png`, `58-solver-running.png`, `59-solver-explorer-drawer.png`, `60-solver-no-assemblies.png`, `61-solver-dark.png`. The viewport card, floating toolbar, view cube (C13), camera and Home/Fit keys are the **same components as C06**; the workspace decides the content.

## Scene
* Shows the **selected solution**: all its pieces in their assembled positions, offset by the **current move position** of the player (C20) — pieces slide along the voxel axes; between integer positions the motion is interpolated. **One shared camera rotates the whole assembly as a single object** (unlike the Puzzle scene, C18). Piece colours are the piece's shape colour (Display ▸ Voxel colour applies); boundaries between touching pieces stay visible.
* **Motion frame follows shape weight (Shapes ▸ W+/W−):** during the disassembly animation **heavier shapes stay in place and lighter shapes move** relative to them (e.g. a tray stays still while the pieces leave it), instead of letting every piece fly back and forth. The player's move sequence is the same; only which piece is shown as stationary changes.
* **Piece states** come from the Piece display (C20): *solid* (shaded faces), *wireframe*, *hidden* (not drawn).
* **Wireframe rendering:** a wireframe piece draws **every edge of its hull — back edges as well as front (back edges slightly fainter)** — with a very faint (≈ 6 %) face tint. **Display ▸ "Show through solid pieces"** (menu section *Wireframe pieces*, **on by default**) draws wireframe pieces **on top** of the solids (X-ray) so a buried piece stays readable; when **off**, solid pieces in front hide the wireframe like any other geometry.
* **Assembler-state mode** (entered by **Step** in C19): shows the pieces placed so far in their assembled positions, with the caption "Assembler state — k of n pieces placed" (accent, top centre); leaves the mode when a solution is selected.
* **Explorer drawer** (below) may dock over the lower part of the viewport; the 3D view stays interactive above it.
* **Empty scene messages:** "Run the solver to see solutions here" · "Searching…" · "No assembly can be taken apart — nothing to show".
* **Read-only:** no Edit button, edit cursors, voxel modifiers, layer slab, dim, axes/bounds overlays; keys `1–4`, `E` are ignored; hover does nothing.
* **Viewport tag** (top-left): P-chip + problem label + "· Solution 3 of 5". **Hint:** the navigation line only ("Drag orbit · Shift/middle-drag pan · Wheel zoom"). **Toolbar:** Orbit, Pan, Display, Focus. **Display menu:** *Wireframe pieces ▸ Show through solid pieces*, *Projection*, *Voxel colour* (the Show ▸ items are hidden).
* **Status bar:** "Problem **P1** — 12 assemblies found, 5 can be disassembled · Activity: finished".
* **Voxel types:** the scene uses the voxel geometry of the file's type (C14), like the Entities and Puzzle views.

## Explorer drawer (Placements / Movements) — layout only
Replaces the two legacy browser windows. A **docked panel over the bottom of the viewport**: height **250 dp**, 12 dp inset left/right/bottom, radius 14, shadow; header: segmented **Placements | Movements**, a muted caption ("Assembly search tree" / "Disassembly search (breadth-first)"), spacer, close ×. Body: left list (230 dp; rows with a label and a count, selected row accent) and a right pane with the controls **◀ Back · Step ▶ (primary) · Run ▶▶ · Reset**, an info paragraph describing the current depth/state, and a row of numbered **depth chips** (reached ones filled). *Placements* lists, per piece, the number of possible placements and the depth of the assembly search; *Movements* lists the breadth-first levels of the disassembly search with their state counts, and the 3D view shows a representative position of the selected state. **The contents of the legacy windows are not specified yet `[OQ-28]`**; this section fixes only where they live and how they are operated. The drawer can be open while the player and lists are used.

## Acceptance criteria
* AC-C21-01 The scene shows the selected solution at the player's move position with the whole assembly rotating as one object; piece states (solid/wireframe/hidden) are honoured.
* AC-C21-02 Wireframe pieces show all hull edges (back edges fainter) with a ≈ 6 % tint; the Display option "Show through solid pieces" (default on) toggles X-ray vs normal occlusion.
* AC-C21-03 Step shows assembler-state mode with the caption and the placed pieces; selecting a solution leaves it.
* AC-C21-06 With unequal shape weights, heavier shapes stay stationary and lighter ones move during playback.
* AC-C21-04 The Solver viewport has no editing affordances; toolbar and Display menu are as listed; tag, hint and status text as specified.
* AC-C21-05 The explorer drawer docks over the viewport (250 dp) with the specified header, list, controls and depth chips; opening/closing never changes the card layouts.


---
<!-- FILE: components/C22-keyboard-shortcuts-help.md -->

# C22 — Keyboard shortcuts (help window)

**Phase:** P8 (polish). **Images:** `74-keyboard-shortcuts-help.png`, `75-keyboard-shortcuts-search.png`. **Location:** in the application **Help ▸ Keyboard shortcuts** (and `F1`); *in the mock it is the "Keyboard shortcuts" page of the Settings dialog* (C12), which `F1` opens directly. Authoritative behaviour: `overview/04`; this window is the user-facing copy and must be generated from the same table.

## Layout
Modal or non-modal window (720 × 560 dp, resizable; in the mock: the Settings content pane). Title "Keyboard shortcuts" + one-line note: *"Grouped by workspace. Keys act when focus is not in a text field; menu shortcuts (File, Edit, …) are shown in the menus."* A **search field** filters rows by action, key or group (the Settings search also returns matching shortcut rows under a "Keyboard shortcuts" heading). Groups: small uppercase heading (+ optional muted sub-caption), then rows: **action text left**, **keys right**. Keys render as **key caps** (`kbd`: monospace 11, 1 dp border, 2 dp bottom border, radius 4); combinations join with "+", alternatives with "/" and multi-part sequences with "·"; **mouse inputs** render as dashed-outline tokens ("Left-drag", "Right-click", "Double-click").

## Content (groups and rows)
| Group | Rows |
|---|---|
| **Everywhere** | `Ctrl+1/2/3` workspaces · `F1` this list · `Ctrl+,` Settings · `Ctrl+[` / `Ctrl+]` collapse/expand left/right card · `Ctrl+Space` Focus 3D toggle · `Esc` step back (close dialog → close menu → cancel rename → pause playback → exit focus mode) · `Tab`/`Shift+Tab` move focus · `Enter`/`Space` activate focused control |
| **3D view** *(every workspace)* | Left-drag orbit/pan per mode · `Shift`+Left-drag the other action · Middle-drag pan · Wheel zoom · `O`/`P` Orbit/Pan mode · `Home` home view · `F` fit · click view-cube face/edge/corner · double-click face = upright · Minimal: pointer at top / `Tab` / `O P E F` show the toolbar |
| **Entities — drawing** | `1`–`4` tools · `E` edit mode · `Shift`/`Alt` (`⌘`)/`Ctrl`+click quick tools 1/2/3 · **Right-click / right-drag in the 2D grid = erase** · drag across cells (one undo step) · `PgUp`/`PgDn` layer |
| **Entities — shapes list** | `↑`/`↓` previous/next shape · `Alt+↑`/`Alt+↓` move earlier/later (renumbers) · `F2`/`Enter`/double-click rename (Enter commits, Esc cancels) · `Del` delete · `Ctrl+D` duplicate · right-click row menu · drag to reorder |
| **Puzzle** | `+`/`−` one more/one fewer copy of the selected piece · `Del` remove it from the puzzle · double-click chip / `+` add · drag chip in / tile out · right-click tile or chip menu · click a piece in 3D to select |
| **Solver** *(focus on list, solution slider or move seekbar)* | `Space`/`Enter` play/pause · `←`/`→` one move · `Shift+←/→` assembled / fully disassembled · `↑`/`↓` previous/next solution · `Home`/`End` first/last (seekbar: start/end) · `PgUp`/`PgDn` ±8 solutions · `Del` remove solution (list, asks first) · `Esc` pause · double-click row play · header click sort/reverse · piece chip `Space`/`Enter` cycle, `←`/`→` move · orbiting/clicking in 3D keeps focus in the Solutions card |
| **Dialogs and menus** | `Esc` close · `Enter` commit rename / number field · `Space`/`Enter` toggle switch |

## Acceptance criteria
* AC-C22-01 `F1` (and Help ▸ Keyboard shortcuts in the app) opens the window on the full grouped list; content equals `overview/04`.
* AC-C22-02 Search filters rows by action, key or group; the Settings search also lists matching shortcuts.
* AC-C22-03 Keys use key-cap styling; mouse inputs use the dashed token style; groups are in the order listed.


---
<!-- FILE: migration/00-phase-plan.md -->

# Migration phase plan

Strategy: first run the **framework spike** (`migration/04-framework-spike-plan.md`, P−1, two weeks) and extract the **framework-neutral controller layer**; then build the new front-end (**Qt 6 Quick** or **RmlUi**, per the spike) with C++ controllers on top of the existing model/command layer, one component at a time. Each phase delivers markup, styling, controller logic and tests for its components and is verified in a **component harness** (a dev window/test mode that hosts the new UI against the real model) before being wired into the Entities tab shell. The legacy UI remains the default until cut-over (P8). How the new front-end co-exists with or replaces the legacy FLTK GUI is an integration question to settle in P0 (OQ-13); the phase scopes below do not depend on the answer.

| Phase | Name | Components | Depends on |
|---|---|---|---|
| P−1 | Framework spike + controller layer | spike in both frameworks (scope/metrics in `04`); decision logged as OQ-35; extract controllers from the FLTK `mainwindow` against `overview/06` | — |
| P0 | Framework foundations | **Qt:** QML module, Theme singleton, QQuickRhiItem host, Qt Quick Test harness · **RmlUi:** integration (context, render/system/file interfaces, GL-surface host), theme sheets, primitives, assets (glyphs, cursors), element-id + input-injection test harness, component gallery | — |
| P1 | Shell & layout skeleton | C00, C10, C11 | P0 |
| P2 | Shapes sidebar | C01 | P1 |
| P3 | Drawing colour | C09 | P1 |
| P4 | Voxel editor | C07, C08, **C02 (Grid size)** | P2, P3 |
| P5 | 3D viewport | C06 (incl. Display menu) | P4 |
| P6 | Inspector | C03, C04, C05 | P2, P4 |
| P7 | Settings dialog | C12 | P1 |
| P8 | Polish & cut-over | dark theme audit, a11y, perf, remove legacy | all |
(P3, P6 and P7 can run in parallel with P4/P5 once their dependencies are met.)

---
## P0 — Framework foundations (Qt Quick profile or RmlUi profile)
**Goal:** the plumbing and reusable building blocks, with no Entities behaviour yet.
**Build:** RmlUi integration (Context, render/system/file interfaces, input bridge incl. middle button, wheel and **modifier state events**); loading of theme sheets generated from `design-tokens.json` (light/dark, runtime swap); fonts; primitives PR-01…PR-20 as RML templates + RCSS + C++ behaviour (Stepper commit/revert/clamp/repeat, Segmented, Switch, Accordion, PopupMenu in a popup layer, modal Dialog, Tooltip, Dropdown, StatusFlash); asset loading (glyphs per theme/scale, UI icons, **runtime-composed edit cursors (pointer + badge for Fixed/Variable; icon-only for Erase/Paint) via the system interface**); the **viewport surface host** (GL scene drawn into an element rect behind the RmlUi overlay); test harness that finds elements by id and injects mouse/keyboard/wheel events with modifiers; a **component gallery** document showing every primitive in every state, both themes, dp ratios 1.0/1.5/2.0. Verify RmlUi capabilities and record fallbacks (OQ-13).
**Exit criteria:** gallery matches `design-tokens.md §4–5` (visual compare); theme swap live; glyph contact sheet reproduced; edit cursors compose correctly (OS pointer + badge, theme and colour aware); popup/dialog layering and focus trapping work; harness can click, drag with modifiers and read element classes.
**Tests:** unit (token generator output, dp scaling), element-level (Stepper, Segmented, Switch, PopupMenu placement/flip/keyboard, Dropdown), visual snapshots of the gallery.
**Rollback:** nothing user-facing.

## P1 — Shell & layout skeleton
**Build:** top bar/menu/tabs/gear (C00); Entities root laid out as **three cards + status bar** (C11 widths; `LayoutController`: collapse/focus state machine, `editor_visible`, rails as empty shells, 200 ms transitions, persistence); status bar with live text, cursor readout and flash (C10); the viewport surface placed in the centre card with the existing renderer drawing into it.
**Exit:** AC-C00-*, AC-C11-*, AC-C10-*; the existing scene renders in the centre card; all C11 layouts reachable.
**Tests:** layout width table at 1600 dp for the 6 modes; persistence round-trip; Esc ladder (partial); modifier/mouse events reach the viewport surface.

## P1b — File ▸ New voxel type choice (C14)
Restyled File ▸ New with the one-time **voxel type** radio cards (Brick preselected; Triangular Prism, Spheres, Rhombic Tetrahedra, Tetrahedra-Octahedra); type stored in the file; `voxel_type`/descriptor exposed read-only to the UI. **Exit:** AC-C14-02. **Tests:** create one file per type, save/load, confirm type is shown and immutable.

## P2 — Shapes sidebar (C01)
**Replaces:** L10–L18 (Shapes group). **Build:** header, rows, hover actions, selection, inline rename, duplicate, delete, weight, drag reorder, context menu, rail chips, keyboard.
**Exit:** AC-C01-*; save/load preserves order & weights; **ids/colours are positional and every reorder/delete renumbers (and cross-tab references stay correct `[OQ-16]`)**.
**Tests:** row/list widget tests; undo tests for each action; reorder persistence test; journey J5.

## P3 — Drawing colour (C09)
**Replaces:** L80–L83 (Colours group). **Build:** footer, palette, menu, add/edit/remove wiring to legacy colour model, rail swatch; expose `drawColour` state used by P4/P5.
**Exit:** AC-C09-*.
**Tests:** colour removal reassigns voxels; Default follows shape colour; journey J6 (colour part).

## P4 — Voxel editor (C07, C08, C02)
**Replaces:** L60–L63 tool strip, L70 layer slider, L71 grid host (and wires plane selection). **Build:** voxel-type chip + `voxel-grid` cell shapes per type (C14), card header, tool strip (4 tools, numbered), mirror/span with the pure `expand()` function (unit-tested), **quick-tool modifier handling in the grid**, plane buttons, layer strip, `voxel-grid` element (cell size, rulers, variable hatch, hover, cursor, stroke with single undo), cursor readout, grid hover → `hover_preview`, **Grid size row (C02: steppers + apply-to-all, replaces legacy Size-tab dimension controls)**.
**Exit:** AC-C02-*, AC-C07-*, AC-C08-*; Focus-2D works; the 2D grid shows tool cursors and publishes `hover_preview` (rendered in P5).
**Tests:** unit tests for `expand()` (fixtures), plane mapping, cell-size formula; stroke undo; size edits (blocked shrink, apply-to-all); performance budgets; journeys J1, J3, J7.

## P5 — 3D viewport (C06) incl. Display menu
**Build:** **view cube (C13: hover regions, face/edge/corner snapping, Home, 90° arrows, roll, 360 ms eased camera animation, `Home`/`F` keys)**, floating toolbar (Orbit/Pan, **Edit button with per-tool glyph, badge 1–4 and `on`/`override` states**, Views, Fit, Display, Focus — no Views/Fit), mouse mapping (left per mode, Shift = other, middle = pan, wheel = zoom), Views menu, **Display menu (Show incl. layer slab & dim-other-layers; Projection; Voxel colour — absorbs the legacy status glyph pairs)**, Fit, hint, view tag, **3D editing** (viewport action resolution = quick tool ▸ Edit-mode tool ▸ none; picking reuse; **tool cursors, ghost preview incl. mirror/span, replace overlay; shared `hover_preview` fed by 3D and by the 2D grid**; Edit mode + `E`; click-vs-drag), **volumetric layer slab and dim-other-layers drawn only while `editor_visible`**, Focus-3D.
**Exit:** AC-C06-*, AC-C10-03.
**Tests:** picking tests (fixture voxels, ray/face → target), modifier precedence matrix, quick-tool/Edit-mode resolution table, cursor and ghost update on modifier press without pointer move, 2D-hover → 3D ghost, slab visibility per layout, click-vs-drag threshold; visual snapshots of ghosts and Edit-button states; journeys J2, J3.

## P6 — Inspector (C02–C05)
**Replaces:** Edit group & tabs (L20–L22, L34–L42, L50–L53; L54 Grid Scale is dropped; L30–L33 were replaced in P4). **Build:** accordion + head; **descriptor-driven Transform (C04/C14: per-voxel-type button sets, sphere/triangle nudge pads)**; Fit & scale (C03: prune, center, origin, minimize, ×2/×3); Repair & surface (C05: fill, surface table). Wire each to existing commands; implement blocked-op messaging.
**Exit:** AC-C03…C05-*; every legacy Edit-tab control accounted for in `01-legacy-to-new-map.md`.
**Tests:** op result tests on fixtures (flip/rotate/nudge identities, prune/centre/origin, ×n/minimize round trip, flood-fill sets), blocked cases, undo; journeys J1, J4.

## P7 — Settings dialog (C12)
**Replaces:** legacy Settings window; removes Config menu entry. **Build:** dialog, nav, rows, search, reset, shortcuts page, live-apply wiring to existing config.
**Exit:** AC-C12-*; every legacy setting present once.
**Tests:** config round-trip, live effects, search, focus trap; journey J8.

## P9 — Workspace rail (C15)
**Build:** 60 dp rail (44 in Minimal), three buttons with states, per-workspace state memory, `Ctrl+1/2/3`, remove top-bar tabs. **Exit:** AC-C15-01…04. **Tests:** switch workspaces repeatedly, state retention, keyboard roving.

## P10 — Puzzle tab (C16, C17, C18) — after P9 and the 3D viewport (P5)
* **P10a Model & state:** problems/pieces/groups/colour-rule model bindings, undo transactions, derived values (voxel range, fit, holes). **Exit:** unit tests for stats and group/range rules.
* **P10b Left card (C16):** problems list, result card, used-piece tiles + table, library chips, drag & drop with drop-zone feedback, summary strip, status text. **Exit:** AC-C16-01…07.
* **P10c Right card (C17):** fixed-count stepper, range switch with inline-label steppers, group selector and group cards, colour matrix. **Exit:** AC-C17-01…05.
* **P10d Scene (C18):** per-copy object layout, anchors fixed on screen, per-object rotation, selection sync, all voxel types. **Exit:** AC-C18-01…05.

## P11 — Solver tab (C19, C20, C21) — after P10 and P5
* **P11a Model & controller:** bind options, run state machine, progress/counters, solutions (+ stored disassemblies/levels), Analyse and Delete commands (confirmation, not undoable), derived `solutions_shown`. **Exit:** unit tests for filtering, sorting (incl. reverse), enabling rules.
* **P11b Left card (C19):** problem selector, radio cards, collapsible options, run row + progress + status grid, explorer buttons. **Exit:** AC-C19-01…05.
* **P11c Right card (C20):** List and Slider views with smooth scrubbing, fixed-height player (0.5–8×), Manage menu with hover preview and confirmation, Export, fixed-height piece display. **Exit:** AC-C20-01…06.
* **P11d Scene & drawer (C21):** per-move scene, wireframe/X-ray rendering, assembler-state mode, read-only viewport, docked explorer drawer. **Exit:** AC-C21-01…05.

## P12 — Density, keyboard model, help window (after P11; can run in parallel with P8)
* **P12a Density:** geometry tokens for Standard/Minimal (`foundations/density.md`), the Settings row, Minimal caption/help-to-tooltip rules, rotated rail captions, inline short labels, auto-hiding toolbar. **Exit:** AC-DEN-01…05.
* **P12b Keyboard:** global keys (`Ctrl+1/2/3`, `F1`, `Ctrl+[ ]`, `Ctrl+Space`, `O`/`P`), Shapes-list keys, Puzzle `+ − Del`, Entities-only scoping, 2D right-click erase, Solver keyboard model with sticky focus. **Exit:** AC-C01-K, AC-C08-RC, AC-C20-07, T-KB-1/2.
* **P12c Help window (C22):** generated from the input-map table; search. **Exit:** AC-C22-01…03.

## P8 — Polish & cut-over
Dark-theme audit (all components), accessibility pass (tab order, focus rings, names), performance budgets (test plan §6), tooltip texts audit vs specs, high-contrast check, minimum window 1280×800, make the RmlUi front-end the default and remove the legacy Entities UI, delete dead code, update user docs/screenshots.
**Exit:** full test plan green; screenshot diff vs `reference/screenshots` accepted; legacy code removed; changelog entry.


---
<!-- FILE: migration/01-legacy-to-new-map.md -->

# Legacy → new: 1:1 control map

Images: legacy `reference/legacy/legacy-*-annotated.png` (L-ids) · new `reference/screenshots/00-new-annotated.png` (N-ids; **N44** = Display menu, shown in `15-display-menu.png`/`26-…png`; **N70** = Settings dialog, `09–13-settings-*.png`). "Fate": **Kept** (same function, new look), **Moved**, **Merged**, **Replaced**, **Dropped**.

## A. Shell
| L | Legacy control | N | New location | Fate / notes |
|---|---|---|---|---|
| L01 | Menu: File, Toggle 3D, Export, Status, Edit Comment, Help, About | N01 | Top-bar menu buttons | Kept (restyled) `[OQ-10]` |
| L01 | Menu: **Config** | N03 | Gear button + `Ctrl+,` → Settings | Replaced |
| L02 | Tabs Entities / Puzzle / Solver | N02 | Centred segmented tabs | Kept |
| — | (title bar path) | — | File name in top bar (tooltip: full path) | Added |

## B. Shapes group → C01
| L | Legacy | N | New | Fate |
|---|---|---|---|---|
| L10 | New | N10 | Header `+ New` (+ rail `+`) | Moved |
| L11 | Delete | N13 | Row trash / `Del` / menu | Moved (per-row); later shapes renumber |
| L12 | Copy | N13 | Row copy / `Ctrl+D` / menu | Moved; copy **appended at the end** |
| L13 | Label | N13 | Row pencil / `F2` / dbl-click / menu (inline) | Replaced (inline edit) |
| L14 | W+ | N13 | Row `W+` / menu | Moved |
| L15 | W− | N13 | Row `W−` / menu | Moved |
| L16, L17 | ← → reorder | N14 | Drag grip, menu Move earlier/later, `Alt+↑/↓` | Replaced; **reordering renumbers ids/colours** |
| L18 | Chip strip list | N11/N12 | Vertical list; id and colour derive from position | Replaced |
| — | (none) | N15 | Collapse-left button / rail | Added |

## C. Edit group (tabs removed) → Grid size (right card) + inspector
| L | Legacy | N | New | Fate |
|---|---|---|---|---|
| L20 | Edit ▸ **Size** tab | N58 + N22–N26 | **Split**: dimensions → Voxel editor ▸ Grid size (C02); fit/scale ops → Inspector ▸ Fit & scale (C03) | Tab removed |
| L21 | Edit ▸ **Transform** tab | N21 | Inspector ▸ Transform (C04) | Tab → accordion |
| L22 | Edit ▸ **Tools** tab | N28–N29 | Inspector ▸ Repair & surface (C05) | Tab → accordion |
| L30 | Apply to All Shapes ☐ | N58 | Switch "Apply to all shapes" (Grid size header) | Kept |
| L31 | X/Y/Z numeric fields | N58 | Editable stepper value | Merged with slider |
| L32 | X/Y/Z sliders | N58 | Stepper ± (hold repeat), typed value, wheel | Replaced |
| L33 | Per-axis checkboxes | — | — | **Dropped** (product decision) |
| L34 | Grid ▸ prune icon | N22 | Fit & scale ▸ Prune grid to fit piece | Moved/labelled |
| L35 | Grid ▸ centre icon | N23 | Fit & scale ▸ Center | Moved/labelled |
| L36 | Grid ▸ origin icon | N24 | Fit & scale ▸ To origin | Moved/labelled |
| L37 | Shape ▸ minimize icon | N25 | Fit & scale ▸ Minimize | Moved/labelled |
| L38 | Shape ▸ ×2 icon | N26 | Fit & scale ▸ Double ×2 | Moved |
| L39 | Shape ▸ ×3 icon | N26 | Fit & scale ▸ Triple ×3 | Moved |
| L40 | Flip (3 rows × 2 icons) | N21 | Transform ▸ Flip: **1 button per axis** | Merged (6→3) |
| L41 | Nudge (3×2) | N21 | Transform ▸ Nudge −1/+1 per axis | Kept |
| L42 | Rotate (3×2) | N21 | Transform ▸ Rotate −90°/+90° per axis | Kept |
| L50 | Constrain pair A | N29 | Surface ▸ Make fixed: Inner / Outer | Labelled |
| L51 | Constrain pair B | N29 | Surface ▸ Make variable: Inner / Outer | Labelled |
| L52 | Constrain pair C | N29 | Surface ▸ Clear colour: Inner / Outer | Labelled |
| L53 | Fill Holes | N28 | Repair & surface ▸ Fill interior holes | Kept |
| L54 | Grid Scale | — | — | **Dropped** (product decision; N27 retired) |

## D. Editing surfaces → C07, C08, C09
| L | Legacy | N | New | Fate |
|---|---|---|---|---|
| L60 | Tool icons: fixed / variable / erase / paint | N52 | Tool strip (captions + 1–4 keys) | Kept (labelled) |
| L61 | Select-box / select-cell icons | — | — | **Dropped** (product decision) |
| L62 | 3 axis icons — mirror placement | N53 | Mirror X/Y/Z toggles | Kept (labelled) |
| L63 | 3 axis icons — span across grid | N54 | Span X/Y/Z toggles | Kept (labelled) |
| L70 | Vertical layer slider | N56 | Layer strip chips + PgUp/PgDn | Replaced |
| (not visible) | 2D plane choice | N55 | XY/XZ/YZ buttons | Added/Replaced `[OQ-4]` |
| L71 | 2D grid | N57 | 2D grid (same drawing semantics); tool cursors; hover previews in 3D; quick-tool modifiers | Kept + extended |
| L80 | Colours ▸ Add | N59 | `＋` | Moved |
| L81 | Colours ▸ Remove | N59 | Colour menu ▸ Remove | Moved |
| L82 | Colours ▸ Edit | N59 | Colour menu ▸ Edit colour… | Moved |
| L83 | Colour list | N59 | Palette + big swatch | Replaced |

## E. 3D view & status → C06, C10
New cursors (`edit-fixed|variable|erase|paint`) have no legacy equivalent.
| L | Legacy | N | New | Fate |
|---|---|---|---|---|
| L95 | 3D canvas (legacy modifier-click editing) | N41 | Canvas + toolbar (N40), hint (N42), view tag (N43). Quick tools: Shift = 1 Fixed · Alt/⌘ = 2 Variable · Ctrl = 3 Erase (+ latched **Edit mode**, key `E`) `[OQ-9]` | Kept + new map |
| — | (rotate/pan/zoom by mouse) | N40 | Toolbar **Orbit / Pan** mode; left-drag per mode, Shift-drag the other, **middle-drag pan**, **wheel zoom** (no Zoom button) | Added |
| L96 | View cube (faces/edges/corners snapping, Home, 90° arrows, roll arrows, smooth animation) | N45 | **View cube** (C13), same behaviour, smooth animation; Home = default view + fit | Kept/modernised |
| — | (none) | N40 | **Edit** (per-tool glyph + number 1–4; `on`/`override` states), Views, Fit, Display, Focus | Added |
| L90 | Status text | N60 | Status text (+ cursor readout) | Kept |
| L91 | Status glyph pair — piece vs voxel colour | N44 | **Display menu ▸ Voxel colour**: Piece / Voxel | Moved |
| L92 | Status glyph pair — perspective | N44 | **Display menu ▸ Projection**: Perspective / Orthographic | Moved |
| — | (none) | N44 | Display menu ▸ Show: Axes, Grid boundary, **Active layer slab**, **Dim other layers** | Added |
| — | (none) | N50/N51 | Focus-2D and collapse-right buttons | Added |

## E2. Voxel space types → C14 (images `legacy-voxeltype-*.png`)
| Legacy | New | Fate |
|---|---|---|
| Voxel type chosen in File ▸ New, fixed for the file (spheres / triangles / tetrahedra / cubes screens) | Same rule; File ▸ New radio cards; read-only lock chip in the Voxel editor header (N46) | Kept + made visible |
| Transform tab content differs per type (e.g. Spheres: 12 nudges *u/in/d*; Triangles: rotate X1 Y1 Z2, 6+2 nudges) | Transform section generated from the type's transform set (C04/C14) | Kept (descriptor-driven) |
| Size tab "Shape" column absent for Spheres | Scale sub-section hidden for types without scale support | Kept |
| 2D grid shows circles / triangles / split squares | `voxel-grid` draws the type's cell shape | Kept |
| Toolbar "Views"/"Fit" (new in mock Rev 2) | Removed — replaced by the view cube (Home, snaps) and keys `Home`/`F` | Merged |

## E3. Puzzle tab → C15–C18 (images `legacy-puzzle-tab-18-pieces.png`, `legacy-puzzle-tab-7-shapes.png`, `legacy-puzzle-tab-annotated.png`, `legacy-puzzle-problem-details-dialog.png`)
| L | Legacy control | N | New location | Fate / notes |
|---|---|---|---|---|
| L100 | Tabs Entities / Puzzle / Solver (top-left) | N50 | Vertical **workspace rail** (C15) | Moved |
| L101 | Problems: New · Delete · Copy · Label · ← → | N51 | **+ New** header button; per-row Rename / Duplicate / Delete; drag-reorder via grip | Kept; ←/→ replaced by dragging `[OQ-21]` |
| L102 | Problems list ("P1 – Small") | N52 | Problem rows: positional **P-id chip + editable label** | Kept |
| L103 | "Result: S1 – Target" bar + **Set Result** | N53 | **Result card** with **Change…**; "Set as result" in piece menus | Merged |
| L104 | Piece Assignment list of all shapes | N54 | **All pieces** library chips (+ / double-click / drag to add) | Kept, restyled |
| L105 | **+1**, **−1** | N55 | Tile steppers (− count +) and the selected-piece stepper | Kept |
| L105 | **min=0** | N56 | **"Allow a range"** switch + Min stepper (min 0 = optional) | Replaced |
| L106 | **all+1**, **Clr** | N57 | ⋯ menu: *Add one of each piece*, *Remove all pieces* | Kept |
| L106 | **Detail** (Problem Details dialog) | N58 | **Chips ⁄ Table** toggle (table = Shape · Count · Range · Group) | Replaced (inline, no dialog) |
| L106 | ← → next to the piece list | — | — | **Dropped** (order = shape order) `[OQ-21]` |
| L107 | Added-pieces list (one chip per piece) | N59 | **In this puzzle** tiles / table rows | Kept |
| L108 | Colour Assignment chips C1…C5 | N60 | Colour **matrix headers** (swatches) | Replaced |
| L109 | Piece-colour → result-colour pair list; **Sort by Piece / Sort by Result** | N61 | **Colour-rules matrix** (+ helper buttons); sorting not needed | Replaced `[OQ-22]` |
| L110 | 3D view: result, selected piece, all pieces (layout fixed on screen) | N62 | Puzzle scene (C18): **one object per copy**, per-object rotation, group pan/zoom | Kept, extended |
| L111 | Status line "Problem P1 result can contain … voxels, pieces (n = …) contain … voxels" | N63 | Status bar, same wording | Kept |
| L112 | Problem Details dialog: Shape · Min · Max · Gr 1 columns, **Add Group**, **Maximum Number of Holes** | N64 | Table view + group selector (**one group per piece**, **+ New**) ; **hole limit dropped** (derived: counts determine holes) | Replaced / Dropped |

## E4. Solver tab → C19–C21 (images `legacy-solver-tab.png`, `legacy-solver-tab-annotated.png`)
| L | Legacy control | N | New location | Fate / notes |
|---|---|---|---|---|
| L120 | Tabs Entities / Puzzle / Solver | N50 | Workspace rail (C15) | Moved |
| L121 | "Parameters" problem list | N70 | **Problem selector** button + popover (C19) | Replaced |
| L122 | **Disassemble**, **Drop Disassemblies** check boxes | N71 | **"What to check" radio cards** + sub-switch "Don't keep the disassemblies" | Replaced |
| L122 | **Just Count**, **Keep Mirror Solutions**, **Expnsv Rot Check**, **Keep Rotated Solutions** | N72 | Search-options switches (collapsible) | Kept, renamed ("Thorough rotation check") |
| L123 | **Sort by** dropdown (Moves for Complete Disassembly …) | N73 | "Sort found solutions by": Search order (id), Number of pieces, Level, Moves | Extended (id, pieces); Level/Moves need disassembly `[OQ-27]` |
| L124 | **Drop**, **Limit** inputs | N74 | Steppers "Drop below level", "Limit" | Kept `[OQ-26]` |
| L125 | **Prepare / Start / Continue / Stop** | N75 | Run row with state-based enabling | Kept `[OQ-29]` |
| L126 | **Placements / Movements / Step** (separate windows) | N76 | Step-by-step explorer buttons + **docked explorer drawer** (C21) | Kept, docked `[OQ-28]` |
| L127 | Progress bar "0.0000%" | N77 | Progress bar + "0.0 %" | Kept |
| L128 | Activity / Assemblies / Solutions / Time used / Time left | N78 | Status grid | Kept |
| L129 | **Solution** slider | N79 | Solutions **List ⁄ Slider** (smooth scrubbing) | Kept + new List |
| L130 | **Move** slider "Move 19 (14.2)" | N80 | **Disassembly player** (transport, speed 0.5–8×, full-width slider, "Move 14.2 / 19" above) | Replaced `[OQ-30]` |
| L131 | "Assembly:1 Solution:1" | N81 | Detail tiles (Slider) / columns (List) | Merged |
| L132 | **Sort by Number / Level / Disasm / Pieces** | N82 | List column headers + Sort-by segment (same state); "Disasm" → "Moves" | Kept |
| L133 | **Delete: All / Before / At / After / w/o DA** | N83 | **Manage solutions ▾ ▸ Remove assemblies** (with hover preview + confirmation) | Replaced `[OQ-25]` |
| L134 | **D DA / D A DA / A DA / A A DA / A M DA** | N84 | Manage solutions ▸ **Disassemblies** (plain-language labels, legacy abbreviations shown) | Replaced `[OQ-25]` |
| L135 | **Export Solution to STL** | N85 | **Export to STL** button | Kept |
| L136 | Piece list (S2…S7) with visibility cycling | N86 | **Piece display** chips (appearance = state; S5.1/S5.2 for ranged pieces) | Kept, restyled |
| L137 | 3D viewer with view cube (read-only) | N87 | Solver scene (C21) | Kept, + wireframe X-ray option |

## F. Settings dialog → C12 (image `legacy-settings-dialog.png`)
| Legacy item | New | Fate |
|---|---|---|
| Worker Threads (slider + number) | Performance ▸ Worker threads | Kept |
| Undo History Depth (spin) | General ▸ Undo history depth — **dropdown 25 · 50 · 100 · 200 · 500** | Replaced (dropdown) |
| Show View Cube | 3D view ▸ Show view cube | Kept |
| Reverse scroll zoom direction | 3D view ▸ Reverse scroll zoom direction | Kept |
| Use new rotation method ☑ | 3D view ▸ Rotation method: Drag / Arc-ball | Replaced (segmented) |
| Use openGL display lists | Performance ▸ Use OpenGL display lists | Kept |
| Fade Out Pieces | 3D view ▸ Fade out removed pieces | Kept |
| Use Lights in 3D View | 3D view ▸ Lighting | Kept |
| Use Tooltips | General ▸ Show tooltips | Kept |
| Restore Defaults | Footer ▸ Restore all defaults (+ per-page Reset section) | Kept |
| Close | Footer ▸ Done | Kept |
| — | Search, Shortcuts page, Appearance (spec-only) | Added |


---
<!-- FILE: migration/02-test-plan.md -->

# Test plan

## 1. Element ids and the test harness
Every interactive element carries a stable id (PR-18). Convention: `area.component.element[.variant]`, lowercase, dot-separated, 0-based indices.
Prefixes: `shell.*` (C00), `entities.shapes.*` (C01), `entities.editor.*` (C07–C09 and **Grid size `entities.editor.size.*` (C02)**), `entities.fit.*` (C03), `entities.transform.*` (C04), `entities.repair.*` (C05), `entities.inspector.*` (inspector frame), `entities.viewport.*` (C06), `status.*` (C10), `entities.layout.*` (C11), `settings.*` (C12). Full per-widget ids are listed in each component spec. Tests locate widgets only by id, never by position or label text.
Test harness (C++): find an element by id; read its classes (`on`, `selected`, `disabled`, `open`, `override`, …), text, `title`, visibility and bounds (dp); inject input through the RmlUi Context — mouse move/press/release (left, middle, right), wheel, key down/up with **Shift / Alt / Ctrl / Meta states** (modifier key events alone must be injectable, to test cursor/ghost changes without pointer motion). Controllers can also be driven directly with the events listed in `overview/06-ui-contract.md`.

## 2. Test layers
| Layer | What | How |
|---|---|---|
| Unit (pure logic) | `expand()` (mirror/span), plane mapping, cell-size formula, flood-fill sets, flip/rotate/nudge transforms, scale/minimize, cursor readout string, **tool-in-effect resolution** (quick tool ▸ Edit-mode tool ▸ none), positional id/colour assignment | headless, no GUI |
| Element/controller | primitives and components in the gallery/harness | injected events, class/text assertions, snapshot |
| Integration | component ↔ model ↔ undo (e.g. click Fill → voxel count, undo) | headless model + GUI harness |
| Visual regression | 2560×1600 screenshots of states vs `reference/screenshots/*` | pixel-diff, tolerance 2 % per region; fonts may differ |
| Manual scripts | journeys (`03-user-journeys.md`) and exploratory | §7 |

## 3. Fixtures (use in unit/integration tests)
| Id | Shape | Purpose |
|---|---|---|
| FX-A | 4×3×4, 14 voxels as in the reference mock S2: (1,0,0)(1,1,0)(0,0,1)(1,0,1)(1,1,1)(2,1,1)(1,1,2)(2,1,2)(3,1,2)(1,2,2)(2,1,3)(3,1,3)(3,2,3)(2,2,3) | general ops, identities |
| FX-B | 3×3×3 solid cube (27) | surface sets (26 outer, 1 inner) |
| FX-C | 5×5×5 hollow shell (98) | fill holes → 125 |
| FX-D | 2×2×2 solid cube in a 4×4×4 grid, offset (1,1,1) | prune/center/origin |
| FX-E | 2×1×1 bar scaled ×2 (4×2×2 grid fully uniform blocks) | minimize/×n round trip |
| FX-F | L-tromino 2×2×1 (3 voxels) | minimize disabled |
| FX-G | 4×4×4 empty grid | placement/mirror/span |

## 4. Test cases (Given / When / Then)
IDs map to acceptance criteria (AC) in the component specs.

### Shell / layout
* **T-C00-1** Given flag on, When opening the app, Then menu has File, Toggle 3D, Export, Status, Edit comment, Help, About and no Config; gear opens Settings. (AC-C00-01/02)
* **T-C11-1** For each of 6 layouts assert column widths at 1600 dp: default 380/flex/400; left-collapsed 56; right-collapsed 56; both 56/flex/56; focus-2d 56/360/flex; focus-3d 56/flex/56. (AC-C11-01)
* **T-C11-2** Collapse both sidebars, restart → still collapsed; enter Focus-2D, restart → focus none. (AC-C11-02)
* **T-C11-3** In Focus-3D press Esc → previous collapse flags restored. (AC-C11-04/05)

### Shapes
* **T-C01-1** Hover row shows 5 actions; leave hides them (selected row always shows). (AC-C01-01)
* **T-C01-2** Rename: F2, type "Arm", Enter → label "Arm"; F2, type "x", Esc → unchanged. (AC-C01-03)
* **T-C01-3** Duplicate S2 → new shape appended as the last shape (`S5`), same voxels/dims/weight/label, selected; S1–S4 unchanged; undo removes it. (AC-C01-04)
* **T-C01-4** With one shape, Delete disabled; with two, delete selects neighbour. (AC-C01-05)
* **T-C01-5** W+ ×2 then W− ×3 → weight 0, W− disabled, badge hidden when weight 1 and shown as `W2`. (AC-C01-06)
* **T-C01-6** Give the four shapes labels A,B,C,D. Drag D above A → list reads D,A,B,C and the chips read **S1,S2,S3,S4 in that order** (D is now S1 with the colour of position 1; A is S2…); status flash "Order changed — shapes renumbered S1…S4"; save/load preserves order; one undo restores the original ids/colours. (AC-C01-07/11)
* **T-C01-8** Delete the second of four shapes → remaining three are S1,S2,S3 (old S3→S2, S4→S3); cross-tab references still point at the same shapes. (AC-C01-05/11)
* **T-C01-7** Right-click opens menu with items & disabled states per C01. (AC-C01-08)

### Colour
* **T-C09-1** Add colour C2, select, draw Fixed → voxel colour C2; remove C2 → voxel becomes Default. (AC-C09-02/04)
* **T-C09-2** Edit/Remove disabled for Default in menu. (AC-C09-04)

### Voxel editor
* **T-C07-1** Press `3`, click filled cell → voxel removed; `4` with colour C2 on a filled cell → colour changes, state unchanged; `4` on empty cell → no change. (AC-C07-02)
* **T-C07-2 (unit)** `expand` on FX-G: Mirror X, target (0,1,1) → {(0,1,1),(3,1,1)}; 3-wide: target x=1 → single cell. (AC-C07-03)
* **T-C07-3 (unit)** `expand` Span Z target (2,1,0) in 4-layer → 4 cells; Span X+Z → 16 cells; Span Y + Mirror X on target (0,1,1) in 4×4×4 → 4 (span) × 2 (mirror) = 8 cells. (AC-C07-04/05)
* **T-C07-4** Drag across 5 cells in a row → 5 voxels, one undo step. (AC-C08-05)
* **T-C08-1 (unit)** Plane mapping for a 4×3×5 shape (FX-A resized): n=5/3/4 for XY/XZ/YZ; layer clamp when switching from layer 4 (XY) to XZ. (AC-C08-01)
* **T-C08-2 (unit)** Cell size: area 360×500 for 4×3 grid → `floor(min((360−70)/4,(500−60)/3))=72`; clamp ≤120 / ≥16. (AC-C08-03)
* **T-C08-3** Layer strip dot appears iff layer has voxels; PgUp/PgDn clamp. (AC-C08-02)
* **T-C08-4** Hover cell (col 2,row 1,layer 0) in XY → status "Cursor X2 Y1 · Z0". (AC-C08-06)

### 3D viewport
* **T-C06-1 (unit)** Tool-in-effect resolution: ctrl→erase; alt→variable; shift→fixed; ctrl+alt→erase; alt+shift→variable; none + edit off → none; none + edit on + tool Paint → paint; **quick tool beats the active tool when Edit mode is on**. (AC-C06-06/07)
* **T-C06-2** Edit mode off, Shift held, hover a face: ghost + `edit-fixed` cursor appear; click adds a fixed voxel at voxel+normal; Alt/⌘ → variable; Ctrl → erases the voxel under the pointer; no modifier → click does nothing. Target outside grid → no ghost, dashed red face outline, no change. (AC-C06-06/10)
* **T-C06-3** Press-move 3 dp-release → edit; press-move 10 dp → orbit, no edit; middle button never edits. (AC-C06-12)
* **T-C06-4** With Mirror X on, ghost shows two cubes; click places both in one undo step. (AC-C06-10/12)
* **T-C06-5** Views ▸ Top sets pitch 89°; Fit leaves orientation. (AC-C06-03)
* **T-C06-6** Compact toolbar in Focus-2D: 34 dp icon-only buttons with tooltips. (AC-C06-01)
* **T-C06-7 (nav)** Orbit mode: left-drag changes yaw/pitch; Shift-drag and middle-drag pan; wheel zooms. Pan mode: left-drag pans; Shift-drag orbits; middle-drag pans. No Zoom button; hint line 2 follows the mode. (AC-C06-01/02)
* **T-C06-8 (display)** Display menu: sections/defaults, stays open on toggle, Projection and Voxel colour change 3D immediately and persist; status bar has no toggles. (AC-C06-04)
* **T-C06-9 (slab)** On a 4×3×5 shape, XY layer 2 → slab spans the full 4×3 and exactly one cell along Z (z∈[1,2]) with all six faces tinted and 12 edges; Dim other layers fades voxels with z≠1, picking still works. Collapse the right sidebar → slab and dim disappear and their Display rows are disabled with "needs Voxel editor"; expand → they return. Enter Focus-3D → hidden; Focus-2D → shown. (AC-C06-05, AC-C11-08)
* **T-C06-10 (edit button)** Press `3` → Edit button shows the eraser icon (no arrow) and number 3; press `E` → `on`; hold Alt (pointer stationary) → glyph/badge switch to 2 with `override`, release → back to 3 `on`; press `E` → off. (AC-C06-07/08/09)
* **T-C06-11 (cursor)** Fixed: OS default pointer + rounded-square badge with a “+” at its upper right; Variable: same with a dotted boundary. Default drawing colour → white inside in the light theme (near-black in dark) with theme-ink outline and a dark/white “+” chosen by contrast; a specific colour → that colour inside (the “+” contrast-adjusted). Erase: monochrome eraser icon alone; Paint: monochrome bucket icon alone with a drop in the drawing colour (Default → monochrome drop); icon-only cursors use the glyph centre as hotspot. Colour/theme changes re-compose the cursor immediately; it appears the instant Shift/Alt·⌘/Ctrl is pressed (pointer stationary) and reverts on release. Size follows the OS cursor scale (32/48/64 px). (AC-C06-08)
* **T-C06-12 (2D→3D preview)** Hover a 2D cell: `edit-<tool>` cursor, grid cell outline, and a 3D ghost at that cell of the active layer (mirror/span copies included); hold Ctrl → erase ghost on existing voxels and `edit-erase` cursor; leave the grid → ghost gone. Press a quick tool, drag a stroke: the tool captured at press is used throughout. (AC-C06-11, AC-C08-10)

### View cube (C13)
* **T-C13-1** Hover regions: for the +Z face, points in the inner square → "face"; in an edge strip → "edge"; in a corner → "corner" (threshold 0.56); only that region is highlighted. (AC-C13-02)
* **T-C13-2** Click each of the 6 faces, 12 edges, 8 corners → after 360 ms the camera direction equals the face normal / sum of two / sum of three; top/bottom faces end axis-aligned. (AC-C13-03)
* **T-C13-3** Home from any orientation → default view, zoom 1, pan 0, animated (intermediate frames differ; final equals defaults); `F` keeps orientation. (AC-C13-04/08)
* **T-C13-4** Face-aligned view shows 4 arrows + 2 roll arrows; any non-aligned view hides them; ▶ then ◀ returns to the original face; four ↷ clicks return to the start. (AC-C13-05/06)
* **T-C13-5** Start an orbit drag during an animation → animation stops; orbit clears roll; with the cube hidden `Home`/`F` still work and the toolbar has no Views/Fit. (AC-C13-07/08)

* **T-C13-6 (perspective)** With Projection = Perspective the cube's back edges converge (near edges longer than far edges); switching to Orthographic redraws it with parallel edges immediately; hover regions still map correctly. (AC-C13-09)
* **T-C13-7 (no jump)** Click ▶, wait for the animation to end, drag-orbit: the first drag frames continue from the arrived orientation; repeat after a roll click (roll stays, no snap-back); repeat by starting the drag *during* the animation (animation stops at its current frame, no jump). (AC-C13-07)
* **T-C13-8 (highlights)** Hover a corner → patches on all visible faces of that corner are highlighted; hover an edge → strips on both adjoining faces; hover a face centre → one patch. (AC-C13-11)
* **T-C13-9 (layout)** Roll icons: mirror images, same y, arrowheads opposing; Home within ~12 dp of the cube silhouette. (AC-C13-12)

* **T-C13-10 (roll icons)** Face-aligned view: an arch icon above the cube (head on its left end pointing down) and a “)” icon on its right (head at the bottom pointing left); clicking the arch rolls +90° (counter-clockwise), clicking the “)” rolls −90°. (AC-C13-12, AC-C13-06)
* **T-C06-13 (smooth zoom)** Inject one wheel event of Δ=−100: sampled every 40 ms the zoom takes ≥ 5 distinct increasing values and converges (≈ ×1.17 target) within ≈ 400 ms; two quick notches accumulate; a Home click during the glide cancels it without a jump. (AC-C06-13)

* **T-C13-11 (upright)** For each of the 6 faces: set the camera face-on at roll 0, double-click the face centre → after 360 ms the face label's baseline is horizontal (left→right) — `+X` rolled 90° clockwise; edges/corners ignore double-click. (AC-C13-13)
* **T-C13-12 (icon placement)** Roll icons radius 9 dp with ≥ 8 dp heads, Home, roll icons and the four 90° arrows ≥ 6 dp clear of the cube silhouette in both projections (the arrows' centres 20 dp beyond the cube edge). (AC-C13-14)

* **T-C13-13 (placement)** Face-aligned view, default layout: measure the visible content's extreme top and right points → both 16 dp (±1) from the viewport edges; narrow the centre card to 600 dp → the widget moves below the toolbar and no pixels overlap the toolbar. (AC-C13-15)

### Voxel space types (C14)
* **T-C14-1** For each type fixture file: the header chip shows the type with a lock; no control changes it; tooltip text matches. (AC-C14-01)
* **T-C14-2** File ▸ New → choose each type → save → reload: same type shown; default selection is Brick; the list has exactly Brick, Triangular Prism, Spheres, Rhombic Tetrahedra, Tetrahedra-Octahedra in that order. (AC-C14-02)
* **T-C14-3** Button counts in the Transform section: Brick 15; spheres 3 flip + 6 rotate + 12 nudge; Triangular Prism 3 flip + 4 rotate + 8 nudge (6 in-plane + 2 perpendicular); each button triggers its legacy command. (AC-C14-03)
* **T-C14-4** Spheres: Fit & scale shows only Fit-to-grid; Brick and Triangular Prism show Scale; other types follow `supports_scale`. (AC-C14-04)
* **T-C14-5** Grid hit-testing: clicking inside a circle / triangle / split-square triangle selects exactly that cell; hover outline follows the shape; a drag stroke paints crossed cells. (AC-C14-05)

* **T-C14-6 (sphere packing)** Spheres 3×3×3: in layer 1 the circles at (0,0) and (1,1) touch (distance between centres = 2 × radius within 0.5 %); in 3D the spheres (0,0,0) and (1,0,1) touch; an interior sphere has exactly 12 neighbours. (AC-C14-07)
* **T-C14-7 (edit every type)** For each type: click an empty 2D cell → voxel count +1; hover a cell → 3D ghost appears at that cell; Shift+click on an exposed face (polyhedra) / sphere surface → a voxel is added at the face neighbour / best free neighbour; Ctrl+click erases; outside-grid targets show the red dashed outline and change nothing. (AC-C14-08)

### Workspace rail (C15)
* **T-C15-1** Rail shows Entities / Puzzle / Solver with icon + caption; the selected one has accent background and left bar; top bar has no tabs. (AC-C15-01/02)
* **T-C15-2** Switch workspaces: shapes selection, problem selection, scroll positions persist per workspace; Focus modes reset. `Ctrl+1/2/3` work. (AC-C15-03)

### Puzzle tab (C16–C18)
* **T-C16-1** Problems: rename by double-click/pencil/F2; drag a grip to reorder → ids renumber; New/Duplicate/Delete (+ undo). (AC-C16-01)
* **T-C16-2** Result: Change… swaps the result; the old result becomes available as a piece; the new result vanishes from the pieces. (AC-C16-02)
* **T-C16-3** Tile stepper: press + ×3, − ×4 → count 3 then 0 and the tile disappears; neighbouring tiles never move or resize during hover/press. (AC-C16-03)
* **T-C16-4** Add via +, double-click and drag (drop zone highlighted while dragging); remove via − at 1, context menu and by dragging onto All pieces. (AC-C16-04)
* **T-C16-5** Chips ⁄ Table: edit count, range and group in the table → chips, 3D copies and summary update; switch back preserves data. (AC-C16-05)
* **T-C16-6** Summary/status: result 19 voxels, pieces fixed 18 → "Holes: 1"; add a piece so pieces > 19 → "Pieces exceed…"; status bar wording identical to legacy; no hole-count input. (AC-C16-06)
* **T-C16-7** Select a piece in each of the 5 places → selected-piece card, frames, 3D Selected slot and list highlight all agree. (AC-C16-07)
* **T-C17-1** Fixed count is the default editor; enabling the range switch shows two inline-label steppers (labels inside the boxes); min cannot exceed max; disabling collapses to a fixed count. (AC-C17-01/02)
* **T-C17-2** Group selector None/G1/+ New; at most one group per piece; Groups section appears with a member list only when a group exists; delete clears members. (AC-C17-03)
* **T-C17-3** Colour matrix: toggle cells; Identity/Allow all/Clear set the whole matrix; rules are per problem. (AC-C17-04)
* **T-C18-1** A piece with count 3 shows three separate objects captioned 1/3, 2/3, 3/3; a range 0–2 shows two translucent dashed "(optional)" copies. (AC-C18-01)
* **T-C18-2** Orbit 90°: every object's anchor (frame centre) stays within 1 px while its geometry rotates; pan/zoom move/scale all anchors together. (AC-C18-02)
* **T-C18-3** Hover/click on any copy selects the piece (all copies framed); clicking the result/empty space does nothing; Edit button, 1–4/E keys and voxel modifiers do nothing in this workspace. (AC-C18-03/04)
* **T-C18-4** For each of the 5 voxel space types the scene renders the type's own voxel geometry; spheres touch. (AC-C18-05)

### Solver tab (C19–C21)
* **T-C19-1** Selecting each radio card toggles disassembly; "Don't keep the disassemblies" appears only with disassembly and turns disassembly on if enabled. (AC-C19-02)
* **T-C19-2** Search options collapsed by default; change one → header shows "· 1 changed"; in Assemblies mode the Level and Moves sort options are disabled "(needs disassembly)" and the effective sort is Search order. (AC-C19-03)
* **T-C19-3** Run states: idle → Prepare (preparing → ready) → Start (running: only Stop enabled, progress and counters rise, results appear incrementally) → finished; Stop → paused (Continue enabled) → Continue. (AC-C19-04)
* **T-C19-4** Placements/Movements/Step enabling; Movements closes when the selected solution has no disassembly; Step shows the assembler-state caption and cycles 0…n. (AC-C19-05, AC-C21-03)
* **T-C20-1** Fixture with 12 assemblies / 5 disassemblable: Disassembly mode lists 5, Assemblies mode lists 12; fixture with 0 disassemblable shows the empty state and the "Show the assemblies" shortcut. (AC-C20-03)
* **T-C20-2** List: header click sorts, second click reverses (▲▼), arrows/Home/End/PageUp/PageDown move selection, double-click plays; 100 rows scroll smoothly. (AC-C20-01)
* **T-C20-3** Slider: drag across all solutions — the slider DOM element is never replaced and the 3D/labels update at least once per frame; Prev/Next buttons disable at the ends. (AC-C20-02)
* **T-C20-4** Player height is 112 dp (84 in Minimal) for stored / level-only / not-checked / cannot-disassemble / nothing states and for solutions with 6 or 7 pieces; counter above a full-width slider; speeds 0.5×–8× (8× covers ≈ 9.6 moves/s). (AC-C20-04)
* **T-C20-5** Manage: each Remove item shows the hover preview on exactly the affected rows, asks for confirmation, and removes them; Analyse updates DA icons and drops non-disassemblable assemblies in disassembly mode. (AC-C20-05)
* **T-C20-6** Piece display: fixed 104 dp (76 in Minimal) with 6 or 7 chips; click cycles solid → dotted-border → greyed-out; no state words; Show all resets; S5.1/S5.2 naming. (AC-C20-06)
* **T-C21-1** Wireframe piece: all hull edges drawn (back fainter) with ≈ 6 % tint; Display ▸ "Show through solid pieces" on → visible through solids, off → occluded. (AC-C21-02)
* **T-C21-2** Solver viewport has no Edit button, modifiers or overlays; `1–4`/`E` do nothing; Display menu shows Wireframe pieces, Projection, Voxel colour only. (AC-C21-04)
* **T-C21-3** Explorer drawer docks (250 dp) over the viewport, switches Placements ⁄ Movements, closes with ×; the sidebars do not change. (AC-C21-05)

### Rev 5.2 additions
* **T-C11-5** In each workspace collapse the left, then the right card: the centre grows to the freed width (Standard 864 → 1156 → ≈ 1410 dp at 1600 dp; Minimal 970 → 1250 → ≈ 1460), nothing overlaps, the rails show the expand button and a vertical title; switch workspaces and back — each workspace restored its own collapse state. (AC-C11-02, AC-C11-04)
* **T-C11-6** Window 960 dp wide, both cards collapsed: toolbar, view cube, hint and status bar remain usable. (AC-C11-04)
* **T-C12-6** Settings ▸ General ▸ Theme shows Light | Dark | System (System selected); choosing Dark/Light applies at once; with System the app follows an OS light/dark change at runtime and falls back to Light when the OS gives no preference; no theme button in the top bar. (AC-C12-08)
* **T-C16-8** Puzzle ⋯ ▸ *Set range for all pieces…*: Min 0, Max 2, Apply → every tile reads 0–2; raising Min above Max raises Max. (AC-C16-08)
* **T-C21-4** Give a big shape weight 3 and others weight 1: during playback the heavy shape stays put and the others move. (AC-C21-06)

### Rev 6.0 additions — density, keyboard, help
* **T-DEN-1** Settings ▸ General ▸ Interface density shows Standard | Minimal (Standard selected); switching applies at once in all workspaces and persists. (AC-DEN-01)
* **T-DEN-2** At 1600 × 1000, in each density and workspace (Entities with Fit & scale and Repair open, Puzzle, Solver): no element overflows its card, no button label/header wraps, Puzzle tiles 3 per row; measured widths match `density.md` §1 (Standard 320/864/340, rail 60; Minimal 264/970/320, rail 44). (AC-DEN-02)
* **T-DEN-3** Minimal: every icon-only control and every hidden help text has a tooltip (automated scan finds none missing); the rail shows rotated captions; Fit & scale shows Prune/Center/Origin on one row with short labels; Surface/Repair rows show the first line only. (AC-DEN-03/04)
* **T-DEN-4** Minimal toolbar: hidden with the pointer mid-view (handle visible); shown within 72 dp of the top; stays while its Display menu is open; hides ≈ 0.7 s after leaving once the menu is closed; `P` shows it for ≈ 1.4 s; Standard always shows it. (AC-DEN-05)
* **T-C08-RC** With Fixed, Variable, Erase or Paint active (and with Shift/Alt held), right-click and right-drag on cells erase; no context menu. (AC-C08-RC)
* **T-C01-K** Click a shape row, `↓` selects the next shape, `Alt+↑` moves it earlier and renumbers (status flash), `Enter` starts rename. (AC-C01-K)
* **T-KB-1** `Ctrl+1/2/3` switch workspaces; `Ctrl+]` then `Ctrl+[` collapse right/left (centre 864 → 1156 → ≈ 1410), again restores; `Ctrl+Space` enters Focus 3D and `Esc` leaves it; `O`/`P` switch Orbit/Pan; Entities-only keys do nothing in Puzzle/Solver (`PgUp`, `F2`, `Del` there never touch Entities).
* **T-KB-2** Puzzle: select a tile, `+` → count +1, `−` → −1, `Del` → piece leaves the puzzle.
* **T-C20-7** Solver list focused: `→ →` = Move 2.0; orbit-drag in the 3D view, click the Display button and the view cube — focus is still on the list; `→`, `←`, `Shift+→` (end), `Shift+←` (start), `↓` (next solution, move reset), `Space` plays, `Enter` pauses, `Del` opens the removal confirmation. (AC-C20-07)
* **T-C20-8** Slider view: `↓` changes solution, `→` steps a move without changing solution, `Space` plays, `Esc` pauses; move seekbar: `Home` then `→ → →` = Move 3.0, `Space` plays and focus stays on the seekbar. Piece chip: `Enter` cycles, `→` moves focus. (AC-C20-07)
* **T-C22-1** `F1` opens Keyboard shortcuts with groups Everywhere, 3D view, Entities (drawing), Entities (shapes list), Puzzle, Solver, Dialogs and menus; searching "pause" lists the three pause-related rows; `Esc` closes. (AC-C22-01/02)

### Grid size (C02)
* **T-C02-1** Stepper X +1 on FX-A → 5×3×4; type "2" Enter on a shape with voxels beyond x=2 → blocked with flash, data unchanged. (AC-C02-02/04)
* **T-C02-2** Apply-to-all ON, press Y − once → every shape's Y decreases by 1 where legal; OFF → only the selected shape changes. (AC-C02-03)
* **T-C02-3** The Grid-size row is visible between grid and colour footer in default and Focus-2D layouts and contains no per-axis checkboxes. (AC-C02-01)
* **T-C02-4** After a size change the layer is clamped and the 3D slab/bounds follow. (AC-C02-05)

### Fit & scale (C03)
* **T-C03-1** FX-D: Prune → 2×2×2, voxels at origin; Center on FX-D (offset 1 in 4×4×4) → offset 1; To origin → offset 0; all disabled for FX-G (empty). (AC-C03-02)
* **T-C03-2** FX-E: Minimize → 2×1×1 (k=2); ×2 then Minimize = original; ×3 then Minimize = original; FX-F Minimize disabled with "Already at minimum resolution". (AC-C03-03/04)
* **T-C03-3** ×n blocked at the limit with message; voxel count after ×n = old·n³. (AC-C03-05)
* **T-C03-4** After each op the Grid-size steppers show the new dims. (AC-C03-07)

### Transform / Repair (C04, C05)
* **T-C04-1** FX-A: flip twice = identity; rotate +90 ×4 = identity; rotating a 4×3×5 shape about Y → 5×3×4. (AC-C04-02/04)
* **T-C04-2** Nudge FX-D(+offset) until boundary succeeds, next blocked with flash. (AC-C04-03)
* **T-C05-1** FX-C Fill → 125 voxels; again → "No interior holes found". (AC-C05-01)
* **T-C05-2** FX-B: Make variable ▸ Inner → only centre voxel variable; Outer count = 26. (AC-C05-02)
* **T-C05-3** Clear colour ▸ Outer on coloured shell clears only outer colours. (AC-C05-03)

### Status / settings
* **T-C10-1** After painting, status text counts fixed/variable correctly; flash reverts after 2.4 s; the status bar contains no toggle controls. (AC-C10-01/03/04)
* **T-C12-1** Search "zoom" → shows only "Reverse scroll zoom direction" under 3D view. (AC-C12-02)
* **T-C12-2** Toggle Lighting off → 3D flat immediately; restart → still off; Reset section restores. (AC-C12-03/06)
* **T-C12-3** `Ctrl+,` opens, `Esc` (focus in search) closes. (AC-C12-05)
* **T-C12-4** Undo history depth is a dropdown with exactly 25/50/100/200/500, default 25; choosing 100 persists; a stored value 30 loads as 25 (nearest) and 80 as 100. (AC-C12-08)

## 5. Visual regression set
Capture at 2560×1600 (dsf 1.6 of 1600×1000) for: default light/dark, focus-2d, focus-3d, left/right/both collapsed, each settings page, views menu, display menu (incl. dim-other-layers on), shape row hover, shape context menu, colour menu, ghost add-fixed / add-variable / delete / mirror+span, 2D-hover preview, quick-tool erase, Edit-button states (1–4 + momentary), Focus-3D without slab. Compare against `reference/screenshots/` numbers 01–30. Regions to compare separately: left card, centre, right card, status bar.

## 6. Performance budgets (release build, 3-year-old laptop GPU)
| Operation | Budget |
|---|---|
| 2D grid redraw 32×32 | < 8 ms |
| 3D redraw after stroke segment (≤ 2 000 voxels) | < 16 ms |
| 3D hover pick + ghost update (3D or 2D source) | < 4 ms |
| Open Settings dialog | < 100 ms |
| Layout mode transition | 200 ms animation, no dropped frames at 60 Hz in 3D focus |
| Shape switch (select row) | < 50 ms to fully refreshed UI |

## 7. Manual scripts
Run all journeys J1–J8 (`03-user-journeys.md`) in light and dark, at scale 1.0 and 1.6, and at 1280×800 (one card collapsed) and at the **minimum width 960 dp with both side cards collapsed**. Record pass/fail and screenshots. Additionally verify: tab order (`overview/04`), every tooltip text vs spec, no clipped captions at 960 dp, keyboard-only completion of J1, high-contrast OS mode legibility.


---
<!-- FILE: migration/03-user-journeys.md -->

# User journeys (integration tests)

Each journey lists steps → expected UI/state; use as manual scripts and as automated end-to-end tests. Component references in brackets.

## J1 — Create a new piece from scratch
1. Click **+ New** [C01] → new shape `S5` appended, selected, empty; 3D shows empty message; status "Shape S5 has 0 voxels (0 fixed, 0 variable)".
2. Voxel editor ▸ **Grid size** (under the grid): set X=4, Y=3, Z=4 with the steppers [C02] → grid, rulers and 3D bounds update; layer strip shows 4 chips.
3. Voxel editor: tool **Fixed** (key `1`), plane **XY**, layer 1. Click and drag across a row of cells [C07/C08] → voxels appear in grid and 3D; status updates; one undo removes the stroke.
4. `PgUp` to layer 2; draw more. Layer-strip dots show layers with voxels.
5. Rename: `F2`, "Arm piece", Enter [C01]. Weight: row **W+** → badge `W2`.
**Expected:** each action is one undo step; everything reachable without switching tabs; the new shape is `S5` (list position 5).

## J2 — Edit an existing piece directly in 3D
1. Select `S2` (Edit mode off). Hold **Shift**, hover a voxel face in 3D [C06] → cursor becomes the pointer with a coloured-square badge at its upper right (Fixed), ghost cube appears adjacent to the face, the toolbar **Edit** button shows a dashed momentary highlight with badge **1**.
2. **Shift+click** → fixed voxel added. **Alt/⌘+click** another face → **2** Variable voxel (dashed, translucent). **Ctrl+click** a voxel → **3** Erase. Release the key → cursor/ghost vanish instantly.
3. Hover outside the grid bounds with Shift → red dashed face outline; click does nothing.
3b. Display menu ▸ **Dim other layers** on → voxels outside the active layer fade; the **layer slab** (tinted volume with edges) shows the editing layer. Collapse the right sidebar or enter Focus-3D → slab and dimming disappear (the 2D editor is not visible); bring the editor back → they return.
4. Press `E` (Edit mode on; Edit button shows the active tool's icon and number). Choose **Erase** (key `3`; the cursor becomes the eraser icon alone and the button icon/number change in place); **Paint** (`4`) shows the bucket icon with a drop in the drawing colour, plain-click voxels → they delete; switch to **Paint** (`4`) with colour C2 → plain click recolours; hold Shift briefly → quick tool 1 overrides, release → back to Paint.
4b. Hover cells in the **2D grid**: the same ghost appears in 3D at that cell of the active layer; with Ctrl held it previews erasing.
5. Drag to orbit (movement > 4 dp) → no edits made. Middle-drag pans; wheel zooms; Shift-drag pans (or orbits if toolbar is in Pan mode).
**Expected:** grid, layer dots and status stay in sync; undo per click.

## J3 — Symmetric / bulk drawing with Mirror and Span
1. Empty 4×4×4 shape. Toggle **Mirror X** [C07]. Click cell (0,1) on layer 1 → voxels at x=0 and x=3.
2. Toggle **Span Z** also. Click (1,1) → voxels through all 4 layers at x=1 and x=2 (mirrored), 8 voxels.
3. In 3D hover with Shift → ghosts show all copies before clicking.
4. Toggle both off; verify normal single placement.
**Expected:** counts match expansion algorithm; single undo restores.

## J4 — Repair and tidy a piece
1. Load `S3`-like shell with an enclosed cavity. Inspector ▸ **Repair & surface** → **Fill interior holes** [C05] → flash "Filled N interior voxels"; second click → "No interior holes found".
2. **Make variable ▸ Inner** → interior voxels hatched/dashed in grid and translucent in 3D.
3. **Clear colour ▸ Outer** → outer voxels return to Default colour.
4. Inspector ▸ **Fit & scale** ▸ **Prune grid to fit piece**, then **Center** [C03] → grid trimmed; piece centred; Grid-size steppers update.
5. Fit & scale ▸ **Double ×2** then **Minimize** → returns to original size.
**Expected:** all single undo steps; blocked cases show messages.

## J5 — Organise shapes and weights
1. Hover rows → actions appear; **Duplicate** `S2` → copy appended as `S5`.
2. Drag `S4` above `S1` [C01] → it becomes `S1` (ids and colours renumber by position; flash "Order changed — shapes renumbered"); right-click the new `S3` → Move later, Weight +1, Delete (later shapes renumber).
3. Collapse left sidebar → rail shows chips; click a chip to select; expand again.
**Expected:** ids are always S1…SN in list order; selection follows the moved shape; order persists; weight badges correct; rail in sync with list.

## J6 — Colour work and display toggles
1. Footer **＋** → new colour; select it; tool **Paint** (`4`) and recolour voxels [C09].
2. Display menu ▸ **Voxel colour** ▸ Voxel colour → 3D shows per-voxel colours; **Piece colour** → single colour.
3. `⋯` ▸ Remove colour → voxels revert to Default.
4. Display menu ▸ **Projection** ▸ Orthographic → 3D projection changes; Perspective → back.
**Expected:** toggles persist after restart.

## J7 — Focus modes for precision and inspection
1. **Focus 2D** (editor header) [C11] → left collapses to rail, 3D narrows to compact view, grid enlarges; draw precisely with `1`–`4`, `PgUp/PgDn`.
2. `Esc` → layout restored.
3. 3D toolbar **Focus** → both sidebars rail, large canvas; orbit and use **Views ▸ Top**, **Fit**; `Esc`.
**Expected:** focus never persists after restart; collapse flags restored on exit.

## J8 — Settings
1. Click gear (or `Ctrl+,`) [C12]. Search "undo" → only Undo history depth; choose 100 from its dropdown (25/50/100/200/500).
2. Set Lighting off → 3D flat immediately; set Rotation method to Arc-ball → verify rotation behaviour.
3. Performance ▸ Worker threads slider; **Reset section**; **Restore all defaults**.
4. `Esc` closes. Restart app → settings persisted.
**Expected:** changes apply live; every legacy setting accounted for.

## J9 — Navigate with the view cube
1. Hover the cube → regions highlight (face centre, edge strip, corner). Click a face → smooth 360 ms snap to that face view; the cube becomes flat and shows ▲▼◀▶ plus roll arrows [C13].
2. Click ▶ → the right-hand face comes to the front; click ↷ twice → view rolled 180°.
3. Click an edge → 45° view; click a corner → corner view.
4. Click **Home** → smooth return to the default view with the shape framed. Press `F` after zooming → framing without changing orientation.
5. Turn off Settings ▸ Show view cube → cube disappears; `Home`/`F` still work.
**Expected:** all moves animate; manual orbit interrupts an animation.

## J10 — Work in a non-cubic voxel space
1. File ▸ New → choose **Spheres** from the five official types (Brick, Triangular Prism, Spheres, Rhombic Tetrahedra, Tetrahedra-Octahedra) → create. The Voxel editor header shows a lock chip "Spheres" [C14].
2. The 2D grid shows circles on an offset lattice; draw with the tools; the 3D view shows sphere voxels.
3. Open Transform → three flip buttons, six rotates and a 12-way nudge pad (UP LAYER / IN LAYER / DOWN LAYER); open Fit & scale → only Fit-to-grid (no Scale).
4. Try to change the type anywhere → there is no control; the chip tooltip explains it is fixed at File ▸ New.
5. Repeat with **Triangular Prism** (8-direction nudge pad, scale available) and the tetrahedral types and confirm their transform sets and cell shapes.
**Expected:** all type-dependent UI is derived from the type's descriptor.

## J11 — Define a puzzle problem
1. Click **Puzzle** in the workspace rail (C15). The Problems list shows P1; click **+ New** for P2 and double-click its label to name it "Large".
2. In **Result**, click **Change…** and pick the target shape [C16].
3. In **All pieces**, **double-click** the pieces you want, or **drag** chips into *In this puzzle* (the drop zone highlights); use each tile's **− count +** to set fixed counts.
4. Read the **summary strip** ("Pieces can fill the result", derived hole count) and the status bar.
**Expected:** every copy appears separately in the 3D scene; nothing resizes while counts change.

## J12 — Use a range, a group and colour rules (rare features)
1. Select a piece; in the right card enable **Allow a range** and set **Min 0 / Max 1** (inline-label steppers) — the 3D scene shows an optional translucent copy [C17].
2. Under **Exclusive group** press **+ New** on one piece, then pick **G1** on another — the Groups section appears listing both.
3. In **Colour rules**, tick additional piece→result colour cells or press *Allow all*.
**Expected:** features stay hidden until needed; the default editor is a single fixed count.

## J13 — Inspect pieces together
1. Drag in the 3D view: **every piece rotates in place** while the layout stays put; pan/zoom move/scale the whole arrangement.
2. Click a piece in 3D: it is framed (all its copies) and the left/right cards show it.
3. Open a file of another voxel space type — spheres, prisms and tetrahedral voxels render with their own geometry in the same scene [C18].

## J14 — Check that a puzzle can be assembled
1. Open **Solver** in the rail (C15), pick the problem in **Problem** (C19), keep **Assemblies**.
2. Press **Start**: the progress bar, "Assemblies" counter and the **Solutions list** fill while it runs; **Stop** pauses, **Continue** resumes.
3. Browse the list (↑ ↓) — the 3D view shows each assembly; sort by **Pcs** to group by piece count.
**Expected:** all browsing happens in the right card; the 3D view is read-only.

## J15 — Check disassembly and replay it
1. Choose **Assemblies and disassembly** and press **Start**. Only assemblies that can be taken apart are listed (possibly none — then use *Show the assemblies*).
2. Sort by **Moves** or **Level** (click the header; click again to reverse), double-click a row — the **Disassembly player** plays; use 4×/8× to skim, drag the slider to inspect a move.
3. Set one piece to **wireframe** and another to **hidden** with the chips; turn **Display ▸ Show through solid pieces** off to hide the wireframe behind solids.
**Expected:** the player and piece chips keep fixed sizes while you switch solutions.

## J16 — Tidy the results and recompute
1. Select a solution; open **Manage solutions ▾**, hover *All after this one* — the affected rows are struck through; click it and confirm.
2. Choose *Delete all disassemblies*, later *Analyse only the missing disassemblies*; the DA column updates.
3. **Export to STL** for the selected solution.
**Expected:** destructive actions are previewed and confirmed; analysing shows progress and updates statuses.

## J17 — Work with two BurrTools windows side by side
1. Resize the window to half of a 1080p/1440p screen (≥ 960 dp wide).
2. Collapse the right card (and, if needed, the left card) with the `panelR`/`panelL` buttons in the card headers — in any workspace; the 3D view takes the freed width.
3. Switch workspaces (Entities / Puzzle / Solver): each remembers its own collapse state.
**Expected:** nothing auto-collapses; every workspace is usable at 960 dp with both cards collapsed.

## J18 — Choose a theme
1. Open Settings (`Ctrl+,`) ▸ General ▸ **Theme** and pick **Light**, **Dark** or **System** (default).
2. With **System**, change the OS light/dark mode — the app follows at once; if the OS has no such setting the app stays Light.
**Expected:** the choice applies immediately and persists.

## J19 — Review solutions from the keyboard while orbiting with the mouse
1. Solver ▸ click the first row of the solutions list.
2. Press `Space` to play, `Space` to pause, `←`/`→` to step moves, `Shift+→` to see the separated pieces, `↓` for the next solution.
3. While doing so, orbit and zoom the 3D view with the mouse and click the view cube — the keys keep acting on the list.
4. `Del` on a poor solution → confirm.
**Expected:** focus never leaves the Solutions card because of viewport interaction; playback and stepping are instant.

## J20 — Work in Minimal density
1. Settings ▸ General ▸ **Interface density** → **Minimal** (or keep Standard).
2. Note the narrower cards (264 / 320), icon-only toolbar and tools (hover for names), rotated workspace captions, Fit & scale short labels.
3. Move the pointer to the top of the 3D view to reveal the toolbar; press `P` to switch to Pan (the toolbar flashes).
4. Press `F1` to see every shortcut grouped by workspace.
**Expected:** every function used in Standard is available; nothing needs a tooltip-less guess.


---
<!-- FILE: migration/04-framework-spike-plan.md -->

# Framework spike plan — Qt 6 Quick vs RmlUi

**Purpose:** replace estimates with measurements before committing. **Default decision: Qt 6 Quick**, unless the spike shows it misses a hard budget below that RmlUi meets (recommendation and rationale: framework analysis, Rev 5.3).
**Timebox:** 2 weeks per framework (two people in parallel, or sequentially). Same machine, same release build flags, same test puzzle (an example from `examples/` with ≥ 2 000 voxels and ≥ 100 solutions).

## Scope (identical in both)
1. **Shell:** workspace rail, top bar, three-card layout with **collapsible left/right cards** (per-workspace memory), Light/Dark/System theme with live OS switching.
2. **3D viewport:** the existing model rendered with modern GL (or QRhi for Qt), orbit/pan/zoom, **view cube** with snapping animation, hover pick.
3. **2D voxel grid (C08):** 32 × 32 layer, paint stroke with Shift/Alt/Ctrl, hatching for variable voxels.
4. **Solver right card (C20):** List view with **1 000** rows and header sorting, Slider view with scrubbing, fixed-height player, piece chips (solid / dotted / greyed).
5. **One modal** (Set range for all pieces) and **one native file dialog**.
6. **Tests in CI:** one controller unit test, one UI behaviour test (click a row → selection changes), one screenshot comparison — all headless.

## Measurements
| # | Metric | How | Hard budget |
|---|---|---|---|
| M1 | Cold start to first interactive frame | Process start → first frame with the test puzzle loaded | ≤ 1.5 s |
| M2 | Idle memory (RSS) after load | OS tools | ≤ 250 MB |
| M3 | 3D orbit frame time p95 | 10 s continuous orbit, 2 000 voxels | ≤ 16.7 ms |
| M4 | Sidebar collapse animation | dropped frames at 60 Hz, 3D focus | 0 |
| M5 | 2D grid redraw 32 × 32 / stroke segment | instrumented | < 8 ms |
| M6 | Solution scrub (List ↑↓ and Slider drag, 1 000 rows) | time from input to refreshed 3D | < 50 ms |
| M7 | Idle CPU | 60 s idle, window visible | < 1 % |
| M8 | Package size (Windows, zipped) | deploy folder | report |
| M9 | Build integration | Meson build on Linux + Windows artefact (native or MinGW cross) | works in CI |
| M10 | Test harness effort | hours to write the three tests + run headless in CI | report |
| M11 | Visual fidelity | pixel diff vs `reference/screenshots/51…`, `01…` at 1.0× and 1.6× | report |
| M12 | Contributor tasks | time for 3 scripted UI changes (rename a label, add a Search-options row, add a column to the solutions list) — once by hand, once with an AI agent; count agent errors (e.g. unsupported RCSS, QML anti-patterns) | report |

## Decision matrix (weights reflect the stated priorities)
| Criterion | Weight | Evidence |
|---|---|---|
| Performance (M1–M7) | 20 | measured |
| Mock fidelity & effort to 100 % (M11 + spike notes) | 15 | measured + review |
| Testability (M10 + harness quality) | 15 | measured |
| 5-year maintainability & upgrades | 15 | analysis (release/LTS policy, maintainer base, API churn) |
| Contributors & learning curve incl. AI (M12) | 15 | measured |
| Build/deploy (M8, M9) | 10 | measured |
| Platform features (native dialogs, DPI, theme, cursors, accessibility) | 10 | spike notes |
Score 1–5 per criterion; a framework that misses any **hard budget** is excluded unless the other misses it too. Record the result and raw numbers in `migration/RESOLUTIONS.md` (OQ-35).

## Either way — do first
Build the **framework-neutral controller layer** against `overview/06-ui-contract.md` (state, events, list models, undo coupling) and extract it from the legacy FLTK `mainwindow` callbacks. It is the largest piece of work, it is identical for both frameworks, and it keeps a later framework change cheap.


---
<!-- FILE: migration/RESOLUTIONS.md -->

# Open-question resolutions (fill in during implementation)

| OQ | Resolution | Source (file:function) | Spec/Mock updated? | Date / author |
|---|---|---|---|---|
| OQ-1 Flip semantics |  |  |  |  |
| OQ-2 Size-tab icon order / inner-outer pair order *(Grid Scale dropped)* |  |  |  |  |
| OQ-3 *(closed — dropped by product decision)* | n/a |  |  |  |
| OQ-4 2D plane selection, tooltips, shape context menu |  |  |  |  |
| OQ-5 Shrink below occupied voxels |  |  |  |  |
| OQ-6 Size limits |  |  |  |  |
| OQ-7 Out-of-grid transforms |  |  |  |  |
| OQ-8 Colour add/edit/remove |  |  |  |  |
| OQ-9 Legacy shortcuts / modifier-click / status glyph mapping |  |  |  |  |
| OQ-10 Menu commands |  |  |  |  |
| OQ-11 Config keys & ranges |  |  |  |  |
| OQ-12 Picking |  |  |  |  |
| OQ-13 RmlUi version & capabilities, integration with GL renderer/legacy GUI |  |  |  |  |
| OQ-14 New-shape defaults / delete confirm |  |  |  |  |
| OQ-15 Weight limits |  |  |  |  |
| OQ-16 Positional ids / cross-tab references / palette |  |  |  |  |
| OQ-17 OS default pointer bitmap for cursor composition |  |  |  |  |
| OQ-18 File ▸ New flow, voxel type names, view-cube legacy details |  |  |  |  |
| OQ-19 Per-voxel-type transforms/scale/mirror/planes/layers |  |  |  |  |
| OQ-20 Groups meaning/storage |  |  |  |  |
| OQ-21 Legacy ←/→ in Puzzle tab |  |  |  |  |
| OQ-22 Colour rules semantics |  |  |  |  |
| OQ-23 Range storage / optional vs absent |  |  |  |  |
| OQ-24 Result voxel range text |  |  |  |  |
| OQ-25 Manage-solutions semantics (Delete/Analyse buttons) |  |  |  |  |
| OQ-26 Legacy Drop field |  |  |  |  |
| OQ-27 Solver sort options / Limit |  |  |  |  |
| OQ-28 Placement & Movement browsers |  |  |  |  |
| OQ-29 Prepare/Start/Continue/Stop/Step semantics |  |  |  |  |
| OQ-30 Move label / slider semantics |  |  |  |  |
| OQ-31 Groups in Solver UI |  |  |  |  |
| OQ-32 Just count UI |  |  |  |  |
| OQ-33 STL export |  |  |  |  |
| OQ-34 Legacy menu-only features | existing dialogs kept; Config = Settings; bulk range → Puzzle ⋯ menu | product owner | Rev 5.2 | resolved |
| *OQ-25 note:* Before/After = current sorted list order (confirmed by product owner) |  |  |  |  |
| OQ-35 Front-end framework decision (spike results, scores, raw numbers) |  |  |  |  |
| OQ-36 Legacy accelerators vs new keys |  |  |  |  |
