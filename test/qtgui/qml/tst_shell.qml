import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
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
        tryVerify(() => win.contentReady, 5000)       // made after the first frame
    }

    function cleanup() {
        App.settings.tooltips = true
        App.settings.density = "standard"
        App.settings.showMenuBar = true
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

    function test_collapseAndExpandKeepTheKeyboardOnTheirButton() {
        // Space on the focused collapse button collapses the card, and the
        // keyboard is then on the rail's expand button: Space again expands
        activate(win)
        const collapse = item("entities.shapes.collapse")
        for (let i = 0; i < 40 && !collapse.activeFocus; i++)
            keyClick(Qt.Key_Tab)
        verify(collapse.activeFocus, "Tab reaches the collapse button")
        keyClick(Qt.Key_Space)
        verify(App.layout.leftCollapsed)
        const expand = item("workspace.left.rail.expand")
        tryVerify(() => expand.activeFocus, 1000)
        verify(expand.visualFocus)                       // still the keyboard's, ring and all
        keyClick(Qt.Key_Space)
        verify(!App.layout.leftCollapsed)
        tryVerify(() => collapse.activeFocus, 1000)
    }

    function test_tabGoesOnFromTheButtonLastClicked() {
        // a mouse click makes the button the keyboard's starting point (no ring)
        activate(win)
        const entities = item("rail-entities")
        const puzzle = item("rail-puzzle")
        mouseClick(entities)
        verify(entities.activeFocus)
        verify(!entities.visualFocus)
        keyClick(Qt.Key_Tab)
        verify(puzzle.activeFocus)
        verify(puzzle.visualFocus)
        // the 3D toolbar is one roving stop whose buttons take no focus on a
        // click (C20 sticky focus): the keyboard stays where it was
        mouseClick(item("entities.viewport.toolbar.pan"))
        verify(puzzle.activeFocus)
        App.viewport.navMode = "orbit"
    }

    function test_theVoxelTypeChipIsNoTabStop() {
        // read-only: Tab passes it by; assistive tech still reads its note
        const chip = item("entities.editor.voxelType")
        verify(!chip.activeFocusOnTab)
        verify(chip.Accessible.description.length > 0)
    }

    function test_theCollapsedShapesCardStillListsTheShapes_data() {
        return [ { tag: "standard", density: "standard" }, { tag: "minimal", density: "minimal" } ]
    }
    function test_theCollapsedShapesCardStillListsTheShapes(data) {
        // the rail keeps a chip per shape -- colour and id -- that selects it
        App.settings.density = data.density
        App.document.openFile(Qt.resolvedUrl("../../../examples/PelikanBurr.xmpuzzle"))
        verify(App.shapes.count > 1)
        App.shapes.select(0)
        App.layout.leftCollapsed = true
        // while the column still narrows, the chips already have their own
        // width (they neither stretch nor shrink with the animation)
        tryVerify(() => findChild(win.contentItem, "entities.shapes.rail.chip.0") !== null, 1000)
        const first = item("entities.shapes.rail.chip.0")
        if (item("workspace.left").width > Theme.collapsedRail + 20)
            compare(first.width, Theme.collapsedRail - 14)
        tryCompare(item("workspace.left"), "width", Theme.collapsedRail, 1000)
        const list = item("entities.shapes.rail.list")
        verify(list.visible)
        tryCompare(list, "count", App.shapes.count, 1000)
        waitForRendering(win.contentItem)

        const row = item("entities.shapes.rail.row.1")
        const chip = item("entities.shapes.rail.chip.1")
        const rail = item("workspace.left")
        const r = chip.mapToItem(rail, 0, 0)
        verify(r.x >= 0 && r.x + chip.width <= rail.width, "the chip fits the rail")
        mouseClick(row)
        compare(App.shapes.selected, 1)
        verify(row.selected)
        verify(!item("entities.shapes.rail.row.0").selected)

        // the other workspaces' rails have no shapes
        App.layout.workspace = LayoutController.Puzzle
        verify(!list.visible)
    }

    function test_theShellFollowsTheDensity_data() {
        // density.md section 1 and 2, outside the cards
        return [
            { tag: "standard", density: "standard", button: 28, status: 11.5, toolbar: 44 },
            { tag: "minimal", density: "minimal", button: 26, status: 11, toolbar: 32 },
        ]
    }
    function test_theShellFollowsTheDensity(data) {
        App.settings.density = data.density
        const minimal = data.density === "minimal"
        waitForRendering(win.contentItem)

        // the workspace rail: Minimal turns the caption, reading top to bottom,
        // and the button is as tall as it, inside the 44 dp rail
        for (const n of ["rail-entities", "rail-puzzle", "rail-solver"]) {
            const b = item(n)
            const caption = findChild(b, "caption")
            verify(caption.visible, n)
            compare(caption.rotation, minimal ? 90 : 0, n)
            verify(b.width <= Theme.workspaceRail, n)
            if (minimal)
                verify(b.height >= caption.implicitWidth + 22, n + " holds its caption")
        }
        // the menu button (the menu bar hidden): Minimal shows its icon alone
        App.settings.showMenuBar = false
        const menu = item("rail-menu")
        if (menu.visible) {
            compare(findChild(menu, "caption").visible, !minimal)
            compare(menu.Accessible.name, "Menu")
            compare(menu.tip, "All menus")
        }

        // Shapes' New: "+" alone in Minimal, square; named for assistive tech either way
        const add = item("entities.shapes.new")
        compare(add.text, minimal ? "+" : "+ New")
        if (minimal)
            compare(add.width, add.height)
        compare(add.Accessible.name, "New shape")

        // buttons, the status bar's text, the 3D toolbar
        compare(item("entities.shapes.new").height, 26)              // small in both
        compare(Theme.button, data.button)
        // a font's pixel size is whole: 11.5 is drawn at 12
        compare(item("status.text").font.pixelSize, Math.round(data.status))
        compare(item("status.cursor").font.pixelSize, Math.round(data.status))
        compare(item("entities.viewport.toolbar.orbit").height, data.toolbar)
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

    // Tab from the window's start round to it again: every stop, in order
    function tabStops() {
        win.requestActivate()
        tryVerify(() => win.active, 3000)
        win.contentItem.forceActiveFocus()
        const stops = []
        for (let i = 0; i < 80; i++) {
            keyClick(Qt.Key_Tab)
            const f = win.activeFocusItem
            verify(f !== null, "Tab left the focus nowhere after " + stops.map(s => s.name).join(", "))
            if (f === win.contentItem)
                continue                                   // not yet in the chain
            if (stops.length > 0 && f === stops[0].item)
                break
            stops.push({ item: f, name: f.objectName })
        }
        return stops
    }

    function test_tabStopsOnlyOnControlsInReadingOrder_data() {
        return [
            { tag: "cards", rails: false, order: [
                "rail-entities", "rail-puzzle", "rail-solver",
                "entities.shapes.new", "entities.shapes.collapse", "entities.shapes.row.0",
                // the inspector: Transform open (its buttons row by row), the others closed
                "entities.inspector.transform.header",
                "entities.transform.flip.x", "entities.transform.rotate.x.neg", "entities.transform.rotate.x.pos",
                "entities.transform.nudge.x.neg", "entities.transform.nudge.x.pos",
                "entities.transform.flip.y", "entities.transform.rotate.y.neg", "entities.transform.rotate.y.pos",
                "entities.transform.nudge.y.neg", "entities.transform.nudge.y.pos",
                "entities.transform.flip.z", "entities.transform.rotate.z.neg", "entities.transform.rotate.z.pos",
                "entities.transform.nudge.z.neg", "entities.transform.nudge.z.pos",
                "entities.inspector.fit.header", "entities.inspector.repair.header",
                "entities.viewport.toolbar", "entities.viewport.surface",
                "entities.editor.focus2d", "entities.editor.collapse",
                // the tool strip and the planes are one stop each
                "entities.editor.tools.group",
                "entities.editor.mirror.x", "entities.editor.mirror.y", "entities.editor.mirror.z",
                "entities.editor.span.x", "entities.editor.span.y", "entities.editor.span.z",
                "entities.editor.plane.group", "entities.editor.grid",
                "entities.editor.size.applyAll",
                "entities.editor.size.x.input", "entities.editor.size.y.input", "entities.editor.size.z.input",
                "entities.editor.colour.info", "entities.editor.colour.add", "entities.editor.colour.more",
                "entities.editor.colour.palette" ] },
            { tag: "rails", rails: true, order: [
                "rail-entities", "rail-puzzle", "rail-solver",
                "workspace.left.rail.expand", "entities.shapes.rail.row.0", "entities.shapes.rail.new",
                "entities.viewport.toolbar", "entities.viewport.surface",
                "workspace.right.rail.expand" ] },
        ]
    }
    function test_tabStopsOnlyOnControlsInReadingOrder(data) {
        // WCAG 2.4.3 / 2.1.1: Tab reaches every control once, in the order the
        // eye reads the window (rail, left card, 3D toolbar, right card), and
        // stops on nothing that does nothing -- no chip, label, read-only voxel
        // type, row action (the list is one stop; its keys and menu have
        // those) or disabled button. The 3D canvas is one (its keys, P5), after
        // the toolbar. A phase that adds a control adds it here, where it belongs.
        App.document.openFile(Qt.resolvedUrl("../../../examples/PelikanBurr.xmpuzzle"))
        // the puzzle's comment comes up in a message: out of the way
        for (let i = 0; i < 5; i++) {
            wait(50)
            const m = findChild(win, "shell.message")
            if (m === null || !m.visible)
                break
            m.close()
        }
        App.shapes.select(0)
        App.layout.leftCollapsed = data.rails
        App.layout.rightCollapsed = data.rails
        wait(400)                                          // the columns' animation
        const stops = tabStops()
        compare(stops.map(s => s.name), data.order)
        for (const s of stops) {
            verify(s.item.visible && s.item.enabled, s.name + " is a stop but hidden or disabled")
            verify(s.item.Accessible.name.length > 0, s.name + " has no accessible name")
        }
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

    function test_altOpensTheRailMenuWhileTheMenuBarIsHidden() {
        // View > Show menu bar off: an Alt tap opens the rail's menu (never
        // the menu bar), a menu's access key its submenu; Esc or another tap
        // closes it
        if (!App.commands.menuBarHideable)
            skip("macOS keeps the system menu bar")
        App.settings.showMenuBar = false
        activate(win)
        const railMenu = item("rail-menu")
        tryVerify(() => railMenu.visible, 1000)
        waitForRendering(win.contentItem)
        const popup = findChild(railMenu, "rail-menu.popup")

        keyPress(Qt.Key_Alt)
        keyRelease(Qt.Key_Alt)
        verify(App.keyboardCues.menuArmed)
        tryVerify(() => popup.opened, 2000)
        verify(!win.menuBar.visible)
        compare(findChild(popup.itemAt(0), "label").text, "<u>F</u>ile")
        // the access key opens that submenu
        keyClick(Qt.Key_X)
        tryVerify(() => popup.menuAt(3).opened, 2000)
        compare(popup.menuAt(3).objectName, "rail-menu.export")
        verify(!win.menuBar.visible)
        popup.menuAt(3).close()
        popup.close()
        tryVerify(() => !popup.visible, 2000)

        // a second tap closes it
        keyPress(Qt.Key_Alt)
        keyRelease(Qt.Key_Alt)
        tryVerify(() => popup.opened, 2000)
        keyPress(Qt.Key_Alt)
        keyRelease(Qt.Key_Alt)
        tryVerify(() => !popup.visible, 2000)
        verify(!App.keyboardCues.menuArmed)

        // Alt with the key at once opens the submenu directly
        keyClick(Qt.Key_V, Qt.AltModifier)
        tryVerify(() => popup.menuAt(2).opened, 2000)
        verify(!win.menuBar.visible)
        popup.menuAt(2).close()
        popup.close()
        tryVerify(() => !popup.visible, 2000)

        // Alt held through a click is no tap: nothing opens
        keyPress(Qt.Key_Alt)
        mouseClick(item("entities.viewport"), 20, 300, Qt.LeftButton, Qt.AltModifier)
        keyRelease(Qt.Key_Alt)
        verify(!App.keyboardCues.menuArmed)
        verify(!popup.visible)
    }

    function test_leavingTheRailMenuClosesItsSubmenusToo() {
        // opened by Alt with a submenu open: a second tap, or a click in the
        // window outside them, closes both
        if (!App.commands.menuBarHideable)
            skip("macOS keeps the system menu bar")
        App.settings.showMenuBar = false
        activate(win)
        const railMenu = item("rail-menu")
        tryVerify(() => railMenu.visible, 1000)
        waitForRendering(win.contentItem)
        const popup = findChild(railMenu, "rail-menu.popup")
        const exportMenu = popup.menuAt(3)

        keyPress(Qt.Key_Alt)
        keyRelease(Qt.Key_Alt)
        keyClick(Qt.Key_X)
        tryVerify(() => exportMenu.opened, 2000)
        keyPress(Qt.Key_Alt)
        keyRelease(Qt.Key_Alt)
        tryVerify(() => !exportMenu.visible && !popup.visible, 2000)

        keyPress(Qt.Key_Alt)
        keyRelease(Qt.Key_Alt)
        keyClick(Qt.Key_X)
        tryVerify(() => exportMenu.opened, 2000)
        mouseClick(item("entities.viewport"), 300, 400)
        tryVerify(() => !exportMenu.visible && !popup.visible, 2000)
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

    function test_theOutlineComesFirstThenTheWorkspace() {
        // a window of its own, looked at before its first frame: only the
        // outline (no text, which would wait for the font fallbacks), no
        // menu bar yet; the workspace is made once a frame is on screen
        const w = createTemporaryObject(mainComponent, tc)
        verify(!w.contentReady)
        verify(findChild(w, "shell.outline").visible)
        compare(w.menuBar, null)
        compare(findChild(w, "workspace"), null)
        tryVerify(() => w.contentReady, 5000)
        verify(!findChild(w, "shell.outline").visible)
        verify(w.menuBar !== null)
        verify(findChild(w, "workspace") !== null)
        // and a dialog only once it is opened
        for (const name of ["settings.dialog", "shell.newfile.dialog", "export.stl.dialog", "shell.message"])
            compare(findChild(w, name), null, name)
    }

    function test_messagesOpenOneAtATime() {
        // one file can raise two messages (an unfinished search, then its
        // comment): they open in turn, the second not over the first
        App.document.messageRequested("First", "one")
        App.document.messageRequested("Second", "two")
        tryVerify(() => findChild(win, "shell.message") !== null, 2000)
        const dialog = findChild(win, "shell.message")
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

    // --- the document's dialogs (C14 and the Edit / Help items) -----------

    function dialog(name) {
        const d = findChild(win, name)
        verify(d !== null, "no dialog " + name)
        return d
    }

    function test_newFileDialogMakesTheChosenType() {
        App.document.newDocument(0)                // unmodified: no question first
        App.commands.trigger("file.new")
        const d = dialog("shell.newfile.dialog")
        tryCompare(d, "opened", true)
        mouseClick(findChild(d.contentItem, "shell.newfile.type.spheres"))
        compare(d.selectedType, 2)
        mouseClick(findChild(d.footer, "shell.newfile.ok"))
        tryCompare(d, "visible", false)
        compare(App.document.gridTypeName, "Spheres")
        verify(!App.document.flowPending)

        // Cancel keeps the document, and the choice starts at Brick again
        App.commands.trigger("file.new")
        tryCompare(d, "opened", true)
        compare(d.selectedType, 0)
        mouseClick(findChild(d.contentItem, "shell.newfile.type.prism"))
        mouseClick(findChild(d.footer, "shell.newfile.cancel"))
        tryCompare(d, "visible", false)
        compare(App.document.gridTypeName, "Spheres")
        verify(!App.document.flowPending)
        App.document.newDocument(0)
    }

    function test_commentDialogWritesOnlyOnOk() {
        App.document.newDocument(0)
        App.commands.trigger("editcomment")
        const d = dialog("shell.comment.dialog")
        tryCompare(d, "opened", true)
        const text = findChild(d.contentItem, "shell.comment.text")
        compare(text.text, "")
        mouseClick(text)
        keyClick(Qt.Key_H)
        keyClick(Qt.Key_I)
        compare(text.text, "hi")
        mouseClick(findChild(d.footer, "shell.comment.ok"))
        tryCompare(d, "visible", false)
        compare(App.document.comment, "hi")
        verify(App.document.modified)

        // Cancel and Esc throw the edit away; the dialog opens on the saved text
        App.commands.trigger("editcomment")
        tryCompare(d, "opened", true)
        compare(text.text, "hi")
        text.text = "changed"
        mouseClick(findChild(d.footer, "shell.comment.cancel"))
        tryCompare(d, "visible", false)
        compare(App.document.comment, "hi")
        App.commands.trigger("editcomment")
        tryCompare(d, "opened", true)
        compare(text.text, "hi")
        text.text = "changed again"
        keyClick(Qt.Key_Escape)
        tryCompare(d, "visible", false)
        compare(App.document.comment, "hi")
        App.document.newDocument(0)
    }

    function test_aboutNamesTheVersion() {
        App.commands.trigger("about")
        const d = dialog("shell.about.dialog")
        tryCompare(d, "opened", true)
        const text = d.contentItem
        compare(text.objectName, "shell.about.text")
        verify(App.version.length > 0)
        verify(text.text.indexOf("BurrTools " + App.version) >= 0, text.text)
        mouseClick(findChild(d.footer, "shell.about.close"))
        tryCompare(d, "visible", false)
    }

    function test_theUnsavedChangesQuestionPassesItsAnswerOn() {
        App.document.newDocument(0)
        App.document.setComment("unsaved")

        // Cancel: the document stays
        App.commands.trigger("file.new")
        const d = dialog("shell.discard.dialog")
        tryCompare(d, "visible", true)
        verify(d.informativeText.indexOf("create a new puzzle") >= 0)
        d.buttonClicked(MessageDialog.Cancel, MessageDialog.RejectRole)
        d.close()
        tryCompare(d, "visible", false)
        verify(!App.document.flowPending)
        verify(App.document.modified)

        // closing it any other way is Cancel too
        App.commands.trigger("file.new")
        tryCompare(d, "visible", true)
        d.reject()
        tryCompare(d, "visible", false)
        verify(!App.document.flowPending)

        // Discard goes on to the voxel type
        App.commands.trigger("file.new")
        tryCompare(d, "visible", true)
        d.buttonClicked(MessageDialog.Discard, MessageDialog.DestructiveRole)
        d.close()
        const types = dialog("shell.newfile.dialog")
        tryCompare(types, "opened", true)
        mouseClick(findChild(types.footer, "shell.newfile.ok"))
        tryCompare(types, "visible", false)
        verify(!App.document.modified)
        compare(App.document.comment, "")
    }
}
