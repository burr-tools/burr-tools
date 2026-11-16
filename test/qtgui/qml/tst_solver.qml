import QtQuick
import QtQuick.Controls
import QtTest
import BurrTools.Ui

// The Solver workspace through the real Main.qml: the cards replace the
// placeholders, their fixed heights per density, the list's keys and its
// sticky focus, Manage's preview and confirmation, the piece chips
// (test plan, C19, C20, C21).
TestCase {
    id: tc
    name: "Solver"
    when: windowShown

    Component { id: mainComponent; Main {} }

    property var win

    function init() {
        failOnWarning(/^(?!No QRhi found for window|This plugin does not support grabbing the keyboard|QUnifiedTimer::stopAnimationDriver: driver is not running).*/)
        App.settings.density = "standard"
        App.layout.workspace = LayoutController.Entities
        App.layout.leftCollapsed = false
        App.layout.rightCollapsed = false
        App.document.newDocument(0)
        win = createTemporaryObject(mainComponent, tc)
        verify(win !== null, "Main.qml failed to load")
        win.width = 1600
        win.height = 1000
        waitForRendering(win.contentItem)
        tryVerify(() => win.contentReady, 5000)
        win.requestActivate()
        tryVerify(() => win.active, 3000)
        // Pelikan Burr: one stored solution, found with the disassembly
        App.document.openFile(Qt.resolvedUrl("../../../examples/PelikanBurr.xmpuzzle"))
        for (let i = 0; i < 5; i++) {
            wait(50)
            const m = findChild(win, "shell.message")
            if (m !== null && m.visible)
                m.close()
        }
        App.layout.workspace = LayoutController.Solver
        waitForRendering(win.contentItem)
        // the closed message leaves the window without a focus item
        win.requestActivate()
        tryVerify(() => win.active, 3000)
        win.contentItem.forceActiveFocus()
    }

    function cleanup() {
        // a staged open left behind by a failed test would keep every later
        // test's document busy: the settings back, and the reading ended
        App.document.resetLoadingForTesting()
        tryVerify(() => !App.document.loadingSolutions && !App.document.opening, 10000)
        App.settings.density = "standard"
        App.solver.view = "list"
        App.layout.workspace = LayoutController.Solver
        App.layout.leftCollapsed = false
        App.layout.rightCollapsed = false
        App.layout.workspace = LayoutController.Entities
    }

    function item(name) {
        let o = null
        tryVerify(() => (o = findChild(win.contentItem, name)) !== null, 2000, "no item " + name)
        return o
    }

    // the assemblies of a run without the disassembly check: twelve rows
    function solveAssemblies() {
        App.solver.disassemble = false
        App.solver.start()
        tryVerify(() => !App.solver.solving, 30000)
        compare(App.solver.shownCount, 12)
        waitForRendering(win.contentItem)
    }

    function test_theCardsReplaceThePlaceholders() {
        // AC-C19-01: the sections in their order
        verify(item("solver.left").visible)
        verify(item("solver.right").visible)
        compare(item("solver.right").title, "Solutions")
        const order = ["solver.problem", "solver.mode.asm", "solver.options", "solver.run", "solver.explorer"]
            .map((n) => item(n).mapToItem(item("solver.left"), 0, 0).y)
        for (let i = 1; i < order.length; i++)
            verify(order[i - 1] < order[i], "section " + i + " below the one before")
        verify(item("solver.mode.dis").selected, "the stored results were found with the disassembly")
        verify(item("solver.drop").visible)
        compare(item("solver.list").count, 1)
    }

    function test_searchOptionsAreCollapsedAndCountTheirChanges() {
        // AC-C19-03
        const options = item("solver.options")
        verify(!options.open)
        compare(options.summary, "")
        App.solver.keepMirror = true
        App.solver.limit = 40
        compare(options.summary, "· 2 changed")
        mouseClick(item("solver.options.header"))
        tryVerify(() => options.open)
        compare(options.summary, "")
        App.solver.resetOptions()
        App.solver.optionsOpen = false
    }

    function test_theFixedHeightsFollowTheDensity_data() {
        // density.md §1 (AC-C20-08)
        return [
            { tag: "standard", density: "standard", player: 112, pieces: 104, row: 28 },
            { tag: "minimal", density: "minimal", player: 84, pieces: 76, row: 26 },
        ]
    }
    function test_theFixedHeightsFollowTheDensity(data) {
        App.settings.density = data.density
        waitForRendering(win.contentItem)
        compare(item("solver.player").height, data.player)
        compare(item("solver.pieces").height, Theme.cardHeader + data.pieces)
        compare(item("solver.list.row.0").height, data.row)
        // the player keeps its height without a disassembly to play
        App.solver.deleteDisassembly(false)
        waitForRendering(win.contentItem)
        compare(item("solver.player").height, data.player)
        verify(item("solver.player.message").visible)
        compare(item("solver.list.footer").visible, data.density === "standard")
    }

    function test_theListTakesTheKeysAndKeepsThemOverTheView() {
        // AC-C20-07: a click on a row gives the list the keys; the 3D view's
        // toolbar and the player's transport do not take them away
        solveAssemblies()
        mouseClick(item("solver.list.row.0"))
        const list = item("solver.list")
        tryVerify(() => list.activeFocus)
        keyClick(Qt.Key_Down)
        compare(App.solver.selectedRow, 1)
        keyClick(Qt.Key_PageDown)
        compare(App.solver.selectedRow, 9)
        mouseClick(item("entities.viewport.toolbar.orbit"))
        mouseClick(item("entities.viewport.surface"))
        mouseClick(item("entities.viewport.cube"))
        verify(list.activeFocus, "the list still has the keys")
        keyClick(Qt.Key_Home)
        compare(App.solver.selectedRow, 0)
    }

    function test_deleteAsksAndRemoves() {
        // PR-32: not undoable, so it asks; Del on the list is Manage ▸ At
        solveAssemblies()
        mouseClick(item("solver.list.row.3"))
        keyClick(Qt.Key_Delete)
        let dialog = null
        tryVerify(() => (dialog = findChild(win, "solver.removeDialog")) !== null && dialog.opened, 2000)
        compare(dialog.title, "Delete 1 solution?")
        verify(App.commands.blocked)                     // counted as modal
        compare(App.solver.shownCount, 12)
        mouseClick(findChild(dialog.footer, "solver.removeDialog.confirm"))
        tryVerify(() => !dialog.visible, 1000)
        compare(App.solver.shownCount, 11)
        compare(App.solver.selectedRow, 3)
        // the keys go back to the list the Delete came from
        tryVerify(() => item("solver.list").activeFocus)
    }

    function test_manageItemsFollowTheResults() {
        // a Remove item counts the rows it would take, again after each run: the
        // stored solution alone has nothing before it
        App.solver.selectRow(0)
        mouseClick(item("solver.act.manage"))
        let menu = null
        tryVerify(() => (menu = findChild(win, "solver.manage")) !== null && menu.opened, 2000)
        verify(!findChild(menu.contentItem, "solver.manage.remove.1").enabled)
        menu.close()
        tryVerify(() => !menu.visible)
        solveAssemblies()
        App.solver.selectRow(9)
        mouseClick(item("solver.act.manage"))
        tryVerify(() => menu.opened, 2000)
        tryVerify(() => findChild(menu.contentItem, "solver.manage.remove.1").enabled)
        menu.close()
        tryVerify(() => !menu.visible)
    }

    function test_aRunButtonThatGreysOutHandsTheFocusOn() {
        // Start disables itself: the focus goes on to Stop, then to Continue or Start
        App.solver.disassemble = false
        const start = item("solver.run.start")
        mouseClick(start)
        tryVerify(() => !App.solver.solving, 30000)
        const run = ["solver.run.prep", "solver.run.start", "solver.run.cont", "solver.run.stop"].map((n) => item(n))
        tryVerify(() => run.some((b) => b.activeFocus && b.enabled), 2000, "a Run button keeps the keys")
    }

    function test_hoveringARemoveItemPreviewsItsRows() {
        // AC-C20-05: the rows a Remove item would take are tinted
        solveAssemblies()
        App.solver.selectRow(9)
        mouseClick(item("solver.act.manage"))
        let menu = null
        tryVerify(() => (menu = findChild(win, "solver.manage")) !== null && menu.opened, 2000)
        const after = findChild(menu.contentItem, "solver.manage.remove.2")
        mouseMove(after, after.width / 2, after.height / 2)
        tryCompare(App.solver, "previewRemove", 2)
        tryVerify(() => App.solver.solutions.value(10, "willDelete") && App.solver.solutions.value(11, "willDelete"))
        verify(!App.solver.solutions.value(9, "willDelete"))
        keyClick(Qt.Key_Escape)
        tryVerify(() => !menu.visible)
        compare(App.solver.previewRemove, -1)
    }

    function test_aChipCyclesItsStateByClick() {
        // AC-C20-06: the state in the look, a click cycles it
        const chip = item("solver.pieces.chip." + App.solver.pieces.value(0, "pid"))
        compare(chip.pieceState, 0)
        mouseClick(chip)
        tryCompare(chip, "pieceState", 1)
        verify(chip.toolTipText.indexOf("wireframe") >= 0)
        mouseClick(chip)
        tryCompare(chip, "pieceState", 2)
        // hidden: the chip at 42 % opacity, in grey (PR-30)
        tryCompare(chip.contentItem, "opacity", 0.42)
        compare((chip.contentItem.children[1] as Text).color, Theme.muted)
        mouseClick(item("solver.showAll"))
        tryCompare(chip, "pieceState", 0)
    }

    function test_theSliderViewScrubsWithoutANewSlider() {
        // AC-C20-02
        solveAssemblies()
        App.solver.view = "slider"
        waitForRendering(win.contentItem)
        const slider = item("solver.slider")
        verify(slider.visible)
        verify(!item("solver.list").visible)
        slider.value = 6
        slider.moved()
        compare(App.solver.selectedRow, 5)
        compare(item("solver.slider"), slider)
        verify(item("solver.sort").visible)
    }

    function test_theReadingBarTakesTheRowWhileSolutionsAreRead() {
        // a staged open (any file stages here), its end not looked at: the
        // bar stays up, as wide as the words leave it, not a pixel
        App.document.setStageAboveForTesting(0)
        App.document.setLoadPollIntervalForTesting(3600 * 1000)
        App.document.openFile(Qt.resolvedUrl("../../../examples/SolidSixPieceBurrs.xmpuzzle"))
        for (let i = 0; i < 5; i++) {
            wait(50)
            const m = findChild(win, "shell.message")
            if (m !== null && m.visible)
                m.close()
        }
        verify(App.document.loadingSolutions)
        const row = item("solver.loading")
        const bar = item("solver.loading.bar")
        const words = item("solver.loading.text")
        tryVerify(() => row.visible, 2000)
        waitForRendering(win.contentItem)
        const pct = item("solver.loading.pct")
        // the words and the percentage at their widths, the bar all the rest
        fuzzyCompare(words.width, words.implicitWidth, 1)       // laid out to whole pixels
        fuzzyCompare(bar.width, row.width - words.width - pct.width - 2 * row.spacing, 1)
        verify(bar.width >= 60, "the bar " + bar.width + " of " + row.width)
        verify(item("solver.loading.fill").width <= bar.width)
        // the reading's end: the row goes (cleanup() puts the settings back)
        App.document.setLoadPollIntervalForTesting(100)
        tryVerify(() => !App.document.loadingSolutions, 10000)
        tryVerify(() => !row.visible, 2000)
    }

    function test_theViewIsReadOnly() {
        // AC-C21-04: no Edit button, the tag, the Display menu's wireframe item
        verify(!item("entities.viewport.toolbar.edit").visible)
        verify(item("solver.viewport.tag").visible)
        mouseClick(item("entities.viewport.toolbar.display"))
        let xray = null
        tryVerify(() => (xray = findChild(win, "solver.viewport.display.xray")) !== null && xray.visible, 2000)
        verify(xray.selected, "Show through solid pieces is on by default")
        verify(!findChild(win, "entities.viewport.display.axes").visible)
        keyClick(Qt.Key_Escape)
    }
}
