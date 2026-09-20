# macOS-Native GUI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the BurrTools FLTK GUI look and behave like a native macOS application — system menu bar, ⌘ shortcuts, a modern widget scheme, Finder document opens, real icons, document-style titles, and a Save/Don't Save/Cancel prompt — without adding Objective-C sources or regressing Linux and Windows.

**Architecture:** All platform-conditional behaviour is confined to one new file, `src/gui/platform.cpp`, behind a six-function portable interface. The menu table and its lookup logic move out of the 134 KB `mainwindow.cpp` into `src/gui/mainmenu.cpp`, which holds one table per platform built from shared callbacks. Neither `mainwindow.cpp` nor `main.cpp` gains a single new `#ifdef __APPLE__`.

**Tech Stack:** C++20, FLTK 1.4.4 (`subprojects/fltk`), OpenGL, Meson + `just`, Catch2 v3, macOS Cocoa frameworks (already linked).

**Spec:** [`design/2026-09-20-macos-native-gui.md`](2026-09-20-macos-native-gui.md) — read it first; this plan argues from it.

## Global Constraints

- **No Objective-C or Objective-C++ source files.** Where a Cocoa call is unavoidable (`setDocumentEdited:`), use the plain-C `<objc/message.h>` runtime API from ordinary C++.
- **No new `#ifdef __APPLE__` in `mainwindow.cpp` or `main.cpp`.** Platform branching lives only in `platform.cpp` and `mainmenu.cpp`.
- **Linux and Windows behaviour is unchanged** except for two deliberate cross-platform improvements: the widget scheme (Task 2) and the Save/Don't Save/Cancel prompt (Task 5).
- **Minimum deployment target `11.0`** (`meson.build:39`); do not raise it.
- **Never edit `subprojects/` or `src/lua/`.**
- **Do not assert exact solver iteration counts** in any test (project rule; no task here should touch solver tests at all).
- **Quality gate after every task:** `just build && just test-all && just check && just build-werror` must pass, plus `./build/burrtools --self-check` once Task 1 lands.
- **Verified baseline (2026-09-20, commit `91c9df9`):** build OK, 4/4 test suites OK, cppcheck clean. Any failure after this point is introduced by this work.

---

## Testing Reality — Read This Before Task 1

`test_burrtools` links `src/lib` only and has no FLTK dependency. The spec's Section 2.3 decision was to keep it that way, so **there is no way to unit-test GUI code in this project, and this plan does not pretend otherwise.** Tasks below therefore substitute:

1. **A `--self-check` startup mode** (built in Task 1), which runs the invariant assertions and exits without opening a window. This is the spec's "startup assertion" made runnable in CI — a small extension of the approved design, flagged here for the reviewer.
2. **The existing automated gates** — build, `test-all`, cppcheck, `-Werror` — which catch compile and regression damage.
3. **Explicit manual verification steps** with stated expected observations. These are the real test for visual and behavioural work; do not skip them or mark them done without performing them.

Where a step says "observe", the executor must actually run the application and look.

---

## File Structure

| File | Status | Responsibility |
| :--- | :--- | :--- |
| `src/gui/mainmenu.h` | Create | Menu table accessor, callback-keyed lookup, app-menu install, drift assertion |
| `src/gui/mainmenu.cpp` | Create | Portable + macOS menu tables; owns the 16 menu callback stubs' declarations |
| `src/gui/platform.h` | Create | Six-function portable platform interface |
| `src/gui/platform.cpp` | Create | macOS bodies behind `#ifdef __APPLE__`; no-op bodies elsewhere |
| `src/gui/Layouter.h` | Modify | Add `LFl_Sys_Menu_Bar` beside `LFl_Menu_Bar` (Task 3) |
| `src/gui/mainwindow.h` | Modify | Remove `menu_MainMenu`/`findMenuEntry`; add `confirmDiscard`, `openFromSystem`, and the menu-state cache members |
| `src/gui/mainwindow.cpp` | Modify | Menu table moves out; prompt sites consolidate; titles via `platform::` |
| `src/gui/main.cpp` | Modify | `--self-check`; `applyLookAndFeel()`; open-handler registration order |
| `meson.build` | Modify | Add two sources; delete the dead `help_src` variable |
| `scripts/create-macos-bundle.sh` | Modify | Icon keys and `NSRequiresAquaSystemAppearance` |
| `scripts/make-macos-icons.sh` | Create | Renders and packs `.icns` from a bundled example puzzle |
| `mac/BurrTools.icns` | Create | Generated app icon (committed) |
| `mac/BurrToolsDoc.icns` | Create | Generated document icon (committed) |

---

## Task 1: Extract the Menu Module

Pure refactoring. No user-visible change on any platform. This task exists to make Task 3 a small diff rather than a large one, and to fix a latent crash before it can be triggered.

**Files:**
- Create: `src/gui/mainmenu.h`, `src/gui/mainmenu.cpp`
- Modify: `src/gui/mainwindow.h:155` (remove `menu_MainMenu`), `:232` (remove `findMenuEntry`)
- Modify: `src/gui/mainwindow.cpp:2107-2129` (table), `:2348-2359` (`findMenuEntry`), `:2364-2379` (`updateInterface`), `:4012-4013` (construction)
- Modify: `src/gui/main.cpp` (add `--self-check`)
- Modify: `meson.build:136-190` (add source), `:197-201` (delete dead `help_src`)

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `const Fl_Menu_Item* mainmenu::table()`
  - `int mainmenu::findEntry(Fl_Callback* cb)`
  - `Fl_Menu_Item* mainmenu::mutableTable()` — used by `updateInterface()` to activate/deactivate
  - `void mainmenu::assertTablesConsistent()`
  - `void mainmenu::installApplicationMenu(mainWindow_c*)` — no-op body in this task; filled in Task 3
  - 16 menu callback stubs made non-static and declared in `mainmenu.h`

### Why the lookup must change

`mainWindow_c::findMenuEntry()` (`mainwindow.cpp:2348-2359`) matches on the label string and calls `bt_assert(found >= 0)` when nothing matches. `updateInterface()` passes the literals `"Images"` and `"STL"`. Task 3 renames those items to `"Image..."` and `"STL..."` in the macOS table, which would make this assertion **abort the application at startup on macOS**. Keying on the callback pointer is immune to label changes and keeps the duplicate-detection assertion meaningful.

- [ ] **Step 1: Make the 16 menu callback stubs externally visible**

In `src/gui/mainwindow.cpp`, remove the `static` keyword from exactly these 16 stub definitions (leave the other 72 stubs untouched — they are button callbacks used only within that file):

`cb_New_stub`, `cb_Load_stub`, `cb_Load_Ps3d_stub`, `cb_Save_stub`, `cb_SaveAs_stub`, `cb_Convert_stub`, `cb_AssembliesToShapes_stub`, `cb_Quit_stub`, `cb_Toggle3D_stub`, `cb_ImageExport_stub`, `cb_ImageExportVector_stub`, `cb_STLExport_stub`, `cb_StatusWindow_stub`, `cb_Comment_stub`, `cb_Config_stub`, `cb_About_stub`

Find them with:

```bash
grep -n "^static void cb_\(New\|Load\|Load_Ps3d\|Save\|SaveAs\|Convert\|AssembliesToShapes\|Quit\|Toggle3D\|ImageExport\|ImageExportVector\|STLExport\|StatusWindow\|Comment\|Config\|About\)_stub" src/gui/mainwindow.cpp
```

- [ ] **Step 2: Create `src/gui/mainmenu.h`**

```cpp
/* BurrTools
 *
 * BurrTools is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef __MAINMENU_H__
#define __MAINMENU_H__

#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Widget.H>

class mainWindow_c;

/* The main menu lives here rather than in mainwindow.cpp because there is
 * more than one of it: macOS gets a table restructured for the system menu
 * bar, every other platform keeps the historical in-window table. Both are
 * built from the same callbacks.
 */
namespace mainmenu {

  /* the table for the platform this build targets */
  const Fl_Menu_Item * table(void);

  /* the same table, writable, for activate()/deactivate() */
  Fl_Menu_Item * mutableTable(void);

  /* Index of the entry with the given callback.
   *
   * Deliberately keyed on the callback rather than the label: the macOS
   * table uses different wording ("Image..." where the portable table says
   * "Images"), so a label-keyed lookup would assert on one platform and
   * not the other.
   */
  int findEntry(Fl_Callback * cb);

  /* Assert that both tables expose the same set of callbacks.
   *
   * Adding an item to one table and forgetting the other is the failure
   * mode this design creates; this is the check that catches it. Labels and
   * shortcuts are intentionally not compared, as they legitimately differ.
   */
  void assertTablesConsistent(void);

  /* Populate the macOS application menu (About, Settings). No-op elsewhere. */
  void installApplicationMenu(mainWindow_c * win);
}

/* The menu callbacks. Defined in mainwindow.cpp next to the methods they
 * forward to; declared here because the tables above reference them.
 */
void cb_New_stub(Fl_Widget*, void*);
void cb_Load_stub(Fl_Widget*, void*);
void cb_Load_Ps3d_stub(Fl_Widget*, void*);
void cb_Save_stub(Fl_Widget*, void*);
void cb_SaveAs_stub(Fl_Widget*, void*);
void cb_Convert_stub(Fl_Widget*, void*);
void cb_AssembliesToShapes_stub(Fl_Widget*, void*);
void cb_Quit_stub(Fl_Widget*, void*);
void cb_Toggle3D_stub(Fl_Widget*, void*);
void cb_ImageExport_stub(Fl_Widget*, void*);
void cb_ImageExportVector_stub(Fl_Widget*, void*);
void cb_STLExport_stub(Fl_Widget*, void*);
void cb_StatusWindow_stub(Fl_Widget*, void*);
void cb_Comment_stub(Fl_Widget*, void*);
void cb_Config_stub(Fl_Widget*, void*);
void cb_About_stub(Fl_Widget*, void*);

#endif
```

- [ ] **Step 3: Create `src/gui/mainmenu.cpp`**

Use the same GPL header as above. The table is moved verbatim from `mainwindow.cpp:2107-2129` — do not retype it or "improve" the formatting, so the diff shows a pure move.

```cpp
#include "mainmenu.h"

#include "../lib/bt_assert.h"

#include <FL/Fl.H>

namespace {

  /* The historical in-window menu. Unchanged from the table that lived in
   * mainwindow.cpp, so Linux and Windows see exactly what they always have.
   */
  Fl_Menu_Item menu_Portable[] = {
    { "&File",           0, 0, 0, FL_SUBMENU, 0, 0, 0, 0 },
      {"New",            0, cb_New_stub,         0, 0, 0, 0, 14, 56},
      {"Load",    FL_F + 3, cb_Load_stub,        0, 0, 0, 0, 14, 56},
      {"Import",         0, cb_Load_Ps3d_stub,   0, 0, 0, 0, 14, 56},
      {"Save",    FL_F + 2, cb_Save_stub,        0, 0, 0, 0, 14, 56},
      {"Save As",        0, cb_SaveAs_stub,      0, FL_MENU_DIVIDER, 0, 0, 14, 56},
      {"Convert",        0, cb_Convert_stub,     0, 0, 0, 0, 14, 56},
      {"Import Assms",   0, cb_AssembliesToShapes_stub,     0, 0, 0, 0, 14, 56},
      {"Quit",           0, cb_Quit_stub,        0, 0, 3, 0, 14, 56},
      { },
    {"Toggle 3D", FL_F + 4, cb_Toggle3D_stub,    0, 0, 0, 0, 14, 56},
    { "&Export",         0, 0, 0, FL_SUBMENU, 0, 0, 0, 0 },
      {"Images",             0, cb_ImageExport_stub, 0, 0, 0, 0, 14, 56},
      {"Vector Image",       0, cb_ImageExportVector_stub, 0, 0, 0, 0, 14, 56},
      {"STL",             0, cb_STLExport_stub, 0, 0, 0, 0, 14, 56},
      { },
    {"Status",           0, cb_StatusWindow_stub,  0, 0, 0, 0, 14, 56},
    {"Edit Comment",     0, cb_Comment_stub,     0, 0, 0, 0, 14, 56},
    {"Settings",         0, cb_Config_stub,      0, 0, 0, 0, 14, 56},
    {"About",            0, cb_About_stub,       0, 0, 3, 0, 14, 56},
    { }
  };

  /* Task 3 adds menu_Mac[] here. */

  Fl_Menu_Item * activeTable(void) {
    return menu_Portable;
  }

  size_t activeTableSize(void) {
    return sizeof(menu_Portable) / sizeof(menu_Portable[0]);
  }
}

const Fl_Menu_Item * mainmenu::table(void) {
  return activeTable();
}

Fl_Menu_Item * mainmenu::mutableTable(void) {
  return activeTable();
}

int mainmenu::findEntry(Fl_Callback * cb) {

  bt_assert(cb);

  int found = -1;

  for (size_t i = 0; i < activeTableSize(); i++)
    if (activeTable()[i].callback() == cb) {
      bt_assert(found == -1);
      found = (int)i;
    }

  bt_assert(found >= 0);
  return found;
}

void mainmenu::assertTablesConsistent(void) {
  /* Task 3 fills this in once there is a second table to compare against. */
}

void mainmenu::installApplicationMenu(mainWindow_c * /*win*/) {
  /* Task 3 fills this in. */
}
```

- [ ] **Step 4: Update `mainwindow.h`**

Delete line 155 (`static Fl_Menu_Item menu_MainMenu[];`) and line 232 with its comment (`int findMenuEntry(const char * txt);`). Leave `Fl_Menu_Bar *MainMenu;` at line 154 alone.

- [ ] **Step 5: Update `mainwindow.cpp`**

Add `#include "mainmenu.h"` beside the existing includes. Delete the table at `2107-2129` and `findMenuEntry` at `2348-2359`. Then rewrite the head of `updateInterface()`:

```cpp
void mainWindow_c::updateInterface(void) {

  // update the menu items activate state

  // there must be at least one shape before there is something to export...
  if (puzzle->getNumberOfShapes() > 0)
    mainmenu::mutableTable()[mainmenu::findEntry(cb_ImageExport_stub)].activate();
  else
    mainmenu::mutableTable()[mainmenu::findEntry(cb_ImageExport_stub)].deactivate();

  if (ggt->getGridType()->getCapabilities() & gridType_c::CAP_STLEXPORT &&
      puzzle->getNumberOfShapes() > 0)
    mainmenu::mutableTable()[mainmenu::findEntry(cb_STLExport_stub)].activate();
  else
    mainmenu::mutableTable()[mainmenu::findEntry(cb_STLExport_stub)].deactivate();

  MainMenu->copy(mainmenu::table(), this);
  MainMenu->update();
```

**`MainMenu->update()` is not cosmetic and must not be dropped.** `Fl_Menu_::copy()` and `Fl_Menu_::menu()` are *non-virtual* (`subprojects/fltk/FL/Fl_Menu_.H:135-136`), so once `MainMenu` holds an `Fl_Sys_Menu_Bar` in Task 3, `copy()` through the `Fl_Menu_Bar*` updates the item array but never rebuilds the actual system menu. `Fl_Menu_Bar::update()` *is* virtual and empty (`subprojects/fltk/FL/Fl_Menu_Bar.H:93`), while `Fl_Sys_Menu_Bar::update()` rebuilds the system menu. Calling it here costs nothing today and makes Task 3 work without a single `#ifdef`.

Apply the same two-line change at `mainwindow.cpp:4013`.

- [ ] **Step 6: Add `--self-check` to `src/gui/main.cpp`**

Insert at the top of `main()`, before `bt_assert_init()` is followed by any window construction:

```cpp
  // A headless invariant check, so CI can catch menu-table drift without
  // linking FLTK into test_burrtools. Must run before any window exists.
  if (argc == 2 && strcmp(argv[1], "--self-check") == 0) {
    bt_assert_init();
    mainmenu::assertTablesConsistent();
    printf("self-check OK\n");
    return 0;
  }
```

Add `#include <string.h>` and `#include "mainmenu.h"` to that file.

- [ ] **Step 7: Update `meson.build`**

Add `'src/gui/mainmenu.cpp',` to `gui_src` in alphabetical position (after `'src/gui/main.cpp',`).

Separately, delete the `help_src` block at lines 197-201. It declares `src/help/Fl_Help_Dialog.cpp`, `src/help/Fl_Help_View.cpp` and `src/help/helpdata.cpp`, but **`src/help/` does not exist and the variable is referenced nowhere** — verified with `grep -n help_src meson.build`, which returns only the definition. It is dead weight that misleads anyone looking for the in-app help the README claims exists.

- [ ] **Step 8: Add a `just` recipe for the self-check**

Append to `justfile`:

```just
# Headless GUI invariant check (menu table consistency)
check-gui: build
    ./build/burrtools --self-check
```

- [ ] **Step 9: Build and run every gate**

```bash
just build && just test-all && just check && just build-werror && just check-gui
```

Expected: all pass; `check-gui` prints `self-check OK`.

- [ ] **Step 10: Observe that nothing changed**

Launch `./build/burrtools`. Confirm by eye: the in-window menu bar is present with File / Toggle 3D / Export / Status / Edit Comment / Settings / About in that order; F2 saves; F3 loads; the Export submenu greys out with no shapes loaded and becomes active after adding one. This is a refactoring task — anything visibly different is a bug.

- [ ] **Step 11: Commit**

```bash
git add src/gui/mainmenu.h src/gui/mainmenu.cpp src/gui/mainwindow.h src/gui/mainwindow.cpp src/gui/main.cpp meson.build justfile
git commit -m "refactor(gui): extract the main menu into its own module

Moves menu_MainMenu and findMenuEntry out of the 134 KB mainwindow.cpp
into src/gui/mainmenu.*, ahead of adding a second, macOS-specific table.

findEntry() now keys on the callback pointer rather than the label.
findMenuEntry() matched labels and bt_assert()ed on a miss, so the
upcoming macOS table -- which renames Images to Image... -- would have
aborted the application at startup.

updateInterface() now calls the virtual Fl_Menu_Bar::update() after
copy(). copy() is non-virtual, so without this an Fl_Sys_Menu_Bar would
silently fail to rebuild. It is a no-op on every current platform.

Adds a --self-check mode and a check-gui recipe so menu-table invariants
can be gated in CI without linking FLTK into test_burrtools.

Also deletes the dead help_src variable, which listed three files under
a src/help directory that does not exist and was referenced nowhere.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

## Task 2: Platform Seam and Widget Scheme

**Files:**
- Create: `src/gui/platform.h`, `src/gui/platform.cpp`
- Modify: `src/gui/main.cpp:66-71` (replace boxtype overrides)
- Modify: `meson.build` (add source)

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces: the full `platform::` namespace. Tasks 3, 6 and 7 fill in behaviour behind these signatures; this task implements `applyLookAndFeel()` and `openHelp()` and leaves the rest as working no-ops.

- [ ] **Step 1: Create `src/gui/platform.h`**

Use the project GPL header, then:

```cpp
#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include <string>

class Fl_Window;

/* Everything BurrTools does differently on one operating system than
 * another. The point of this seam is that no caller branches: each function
 * has a meaningful body on macOS and a no-op or portable fallback
 * everywhere else, so mainwindow.cpp and main.cpp contain no platform
 * conditionals at all.
 */
namespace platform {

  /* Widget scheme, fonts and metrics. Call once, before any window is
   * constructed.
   */
  void applyLookAndFeel(void);

  /* True when the menu bar is drawn by the operating system rather than
   * inside the application window.
   */
  bool usesSystemMenuBar(void);

  /* Register a handler for documents the OS asks us to open -- a Finder
   * double-click, a drop on the Dock icon, "Open With". The handler may be
   * called before the first window is shown, so it must be installed after
   * the main window is constructed.
   */
  void installOpenHandler(void (*handler)(const char * filename));

  /* The window title for a document. macOS wants the bare file name;
   * everywhere else keeps the historical "BurrTools - <name>".
   * A null or empty file name yields the untitled form.
   */
  std::string windowTitle(const char * filename, bool edited);

  /* Reflect unsaved changes in the window chrome (the dot in the macOS
   * close button). No-op where the platform has no such affordance.
   */
  void setDocumentEdited(Fl_Window * win, bool edited);

  /* Open the user guide in the user's browser. */
  void openHelp(void);
}

#endif
```

- [ ] **Step 2: Create `src/gui/platform.cpp` with the scheme and help bodies**

```cpp
#include "platform.h"

#include <FL/Fl.H>
#include <FL/filename.H>
#include <FL/Fl_Window.H>

#include <string.h>

namespace {
  const char * const USER_GUIDE_URL =
    "https://burrtools.sourceforge.net/gui-doc/toc.html";

#ifdef __APPLE__
  /* basename without the directory part; returns the whole string when
   * there is no separator.
   *
   * Guarded because it is only reachable on macOS -- left unguarded it is
   * an unused static function everywhere else, which just build-werror
   * rejects.
   */
  const char * baseName(const char * path) {
    const char * slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
  }
#endif
}

void platform::applyLookAndFeel(void) {

  /* A scheme replaces the default Motif-derived boxes. This is deliberately
   * applied on every platform: the default scheme looks equally dated on
   * Linux and Windows.
   *
   * Note that this must not be combined with the FL_THIN_* boxtype
   * overrides main() used to install -- schemes install their own box
   * drawing functions and the two fight, leaving the scheme half applied.
   */
  Fl::scheme("gleam");

#ifdef __APPLE__
  /* Render labels in the system UI font (SF Pro) rather than Helvetica, and
   * match the 13pt macOS control text size rather than FLTK's 14.
   */
  Fl::set_font(FL_HELVETICA, ".AppleSystemUIFont");
  FL_NORMAL_SIZE = 13;
#endif

  Fl::get_system_colors();
}

bool platform::usesSystemMenuBar(void) {
#ifdef __APPLE__
  return true;
#else
  return false;
#endif
}

void platform::installOpenHandler(void (* /*handler*/)(const char *)) {
  /* Task 6 */
}

std::string platform::windowTitle(const char * filename, bool /*edited*/) {

  const bool untitled = !filename || !filename[0];

#ifdef __APPLE__
  /* macOS titles a document window with the document's name and nothing
   * else; the application name belongs in the menu bar.
   */
  return untitled ? std::string("Untitled") : std::string(baseName(filename));
#else
  return untitled ? std::string("BurrTools - Untitled")
                  : std::string("BurrTools - ") + filename;
#endif
}

void platform::setDocumentEdited(Fl_Window * /*win*/, bool /*edited*/) {
  /* Task 7 */
}

void platform::openHelp(void) {
  char msg[512];
  fl_open_uri(USER_GUIDE_URL, msg, sizeof(msg));
}
```

Note `FL_NORMAL_SIZE` is a writable global (`Fl_Widget.H`), so assigning it before any widget is constructed sets the default label size.

- [ ] **Step 3: Update `src/gui/main.cpp`**

Replace lines 66-71 — the four `Fl::set_boxtype` calls and `Fl::get_system_colors()` — with a single call. `get_system_colors()` now lives inside `applyLookAndFeel()`; do not call it twice.

```cpp
  platform::applyLookAndFeel();
```

Add `#include "platform.h"`.

- [ ] **Step 4: Add the source to `meson.build`**

Add `'src/gui/platform.cpp',` to `gui_src` in alphabetical position.

- [ ] **Step 5: Build and run every gate**

```bash
just build && just test-all && just check && just build-werror && just check-gui
```

- [ ] **Step 6: Compare `gleam` against `oxy` and decide**

Launch the app, then edit `Fl::scheme("gleam")` to `Fl::scheme("oxy")`, rebuild, and launch again. Compare on the Entities, Puzzle and Solver tabs. Keep whichever looks better; keep `gleam` if they are a wash, as it is the better-tested of the two. **Record the choice and one sentence of reasoning in the commit message** — a later reader should not have to re-litigate it.

- [ ] **Step 7: Confirm the font actually resolved (macOS)**

`.AppleSystemUIFont` is a private system alias, and a failure to resolve it is a silent fallback rather than an error. Compare a label against a known Helvetica rendering — SF Pro's digits and its lowercase `a` and `g` are visibly different. If it did not take, substitute `"Helvetica Neue"` and note it.

- [ ] **Step 8: Walk every tab and dialog for scheme damage**

The spec's Section 4.2 names the widgets that draw themselves with raw `fl_draw` calls and therefore do not follow a scheme: `BlockList.cpp`, `Fl_Table.cpp`, `grideditor.cpp`, `grideditor_0..4.cpp`, `separator.cpp`, `statusline.cpp`.

Open each of the three tabs, plus Settings, Status, Edit Comment, Convert, and both Export dialogs. Look for backgrounds that no longer match their surroundings and borders that no longer align.

**Scope boundary — this is a decision point, not a checkbox.** Touching up a handful of these is in scope for this task. If most of them need rework, stop and report rather than absorbing an open-ended visual-rework project. Record what you found either way.

- [ ] **Step 9: Verify Linux is not broken**

If a Linux machine or container is available, build and launch there and confirm the scheme applied cleanly. If one is not available, say so explicitly in the commit message rather than implying it was checked.

- [ ] **Step 10: Commit**

```bash
git add src/gui/platform.h src/gui/platform.cpp src/gui/main.cpp meson.build
git commit -m "feat(gui): add the platform seam and set a widget scheme

Adds src/gui/platform.*, the single place where this codebase branches on
operating system. Every function has a portable no-op or fallback body, so
callers never carry an ifdef.

Sets an FLTK scheme on all platforms, replacing the default Motif-derived
boxes. The four FL_THIN_* boxtype overrides in main() are removed: schemes
install their own box drawing, and the two fought each other.

<record the gleam/oxy choice and the reason here>

On macOS, labels now render in the system UI font at the 13pt macOS
control size.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

## Task 3: The macOS System Menu Bar

The largest task, and the one holding the plan's one blocking unknown. **Step 8 must be performed before this task is considered done**, because a bad answer there changes the design.

**Files:**
- Modify: `src/gui/mainmenu.cpp` (add the macOS table, the assertion, the app menu)
- Modify: `src/gui/mainwindow.cpp:4012` (construct the right menu bar class), `updateInterface()` (rebuild only on change)
- Modify: `src/gui/mainwindow.h` (add the `exportActive`/`stlActive` cache members)

**Interfaces:**
- Consumes: `mainmenu::table()`, `mainmenu::findEntry()`, `platform::usesSystemMenuBar()`, `platform::openHelp()`.
- Produces: `void cb_Help_stub(Fl_Widget*, void*)` — declared in `mainmenu.h`, defined in `mainmenu.cpp` (it forwards to `platform::openHelp()` and needs no window).

- [ ] **Step 1: Add the help callback to `mainmenu.h`**

Append to the stub declarations:

```cpp
void cb_Help_stub(Fl_Widget*, void*);
```

- [ ] **Step 2: Add the macOS table to `mainmenu.cpp`**

Inside the anonymous namespace, after `menu_Portable[]`. Note that About, Settings and Quit are absent — macOS puts them in the application menu, which Step 4 builds.

```cpp
#ifdef __APPLE__

  /* The macOS table. Restructured for the system menu bar: Export becomes a
   * File submenu rather than a top-level menu, the document-level actions
   * gather under Puzzle, and every item that opens a dialog gains an
   * ellipsis.
   *
   * There is no Edit menu. The only candidate for one is Edit Comment --
   * BurrTools has no Undo, Cut, Copy or Paste -- and a one-item Edit menu
   * reads worse than none.
   *
   * About, Settings and Quit are deliberately absent: they belong to the
   * application menu, built in installApplicationMenu() below.
   */
  Fl_Menu_Item menu_Mac[] = {
    { "&File",             0, 0, 0, FL_SUBMENU, 0, 0, 0, 0 },
      {"New",              FL_COMMAND + 'n', cb_New_stub,       0, 0, 0, 0, 14, 56},
      {"Open...",          FL_COMMAND + 'o', cb_Load_stub,      0, 0, 0, 0, 14, 56},
      {"Import...",        0,                cb_Load_Ps3d_stub, 0, FL_MENU_DIVIDER, 0, 0, 14, 56},
      {"Save",             FL_COMMAND + 's', cb_Save_stub,      0, 0, 0, 0, 14, 56},
      {"Save As...",       FL_COMMAND + FL_SHIFT + 's', cb_SaveAs_stub, 0, FL_MENU_DIVIDER, 0, 0, 14, 56},
      { "Export",          0, 0, 0, FL_SUBMENU | FL_MENU_DIVIDER, 0, 0, 0, 0 },
        {"Image...",        0, cb_ImageExport_stub,       0, 0, 0, 0, 14, 56},
        {"Vector Image...", 0, cb_ImageExportVector_stub, 0, 0, 0, 0, 14, 56},
        {"STL...",          0, cb_STLExport_stub,         0, 0, 0, 0, 14, 56},
        { },
      {"Close",            FL_COMMAND + 'w', cb_Quit_stub,      0, 0, 0, 0, 14, 56},
      { },
    { "&Puzzle",           0, 0, 0, FL_SUBMENU, 0, 0, 0, 0 },
      {"Edit Comment...",      0, cb_Comment_stub,            0, 0, 0, 0, 14, 56},
      {"Convert...",           0, cb_Convert_stub,            0, 0, 0, 0, 14, 56},
      {"Import Assemblies...", 0, cb_AssembliesToShapes_stub, 0, FL_MENU_DIVIDER, 0, 0, 14, 56},
      {"Status",           FL_COMMAND + 'i', cb_StatusWindow_stub, 0, 0, 0, 0, 14, 56},
      { },
    { "&View",             0, 0, 0, FL_SUBMENU, 0, 0, 0, 0 },
      {"Toggle 3D",        FL_COMMAND + '3', cb_Toggle3D_stub, 0, 0, 0, 0, 14, 56},
      { },
    { "&Help",             0, 0, 0, FL_SUBMENU, 0, 0, 0, 0 },
      {"BurrTools User Guide", FL_COMMAND + '?', cb_Help_stub, 0, 0, 0, 0, 14, 56},
      { },
    { }
  };

#endif
```

Then make `activeTable()` and `activeTableSize()` select it:

```cpp
  Fl_Menu_Item * activeTable(void) {
#ifdef __APPLE__
    return menu_Mac;
#else
    return menu_Portable;
#endif
  }

  size_t activeTableSize(void) {
#ifdef __APPLE__
    return sizeof(menu_Mac) / sizeof(menu_Mac[0]);
#else
    return sizeof(menu_Portable) / sizeof(menu_Portable[0]);
#endif
  }
```

- [ ] **Step 3: Define `cb_Help_stub` in `mainmenu.cpp`**

```cpp
void cb_Help_stub(Fl_Widget*, void*) {
  platform::openHelp();
}
```

Add `#include "platform.h"`.

- [ ] **Step 4: Implement `installApplicationMenu`**

```cpp
void mainmenu::installApplicationMenu(mainWindow_c * win) {
#ifdef __APPLE__

  /* The About item is a property of the application menu, not of our
   * table, so FLTK wants it separately.
   */
  Fl_Sys_Menu_Bar::about(cb_About_stub, win);

  /* Settings belongs in the application menu on macOS. The array must
   * outlive the call, hence the static; user_data cannot be set in the
   * initialiser because the window does not exist until runtime.
   */
  static Fl_Menu_Item appItems[] = {
    { "Settings...", FL_COMMAND + ',', cb_Config_stub, 0, 0, 0, 0, 14, 56 },
    { }
  };
  appItems[0].user_data(win);

  Fl_Mac_App_Menu::custom_application_menu_items(appItems);

  /* Gives us Minimize, Zoom and the window list for free. */
  Fl_Sys_Menu_Bar::window_menu_style(Fl_Sys_Menu_Bar::tabbing_mode_none);

#else
  (void)win;
#endif
}
```

Add `#include <FL/Fl_Sys_Menu_Bar.H>` and, for `Fl_Mac_App_Menu`, `#include <FL/platform.H>`.

`tabbing_mode_none` is chosen over the `tabbing_mode_automatic` default because BurrTools is single-window; offering to tab windows that cannot exist would be noise.

- [ ] **Step 5: Implement `assertTablesConsistent`**

```cpp
void mainmenu::assertTablesConsistent(void) {

  /* Compare the two tables by the set of callbacks they expose. Labels and
   * shortcuts differ by design; a missing or extra callback does not.
   *
   * The macOS table intentionally omits About, Settings and Quit, which
   * live in the application menu, so those are excluded from the
   * comparison.
   */
  static Fl_Callback * const appMenuOnly[] = {
    cb_About_stub, cb_Config_stub
  };

  const size_t portableSize = sizeof(menu_Portable) / sizeof(menu_Portable[0]);

  for (size_t i = 0; i < portableSize; i++) {

    Fl_Callback * cb = menu_Portable[i].callback();
    if (!cb) continue;

    bool skip = false;
    for (size_t k = 0; k < sizeof(appMenuOnly)/sizeof(appMenuOnly[0]); k++)
      if (cb == appMenuOnly[k]) skip = true;
    if (skip) continue;

    bool found = false;
    for (size_t j = 0; j < activeTableSize(); j++)
      if (activeTable()[j].callback() == cb) found = true;

    /* A menu item exists on one platform but not the other. Add it to the
     * table that is missing it, or add it to appMenuOnly if it genuinely
     * belongs only in the macOS application menu.
     */
    bt_assert(found);
  }
}
```

On non-Apple builds `activeTable()` *is* `menu_Portable`, so this degenerates to a self-comparison that always passes — harmless, and it keeps the function free of a second `#ifdef`.

- [ ] **Step 6: Construct the right menu bar class in `mainwindow.cpp`**

`LFl_Menu_Bar` derives from `Fl_Menu_Bar` (`Layouter.h:510`). The macOS build needs an `Fl_Sys_Menu_Bar`, which is *not* in that hierarchy, so add a parallel layoutable class in `Layouter.h` next to `LFl_Menu_Bar`:

```cpp
#ifdef __APPLE__
#include <FL/Fl_Sys_Menu_Bar.H>
class LFl_Sys_Menu_Bar : public Fl_Sys_Menu_Bar, public layoutable_c {
  public:
    LFl_Sys_Menu_Bar(int x, int y, int w, int h)
      : Fl_Sys_Menu_Bar(0, 0, 100, 100), layoutable_c(x, y, w, h) { }
    void draw(void) FL_OVERRIDE { Fl_Sys_Menu_Bar::draw(); }
};
#endif
```

Copy the body of `LFl_Menu_Bar` (`Layouter.h:510-520`) exactly, substituting the base class — read it before writing, as it may define layout methods beyond the constructor.

Then at `mainwindow.cpp:4012`:

```cpp
#ifdef __APPLE__
  MainMenu = new LFl_Sys_Menu_Bar(0, 0, 1, 1);
#else
  MainMenu = new LFl_Menu_Bar(0, 0, 1, 1);
#endif
```

**This is the one place the no-new-ifdef constraint is knowingly broken**, because the type of a widget cannot be hidden behind a function returning `Fl_Menu_Bar*` without also moving its construction out of the `Fl_Group` `begin()`/`end()` scope that owns it. Two lines, at the single point of construction, is the smaller evil. Note it in the commit message.

Because `Fl_Sys_Menu_Bar` draws nothing in the window, the row it occupies in the layout collapses to zero height on macOS, which is what we want.

- [ ] **Step 7: Rebuild the menu only when its state changes**

`updateInterface()` runs on many interactions, and `MainMenu->update()` rebuilds the entire system menu bar on macOS — not free, and not guaranteed flicker-free. Add two members to `mainwindow.h` beside `bool changed;`:

```cpp
  /* last published menu activation state, so the system menu bar is only
   * rebuilt when it actually changes */
  bool menuExportActive;
  bool menuSTLActive;
```

Initialise both to `false` in the constructor's initialiser list, then replace the head of `updateInterface()` from Task 1 with:

```cpp
  const bool exportActive = puzzle->getNumberOfShapes() > 0;
  const bool stlActive    = (ggt->getGridType()->getCapabilities() & gridType_c::CAP_STLEXPORT)
                            && puzzle->getNumberOfShapes() > 0;

  if (exportActive != menuExportActive || stlActive != menuSTLActive) {

    if (exportActive)
      mainmenu::mutableTable()[mainmenu::findEntry(cb_ImageExport_stub)].activate();
    else
      mainmenu::mutableTable()[mainmenu::findEntry(cb_ImageExport_stub)].deactivate();

    if (stlActive)
      mainmenu::mutableTable()[mainmenu::findEntry(cb_STLExport_stub)].activate();
    else
      mainmenu::mutableTable()[mainmenu::findEntry(cb_STLExport_stub)].deactivate();

    MainMenu->copy(mainmenu::table(), this);
    MainMenu->update();

    menuExportActive = exportActive;
    menuSTLActive    = stlActive;
  }
```

The unconditional `copy()` at `mainwindow.cpp:4013` (construction) stays as it is — the menu must be published once at startup regardless.

- [ ] **Step 8: BLOCKING — determine whether ⌘Q runs the unsaved-changes prompt**

Build and launch. Load `examples/Bermuda.xmpuzzle`, modify a shape so `changed` becomes true, then press ⌘Q.

- **Expected:** the unsaved-changes prompt appears (still the two-button version until Task 5).
- **If the application quits without prompting**, `mainWindow_c::hide()` at `mainwindow.cpp:1769` is being bypassed and **unsaved work is being destroyed**. Do not proceed past this step. Apply the fallback: add a Quit entry to the `appItems` array in Step 4 wired to `cb_Quit_stub`, consulting `Fl_Mac_App_Menu::quit` in `subprojects/fltk/FL/mac.H` for how FLTK's own item is declared, and re-test until the prompt appears.

Record the outcome either way — this is the finding the spec flagged as capable of changing the design.

- [ ] **Step 9: Verify the rest of the menu**

Observe each:

- Menu bar appears at the top of the screen; no menu strip inside the window.
- Application menu shows About BurrTools, Settings… ⌘,, and Quit BurrTools ⌘Q.
- Window menu offers Minimize ⌘M and Zoom.
- ⌘N, ⌘O, ⌘S, ⇧⌘S, ⌘W, ⌘I, ⌘3 each fire the right action.
- ⌘? opens the user guide in the default browser.
- With no shapes loaded, File ▸ Export is greyed; after adding a shape it becomes active.
- Repeatedly adding and removing the last shape causes no visible flicker or lag in the menu bar.

- [ ] **Step 10: Verify Linux is untouched**

`just build` and launch. The in-window menu bar, its wording and the F-keys must be exactly as they were before Task 1.

- [ ] **Step 11: Run every gate**

```bash
just build && just test-all && just check && just build-werror && just check-gui
```

- [ ] **Step 12: Commit**

```bash
git add src/gui/mainmenu.cpp src/gui/mainmenu.h src/gui/mainwindow.cpp src/gui/mainwindow.h src/gui/Layouter.h
git commit -m "feat(gui): put the menu in the macOS system menu bar

Adds a macOS menu table built for the system menu bar: Export becomes a
File submenu, the document-level actions gather under Puzzle, dialogs gain
ellipses, and F-key bindings become Command shortcuts. About, Settings and
Quit move to the application menu, and FLTK supplies the Window menu.

There is no Edit menu: the only candidate item is Edit Comment, and
BurrTools has no Undo, Cut, Copy or Paste.

Linux and Windows keep the historical table unchanged.
assertTablesConsistent() now compares the two by callback set, so an item
added to one and forgotten in the other trips --self-check.

updateInterface() rebuilds the menu only when the activation state
actually changes; on macOS each rebuild reconstructs the real system menu
bar rather than copying an array.

The widget's construction carries the one deliberate ifdef outside
platform.cpp. Fl_Sys_Menu_Bar does derive from Fl_Menu_Bar, which is why
MainMenu can stay an Fl_Menu_Bar*; what cannot be shared is the layoutable
wrapper, since LFl_Sys_Menu_Bar deriving from LFl_Menu_Bar would put a
diamond on Fl_Menu_Bar.

<record the outcome of the Command-Q verification here>

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

## Task 4: F-Key Secondary Bindings on macOS

**Files:**
- Modify: `src/gui/mainwindow.cpp:3260-3285` (the existing `FL_SHORTCUT` key switch in `handle()`)

**Interfaces:**
- Consumes: the menu stubs declared in `mainmenu.h`.
- Produces: nothing.

`mainWindow_c::handle()` already has an `FL_SHORTCUT` branch switching on `Fl::event_key()` with cases for `FL_F + 5` through `FL_F + 8`. F2, F3 and F4 currently come from the portable menu table's shortcuts, which the macOS table replaces with ⌘ bindings — so on macOS they must be handled here instead.

- [ ] **Step 1: Add the three cases**

Inside the existing `switch(Fl::event_key())`, before the closing brace:

```cpp
#ifdef __APPLE__
      /* On macOS the menu carries the Command shortcuts, so the historical
       * F-keys are kept alive here as secondary bindings. They are
       * deliberately not shown in the menu, which displays the Command
       * binding.
       */
      case FL_F + 2:
        cb_Save_stub(this, this);
        return 1;
      case FL_F + 3:
        cb_Load_stub(this, this);
        return 1;
      case FL_F + 4:
        cb_Toggle3D_stub(this, this);
        return 1;
#endif
```

This is a second knowing exception to the no-ifdef-in-mainwindow rule, for the same reason as Task 3 Step 6: the bindings must not fire on platforms where the menu already owns those keys, and routing it through `platform::usesSystemMenuBar()` would compile dead code rather than exclude it. Prefer the runtime form if `-Werror` is content with it:

```cpp
      case FL_F + 2:
        if (!platform::usesSystemMenuBar()) break;
        cb_Save_stub(this, this);
        return 1;
```

Use the runtime form if it builds cleanly on both platforms; fall back to the `#ifdef` if not. Record which you used.

- [ ] **Step 2: Build and verify on macOS**

F2 saves, F3 opens the load dialog, F4 toggles the 3D view — and ⌘S, ⌘O and ⌘3 all still work.

- [ ] **Step 3: Verify on Linux**

F2, F3 and F4 still work via the menu table. F5–F8 still select edit modes on both platforms.

- [ ] **Step 4: Run every gate and commit**

```bash
just build && just test-all && just check && just build-werror && just check-gui
git add src/gui/mainwindow.cpp
git commit -m "feat(gui): keep F2/F3/F4 working on macOS as secondary bindings

The macOS menu table replaces the historical F-key shortcuts with Command
bindings. An Fl_Menu_Item carries only one shortcut, so the F-keys are
handled in handle() alongside the existing F5-F8 cases to preserve muscle
memory. They are intentionally not shown in the menu.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

## Task 5: Save / Don't Save / Cancel

Improves all platforms, not only macOS. Today four sites offer Cancel or lose-your-work with no way to save.

**Files:**
- Modify: `src/gui/mainwindow.h` (declare `confirmDiscard`)
- Modify: `src/gui/mainwindow.cpp:1492-1494` (New), `:1522-1524` (Load), `:1537-1539` (Import), `:1769-1772` (hide/Quit)

**Interfaces:**
- Consumes: nothing.
- Produces: `bool mainWindow_c::confirmDiscard(const char * action)` — Task 6 calls it from the Finder-open path.

### Why no signature changes are needed

`cb_Save()` (`mainwindow.cpp:1570`) already delegates to `cb_SaveAs()` when `fname` is empty, and both set `changed = false` only on a successful write. So `changed` is itself the success signal: call `cb_Save()`, and if `changed` is still true the save did not happen — cancelled, failed, or refused because the solver thread is running. No return types change.

- [ ] **Step 1: Declare the helper in `mainwindow.h`**

In the private section beside `bool tryToLoad(const char *fname);`:

```cpp
  /* Ask about unsaved changes before an operation that would discard them.
   * Returns false if the caller should abort. 'action' is the verb shown to
   * the user, e.g. "create a new puzzle".
   */
  bool confirmDiscard(const char * action);
```

- [ ] **Step 2: Implement it in `mainwindow.cpp`**

Place it immediately before `cb_New_stub` at line 1487:

```cpp
bool mainWindow_c::confirmDiscard(const char * action) {

  if (!changed)
    return true;

  char msg[256];
  snprintf(msg, sizeof(msg),
           "The puzzle has unsaved changes.\nSave before you %s?", action);

  /* fl_choice lays its buttons out right to left from the first argument,
   * so this puts Save rightmost as the default, Cancel beside it and
   * Don't Save furthest left -- the macOS convention.
   */
  switch (fl_choice("%s", "Don't Save", "Cancel", "Save", msg)) {

    case 0:             // Don't Save
      return true;

    case 2:             // Save
      cb_Save();
      /* cb_Save clears 'changed' only on a successful write, so it still
       * being set means the user cancelled the Save As dialog, the write
       * failed, or the solver thread is running. Abort rather than
       * discarding the work.
       */
      return !changed;

    default:            // Cancel
      return false;
  }
}
```

**Verify the button order by looking at the dialog** (Step 5), not by trusting this comment. If Save is not rightmost, swap the argument order and update the comment.

- [ ] **Step 3: Replace the four existing prompt sites**

`cb_New` (was lines 1492-1494):

```cpp
    if (!confirmDiscard("create a new puzzle"))
      return;
```

`cb_Load` (was 1522-1524):

```cpp
    if (!confirmDiscard("open another puzzle"))
      return;
```

`cb_Load_Ps3d` (was 1537-1539):

```cpp
    if (!confirmDiscard("import another puzzle"))
      return;
```

`hide()` (was 1769-1772):

```cpp
void mainWindow_c::hide(void) {
  if (confirmDiscard("quit"))
    Fl_Double_Window::hide();
}
```

- [ ] **Step 4: Run every gate**

```bash
just build && just test-all && just check && just build-werror && just check-gui
```

- [ ] **Step 5: Verify all three answers at all four sites**

For each of New, Open, Import and Quit, with a modified puzzle:

- **Don't Save** — the operation proceeds and the changes are gone.
- **Cancel** — nothing happens; the puzzle is still modified.
- **Save on a puzzle with a filename** — writes, then proceeds.
- **Save on an untitled puzzle** — the Save As dialog appears; completing it writes and proceeds.
- **Save on an untitled puzzle, then cancel the Save As dialog** — the whole operation aborts and the puzzle is still open and still modified. *This is the case most likely to be wrong; test it deliberately.*

Also confirm Save is the rightmost button and Don't Save the leftmost.

- [ ] **Step 6: Commit**

```bash
git add src/gui/mainwindow.h src/gui/mainwindow.cpp
git commit -m "feat(gui): offer Save when discarding unsaved changes

The four confirmation sites offered only Cancel or lose-your-work. They
now share one helper offering Save, Don't Save and Cancel, with Save as
the default.

Choosing Save routes through the existing save path, including falling
through to Save As for an untitled puzzle. If that save does not complete
-- cancelled, failed, or refused while the solver runs -- the whole
operation aborts rather than discarding the work. cb_Save() already clears
'changed' only on a successful write, so no signatures change.

This improves every platform, not just macOS.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

## Task 6: Finder Document Opens

**Files:**
- Modify: `src/gui/platform.cpp` (`installOpenHandler`)
- Modify: `src/gui/mainwindow.h`, `src/gui/mainwindow.cpp` (add `openFromSystem`)
- Modify: `src/gui/main.cpp` (registration, in the right order)

**Interfaces:**
- Consumes: `mainWindow_c::confirmDiscard()` from Task 5; `mainWindow_c::tryToLoad()` (`mainwindow.cpp:1997`).
- Produces: `void mainWindow_c::openFromSystem(const char * filename)` — public.

- [ ] **Step 1: Implement `installOpenHandler` in `platform.cpp`**

```cpp
void platform::installOpenHandler(void (*handler)(const char *)) {
#ifdef __APPLE__
  fl_open_callback(handler);
#else
  (void)handler;
#endif
}
```

Add `#include <FL/platform.H>`. `fl_open_callback` is declared unconditionally there (`subprojects/fltk/FL/platform.H:79`), but the guard keeps the seam honest: only macOS delivers these events.

- [ ] **Step 2: Add `openFromSystem` to `mainwindow.h`**

In the public section, beside `void show(int argn, char ** argv);`:

```cpp
  /* Open a document the operating system handed us -- a Finder
   * double-click, a drop on the Dock icon. Guards unsaved changes, which
   * tryToLoad() does not.
   */
  void openFromSystem(const char * filename);
```

- [ ] **Step 3: Implement it in `mainwindow.cpp`**

Next to `tryToLoad`:

```cpp
void mainWindow_c::openFromSystem(const char * filename) {

  if (!filename || !filename[0])
    return;

  if (!threadStopped())
    return;

  if (!confirmDiscard("open that puzzle"))
    return;

  if (!tryToLoad(filename))
    fl_message("Could not open %s", filename);
}
```

**`confirmDiscard` is the point of this function.** `tryToLoad()` loads unconditionally; wiring `fl_open_callback` straight to it would silently destroy unsaved work.

- [ ] **Step 4: Register the handler in `main.cpp`, in the right order**

`fl_open_callback` takes a plain function pointer with no user data, so the window is reached through a file-scope pointer:

```cpp
static mainWindow_c * g_ui = 0;

static void handleSystemOpen(const char * filename) {
  if (g_ui)
    g_ui->openFromSystem(filename);
}
```

Then in `main()`, between constructing the window and showing it:

```cpp
  mainWindow_c *ui = new mainWindow_c(new gridType_c());

  /* Must come after the window exists and before it is shown: launching by
   * double-clicking a puzzle delivers the open event almost immediately.
   */
  g_ui = ui;
  platform::installOpenHandler(handleSystemOpen);
```

Clear `g_ui = 0;` immediately before `delete ui;` at the end of `main()`, so a late event cannot reach a destroyed window.

- [ ] **Step 5: Run every gate**

```bash
just build && just test-all && just check && just build-werror && just check-gui
```

- [ ] **Step 6: Build a bundle and verify all four paths**

```bash
just build && ./scripts/create-macos-bundle.sh
open BurrTools.app
```

- Double-click an `.xmpuzzle` in Finder **while BurrTools is already running** — it loads.
- Double-click one **with BurrTools not running** — the app launches and the puzzle is loaded when the window appears. *This is the path the registration ordering exists for.*
- Drop an `.xmpuzzle` onto the Dock icon — it loads.
- Double-click one **while the open puzzle has unsaved changes** — the Save/Don't Save/Cancel prompt appears first, and Cancel leaves the original puzzle untouched.

If the file type is not yet associated, right-click ▸ Open With ▸ BurrTools once, or run `touch BurrTools.app` to prompt Launch Services to re-register.

- [ ] **Step 7: Commit**

```bash
git add src/gui/platform.cpp src/gui/mainwindow.h src/gui/mainwindow.cpp src/gui/main.cpp
git commit -m "feat(gui): open puzzles from Finder and the Dock

Registers FLTK's open callback on macOS so a double-clicked .xmpuzzle, a
drop on the Dock icon, or Open With loads the puzzle.

The handler goes through confirmDiscard() rather than calling tryToLoad()
directly: tryToLoad loads unconditionally, so wiring the system event
straight to it would silently destroy unsaved work.

Registration sits between constructing the main window and showing it --
launching by double-clicking a document delivers the event almost
immediately, so any later would miss it.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

## Task 7: Window Title and the Edited Dot

**Files:**
- Modify: `src/gui/platform.cpp` (`setDocumentEdited`)
- Modify: `src/gui/mainwindow.cpp:1506, 1555, 1758, 2025, 4009` (title sites), `update()`

**Interfaces:**
- Consumes: `platform::windowTitle()` (Task 2), `platform::setDocumentEdited()`.
- Produces: nothing.

- [ ] **Step 1: Implement `setDocumentEdited` in `platform.cpp`**

```cpp
void platform::setDocumentEdited(Fl_Window * win, bool edited) {
#ifdef __APPLE__

  if (!win || !win->shown())
    return;

  /* [NSWindow setDocumentEdited:] draws the dot in the close button. This
   * needs no Objective-C source file: <objc/message.h> is a plain C API,
   * and fl_xid() hands back the NSWindow FLTK created for us. libobjc
   * arrives with the Cocoa framework we already link.
   */
  id nsWindow = (id)fl_xid(win);
  if (!nsWindow)
    return;

  typedef void (*SetEditedFn)(id, SEL, BOOL);
  ((SetEditedFn)objc_msgSend)(nsWindow,
                              sel_registerName("setDocumentEdited:"),
                              edited ? YES : NO);
#else
  (void)win;
  (void)edited;
#endif
}
```

Add, inside the `__APPLE__` guard at the top of the file:

```cpp
#ifdef __APPLE__
#include <FL/platform.H>
#include <objc/message.h>
#include <objc/runtime.h>
#include <objc/objc.h>
#endif
```

If this does not compile or the dot does not appear, **delete the body and leave the no-op** — the spec makes this optional and nothing else depends on it. Do not spend more than one attempt's effort on it.

- [ ] **Step 2: Route the five title sites through `platform::windowTitle`**

At `mainwindow.cpp:4009` (constructor) and `:1506`:

```cpp
  copy_label(platform::windowTitle(0, false).c_str());
```

At `:1555`, `:1758` and `:2025`, replacing `copy_label((std::string("BurrTools - ") + fname).c_str());`:

```cpp
  copy_label(platform::windowTitle(fname.c_str(), changed).c_str());
```

Add `#include "platform.h"` to `mainwindow.cpp`.

Confirm with `grep -n "BurrTools - " src/gui/mainwindow.cpp` that no site is left behind.

- [ ] **Step 3: Drive the dot from `update()`**

`mainWindow_c::update()` is already called once a second from the loop in `main.cpp:50-53`. Add at its end:

```cpp
  /* 'changed' is assigned in about fifty places, so rather than hooking
   * every write, publish it on the regular update tick. A dot that appears
   * up to a second late is imperceptible.
   */
  platform::setDocumentEdited(this, changed);
```

- [ ] **Step 4: Run every gate**

```bash
just build && just test-all && just check && just build-werror && just check-gui
```

- [ ] **Step 5: Verify**

- macOS: a freshly launched app titles its window `Untitled`; opening `examples/Bermuda.xmpuzzle` titles it `Bermuda.xmpuzzle` with no path and no `BurrTools - ` prefix.
- Modify a shape; within a second a dot appears in the close button. Save; it clears.
- Linux: the title is still `BurrTools - <path>`, with `BurrTools - Untitled` before anything is loaded.

- [ ] **Step 6: Commit**

```bash
git add src/gui/platform.cpp src/gui/mainwindow.cpp
git commit -m "feat(gui): use document-style window titles on macOS

The window is now titled with the document's name alone on macOS; the
application name belongs in the menu bar. Other platforms keep
'BurrTools - <name>'. The placeholder 'unknown' becomes 'Untitled'
everywhere, since it read like an error.

Unsaved state drives the dot in the close button via
[NSWindow setDocumentEdited:], reached through the plain-C objc runtime
rather than an Objective-C source file. It is published on the existing
one-second update tick rather than by hooking the fifty-odd places
'changed' is assigned.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

## Task 8: Icons and Bundle Metadata

**Files:**
- Create: `scripts/make-macos-icons.sh`, `mac/BurrTools.icns`, `mac/BurrToolsDoc.icns`
- Modify: `scripts/create-macos-bundle.sh`

**Interfaces:**
- Consumes: a working `build/burrtools` for rendering.
- Produces: two `.icns` files referenced by the bundle script.

The only existing art is `burricons.ico` at 64×64, 8-bit (verified with `sips`). macOS wants 1024×1024, so the icon is rendered with BurrTools itself, using the tiled high-resolution export path in `imageexport.cpp` and `tr.c` that already exists to produce images larger than the screen.

- [ ] **Step 1: Determine how to drive an image export non-interactively**

Read `src/gui/imageexport.cpp` and check whether `burrTxt`/`burrTxt2` expose an export path. If no non-interactive route exists, **render once by hand** through the GUI's Export ▸ Image dialog at 1024×1024 with a transparent background, save the PNG to `mac/icon-source.png`, and commit it. Have the script start from that PNG rather than inventing a headless rendering mode — that would be a feature, and it is not in this plan's scope.

Pick a visually distinctive puzzle from `examples/` — a six-piece burr reads better at 32×32 than a dense shape.

- [ ] **Step 2: Write `scripts/make-macos-icons.sh`**

```bash
#!/bin/bash
# Build the macOS .icns files from a rendered puzzle image.
#
# The only historical icon art is burricons.ico at 64x64, far short of the
# 1024x1024 macOS wants, so the icon is a render of an actual puzzle --
# which is also the most honest thing for it to depict.
set -e

SRC="${1:-mac/icon-source.png}"
OUT_DIR="mac"

if [ ! -f "$SRC" ]; then
	echo "error: $SRC not found. See scripts/make-macos-icons.sh for how it is produced." >&2
	exit 1
fi

make_icns() {
	local name="$1"
	local iconset="$(mktemp -d)/${name}.iconset"
	mkdir -p "$iconset"

	for size in 16 32 128 256 512; do
		sips -z $size $size "$SRC" --out "${iconset}/icon_${size}x${size}.png" >/dev/null
		sips -z $((size*2)) $((size*2)) "$SRC" --out "${iconset}/icon_${size}x${size}@2x.png" >/dev/null
	done

	iconutil -c icns "$iconset" -o "${OUT_DIR}/${name}.icns"
	echo "wrote ${OUT_DIR}/${name}.icns"
}

mkdir -p "$OUT_DIR"
make_icns BurrTools
make_icns BurrToolsDoc
```

`chmod +x scripts/make-macos-icons.sh`, then run it.

The document icon uses the same art for now. If it should be visually distinct — a page outline behind the puzzle — that is design work; note it as a follow-up rather than improvising.

- [ ] **Step 3: Copy the icons into the bundle**

In `scripts/create-macos-bundle.sh`, after the existing `cp` of the executable:

```bash
# Icons
if [ -f "mac/BurrTools.icns" ]; then
	cp mac/BurrTools.icns "${BUNDLE}/Contents/Resources/"
	cp mac/BurrToolsDoc.icns "${BUNDLE}/Contents/Resources/"
else
	echo "warning: mac/BurrTools.icns missing; run scripts/make-macos-icons.sh" >&2
fi
```

- [ ] **Step 4: Add the plist keys**

In the same script's `Info.plist` heredoc, add beside `CFBundleName`:

```xml
	<key>CFBundleIconFile</key>
	<string>BurrTools</string>
	<key>NSRequiresAquaSystemAppearance</key>
	<true/>
```

And inside the existing `CFBundleDocumentTypes` dict, beside `CFBundleTypeName`:

```xml
			<key>CFBundleTypeIconFile</key>
			<string>BurrToolsDoc</string>
```

`NSRequiresAquaSystemAppearance` is the spec's dark-mode decision: the app stays light even when the system is dark, because roughly a dozen widgets draw with hard-coded colours and the 125 XPM icons assume a light background.

- [ ] **Step 5: Build the bundle and verify**

```bash
just build && ./scripts/create-macos-bundle.sh
open BurrTools.app
```

- The Dock icon is the rendered puzzle, not a generic placeholder.
- `.xmpuzzle` files in Finder show the document icon.
- ⌘-Tab shows the app icon.
- The icon is legible at 32×32 in Finder's list view — if it is mud at that size, pick a simpler puzzle and re-render.
- With the system set to Dark Mode, the app still renders light and consistent.

- [ ] **Step 6: Commit**

```bash
git add scripts/make-macos-icons.sh scripts/create-macos-bundle.sh mac/
git commit -m "feat(macos): add app and document icons, and opt out of dark mode

The only icon art in the tree was burricons.ico at 64x64, far short of the
1024x1024 macOS wants. The icon is instead rendered from an actual puzzle
through the existing high-resolution export path, so it is reproducible
and depicts what the program makes. scripts/make-macos-icons.sh packs the
.icns files.

Sets NSRequiresAquaSystemAppearance so the app stays light under a dark
system: about a dozen widgets draw with hard-coded colours and the 125 XPM
icons assume a light background, so following the system appearance would
look broken rather than dark.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

## Task 9: Full Verification Pass

No code changes unless this pass finds defects. Work through the spec's Section 8.2 checklist in full, on a real build, on both macOS and Linux.

- [ ] **Step 1: Run every automated gate from a clean build**

```bash
just rebuild && just test-all && just check && just build-werror && just check-gui
```

- [ ] **Step 2: Walk the complete Section 8.2 checklist**

Open `design/2026-09-20-macos-native-gui.md` and perform every item under Blocking, Menus and shortcuts, Appearance, and Integration. Do not tick anything not actually observed.

- [ ] **Step 3: Re-verify the data-loss paths specifically**

These are the ones where a defect costs a user their work, so they get a second pass:

- ⌘Q with unsaved changes prompts, and Cancel actually cancels.
- Finder double-click with unsaved changes prompts first.
- Cancelling Save As inside the prompt aborts the whole operation.
- Quitting from the Dock icon's context menu also prompts. *(Not previously tested — a distinct path from ⌘Q.)*

- [ ] **Step 4: Report**

Write up what was verified, what was found, and anything left outstanding. If the ⌘Q fallback from Task 3 Step 8 was needed, or the edited dot was abandoned, or scheme damage was found and left unfixed, say so plainly. Update the spec's Section 10 (Known Deviations) if the list changed.

- [ ] **Step 5: Commit any documentation updates**

```bash
git add design/2026-09-20-macos-native-gui.md
git commit -m "docs: record the outcome of the macOS GUI verification pass

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

## Self-Review Notes

**Spec coverage.** Every spec section maps to a task: §2 → Tasks 1–2; §3 → Tasks 3–4; §4 → Task 2; §5 → Tasks 6, 8; §6 → Tasks 5, 7; §7 → Tasks 1–2; §8 → Task 9; §9 dark mode → Task 8 Step 4. Nothing is unassigned.

**Deviations from the spec, for the reviewer:**

1. **`--self-check` (Task 1, Step 6)** is an extension. The spec said "startup assertion"; this makes the same assertion runnable headlessly so CI can gate it. Cheap, no new link dependencies. Veto it and the assertion still works on first launch.
2. **Two knowing `#ifdef`s in `mainwindow.cpp`** — the menu bar's construction (Task 3 Step 6) and the F-key cases (Task 4 Step 1) — against the spec's "no new platform ifdefs" constraint. Both are explained at the site; the second has a runtime alternative to try first. The concrete widget type must be chosen somewhere, and it is chosen at the single point of construction.
3. **`LFl_Sys_Menu_Bar` in `Layouter.h`** is a new class the spec did not anticipate. `Fl_Sys_Menu_Bar` *does* derive publicly from `Fl_Menu_Bar` (`FL/Fl_Sys_Menu_Bar.H:96`) — which is precisely why `MainMenu` can remain an `Fl_Menu_Bar*`. What cannot be shared is the layoutable wrapper: `LFl_Sys_Menu_Bar` deriving from `LFl_Menu_Bar` would inherit `Fl_Menu_Bar` twice, so the two wrappers sit side by side instead.

   *(Corrected after implementation. Earlier drafts of this plan and of the spec asserted that `Fl_Sys_Menu_Bar` was outside `Fl_Menu_Bar`'s hierarchy. That was wrong, and the Task 3 review caught it. The parallel class is still needed, for the diamond reason above.)*

**Findings from verifying the FLTK source, which changed the plan:**

- `Fl_Menu_::copy()` and `menu()` are **non-virtual** (`FL/Fl_Menu_.H:135-136`), so a system menu bar held in an `Fl_Menu_Bar*` would never refresh. The fix — calling the virtual `update()` (`FL/Fl_Menu_Bar.H:93`) — is introduced in Task 1, before it is needed, so Task 3 does not have to debug it.
- `help_src` (`meson.build:197`) is dead, naming three files under a `src/help/` directory that does not exist. Removed in Task 1.
- The label-keyed `findMenuEntry()` would have aborted at startup on macOS. Fixed in Task 1.
