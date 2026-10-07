import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtTest
import BurrTools.Ui

// The shell (C00, C10, C11, C15) checked through the real Main.qml, by the
// spec's object ids (test plan section 1).
TestCase {
    id: tc
    name: "Shell"
    when: windowShown

    Component { id: mainComponent; Main {} }

    property var win

    function init() {
        // any QML warning (a binding error, an undefined name) fails the case --
        // except two the headless platform gives: it renders with the software
        // backend, where the 3D view's QQuickRhiItem cannot draw (rendering is
        // tested separately, with an offscreen QRhi), and on Linux it cannot
        // grab the keyboard for a menu that opens as a window of its own
        failOnWarning(/^(?!No QRhi found for window|This plugin does not support grabbing the keyboard).*/)
        App.layout.workspace = LayoutController.Entities
        App.layout.leftCollapsed = false
        App.layout.rightCollapsed = false
        win = createTemporaryObject(mainComponent, tc)
        verify(win !== null, "Main.qml failed to load")
        win.width = 1600
        win.height = 1000
        waitForRendering(win.contentItem)
    }

    function cleanup() {
        App.settings.tooltips = true
        App.settings.density = "standard"
        App.viewport.navMode = "orbit"
        if (App.layout.focus !== LayoutController.FocusNone)
            App.layout.escape()
    }

    function item(name) {
        const o = findChild(win.contentItem, name)
        verify(o !== null, "no item " + name)
        return o
    }

    function test_menuBarHoldsTheMenusInOrder() {
        // T-C00-1, as menus only (native menu bars hold no direct buttons):
        // headless there is no native bar, so this is the Qt Quick one
        const bar = win.menuBar
        verify(bar !== null)
        const keys = ["file", "edit", "view", "export", "help"]
        compare(bar.count, keys.length)
        for (let i = 0; i < keys.length; i++)
            compare(bar.menuAt(i).objectName, "shell.menu." + keys[i])

        // legacy's top-level buttons are items now, Settings is in File
        const items = (menu) => {
            const out = []
            for (let i = 0; i < menu.count; i++)
                if (menu.itemAt(i).objectName.length > 0)
                    out.push(menu.itemAt(i).objectName)
            return out
        }
        verify(items(bar.menuAt(0)).indexOf("shell.menuitem.settings") >= 0)
        verify(items(bar.menuAt(1)).indexOf("shell.menuitem.editcomment") >= 0)
        // Status is in View, but on macOS in File with Cmd+I, where Mac apps keep Get Info
        verify(items(bar.menuAt(Qt.platform.os === "osx" ? 0 : 2)).indexOf("shell.menuitem.status") >= 0)
        verify(items(bar.menuAt(4)).indexOf("shell.menuitem.about") >= 0)

        // no gear and no file name in the window: the title bar names the file
        compare(findChild(win.contentItem, "shell.settings"), null)
        compare(findChild(win.contentItem, "shell.filename"), null)
    }

    function test_menuItemsRunTheirCommands() {
        const bar = win.menuBar
        const edit = bar.menuAt(1)
        let undo = null
        for (let i = 0; i < edit.count; i++)
            if (edit.itemAt(i).objectName === "shell.menuitem.edit.undo")
                undo = edit.itemAt(i)
        verify(undo !== null)
        verify(!undo.enabled)                      // nothing to undo
        // the item shows its shortcuts; it owns the key only on macOS
        compare(undo.action.shortcutText, Qt.platform.os === "osx" ? "⌘Z" : "Ctrl+Z")
        if (Qt.platform.os !== "osx")
            compare(undo.action.shortcut, "")
        App.document.openFile(Qt.resolvedUrl("../../../examples/PelikanBurr.xmpuzzle"))
        App.commands.blocked = true                // as while a dialog is open
        verify(!bar.menuAt(0).itemAt(0).enabled)
        App.commands.blocked = false
        verify(bar.menuAt(0).itemAt(0).enabled)
    }

    function test_railSwitchesWorkspaces() {
        // T-C15-1/2
        verify(item("rail-entities").selected)
        mouseClick(item("rail-puzzle"))
        compare(App.layout.workspace, LayoutController.Puzzle)
        verify(item("rail-puzzle").selected)
        verify(!item("rail-entities").selected)
        verify(item("puzzle.left").visible)
        verify(!item("entities.shapes").visible)
        mouseClick(item("rail-solver"))
        compare(App.layout.workspace, LayoutController.Solver)
        verify(item("solver.right").visible)
        mouseClick(item("rail-entities"))
        verify(item("entities.shapes").visible)
    }

    function test_collapseAndExpandTheCards() {
        // AC-C11-01/04: rails replace the cards, the centre takes the room
        const centre = item("entities.viewport")
        const before = centre.width
        mouseClick(item("entities.shapes.collapse"))
        verify(App.layout.leftCollapsed)
        tryCompare(item("workspace.left"), "width", 48, 1000)
        verify(item("workspace.left.rail").visible)
        tryVerify(() => centre.width > before + 200, 1000)

        mouseClick(item("workspace.left.rail.expand"))
        verify(!App.layout.leftCollapsed)
        tryCompare(item("workspace.left"), "width", 320, 1000)
    }

    function test_columnWidthsAtTheReferenceSize() {
        // T-DEN-2 at Standard density, 1600 dp wide
        tryCompare(item("workspace.left"), "width", 320, 1000)
        tryCompare(item("workspace.right"), "width", 340, 1000)
        tryVerify(() => Math.abs(item("entities.viewport").width - 864) < 1, 1000)
    }

    function test_focusModesUseTheRails() {
        App.layout.toggleFocus3d()
        tryCompare(item("workspace.left"), "width", 48, 1000)
        tryCompare(item("workspace.right"), "width", 48, 1000)
        verify(item("workspace.right.rail").visible)
        verify(App.layout.escape())
        tryCompare(item("workspace.right"), "width", 340, 1000)

        mouseClick(item("entities.editor.focus2d"))
        compare(App.layout.focus, LayoutController.Focus2d)
        tryVerify(() => Math.abs(item("entities.viewport").width - 360) < 1, 1000)
        App.layout.escape()
    }

    function test_loadingAFileFillsTheShapesListAndStatus() {
        App.document.openFile(Qt.resolvedUrl("../../../examples/PelikanBurr.xmpuzzle"))
        compare(App.document.fileName, "PelikanBurr.xmpuzzle")
        // the window title names the document as the platform does
        if (Qt.platform.os === "windows")
            compare(win.title, "PelikanBurr - BurrTools")
        else if (Qt.platform.os === "osx")
            compare(win.title, "PelikanBurr")
        else
            compare(win.title, "PelikanBurr – BurrTools")
        verify(App.shapes.count > 1)
        const list = item("entities.shapes.list")
        tryVerify(() => list.count === App.shapes.count, 1000)
        verify(item("status.text").text.indexOf("S1") >= 0)

        // selecting a row updates the selection and the status sentence
        const row = findChild(list, "entities.shapes.row.1")
        verify(row !== null)
        mouseClick(row)
        compare(App.shapes.selected, 1)
        verify(item("status.text").text.indexOf("S2") >= 0)
    }

    function test_themeSwitchRecolours() {
        App.settings.theme = "light"
        compare(Theme.dark, false)
        compare(win.color, Theme.bg)
        App.settings.theme = "dark"
        compare(Theme.dark, true)
        compare(win.color, Theme.bg)
        App.settings.theme = "system"
    }

    function test_voxelTypeChipIsReadOnly() {
        // AC-C14-01
        const chip = item("entities.editor.voxelType")
        verify(chip.visible)
        App.document.newDocument(2)
        compare(App.document.gridTypeName, "Spheres")
        App.document.newDocument(0)
    }

    function menuItem(menu, key) {
        for (let i = 0; i < menu.count; i++)
            if (menu.itemAt(i).objectName === "shell.menuitem." + key)
                return menu.itemAt(i)
        return null
    }

    function test_menusShowUpToTwoShortcuts() {
        if (Qt.platform.os === "osx")
            skip("the system menu shows one key equivalent")
        const file = win.menuBar.menuAt(0)
        compare(menuItem(file, "file.open").action.shortcutText, "Ctrl+O / F3")
        compare(menuItem(file, "file.save").action.shortcutText, "Ctrl+S / F2")
        compare(menuItem(file, "file.new").action.shortcutText, "Ctrl+N")
        compare(menuItem(file, "file.convert").action.shortcutText, "")
        const redo = menuItem(win.menuBar.menuAt(1), "edit.redo")
        compare(redo.action.shortcutText, "Ctrl+Shift+Z / Ctrl+Y")
    }

    function test_hidingTheMenuBarMovesTheMenusToTheRail() {
        if (!App.commands.menuBarHideable)
            skip("macOS keeps the system menu bar")
        const toggle = menuItem(win.menuBar.menuAt(2), "view.menuBar")
        verify(toggle !== null)
        verify(toggle.checkable && toggle.checked)
        const railMenu = item("rail-menu")
        verify(!railMenu.visible)

        toggle.action.trigger()
        verify(!App.settings.showMenuBar)
        verify(!win.menuBar.visible)
        tryVerify(() => railMenu.visible, 1000)
        verify(!toggle.checked)

        // the rail's menu holds every top-level menu as a submenu
        const popup = findChild(railMenu, "rail-menu.popup")
        verify(popup !== null)
        waitForRendering(win.contentItem)       // the button has just appeared
        mouseClick(railMenu)
        tryVerify(() => popup.opened, 2000)
        compare(popup.count, 5)
        const keys = ["file", "edit", "view", "export", "help"]
        for (let i = 0; i < keys.length; i++)
            compare(popup.menuAt(i).objectName, "rail-menu." + keys[i])
        // and its items run commands like the bar's
        const showAgain = menuItem(popup.menuAt(2), "view.menuBar")
        verify(showAgain !== null && !showAgain.checked)
        popup.close()
        showAgain.action.trigger()
        verify(App.settings.showMenuBar)
        verify(win.menuBar.visible)
        tryVerify(() => !railMenu.visible, 1000)
    }

    function test_shortcutsWorkWithTheMenuBarHidden() {
        if (!App.commands.menuBarHideable)
            skip("macOS keeps the system menu bar")
        App.settings.showMenuBar = false
        // every key binding is on the window, none on the hidden menu items
        const shortcuts = findChild(win, "shell.shortcuts")
        let save = null
        for (let i = 0; i < shortcuts.count; i++)
            if (shortcuts.objectAt(i).sequences.indexOf("Ctrl+S") >= 0)
                save = shortcuts.objectAt(i)
        verify(save !== null && save.enabled)
        App.settings.showMenuBar = true
    }

    // keys reach the focused window; Main is a window of its own
    function activate(w) {
        w.requestActivate()
        tryVerify(() => w.active, 3000)
    }

    function test_altUnderlinesTheMenuBarAccessKeys() {
        // proposal 1 in the window: the bar's labels underline their access
        // key while the keyboard drives the menus
        if (Qt.platform.os === "osx")
            skip("macOS menus have no access keys")
        if (App.keyboardCues.showAccessKeys)
            skip("this system underlines access keys always")
        activate(win)
        const fileItem = win.menuBar.itemAt(0)
        compare(fileItem.contentItem.text, "File")
        keyPress(Qt.Key_Alt)
        verify(App.keyboardCues.showAccessKeys)
        compare(fileItem.contentItem.text, "<u>F</u>ile")
        keyRelease(Qt.Key_Alt)
        keyClick(Qt.Key_Escape)
        verify(!App.keyboardCues.showAccessKeys)
        compare(fileItem.contentItem.text, "File")
        // the mouse ends it too
        keyPress(Qt.Key_Alt)
        keyRelease(Qt.Key_Alt)
        mouseClick(item("entities.viewport"), 20, 300)
        verify(!App.keyboardCues.showAccessKeys)
    }

    function test_theWindowOpensWhereItWasLeft() {
        // proposal 3 through Main.qml: a saved place on this screen comes back
        const screen = Qt.application.screens[0]
        App.settings.saveWindowGeometry(screen.virtualX + 10, screen.virtualY + 20, 980, 660, false)
        const w = createTemporaryObject(mainComponent, tc)
        waitForRendering(w.contentItem)
        compare(w.x, screen.virtualX + 10)
        compare(w.y, screen.virtualY + 20)
        compare(w.width, 980)
        compare(w.height, 660)
        compare(w.visibility, Window.Windowed)
    }

    function test_maximisedComesBackMaximised() {
        App.settings.saveWindowGeometry(0, 0, 0, 0, true)
        const w = createTemporaryObject(mainComponent, tc)
        tryCompare(w, "visibility", Window.Maximized, 2000)
        App.settings.saveWindowGeometry(0, 0, 1000, 700, false)
    }

    function test_fullScreenSavesOnlyWhatItReturnsTo() {
        // leaving full screen returns to the window it came from: saving in
        // full screen keeps the windowed place and records whether that was
        // maximised, never the full-screen size
        App.settings.saveWindowGeometry(30, 40, 990, 670, false)
        win.restoreVisibility = Window.Maximized
        win.showFullScreen()
        tryCompare(win, "visibility", Window.FullScreen, 2000)
        win.saveGeometry()
        const g = App.settings.windowGeometry()
        compare(g.x, 30)
        compare(g.width, 990)
        verify(g.maximized)
        win.showNormal()
        App.settings.saveWindowGeometry(0, 0, 1000, 700, false)
    }

    function test_anOpenMenuTakesEscBeforeTheLayout() {
        // proposal 5: Esc closes the menu and leaves focus mode alone
        App.layout.toggleFocus3d()
        compare(App.layout.focus, LayoutController.Focus3d)
        const esc = findChild(win, "shell.escape")
        verify(esc !== null)
        verify(esc.enabled)

        const menu = win.menuBar.menuAt(0)
        verify(menu.closePolicy & Popup.CloseOnEscape)
        menu.open()
        tryVerify(() => menu.opened, 2000)
        verify(win.openPopups > 0)
        verify(!esc.enabled)                  // the window's Esc waits for the menu

        activate(win)
        keyClick(Qt.Key_Escape)               // reaches whichever window has the keys
        if (menu.visible)
            menu.close()                      // the menu's own window had them: close it as Esc would
        tryVerify(() => !menu.visible, 2000)
        tryVerify(() => esc.enabled, 2000)
        compare(App.layout.focus, LayoutController.Focus3d)     // still focused
        App.layout.escape()
        compare(App.layout.focus, LayoutController.FocusNone)
    }

    // --- tooltips (PR-14) ---------------------------------------------------------

    function test_tooltipsWaitHalfASecondAndFollowTheSetting() {
        const orbit = item("entities.viewport.toolbar.orbit")
        const tip = orbit.toolTip
        mouseMove(orbit, orbit.width / 2, orbit.height / 2)
        verify(orbit.hovered)
        wait(Theme.tooltipDelay / 2)
        verify(!tip.opened, "shown before the delay")
        tryVerify(() => tip.opened, 2000)
        compare(tip.text, "Orbit: drag to rotate the view (O)")

        // Settings ▸ Show tooltips off hides it and keeps it hidden
        App.settings.tooltips = false
        tryVerify(() => !tip.visible, 1000)
        mouseMove(orbit, 4, 4)
        wait(Theme.tooltipDelay + 200)
        verify(!tip.visible)

        App.settings.tooltips = true
        tryVerify(() => tip.opened, 2000)
        mouseMove(item("status"), 4, 4)
        tryVerify(() => !tip.visible, 1000)
    }

    // --- narrow window, collapse transition (C11) -------------------------------------

    function rectOf(it) {
        const p = it.mapToItem(win.contentItem, 0, 0)
        return { x: p.x, y: p.y, w: it.width, h: it.height }
    }
    function overlaps(a, b) {
        return a.x < b.x + b.w - 0.5 && b.x < a.x + a.w - 0.5 && a.y < b.y + b.h - 0.5 && b.y < a.y + a.h - 0.5
    }
    function within(a, b) {
        return a.x >= b.x - 0.5 && a.y >= b.y - 0.5 && a.x + a.w <= b.x + b.w + 0.5 && a.y + a.h <= b.y + b.h + 0.5
    }

    function test_narrowWindowWithBothCardsCollapsedStaysUsable_data() {
        return [ { tag: "standard", density: "standard" }, { tag: "minimal", density: "minimal" } ]
    }
    function test_narrowWindowWithBothCardsCollapsedStaysUsable(data) {
        // T-C11-6 / AC-C11-04: 960 dp wide, both cards collapsed -- toolbar,
        // view cube, hint and status bar remain usable
        App.settings.density = data.density
        App.document.openFile(Qt.resolvedUrl("../../../examples/PelikanBurr.xmpuzzle"))
        win.width = 960
        win.height = 640
        compare(win.width, 960)                      // the window's minimum allows it
        App.layout.leftCollapsed = true
        App.layout.rightCollapsed = true
        tryCompare(item("workspace.left"), "width", Theme.collapsedRail, 1000)
        tryCompare(item("workspace.right"), "width", Theme.collapsedRail, 1000)
        waitForRendering(win.contentItem)

        const view = rectOf(item("entities.viewport"))
        const windowRect = { x: 0, y: 0, w: win.contentItem.width, h: win.contentItem.height }

        // the toolbar inside the view, its buttons whole and apart
        const bar = rectOf(item("entities.viewport.toolbar"))
        verify(within(bar, view), "toolbar inside the viewport")
        const buttons = ["orbit", "pan", "edit", "display", "focus"].map(n => item("entities.viewport.toolbar." + n))
        for (let i = 0; i < buttons.length; i++) {
            verify(buttons[i].visible && buttons[i].width >= 30, buttons[i].objectName)
            verify(within(rectOf(buttons[i]), bar), buttons[i].objectName + " inside the toolbar")
            for (let j = i + 1; j < buttons.length; j++)
                verify(!overlaps(rectOf(buttons[i]), rectOf(buttons[j])), buttons[i].objectName + " overlaps " + buttons[j].objectName)
        }

        // the view cube: shown, across the view's width, clear of the toolbar
        const cube = item("entities.viewport.cube")
        verify(cube.visible)
        const c = rectOf(cube)
        verify(c.x >= view.x && c.x + c.w <= view.x + view.w + 0.5, "cube inside the view's width")
        verify(!overlaps(c, bar), "cube clear of the toolbar")

        // the hint (Standard; Minimal moves it into tooltips): whole, clear of both
        const hint = item("entities.viewport.hint")
        if (data.density === "standard") {
            verify(hint.visible)
            const h = rectOf(hint)
            verify(within(h, view), "hint inside the viewport")
            verify(!overlaps(h, bar) && !overlaps(h, c), "hint clear of toolbar and cube")
        } else {
            verify(!hint.visible)
        }

        // the status bar: text and readout apart, inside the window
        const text = rectOf(item("status.text")), cursor = rectOf(item("status.cursor"))
        verify(within(rectOf(item("status")), windowRect))
        verify(text.w > 100, "status text has room: " + text.w)
        verify(text.x + text.w <= cursor.x + 0.5, "status text runs into the readout")

        // the rails' expand buttons are on screen
        for (const n of ["workspace.left.rail.expand", "workspace.right.rail.expand"])
            verify(within(rectOf(item(n)), windowRect), n)

        // and it all still works
        mouseClick(buttons[1])
        compare(App.viewport.navMode, "pan")
        mouseClick(buttons[4])
        compare(App.layout.focus, LayoutController.Focus3d)
        App.layout.escape()
        mouseClick(item("workspace.right.rail.expand"))
        verify(!App.layout.rightCollapsed)
    }

    function test_collapseTakesTwoHundredMillisecondsAndLeavesNothingStale() {
        // AC-C11-07: a 200 ms transition, after which the 3D view has the
        // centre's final size (the redraw itself: test_viewport.cpp)
        compare(Theme.durCollapse, 200)
        const slot = item("workspace.left")
        const centre = item("entities.viewport")
        const surface = item("entities.viewport.surface")
        tryCompare(slot, "width", 320, 1000)
        const start = slot.width, end = Theme.collapsedRail

        const t0 = Date.now()
        App.layout.leftCollapsed = true
        compare(slot.width, start)                   // no jump: it animates
        let partWay = false
        tryVerify(() => {
            if (slot.width < start - 1 && slot.width > end + 1)
                partWay = true
            return slot.width === end
        }, 2000)
        const took = Date.now() - t0
        verify(partWay, "never seen part-way")
        verify(took >= 150, "took only " + took + " ms")

        // the canvas fills the grown centre and the camera projects for it
        compare(surface.width, centre.width)
        compare(surface.height, centre.height)
        compare(App.viewport.viewportSize.width, surface.width)
        compare(App.viewport.viewportSize.height, surface.height)
    }

    function test_messagesOpenOneAtATime() {
        // one file can raise two messages (an unfinished search, then its
        // comment): they open in turn, the second not over the first
        const dialog = findChild(win, "shell.message")
        verify(dialog !== null)
        App.document.messageRequested("First", "one")
        App.document.messageRequested("Second", "two")
        tryCompare(dialog, "visible", true)
        compare(dialog.title, "First")
        compare(win.pendingMessages.length, 1)
        dialog.close()
        tryCompare(dialog, "title", "Second")
        tryCompare(dialog, "visible", true)
        compare(win.pendingMessages.length, 0)
        dialog.close()
        tryCompare(dialog, "visible", false)
    }
}
