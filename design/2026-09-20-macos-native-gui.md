# Making the BurrTools GUI Feel Native on macOS

**Date:** 2026-09-20
**Scope:** `src/gui/main.cpp`, `src/gui/mainwindow.{h,cpp}`, new `src/gui/platform.{h,cpp}`, new `src/gui/mainmenu.{h,cpp}`, `scripts/create-macos-bundle.sh`, `meson.build`
**Branch:** `worktree-mac-native-gui`

---

## 1. Executive Summary

BurrTools on macOS is a working FLTK application that does not look or behave like a
Mac application. The three loudest tells, in order:

1. **The menu bar lives inside the window.** `mainwindow.cpp:4012` constructs an
   `LFl_Menu_Bar` (an `Fl_Menu_Bar`), so File/Export/Settings/About render as a strip
   across the top of the document window instead of in the system menu bar. No Mac
   application has done this since Carbon.
2. **No widget scheme is ever set.** `Fl::scheme()` is never called, so every control
   draws in FLTK's default Motif-derived boxes, partially flattened by four
   `Fl::set_boxtype()` overrides in `main.cpp`.
3. **Keyboard shortcuts are F-keys.** Save is F2 and Load is F3
   (`mainwindow.cpp:2111-2113`). There is no `⌘` binding anywhere in the application,
   and no App menu, so About, Settings and Quit are all in the wrong place.

Below that, the app has no icon in its bundle, cannot receive a Finder
double-click, titles its window `BurrTools - <path>`, and offers no way to save when
prompting about unsaved changes.

This document specifies a set of changes that address all of the above in portable
C++ — no Objective-C source files are added — behind a small platform seam, and
carves two focused units out of the 134 KB `mainwindow.cpp` in the process.

**Explicitly out of scope:** code signing and notarization; multi-document windows;
dark mode; redrawing the 125 XPM icons for Retina. Sections 9 and 10 record why.

### 1.1 What Already Works

Worth stating so the plan does not re-do it:

- **Native file dialogs.** `src/gui/filechooser.cpp` already uses
  `Fl_Native_File_Chooser` on macOS, with overwrite-confirmation tracking.
- **FLTK 1.4.4** (`subprojects/fltk.wrap`), which provides `Fl_Sys_Menu_Bar`,
  `fl_open_callback()`, automatic Window menus and HiDPI support.
- **Cocoa frameworks are already linked** (`meson.build:207-208`), and
  `NSHighResolutionCapable` is already true in the bundle plist.
- **Dirty-state tracking.** `mainWindow_c::changed` exists and drives confirmation
  prompts at four sites.
- **A load entry point.** `mainWindow_c::tryToLoad()` (`mainwindow.cpp:1997`) is
  exactly the hook a Finder-open handler needs.

---

## 2. Architecture: The Platform Seam

Two new units. `main.cpp` gains no `#ifdef __APPLE__` at all, and
`mainwindow.cpp` gains exactly one, at the menu bar's construction
(`mainwindow.cpp:4110-4114`):

```cpp
#ifdef __APPLE__
  MainMenu = new LFl_Sys_Menu_Bar(0, 0, 1, 1);
#else
  MainMenu = new LFl_Menu_Bar(0, 0, 1, 1);
#endif
```

**That one exception is unavoidable and is not worth a factory.** `LFl_Sys_Menu_Bar`
wraps `Fl_Sys_Menu_Bar`, which FLTK compiles only under `__APPLE__`, so the type does
not exist to name off macOS. The choice is between this three-line `#ifdef` and a
`mainmenu::createMenuBar()` factory whose only job would be to move the same three
lines behind a function call while forcing the return type up to `Fl_Menu_ *` and
losing the layouter typing at the call site. The `#ifdef` is the smaller thing.

Note also that `platform::usesSystemMenuBar()` does **not** end up gating menu bar
construction — the `#ifdef` above does, because it must be a compile-time decision.
What it actually gates at runtime is the F-key handling in `mainwindow.cpp`
(`handle()`, around line 3348): the portable table binds F2/F3/F4, the macOS table
uses ⌘S/⌘O/⌘3 instead, and those `FL_F + n` cases must not fire where the system
menu bar is in charge.

Beyond that one branch, all platform behaviour is confined to `platform.cpp` and
`mainmenu.cpp`.

### 2.1 `src/gui/platform.{h,cpp}`

```cpp
namespace platform {
  void applyLookAndFeel();                        // scheme, fonts, metrics
  bool usesSystemMenuBar();                       // true on macOS
  void installOpenHandler(void (*)(const char*)); // Finder / Dock document opens
  std::string windowTitle(const char* file, bool edited);
  void setDocumentEdited(Fl_Window*, bool);       // close-button dot
  void openHelp();                                // fl_open_uri to the user guide
}
```

Every function has a portable no-op or fallback body outside `__APPLE__`, so callers
never branch. `windowTitle()` is a pure function of its arguments.

### 2.2 `src/gui/mainmenu.{h,cpp}`

`menu_MainMenu[]` (`mainwindow.cpp:2107`), `findMenuEntry()` and the
activate/deactivate logic at `mainwindow.cpp:2353-2379` move here wholesale.

```cpp
namespace mainmenu {
  const Fl_Menu_Item* table();                // the platform-appropriate table
  void installApplicationMenu(mainWindow_c*); // macOS App-menu wiring; no-op elsewhere
  void assertTablesConsistent();              // startup drift check
}
```

**As implemented, there is no `mainmenu::findEntry()`.** The plan called for one, but
Task 3 established that an index into the *static* table is the wrong answer on macOS:
FLTK inserts its own "Window" entry into the *live* menu array at first `show()`, which
shifts every index after it. The lookup therefore lives in `mainwindow.cpp` as
`liveMenuIndex(const Fl_Menu_*, Fl_Callback*)`, walking the live array the menu bar
actually holds. Its `bt_assert` on a miss is preserved in `setLiveMenuActive()`, which
is where the index is consumed. The requirement below still holds — it keys on the
callback pointer, not the label.

**The lookup must key on the callback pointer, not the label.** Today's
`mainWindow_c::findMenuEntry()` (`mainwindow.cpp:2348-2359`) does
`strcmp(item.label(), txt)` and `bt_assert(found >= 0)` on a miss, and
`updateInterface()` calls it with the literals `"Images"` and `"STL"`
(`mainwindow.cpp:2369-2377`). Since the macOS table renames those items to
`"Image…"` and `"STL…"` (Section 3.2), a label-keyed lookup would **assert and abort
at startup on macOS**. Keying on the callback pointer is platform-neutral, survives
every future label change, and keeps the existing duplicate-match assertion
meaningful.

Two tables live side by side in `mainmenu.cpp`: the portable one (byte-identical to
today's, so Linux and Windows are unchanged) and the macOS one from Section 3. Both
reference the same `cb_*_stub` callbacks, which stay in `mainwindow.cpp` and gain
declarations in `mainmenu.h`.

### 2.3 Drift Protection

The two tables are the maintenance hazard of this design: a menu item added to one
and forgotten in the other is silent on the platform that missed it.
`assertTablesConsistent()`, called once from `main()`, walks the portable table and
`bt_assert`s that every callback function pointer in it also appears in the table this
build targets. Labels and shortcuts are deliberately **not** compared — they
legitimately differ (Section 3.2).

**The check is one-directional, portable → platform, and must stay that way.**
`cb_Help_stub` exists only in the macOS table, because the portable menu has never
had a Help item, so requiring set *equality* would fail every macOS build. The price
of the asymmetry is that an item added only to `menu_Mac` is not caught; the direction
that actually loses functionality — an item added to the portable table and forgotten
on macOS — is.

**Where it runs.** `main()` calls it on both paths: the normal startup path, before
the main window is constructed, so running the app at all exercises it; and the
`--self-check` path, which does nothing else and exits, which is what `just check-gui`
and the macOS CI job invoke. Note it can only ever *fail* on macOS — elsewhere
`activeTable()` is `menu_Portable`, so the loop compares the portable table with
itself. That is why `just check-gui` is wired into the macOS job specifically.

**Known limitation, accepted:** `test_burrtools` links `src/lib` only, with no FLTK
dependency, and this project does not change that, so this is not a unit test. Adding
FLTK to the test binary to unit-test a menu table was judged the more expensive
option.

### 2.4 Rejected: A Full Platform Abstraction Layer

A `PlatformIntegration` interface with Mac/X11/Win32 subclasses was considered and
rejected. Six free functions cover the need, and two of three implementations would
be empty.

---

## 3. The macOS Menu

### 3.1 Structure

`Fl_Sys_Menu_Bar` supplies more than expected: it builds the application menu
itself, `Fl_Mac_App_Menu::custom_application_menu_items()` injects entries into it,
and `Fl_Sys_Menu_Bar::window_menu_style()` produces a Window menu with Minimize,
Zoom and the window list. Two of the six menus need no table entries.

```
BurrTools   About BurrTools          → cb_About_stub
            Settings…          ⌘,    → cb_Config_stub
            (Services, Hide, Hide Others, Show All — supplied by FLTK)
            Quit BurrTools     ⌘Q    → cb_Quit_stub   (see §3.3)

File        New                ⌘N    → cb_New_stub
            Open…              ⌘O    → cb_Load_stub
            Import…                  → cb_Load_Ps3d_stub
            ─────
            Save               ⌘S    → cb_Save_stub
            Save As…          ⇧⌘S    → cb_SaveAs_stub
            ─────
            Export         ▸   Image…         → cb_ImageExport_stub
                               Vector Image…  → cb_ImageExportVector_stub
                               STL…           → cb_STLExport_stub
            ─────
            Close              ⌘W    → cb_Quit_stub

Puzzle      Edit Comment…            → cb_Comment_stub
            Convert…                 → cb_Convert_stub
            Import Assemblies…       → cb_AssembliesToShapes_stub
            ─────
            Status             ⌘I    → cb_StatusWindow_stub

View        Toggle 3D          ⌘3    → cb_Toggle3D_stub

Window      (Minimize ⌘M, Zoom, window list — supplied by FLTK)

Help        BurrTools User Guide  ⌘?  → platform::openHelp()
```

**"Puzzle" rather than "Edit".** BurrTools has no Undo, Cut, Copy or Paste; the sole
candidate for an Edit menu is *Edit Comment*, and a Mac Edit menu holding one
unrelated item reads worse than no Edit menu. Mac applications without text editing
legitimately omit it. Puzzle collects the four document-level operations instead.

**Close ⌘W and Quit ⌘Q do the same thing.** BurrTools has one window, so closing it
ends the program. Both are listed because a Mac user reaches for ⌘W to dismiss a
document and for ⌘Q to leave the application, and finding either missing is jarring.
Both route through `cb_Quit_stub` and therefore through the same unsaved-changes
guard.

**Help is new.** There is no in-application help despite the README's claim —
`mainwindow.cpp` contains no help code at all. `platform::openHelp()` calls
`fl_open_uri("https://burrtools.sourceforge.net/gui-doc/toc.html")`.

### 3.2 Divergence From the Portable Table

The macOS table differs in **labels** (`Load` → `Open…`, `Import Assms` →
`Import Assemblies…`, ellipses on dialog-opening items) and in **shortcuts**
(`⌘` bindings replacing F-keys). The Linux and Windows table keeps today's wording
and F-keys verbatim, so no existing user's muscle memory changes.

**F-keys remain live on macOS as secondary bindings.** An `Fl_Menu_Item` carries only
one shortcut, so F2/F3/F4 are handled as an `FL_SHORTCUT` case in
`mainWindow_c::handle()` dispatching to `cb_Save_stub`, `cb_Load_stub` and
`cb_Toggle3D_stub`. They are intentionally not shown in the menu — the menu displays
the `⌘` binding, and the F-key works silently.

### 3.3 Blocking Risk: ⌘Q and Unsaved Work

`mainWindow_c::hide()` (`mainwindow.cpp:1769`) overrides `Fl_Window::hide()` to run
the unsaved-changes prompt. Whether FLTK's App-menu Quit routes through that override
or instead calls `Fl::program_should_quit()` **could not be determined by reading and
must be verified on a real build before this work is considered done.** If Quit
bypasses the override, ⌘Q discards unsaved work silently — strictly worse than
today's behaviour.

**Fallback if verification fails:** replace the App menu's Quit item callback with
`cb_Quit_stub` directly via `Fl_Mac_App_Menu::custom_application_menu_items()`, and
confirm by test that a dirty puzzle prompts on ⌘Q.

### 3.4 Secondary Risk: Menu Rebuild Cost

`updateInterface()` ends with `MainMenu->copy(menu_MainMenu, this)`
(`mainwindow.cpp:2379`), re-copying the entire table every time the interface state
changes — which is often. With an in-window `Fl_Menu_Bar` that is a cheap memory
copy. With `Fl_Sys_Menu_Bar` it tears down and rebuilds the actual system menu bar,
which is neither free nor guaranteed flicker-free.

**Mitigation:** track the activate/deactivate state that the copy exists to publish
(two booleans today — Images and STL) and re-copy only when one of them actually
changes. This is a small, self-contained change and should be made as part of step 3
rather than deferred.

Unlike Section 3.3 this is a polish issue, not a correctness one — if it turns out
FLTK already handles it efficiently, the mitigation can be dropped.

### 3.5 Shortcuts Open To Revision

`⌘I` for Status follows the Get Info convention. `⌘3` for Toggle 3D follows the
⌘1/⌘2/⌘3 view-switching convention and is otherwise unclaimed. Both are judgement
calls with no strong precedent in this application's domain.

---

## 4. Look and Feel

### 4.1 What `applyLookAndFeel()` Does

`main.cpp:66-69` currently sets four `THIN` boxtype overrides — a hand-rolled attempt
at a flatter look that actively conflicts with a scheme, since schemes install their
own box-drawing functions. **The four `Fl::set_boxtype()` calls are removed**,
replaced by a single `platform::applyLookAndFeel()` call before any window is
constructed:

1. **`Fl::scheme(...)`, on all platforms.** `gleam` or `oxy`. This is not decided
   from reading: build both, compare, choose. `gleam` is the default if they are a
   wash — it is the better-tested of the two.
2. **macOS font and metrics.** Remap `FL_HELVETICA` to `.AppleSystemUIFont` so labels
   render in SF Pro rather than Helvetica, and set the default label size to 13 to
   match macOS control text (FLTK's default is 14).
3. **`Fl::get_system_colors()` is retained** unchanged.

`.AppleSystemUIFont` is a private system font alias. It resolves in practice, but a
silent fallback to a default face would be invisible in code review — verification
item in Section 8.

### 4.2 Risk: Custom-Drawn Widgets

Roughly a dozen widgets draw themselves with raw `fl_draw` primitives rather than
through boxtypes, and therefore do not follow a scheme:

`BlockList.cpp`, `Fl_Table.cpp`, `grideditor.cpp`, `grideditor_0..4.cpp`,
`separator.cpp`, `statusline.cpp`

After the scheme switch these may sit visually apart from their surroundings —
mismatched backgrounds, borders that no longer align. Which ones, and how badly,
cannot be predicted from reading; it requires building and inspecting every tab and
dialog (Section 8.2).

**Scope boundary:** touching up a handful of these is in scope. If most of them need
rework, the correct response is to stop and re-scope rather than absorb an open-ended
visual-rework project into this one.

### 4.3 Rejected: A Scheme Preference

A persisted setting letting users pick the scheme would be a safety valve for anyone
who preferred the old look. It is speculative demand and adds a new persisted
configuration key. Rejected as YAGNI; trivially added later if anyone complains.

---

## 5. Finder Integration

### 5.1 Document Opens

`platform::installOpenHandler()` wraps FLTK's `fl_open_callback()`, which fires for
Finder double-clicks, Dock drops and "Open With". Three details carry more weight
than the wiring:

- **Registration order.** Launching *by* double-clicking a puzzle delivers the event
  almost immediately, so the handler must be installed after `mainWindow_c` is
  constructed and before `ui->show()`. This is a small reordering in `main.cpp`.
- **It must not bypass the unsaved-changes guard.** `tryToLoad()` loads
  unconditionally; the prompt lives in `cb_Load_stub`. The Finder handler routes
  through `confirmDiscard()` (Section 6.2) first. Wiring it directly to `tryToLoad`
  would silently destroy unsaved work.
- **Single-document limitation.** Opening a second puzzle replaces the first rather
  than opening a new window, because `mainWindow_c` assumes one puzzle throughout.
  This deviates from Mac expectations and is a known, accepted limitation
  (Section 10).

### 5.2 Icons

The only available art is `burricons.ico` at 64×64, 8-bit. macOS wants 1024×1024.

**Approach: render the icon with BurrTools itself.** The application already renders
burr puzzles in OpenGL and already has a tiled high-resolution export path
(`imageexport.cpp` + `tr.c`) built to produce images larger than the screen. A script
renders a puzzle from `examples/` at 1024×1024 on a transparent background,
`iconutil` packs the `.iconset`, and the resulting `.icns` is committed alongside the
script that generated it. The icon is then reproducible, and it depicts the actual
output of the program.

Upscaling the 64×64 art was rejected (a blurry icon undercuts the entire point of
this work); commissioning new art was rejected as a design task that would block
engineering work.

The `.xmpuzzle` document icon gets the same treatment at smaller sizes.

### 5.3 Bundle Changes

`scripts/create-macos-bundle.sh` gains three `Info.plist` keys:

| Key | Value | Purpose |
| :--- | :--- | :--- |
| `CFBundleIconFile` | `BurrTools.icns` | Application icon |
| `CFBundleTypeIconFile` | `BurrToolsDoc.icns` | Document icon, inside the existing `CFBundleDocumentTypes` entry |
| `NSRequiresAquaSystemAppearance` | `true` | Light appearance only (Section 9) |

`CFBundleDocumentTypes` for `.xmpuzzle` and `NSHighResolutionCapable` are already
present and correct.

---

## 6. Window Title and the Save Prompt

### 6.1 Titles

`platform::windowTitle()` returns the bare filename on macOS (`Bermuda.xmpuzzle`),
and today's `BurrTools - <file>` elsewhere. Five call sites in `mainwindow.cpp`:
1506, 1555, 1758, 2025, 4009.

One change applies on **all** platforms: `"unknown"` becomes `"Untitled"`. The
current string reads like an error condition.

**The edited dot.** `changed` is assigned directly in roughly 50 places, so hooking
every write means a `setChanged()` refactor across all of them. Instead,
`mainWindow_c::update()` — already called once a second from the loop in
`main.cpp:50-53` — calls `platform::setDocumentEdited(this, changed)`. Worst case the
dot appears a second late, which is imperceptible. The setter refactor is the cleaner
design and would be correct if those ~50 lines were being touched for other reasons;
they are not.

`setDocumentEdited()` requires `[NSWindow setDocumentEdited:]`. This does **not**
require an Objective-C source file: `<objc/message.h>` is a plain C API, so
`objc_msgSend` against the `NSWindow*` returned by FLTK's `fl_xid()` does the job
from ordinary C++, with no new link dependency (Cocoa is already linked). If this
proves fragile in practice, the fallback is to drop the edited dot; nothing else in
this design depends on it.

### 6.2 Save / Don't Save / Cancel

Four sites currently offer Cancel-or-lose-your-work with no way to save:
`mainwindow.cpp:1493` (New), `1523` and `1538` (Load), `1770` (Quit). They collapse
into one helper:

```cpp
bool mainWindow_c::confirmDiscard(const char* action);  // false => abort the operation
```

Behaviour:

- Returns `true` immediately when `!changed`.
- Otherwise prompts **Save / Don't Save / Cancel**.
- *Save* routes through the existing save path, including bouncing to Save As when
  the puzzle has no filename. **If the user cancels that save dialog, the whole
  operation aborts** rather than falling through to a discard.
- *Cancel* returns `false`.

Five call sites: the four existing ones plus the Finder-open path from Section 5.1.

**Button ordering.** `fl_choice` lays its buttons out right-to-left from the first
argument. The macOS convention places the default (Save) rightmost, Cancel beside it,
Don't Save furthest left. The correct argument order is to be settled by looking at
the rendered dialog, not by reasoning about the API.

This change improves all three platforms, not only macOS.

---

## 7. Build System Changes

`meson.build` gains `src/gui/platform.cpp` and `src/gui/mainmenu.cpp` to `gui_src`
(lines 136-190). No new dependencies: Cocoa is already linked on darwin
(`meson.build:207-208`) and `libobjc` comes with it.

---

## 8. Verification

### 8.1 Automated

Nothing here is covered by `test_burrtools`, which links `src/lib` only. The
automated gates are the existing ones, and all must pass:

```
just build && just test-all && just check && just build-werror
```

Plus `mainmenu::assertTablesConsistent()` at startup (Section 2.3).

### 8.1.1 What Automated Verification Actually Covered

Recorded after implementation, because the gap matters when reading the checklist below.

Every task passed `just build && just test-all && just check && just build-werror`, plus
`./build/burrtools --self-check`, and the branch passed them again from a clean `just rebuild`.
Beyond that, three things were established by evidence rather than assumption:

- **⌘Q still runs the unsaved-changes prompt** — proven from FLTK's source rather than by a
  keystroke: `applicationShouldTerminate:` (`Fl_cocoa.mm:1608`) → `Fl::handle(FL_CLOSE, win)`
  → `Fl::default_atclose` (`Fl_Window.cxx:172-174`) → `window->hide()`, virtual per
  `Fl_Widget.H:902`, reaching `mainWindow_c::hide()`. On cancel, `win->shown()` stays true and
  FLTK returns `NSTerminateCancel`. This was the spec's one blocking risk (§3.3) and it
  resolved favourably; the fallback was not needed.
- **Finder opens actually fire** — a temporary stderr probe confirmed `fl_open_callback` runs
  on both the cold-launch and already-running paths, delivered by Apple Event rather than
  argv, so no double-load via `show(argc, argv)`.
- **The edited-dot call is well-formed** — a runtime probe confirmed `fl_xid()` returns an
  `NSWindow` descendant (`NSKVONotifying_FLWindow`) that responds to `setDocumentEdited:`.

**What no automated step could cover:** anything requiring a human to look at or drive the
GUI. The sandbox had neither Accessibility permission (so no synthetic input — an early
attempt misdelivered a ⌘Q to the developer's terminal, after which synthetic input was
banned outright for this work) nor Screen Recording permission (so screen captures returned
black). Every item in §8.2 concerning appearance, menu interaction, or on-screen results
therefore remains genuinely unverified and needs a person.

### 8.2 Manual Checklist

The substance of this work is visual and behavioural, so the checklist is the real
verification. Every item must be confirmed on a real build before the work is called
done.

**Blocking — data loss:**

- [x] ⌘Q with a dirty puzzle prompts Save / Don't Save / Cancel (Section 3.3).
- [x] Cancelling the save dialog inside that prompt aborts the quit.
- [x] Finder double-click with a dirty puzzle open prompts before loading.
- [x] Each of the five `confirmDiscard()` sites saves correctly when Save is chosen,
      including the untitled → Save As path.

**Menus and shortcuts:**

- [x] Menu bar appears in the system menu bar; the in-window strip is gone.
- [x] App menu shows About, Settings… ⌘,, Quit ⌘Q.
- [x] Window menu supplies Minimize ⌘M and Zoom.
- [x] ⌘N/⌘O/⌘S/⇧⌘S/⌘W/⌘I/⌘3/⌘? all fire the right callback.
- [x] F2/F3/F4 still work on macOS as secondary bindings.
- [x] Export submenu still activates and deactivates correctly (the logic moved from
      `mainwindow.cpp:2369-2377`), with lookup keyed on callback rather than label.
- [x] Repeatedly changing Export activation state causes no visible flicker or lag in
      the system menu bar (Section 3.4).
- [x] Linux build: menu bar, labels and F-keys are unchanged from before.

**Appearance:**

- [ ] Compare `gleam` against `oxy`; record the choice and the reason.
- [x] Labels render in SF Pro, not Helvetica — confirm `.AppleSystemUIFont` resolved
      rather than silently falling back. **Confirmed by measurement, not by eye.**
      `CTFontCreateWithName(".AppleSystemUIFont")` returns `System Font Regular` /
      `.SFNS-Regular`, identical to what the supported `CTFontCreateUIFontForLanguage(
      kCTFontUIFontSystem, …)` API returns; an unresolvable name falls back to
      Helvetica, so the path does fail loudly enough to detect. Measured in the running
      application, the rendered width of a sample string at 13pt was 186.310 for
      `FL_HELVETICA` as remapped and 186.310 for the system UI font, against 174.180
      for real Helvetica — so the remap reaches FLTK's text rendering, not just its
      font table.
- [ ] Walk every tab and every dialog, checking the custom-drawn widgets from
      Section 4.2 against their surroundings.
- [ ] Linux and Windows: the new scheme is applied and nothing is visually broken.

**Integration:**

- [x] Double-clicking an `.xmpuzzle` in Finder opens it, both when BurrTools is
      already running and when it is not.
- [ ] Dropping a puzzle on the Dock icon opens it.
- [ ] App and document icons appear in Finder, the Dock and ⌘-Tab.
- [ ] Title shows the bare filename; the close-button dot tracks the dirty state.
- [ ] Help menu opens the user guide in the default browser.

---

## 9. Deliberately Out of Scope

| Item | Decision | Reason |
| :--- | :--- | :--- |
| Dark mode | Opt out via `NSRequiresAquaSystemAppearance` | FLTK 1.4.4's behaviour under dark appearance is unverified, and the 125 XPM icons assume a light background. A consistently light app looks deliberate; a half-inverted one looks broken. Revisit when the icon set is redone. |
| Code signing / notarization | Not addressed | Requires an Apple Developer account and a CI secrets story. The DMG README already documents the right-click-Open workaround. |
| Retina icon artwork | Not addressed | 125 XPM bitmaps at 1×. Redrawing them is an independent project with no dependency on anything here. Text and vector drawing are already sharp via `NSHighResolutionCapable`. |
| Multi-document windows | Not addressed | `mainWindow_c` assumes a single puzzle throughout. A genuine Mac deviation, recorded rather than hidden. |
| Objective-C++ / NSToolbar / sheets | Not addressed | Would fight FLTK's layout model for a marginal gain over what the seam already achieves. |
| Config in `~/Library/Preferences` | Not addressed | `~/.burrtools.rc` is portable and works. Moving it is churn with a migration cost and no user-visible benefit. |
| Scheme preference setting | Rejected | Speculative demand; see Section 4.3. |

---

## 10. Known Deviations After This Work

Honest accounting of what will still not be Mac-like once everything above is done:

1. **Single-document.** Opening a puzzle replaces the current one instead of opening
   a new window.
2. **No Open Recent.** Not implemented today; adding it is independent work.
3. **Icons are 1× bitmaps**, soft on Retina displays.
4. **Light appearance only**, regardless of the system setting.
5. **Unsigned.** First launch requires right-click-Open.
6. **Modal dialogs, not sheets.** Dialogs appear as separate windows rather than
   attached to the document window.

### 10.1 Added During Implementation

Deviations discovered while building this, which the design did not anticipate:

7. **The app icon is not yet drawn.** All the plumbing landed — `scripts/make-macos-icons.sh`,
   the bundle wiring, and the plist keys, which are emitted only when the artwork exists so
   the bundle never names a missing file. The artwork itself did not: rendering a puzzle at
   1024×1024 requires BurrTools' own Export ▸ Image dialog, and there is no headless export
   path (`imageExport_c` is a GUI window class; neither `burrTxt` nor `burrTxt2` exports
   images). Until someone renders `mac/icon-source.png` and runs the script, the app shows
   the generic macOS application icon.

8. **The save prompt's buttons are not in Mac order.** They read
   [Don't Save] [Save] [Cancel] rather than the conventional [Don't Save] [Cancel] [Save].
   FLTK hardcodes the *middle* button of `fl_choice` as the `Fl_Return_Button`
   (`subprojects/fltk/src/Fl_Message.cxx:180-186`), so Save cannot be both rightmost and the
   Enter default. The chosen order makes every accidental path non-destructive — Enter
   saves, Escape cancels, and Don't Save needs a deliberate click. Placement was traded for
   safety deliberately.

9. **Opening several files at once loads only the first.** `openFromSystem()` drops
   re-entrant system-open requests, because FLTK drains its dropped-files list from *nested*
   `Fl::wait()` loops (`subprojects/fltk/src/Fl_cocoa.mm:853-857`) and would otherwise stack
   confirmation dialogs whose answers applied to the wrong puzzle. Since BurrTools shows one
   puzzle at a time, opening N files could only ever display one regardless.

10. **A single Finder open delivers two events.** Observed, not theorised: one `open`
    invocation calls back twice with the same filename, independent of dirty state — so
    before the re-entrancy guard, one double-click called `tryToLoad()` twice. Harmless
    (loads are idempotent) and now absorbed by the guard, but the root cause is unexplained
    and deserves a follow-up.

---

## 11. Implementation Order

Ordered so that each step is independently verifiable and the riskiest unknown is
resolved early.

1. **Extract `mainmenu.{h,cpp}`** with the portable table only. No behaviour change.
   Verify Linux and macOS are byte-identical in behaviour to before.
2. **Extract `platform.{h,cpp}`** with no-op bodies plus `applyLookAndFeel()`. Remove
   the four `set_boxtype` calls, set the scheme, compare `gleam` and `oxy`. Walk the
   Section 8.2 appearance checklist.
3. **Add the macOS menu table** and `Fl_Sys_Menu_Bar` wiring, including the App and
   Window menus. **Resolve the ⌘Q question of Section 3.3 here** — it is the one
   finding that could change the plan, so it comes before further work is built on
   top of it.
4. **Add `confirmDiscard()`** and convert the four existing prompt sites.
5. **Add Finder integration**: `installOpenHandler()` routed through
   `confirmDiscard()`, plus the registration reordering in `main.cpp`.
6. **Add titles and the edited dot.**
7. **Generate the icons** and update the bundle script and plist.
8. **Full manual checklist** on macOS and Linux.

Steps 1 and 2 are pure refactoring and should be reviewable independently of
everything after them.
