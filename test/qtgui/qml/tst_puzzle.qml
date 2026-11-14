import QtQuick
import QtQuick.Controls
import QtTest
import BurrTools.Ui

// The Puzzle workspace through the real Main.qml: the cards, the rails, the
// labelled colour matrix, the keys and the density (test plan T-C16, T-C17).
TestCase {
    id: tc
    name: "Puzzle"
    when: windowShown

    Component { id: mainComponent; Main {} }

    property var win

    function init() {
        failOnWarning(/^(?!No QRhi found for window|This plugin does not support grabbing the keyboard).*/)
        App.settings.density = "standard"
        App.document.solutionsWarned = false
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
        for (let i = 0; i < 4; i++)
            App.shapes.addShape()
        App.layout.workspace = LayoutController.Puzzle
        App.puzzle.newProblem()
        App.puzzle.selectPiece(-1)
        waitForRendering(win.contentItem)
    }

    function cleanup() {
        App.settings.density = "standard"
        App.puzzle.piecesView = "chips"
        // collapsing is per workspace, and the layout is the App's
        App.layout.workspace = LayoutController.Puzzle
        App.layout.leftCollapsed = false
        App.layout.rightCollapsed = false
        App.layout.workspace = LayoutController.Entities
    }

    function item(name) {
        let o = null
        tryVerify(() => (o = findChild(win.contentItem, name)) !== null, 2000, "no item " + name)
        return o
    }
    function addColours() {
        for (let i = 0; i < 3; i++)
            App.colours.addColour()
        waitForRendering(win.contentItem)
    }
    function textOf(name) { return item(name).text }

    function test_theCardsReplaceThePlaceholders() {
        verify(item("puzzle.left").visible)
        verify(item("puzzle.right").visible)
        compare(item("puzzle.right").title, "Piece & rules")
        compare(item("puzzle.section.groups.header").Accessible.name, "Groups")
    }

    // the left rail still lists the problems
    function test_theLeftRailListsTheProblems() {
        App.puzzle.newProblem()
        App.layout.leftCollapsed = true
        waitForRendering(win.contentItem)
        verify(item("puzzle.problems.rail").visible)
        const chip = item("puzzle.problems.rail.chip.1")
        compare(chip.text, "P2")
        mouseClick(item("puzzle.problems.rail.row.0"))
        compare(App.puzzle.problem, 0)
        mouseClick(item("puzzle.problems.rail.new"))
        compare(App.puzzle.problems.count, 3)
        compare(App.puzzle.problem, 2)
    }

    // the right rail names every section, and the piece
    function test_theRightRailNamesEverySection() {
        App.layout.rightCollapsed = true
        waitForRendering(win.contentItem)
        verify(item("puzzle.sections.rail").visible)
        compare(item("puzzle.sections.rail.title.piece").text, "Selected piece")
        compare(item("puzzle.sections.rail.title.groups").text, "Groups")
        compare(item("puzzle.sections.rail.title.rules").text, "Colour rules")
        const rows = ["piece", "groups", "rules"].map((k) => item("puzzle.sections.rail.row." + k))
        verify(rows[0].y < rows[1].y && rows[1].y < rows[2].y, "in the card's order")

        App.puzzle.stepPiece(1, 1)
        App.puzzle.selectPiece(1)
        App.layout.rightCollapsed = true
        waitForRendering(win.contentItem)
        compare(App.puzzle.sections.value(0, "badge"), "S2")

        mouseClick(item("puzzle.sections.rail.row.rules"))
        tryVerify(() => !App.layout.rightCollapsed)
        verify(item("puzzle.section.rules").visible)
    }

    function test_theRailsReachTheKeyboard() {
        App.layout.rightCollapsed = true
        waitForRendering(win.contentItem)
        const list = item("puzzle.sections.rail.list")
        verify(list.activeFocusOnTab)
        list.forceActiveFocus(Qt.TabFocusReason)
        keyClick(Qt.Key_Down)
        compare(list.currentIndex, 1)
        keyClick(Qt.Key_Return)
        tryVerify(() => !App.layout.rightCollapsed)
    }

    // both axes say what they are
    function test_theMatrixNamesItsAxes() {
        addColours()
        compare(textOf("puzzle.rules.axis.result"), "Result colour")
        compare(textOf("puzzle.rules.axis.piece"), "Piece colour")
        verify(item("puzzle.rules.help").text.indexOf("piece colour") >= 0)
        verify(item("puzzle.rules.info").tip.indexOf("row") >= 0)
        verify(item("puzzle.rules.default").text.indexOf("Default") >= 0)
        // no row or column for Default: the library lets it go anywhere
        compare(findChild(win.contentItem, "puzzle.rules.piece.Default"), null)
        compare(findChild(win.contentItem, "puzzle.rules.result.Default"), null)
        // the headers carry the names, not the colours alone
        verify(item("puzzle.rules.piece.C1").visible)
        verify(item("puzzle.rules.result.C3").visible)
    }

    function test_theMatrixSaysWhatACellMeans() {
        addColours()
        const cell = item("puzzle.rules.cell.C2.C3")
        compare(cell.on, false)
        mouseMove(cell)
        tryCompare(item("puzzle.rules.cellText"), "text", "Piece C2 on result C3: not allowed")
        mouseClick(cell)
        tryVerify(() => cell.on)
        tryCompare(item("puzzle.rules.cellText"), "text", "Piece C2 on result C3: allowed")
        compare(cell.Accessible.name, "Piece C2 on result C3")
    }

    function test_theMatrixWorksFromTheKeyboard() {
        addColours()
        const grid = item("puzzle.rules.grid")
        verify(grid.activeFocusOnTab)
        grid.forceActiveFocus(Qt.TabFocusReason)
        keyClick(Qt.Key_Right)
        keyClick(Qt.Key_Space)
        tryVerify(() => item("puzzle.rules.cell.C1.C2").on)
    }

    // as the 2D grid's (TestEditor::theKeysAndThePointerTakeTheHighlightInTurn): with the
    // pointer resting on a cell, the arrows' cell is the one shown -- the readout, the
    // headers, Space -- until the pointer moves; the arrows then go on from the pointer's
    function test_theKeysAndThePointerTakeTheMatrixHighlightInTurn() {
        addColours()
        const grid = item("puzzle.rules.grid")
        const text = item("puzzle.rules.cellText")
        const cell = name => item("puzzle.rules.cell." + name)
        const shows = name => text.text.indexOf("Piece " + name.replace(".", " on result ") + ":") === 0

        // a click focuses the matrix; then the resting pointer has the highlight
        mouseClick(cell("C1.C1"))
        verify(grid.activeFocus)
        mouseMove(cell("C3.C3"))
        tryVerify(() => shows("C3.C3"), 1000, text.text)

        // the arrows go on from the pointer's cell, and take the highlight
        keyClick(Qt.Key_Left)
        verify(shows("C3.C2"), text.text)
        verify(cell("C3.C2").cursor)
        verify(!cell("C3.C3").pointerShown)       // no hover mark beside the keyboard's
        compare(item("puzzle.rules.piece.C3").border.width, 2)
        compare(item("puzzle.rules.result.C2").border.width, 2)
        compare(item("puzzle.rules.result.C3").border.width, 1)
        mouseMove(cell("C3.C3"))                  // where the pointer rests: not a move
        verify(shows("C3.C2"), text.text)
        const on32 = cell("C3.C2").on, on33 = cell("C3.C3").on
        keyClick(Qt.Key_Space)                    // applied where it is shown
        compare(cell("C3.C2").on, !on32)
        compare(cell("C3.C3").on, on33)

        // the pointer moves: the highlight is its again
        mouseMove(cell("C3.C3"), cell("C3.C3").width / 2 + 3, cell("C3.C3").height / 2)
        tryVerify(() => shows("C3.C3"), 1000, text.text)
        verify(!cell("C3.C2").cursor)
        verify(cell("C3.C3").pointerShown)

        // focus away while the keys hold it: the highlight back under the pointer
        keyClick(Qt.Key_Up)
        verify(shows("C2.C3"), text.text)
        keyClick(Qt.Key_Tab)
        verify(!grid.activeFocus)
        verify(shows("C3.C3"), text.text)

        // Tab in again: the keys' cell is shown, wherever the pointer rests
        grid.forceActiveFocus(Qt.TabFocusReason)
        verify(shows("C2.C3"), text.text)
        verify(cell("C2.C3").cursor)
        verify(!cell("C3.C3").pointerShown)
    }

    function test_noGroupsSaysSo() {
        compare(textOf("puzzle.groups.empty"), "No groups in this problem")
        verify(item("puzzle.groups.empty").visible)
    }

    function test_aPieceJoinsSeveralGroups() {
        App.puzzle.stepPiece(1, 1)
        App.puzzle.selectPiece(1)
        waitForRendering(win.contentItem)
        mouseClick(item("puzzle.piece.group.new"))
        mouseClick(item("puzzle.piece.group.new"))
        tryCompare(App.puzzle.groups, "count", 2)
        verify(item("puzzle.groups.card.2").visible)
        compare(findChild(win.contentItem, "puzzle.groups.empty").visible, false)
    }

    function test_tilesStepAndLeave() {
        App.puzzle.stepPiece(1, 1)
        waitForRendering(win.contentItem)
        const tile = item("puzzle.used.tile.1")
        mouseClick(tile.plusButton)
        compare(App.puzzle.pieces.value(0, "maxCount"), 2)
        compare(tile.width, Theme.puzzleTileWidth)
        mouseClick(tile.minusButton)
        mouseClick(tile.minusButton)
        tryCompare(App.puzzle.pieces, "count", 0)
        verify(item("puzzle.used.empty").visible)
    }

    function test_aLibraryChipAdds() {
        waitForRendering(win.contentItem)
        const chip = item("puzzle.library.chip.2")
        mouseClick(chip)
        compare(App.puzzle.selectedPiece, 2)
        compare(App.puzzle.pieces.count, 0)
        mouseClick(chip.addButton)
        compare(App.puzzle.pieces.count, 1)
    }

    function test_theTilesKeys() {
        App.puzzle.stepPiece(1, 1)
        App.puzzle.stepPiece(2, 1)
        App.puzzle.selectPiece(1)
        waitForRendering(win.contentItem)
        mouseClick(item("puzzle.used.tile.1"), 40, 8)
        const tiles = item("puzzle.used.tiles")
        verify(tiles.activeFocus, "focus is on " + win.activeFocusItem)
        keyClick(Qt.Key_Right)
        compare(App.puzzle.selectedPiece, 2)
        keyClick(Qt.Key_Equal)
        compare(App.puzzle.pieceMax, 2)
        keyClick(Qt.Key_Minus)
        compare(App.puzzle.pieceMax, 1)
        keyClick(Qt.Key_Delete)
        compare(App.puzzle.pieces.count, 1)
    }

    function test_theTableEditsTheSameData() {
        // AC-C16-05
        App.puzzle.stepPiece(1, 2)
        App.puzzle.piecesView = "table"
        waitForRendering(win.contentItem)
        verify(item("puzzle.used.table").visible)
        verify(!item("puzzle.used.tiles").visible)
        const field = item("puzzle.used.row.1.min")
        compare(field.text, "2")
        typeIn(field, [Qt.Key_5])
        keyClick(Qt.Key_Return)
        compare(App.puzzle.pieces.value(0, "maxCount"), 5)
        mouseClick(item("puzzle.used.row.1.range"))
        compare(App.puzzle.pieces.value(0, "minCount"), 0)
        verify(item("puzzle.used.row.1.max").visible)
    }

    function test_theCardFollowsTheDensity_data() {
        // density.md section 1, the Puzzle rows
        return [
            { tag: "standard", density: "standard", left: 320, right: 340, tile: [96, 44], chip: 26, row: 34 },
            { tag: "minimal", density: "minimal", left: 264, right: 320, tile: [78, 40], chip: 24, row: 30 },
        ]
    }
    function test_theCardFollowsTheDensity(data) {
        App.puzzle.stepPiece(1, 1)
        App.puzzle.stepPiece(2, 1)
        App.puzzle.stepPiece(3, 1)
        App.settings.density = data.density
        tryCompare(item("puzzle.left"), "width", data.left, 1000)
        tryCompare(item("puzzle.right"), "width", data.right, 1000)
        waitForRendering(win.contentItem)
        const tile = item("puzzle.used.tile.1")
        compare(tile.width, data.tile[0])
        compare(tile.height, data.tile[1])
        compare(item("puzzle.library.chip.1").height, data.chip)
        compare(item("puzzle.problems.row.0").height, data.row)
        // AC-DEN-02: three tiles a row
        compare(item("puzzle.used.tile.2").y, tile.y)
        compare(item("puzzle.used.tile.3").y, tile.y)
        // AC-DEN-03: help text hidden in Minimal stays in a tooltip
        addColours()
        const minimal = data.density === "minimal"
        compare(item("puzzle.rules.help").visible, !minimal)
        compare(item("puzzle.rules.default").visible, !minimal)
        verify(item("puzzle.rules.axis.result").visible, "the axes stay in both")
        verify(item("puzzle.rules.info").tip.length > 0)
    }

    function solved() {
        App.document.openFile(Qt.resolvedUrl("../../../examples/PelikanBurr.xmpuzzle"))
        for (let i = 0; i < 5; i++) {
            wait(50)
            const m = findChild(win, "shell.message")
            if (m !== null && m.visible)
                m.close()
        }
        App.document.solutionsWarned = false
    }
    function warning() {
        let dialog = null
        tryVerify(() => (dialog = findChild(win, "shell.solutionsDialog")) !== null && dialog.opened, 2000)
        return dialog
    }

    // the button itself, not the controller's answer: the first version lost it here
    function test_continueInTheWarningMakesTheChange() {
        solved()
        const shape = App.puzzle.pieces.value(0, "shape")
        const before = App.puzzle.pieces.value(0, "maxCount")
        App.puzzle.stepPiece(shape, 1)
        const dialog = warning()
        verify(App.commands.blocked)                     // counted as modal
        verify(dialog.text.indexOf("once a session") >= 0, dialog.text)
        verify(dialog.text.indexOf("without asking") >= 0, dialog.text)
        compare(App.puzzle.pieces.value(0, "maxCount"), before)
        mouseClick(findChild(dialog.footer, "shell.solutionsDialog.confirm"))
        tryVerify(() => !dialog.visible, 1000)
        compare(App.puzzle.pieces.value(0, "maxCount"), before + 1)
        compare(App.puzzle.problems.value(0, "solutions"), 0)
        verify(App.document.solutionsWarned)

        // and no second time
        App.puzzle.stepPiece(shape, 1)
        wait(50)
        verify(!dialog.visible)
        compare(App.puzzle.pieces.value(0, "maxCount"), before + 2)
    }

    function test_cancelInTheWarningChangesNothing() {
        solved()
        const before = App.puzzle.pieces.value(0, "maxCount")
        const solutions = App.puzzle.problems.value(0, "solutions")
        App.puzzle.stepPiece(App.puzzle.pieces.value(0, "shape"), 1)
        const dialog = warning()
        mouseClick(findChild(dialog.footer, "shell.solutionsDialog.cancel"))
        tryVerify(() => !dialog.visible, 1000)
        compare(App.puzzle.pieces.value(0, "maxCount"), before)
        compare(App.puzzle.problems.value(0, "solutions"), solutions)
        verify(!App.document.solutionsWarned)
    }

    function test_thePickerClosesForTheWarningAndKeepsTheTruthOnCancel() {
        solved()
        App.puzzle.piecesView = "table"
        waitForRendering(win.contentItem)
        const shape = App.puzzle.pieces.value(0, "shape")
        const box = item("puzzle.used.row." + shape + ".group")
        const shown = box.groupText
        mouseClick(box)
        tryVerify(() => box.list.opened, 2000)
        waitForRendering(win.contentItem)
        mouseClick(box.newRow)
        const dialog = warning()
        verify(!box.list.visible, "the list gives way to the warning")
        mouseClick(findChild(dialog.footer, "shell.solutionsDialog.cancel"))
        tryVerify(() => !dialog.visible, 1000)
        compare(box.groupText, shown)
    }

    // press on `from`, move to `to`, drop
    function drag(from, to, atX, atY) {
        const a = from.mapToItem(win.contentItem, atX, atY)
        const b = to.mapToItem(win.contentItem, to.width / 2, to.height / 2)
        mousePress(win.contentItem, a.x, a.y)
        for (let i = 1; i <= 8; i++)
            mouseMove(win.contentItem, a.x + (b.x - a.x) * i / 8, a.y + (b.y - a.y) * i / 8)
    }

    function test_aChipDraggedInAddsAndATileDraggedOutRemoves() {
        // AC-C16-04
        waitForRendering(win.contentItem)
        const chip = item("puzzle.library.chip.2")
        const used = item("puzzle.used")
        drag(chip, used, 10, chip.height / 2)
        verify(item("puzzle.used.dropZone").visible, "the zone is shown while dragging")
        verify(item("puzzle.dragGhost").visible)
        const end = used.mapToItem(win.contentItem, used.width / 2, used.height / 2)
        mouseRelease(win.contentItem, end.x, end.y)
        tryCompare(App.puzzle.pieces, "count", 1)
        verify(!item("puzzle.used.dropZone").visible)

        waitForRendering(win.contentItem)
        const tile = item("puzzle.used.tile.2")
        const library = item("puzzle.library")
        drag(tile, library, 20, 8)
        verify(item("puzzle.library.dropZone").visible)
        const back = library.mapToItem(win.contentItem, library.width / 2, library.height / 2)
        mouseRelease(win.contentItem, back.x, back.y)
        tryCompare(App.puzzle.pieces, "count", 0)
    }

    function test_aDragDroppedElsewhereChangesNothing() {
        waitForRendering(win.contentItem)
        const chip = item("puzzle.library.chip.2")
        const summary = item("puzzle.summary")
        drag(chip, summary, 10, chip.height / 2)
        const end = summary.mapToItem(win.contentItem, summary.width / 2, summary.height / 2)
        mouseRelease(win.contentItem, end.x, end.y)
        wait(50)
        compare(App.puzzle.pieces.count, 0)
    }

    function test_theGripReordersTheProblems() {
        // AC-C16-01: ids renumber, the row follows
        App.puzzle.renameProblem(0, "first")
        App.puzzle.newProblem()
        App.puzzle.renameProblem(1, "second")
        App.puzzle.selectProblem(0)
        waitForRendering(win.contentItem)
        const grip = item("puzzle.problems.row.0.grip")
        const target = item("puzzle.problems.row.1")
        const a = grip.mapToItem(win.contentItem, grip.width / 2, grip.height / 2)
        const b = target.mapToItem(win.contentItem, 10, target.height - 2)
        mousePress(win.contentItem, a.x, a.y)
        for (let i = 1; i <= 8; i++)
            mouseMove(win.contentItem, a.x, a.y + (b.y - a.y + 6) * i / 8)
        mouseRelease(win.contentItem, a.x, b.y + 6)
        tryVerify(() => App.puzzle.problems.value(0, "label") === "second", 1000)
        compare(App.puzzle.problem, 1)
    }

    function test_theListsKeys() {
        App.puzzle.renameProblem(0, "first")
        App.puzzle.newProblem()
        App.puzzle.selectProblem(0)
        waitForRendering(win.contentItem)
        const list = item("puzzle.problems.list")
        mouseClick(item("puzzle.problems.row.0"), 60, 10)
        verify(list.activeFocus, "focus is on " + win.activeFocusItem)
        keyClick(Qt.Key_Down)
        compare(App.puzzle.problem, 1)
        keyClick(Qt.Key_Up, Qt.AltModifier)
        compare(App.puzzle.problems.value(1, "label"), "first")
        keyClick(Qt.Key_D, Qt.ControlModifier)
        compare(App.puzzle.problems.count, 3)
        keyClick(Qt.Key_Delete)
        compare(App.puzzle.problems.count, 2)
    }

    function test_enterRenamesInPlace() {
        waitForRendering(win.contentItem)
        mouseClick(item("puzzle.problems.row.0"), 60, 10)
        keyClick(Qt.Key_Return)
        const field = item("puzzle.problems.row.0.labelField")
        tryVerify(() => field.activeFocus, 1000)
        keyClick(Qt.Key_A)
        keyClick(Qt.Key_Return)
        tryVerify(() => App.puzzle.problems.value(0, "label").toLowerCase() === "a", 1000)
    }

    function test_setRangeForAllAppliesFromItsDialog() {
        // AC-C16-08
        App.puzzle.stepPiece(1, 1)
        App.puzzle.stepPiece(2, 1)
        waitForRendering(win.contentItem)
        item("puzzle.left").setRangeRequested()
        let dialog = null
        tryVerify(() => (dialog = findChild(win, "puzzle.setRange.dialog")) !== null && dialog.opened, 2000)
        verify(App.commands.blocked)
        mouseClick(findChild(dialog.footer, "puzzle.setRange.apply"))
        tryVerify(() => !dialog.visible, 1000)
        compare(App.puzzle.pieces.value(0, "countText"), "0–1")
        compare(App.puzzle.pieces.value(1, "countText"), "0–1")
    }

    function test_theMenusAct() {
        App.puzzle.stepPiece(1, 1)
        waitForRendering(win.contentItem)
        const card = item("puzzle.left")
        const tile = item("puzzle.used.tile.1")
        mouseClick(tile, 40, 8, Qt.RightButton)
        tryVerify(() => card.pieceContextMenu.opened, 2000)
        compare(App.puzzle.selectedPiece, 1)
        card.pieceContextMenu.close()
        tryVerify(() => !card.pieceContextMenu.visible, 1000)

        mouseClick(item("puzzle.used.more"))
        tryVerify(() => card.moreContextMenu.opened, 2000)
        card.moreContextMenu.close()
        tryVerify(() => !card.moreContextMenu.visible, 1000)

        mouseClick(item("puzzle.goal.result.change"))
        tryVerify(() => card.resultContextMenu.opened, 2000)
        compare(card.resultContextMenu.count, App.shapes.count)
        card.resultContextMenu.close()
        tryVerify(() => !card.resultContextMenu.visible, 1000)
    }

    function test_aFileWithoutProblemsOffersNew() {
        App.document.newDocument(0)
        waitForRendering(win.contentItem)
        compare(App.puzzle.problem, -1)
        verify(item("puzzle.problems.empty").visible)
        verify(!item("puzzle.result").visible)
        compare(item("entities.viewport.empty").text, "No problem yet — click + New in Problems")
        mouseClick(item("puzzle.problems.new"))
        compare(App.puzzle.problem, 0)
    }

    // --- the chips of All pieces -----------------------------------------------

    function test_aChipsMinusTakesOneAway() {
        App.puzzle.stepPiece(2, 2)
        waitForRendering(win.contentItem)
        const chip = item("puzzle.library.chip.2")
        compare(chip.countText, "×2")
        verify(chip.reduceButton.enabled)
        mouseClick(chip.reduceButton)
        compare(App.puzzle.pieces.value(0, "maxCount"), 1)
        tryCompare(chip, "countText", "×1")
        mouseClick(chip.reduceButton)
        compare(App.puzzle.pieces.count, 0)
        tryCompare(chip, "countText", "")
        verify(!chip.reduceButton.enabled, "nothing to take away")
    }

    function test_chipsKeepTheirWidthWhenAPieceIsAdded() {
        waitForRendering(win.contentItem)
        const chip = item("puzzle.library.chip.2")
        const before = chip.width
        mouseClick(chip.addButton)
        waitForRendering(win.contentItem)
        compare(chip.width, before)
    }

    // the problem's own count, as its tile reads it, whichever problem is selected
    function test_allPiecesFollowsTheSelectedProblem() {
        App.puzzle.stepPiece(1, 2)
        App.puzzle.newProblem()
        App.puzzle.setRange(2, 0, 3)
        waitForRendering(win.contentItem)
        const one = item("puzzle.library.chip.1")
        const two = item("puzzle.library.chip.2")
        compare(one.countText, "")
        compare(two.countText, "0–3")
        verify(!one.used)
        App.puzzle.selectProblem(0)
        tryCompare(one, "countText", "×2")
        tryCompare(two, "countText", "")
        verify(one.used)
        verify(!two.used)
        App.puzzle.selectProblem(1)
        tryCompare(two, "countText", "0–3")
        // a change from anywhere shows at once
        App.puzzle.setCount(2, 5)
        tryCompare(two, "countText", "×5")
    }

    // --- the table ---------------------------------------------------------------

    function test_theTablesChipIsAsWideAsItsText() {
        App.puzzle.stepPiece(2, 1)
        App.shapes.renameShape(1, "A rather long shape name indeed")
        App.puzzle.stepPiece(1, 1)
        App.puzzle.piecesView = "table"
        waitForRendering(win.contentItem)
        const bare = item("puzzle.used.row.2.chip")
        const named = item("puzzle.used.row.1.chip")
        verify(bare.width < 45, "no label: " + bare.width)
        verify(named.width > bare.width * 1.5, "a label needs room")
        verify(named.width <= named.parent.width, "and stops at the column")
    }

    function picker(shape) { return item("puzzle.used.row." + shape + ".group") }
    function openPicker(box) {
        mouseClick(box)
        tryVerify(() => box.list.opened, 2000)
        waitForRendering(win.contentItem)
    }
    function groupRow(box, group) {
        return findChild(box.list.contentItem, box.objectName + ".group." + group)
    }

    function test_theGroupPickerTicksSeveralGroups() {
        App.puzzle.stepPiece(1, 1)
        App.puzzle.piecesView = "table"
        waitForRendering(win.contentItem)
        const box = picker(1)
        compare(item("puzzle.used.row.1.group.text").text, "–")

        openPicker(box)
        mouseClick(box.newRow)
        tryCompare(App.puzzle.groups, "count", 1)
        verify(box.list.opened, "it stays open for the next tick")
        waitForRendering(win.contentItem)
        mouseClick(box.newRow)
        tryCompare(App.puzzle.groups, "count", 2)
        tryCompare(box, "groupText", "G1, G2")
        tryVerify(() => groupRow(box, 1) !== null && groupRow(box, 1).selected && groupRow(box, 2).selected)
        waitForRendering(win.contentItem)                // the rows laid out

        mouseClick(groupRow(box, 1))
        tryCompare(box, "groupText", "G2")
        // its last member gone, the group is too: the row stays to be ticked again
        tryVerify(() => groupRow(box, 1) !== null && !groupRow(box, 1).selected)
        verify(groupRow(box, 2).selected)
        waitForRendering(win.contentItem)
        mouseClick(groupRow(box, 1))
        tryCompare(box, "groupText", "G1, G2")
        mouseClick(groupRow(box, 1))
        tryCompare(box, "groupText", "G2")
        box.list.close()
        tryVerify(() => !box.list.visible, 1000)
        compare(item("puzzle.used.row.1.group.text").text, "G2")
        openPicker(box)
        compare(groupRow(box, 1), null)                  // gone with the list closed
        box.list.close()
        tryVerify(() => !box.list.visible, 1000)
    }

    // a piece in several groups by way of the card reads the same: no made-up entry
    function test_groupsFromTheCardShowInThePicker() {
        App.puzzle.stepPiece(1, 1)
        App.puzzle.selectPiece(1)
        App.puzzle.newGroup(1)
        App.puzzle.newGroup(1)
        App.puzzle.piecesView = "table"
        waitForRendering(win.contentItem)
        const box = picker(1)
        tryCompare(box, "groupText", "G1, G2")
        openPicker(box)
        verify(groupRow(box, 1).selected)
        verify(groupRow(box, 2).selected)
        const rows = box.list.contentItem.children
        for (let i = 0; i < rows.length; i++)
            verify(rows[i].text !== "Several", "no Several entry")
        box.list.close()
        tryVerify(() => !box.list.visible, 1000)
    }

    function test_aLongGroupListElidesInTheBox() {
        App.puzzle.stepPiece(1, 1)
        for (let i = 0; i < 8; i++)
            App.puzzle.newGroup(1)
        App.puzzle.piecesView = "table"
        waitForRendering(win.contentItem)
        const text = item("puzzle.used.row.1.group.text")
        verify(text.truncated, "elided to the box: " + text.text)
        compare(picker(1).width, 66)
    }

    function test_theGroupPickerWorksFromTheKeyboard() {
        App.puzzle.stepPiece(1, 1)
        App.puzzle.newGroup(1)
        App.puzzle.setGroupCount(1, 1, 0)
        App.puzzle.newGroup(1)
        App.puzzle.piecesView = "table"
        waitForRendering(win.contentItem)
        const box = picker(1)
        openPicker(box)
        keyClick(Qt.Key_Space)
        tryVerify(() => App.puzzle.pieceGroupCount(1, 1) === 0)
        keyClick(Qt.Key_Escape)
        tryVerify(() => !box.list.visible, 1000)
    }

    // --- counts ----------------------------------------------------------------------

    function test_theTablesFieldFollowsAnyChangeAtOnce() {
        App.puzzle.stepPiece(1, 2)
        App.puzzle.piecesView = "table"
        waitForRendering(win.contentItem)
        const field = item("puzzle.used.row.1.min")
        mouseClick(field)                    // once typed in, as the old field lost its binding
        field.selectAll()
        keyClick(Qt.Key_4)
        keyClick(Qt.Key_Return)
        tryCompare(App.puzzle.pieces, "revision", App.puzzle.pieces.revision, 2000)
        compare(field.text, "4")
        App.puzzle.stepPiece(1, 1)
        tryCompare(field, "text", "5")
    }

    function test_aTypedCountGoesThroughWithoutEnter() {
        App.puzzle.stepPiece(1, 1)
        App.puzzle.piecesView = "table"
        waitForRendering(win.contentItem)
        const field = item("puzzle.used.row.1.min")
        mouseClick(field)
        field.selectAll()
        keyClick(Qt.Key_7)
        tryCompare(App.puzzle.pieces, "revision", App.puzzle.pieces.revision + 1, 2000)
        compare(App.puzzle.pieces.value(0, "maxCount"), 7)
        compare(item("puzzle.library.chip.1").countText, "×7")
    }

    function test_theCardsStepperFollowsAnyChangeAtOnce() {
        App.puzzle.stepPiece(1, 1)
        App.puzzle.selectPiece(1)
        waitForRendering(win.contentItem)
        const stepper = item("puzzle.piece.count")
        mouseClick(stepper.field)
        verify(stepper.field.activeFocus)
        mouseClick(stepper.incButton)
        tryCompare(stepper.field, "text", "2")      // focused or not
        App.puzzle.stepPiece(1, 1)
        tryCompare(stepper.field, "text", "3")
    }

    function test_aCountTypedInTheCardGoesThroughWithoutEnter() {
        App.puzzle.stepPiece(1, 1)
        App.puzzle.selectPiece(1)
        waitForRendering(win.contentItem)
        const stepper = item("puzzle.piece.count")
        mouseClick(stepper.field)
        stepper.field.selectAll()
        keyClick(Qt.Key_6)
        tryVerify(() => App.puzzle.pieceMax === 6, 2000)
        tryCompare(item("puzzle.library.chip.1"), "countText", "×6")
    }

    // --- typing a number ---------------------------------------------------------------

    function typeIn(field, keys) {
        mouseClick(field)
        tryVerify(() => field.activeFocus, 1000)
        field.selectAll()
        for (let i = 0; i < keys.length; i++)
            keyClick(keys[i])
    }
    function pause() { wait(450) }            // past the field's 350 ms

    // the number stays with the piece it was typed for, whatever is clicked next
    function test_aTypedGroupCountLandsOnItsPiece() {
        App.puzzle.stepPiece(0, 1)
        App.puzzle.setResult(4)
        App.puzzle.stepPiece(1, 3)
        App.puzzle.stepPiece(2, 3)
        App.puzzle.newGroup(1)
        App.puzzle.setGroupCount(1, 1, 1)
        App.puzzle.setGroupCount(2, 1, 1)
        App.puzzle.selectPiece(1)
        waitForRendering(win.contentItem)
        const stepper = item("puzzle.piece.group.1")
        typeIn(stepper.field, [Qt.Key_2])
        mouseClick(item("puzzle.used.tile.2"), 30, 8)
        compare(App.puzzle.selectedPiece, 2)
        tryCompare(App.puzzle, "pieceMax", 3)
        compare(App.puzzle.pieceGroupCount(1, 1), 2)
        compare(App.puzzle.pieceGroupCount(2, 1), 1)
    }

    function test_escGoesBackInTheCard() {
        App.puzzle.stepPiece(1, 2)
        App.puzzle.selectPiece(1)
        waitForRendering(win.contentItem)
        const stepper = item("puzzle.piece.count")
        typeIn(stepper.field, [Qt.Key_7])
        keyClick(Qt.Key_Escape)
        compare(stepper.field.text, "2")
        pause()
        compare(App.puzzle.copiesOf(1), 2)
        verify(stepper.field.activeFocus)

        // a pause has committed it: Esc still goes back
        stepper.field.selectAll()
        keyClick(Qt.Key_7)
        pause()
        compare(App.puzzle.copiesOf(1), 7)
        keyClick(Qt.Key_Escape)
        compare(App.puzzle.copiesOf(1), 2)
        compare(stepper.field.text, "2")
    }

    function test_escGoesBackInTheTable() {
        App.puzzle.stepPiece(1, 2)
        App.puzzle.piecesView = "table"
        waitForRendering(win.contentItem)
        const field = item("puzzle.used.row.1.min")
        typeIn(field, [Qt.Key_7])
        keyClick(Qt.Key_Escape)
        compare(field.text, "2")
        pause()
        compare(App.puzzle.copiesOf(1), 2)
    }

    // AC-C17-02: a pause commits, and the field stays in use for the next digit
    function test_aPauseCommitsAndTheFieldGoesOn_data() {
        return [ { tag: "card", card: true }, { tag: "table", card: false } ]
    }
    function test_aPauseCommitsAndTheFieldGoesOn(data) {
        App.puzzle.setRange(1, 3, 20)
        App.puzzle.selectPiece(1)
        if (!data.card)
            App.puzzle.piecesView = "table"
        waitForRendering(win.contentItem)
        const field = data.card ? item("puzzle.piece.max").field : item("puzzle.used.row.1.max")
        typeIn(field, [Qt.Key_1])
        pause()
        compare(App.puzzle.pieceMax, 1)
        verify(field.visible, "the range stays on show")
        verify(field.activeFocus, "the field keeps the keyboard")
        compare(field.text, "1")
        keyClick(Qt.Key_5)
        pause()
        compare(field.text, "15")
        compare(App.puzzle.pieceMax, 15)
        compare(App.puzzle.pieceMin, 3)
        verify(field.activeFocus)
    }

    function test_zeroWaitsForEnterInTheTable() {
        App.puzzle.stepPiece(1, 2)
        App.puzzle.piecesView = "table"
        waitForRendering(win.contentItem)
        const field = item("puzzle.used.row.1.min")
        typeIn(field, [Qt.Key_0])
        pause()
        compare(App.puzzle.pieces.count, 1, "the row stays while the field is in use")
        verify(field.activeFocus)
        keyClick(Qt.Key_Return)
        tryCompare(App.puzzle.pieces, "count", 0)
    }

    function test_zeroInMaxKeepsThePiece() {
        App.puzzle.setRange(1, 1, 3)
        App.puzzle.piecesView = "table"
        waitForRendering(win.contentItem)
        const field = item("puzzle.used.row.1.max")
        typeIn(field, [Qt.Key_0])
        pause()
        compare(App.puzzle.pieces.count, 1)
        keyClick(Qt.Key_Return)
        compare(App.puzzle.pieces.count, 1)
        compare(App.puzzle.copiesOf(1), 1)
    }

    function test_aClearedFieldComesBack() {
        App.puzzle.stepPiece(1, 2)
        App.puzzle.piecesView = "table"
        waitForRendering(win.contentItem)
        const field = item("puzzle.used.row.1.min")
        typeIn(field, [Qt.Key_Delete])
        compare(field.text, "")
        keyClick(Qt.Key_Tab)
        tryCompare(field, "text", "2")
        compare(App.puzzle.copiesOf(1), 2)
    }

    function test_aStepEndsTheTyping() {
        App.puzzle.stepPiece(1, 2)
        App.puzzle.selectPiece(1)
        waitForRendering(win.contentItem)
        const stepper = item("puzzle.piece.count")
        typeIn(stepper.field, [Qt.Key_9])
        keyClick(Qt.Key_Up)
        compare(App.puzzle.copiesOf(1), 3)
        mouseClick(stepper.incButton)
        compare(App.puzzle.copiesOf(1), 4)
        keyClick(Qt.Key_Tab)
        wait(100)
        compare(App.puzzle.copiesOf(1), 4, "leaving the field keeps the step")
        pause()
        compare(App.puzzle.copiesOf(1), 4)
    }

    // PR 181 review, point 5 for the Puzzle steppers: Ctrl+Z while ＋ is held. The
    // repeats went on from the count the undo restored, as a new step that dropped
    // the redo; now the hold stops until the button goes up
    function test_anUndoStopsAHeldStepUntilTheButtonGoesUp() {
        App.puzzle.stepPiece(1, 1)
        App.puzzle.selectPiece(1)
        waitForRendering(win.contentItem)
        wait(900)    // past PROBLEM_COUNT_COALESCE_MS: the burst below is a step of its own
        const stepper = item("puzzle.piece.count")
        const before = App.puzzle.copiesOf(1)
        mousePress(stepper.incButton)
        tryVerify(() => App.puzzle.copiesOf(1) >= before + 3, 3000)   // repeating
        App.document.undo()
        const undone = App.puzzle.copiesOf(1)
        compare(undone, before)
        verify(App.document.canRedo)
        wait(400)                                 // five repeats' time
        compare(App.puzzle.copiesOf(1), undone)
        mouseRelease(stepper.incButton)
        compare(App.puzzle.copiesOf(1), undone, "nor at the release")
        verify(App.document.canRedo)

        // a new press steps again
        mouseClick(stepper.incButton)
        compare(App.puzzle.copiesOf(1), undone + 1)
    }

    function test_setRangeTakesWhatIsTypedAtEnter() {
        App.puzzle.stepPiece(1, 1)
        App.puzzle.stepPiece(2, 1)
        waitForRendering(win.contentItem)
        item("puzzle.left").setRangeRequested()
        let dialog = null
        tryVerify(() => (dialog = findChild(win, "puzzle.setRange.dialog")) !== null && dialog.opened, 2000)
        const max = findChild(dialog.contentItem, "puzzle.setRange.max")
        mouseClick(max.field)
        tryVerify(() => max.field.activeFocus, 1000)
        max.field.selectAll()
        keyClick(Qt.Key_4)
        keyClick(Qt.Key_Return)
        tryVerify(() => !dialog.visible, 1000)
        compare(App.puzzle.pieces.value(0, "countText"), "0–4")
        compare(App.puzzle.pieces.value(1, "countText"), "0–4")
    }

    // --- what goes from under the user's hands ---------------------------------------

    function test_aMenuOnALastTileLeavesEscWorking() {
        App.puzzle.stepPiece(1, 1)
        waitForRendering(win.contentItem)
        const card = item("puzzle.left")
        compare(win.openPopups, 0)
        mouseClick(item("puzzle.used.tile.1"), 40, 8, Qt.RightButton)
        tryVerify(() => card.pieceContextMenu.opened, 2000)
        compare(win.openPopups, 1)
        App.puzzle.setCount(1, 0)              // the tile the menu opened on goes
        card.pieceContextMenu.close()
        tryVerify(() => !card.pieceContextMenu.visible, 1000)
        compare(win.openPopups, 0)
    }

    function test_anEditDuringADragEndsIt() {
        App.puzzle.stepPiece(2, 1)
        waitForRendering(win.contentItem)
        const tile = item("puzzle.used.tile.2")
        const library = item("puzzle.library")
        drag(tile, library, 20, 8)
        verify(item("puzzle.library.dropZone").visible)
        App.puzzle.setCount(2, 0)              // Delete, or Ctrl+Z
        waitForRendering(win.contentItem)
        verify(!item("puzzle.library.dropZone").visible)
        verify(!item("puzzle.dragGhost").visible)
        const back = library.mapToItem(win.contentItem, library.width / 2, library.height / 2)
        mouseRelease(win.contentItem, back.x, back.y)
        compare(item("puzzle.left").dragFrom, "")
    }

    function test_aRenameSurvivesAnotherRowsDelete() {
        App.puzzle.newProblem()
        App.puzzle.newProblem()
        waitForRendering(win.contentItem)
        mouseClick(item("puzzle.problems.row.1"), 60, 10)
        keyClick(Qt.Key_Return)
        const field = item("puzzle.problems.row.1.labelField")
        tryVerify(() => field.activeFocus, 1000)
        keyClick(Qt.Key_A)
        App.puzzle.deleteProblem(2)
        tryCompare(App.puzzle.problems, "count", 2)
        compare(App.puzzle.problems.value(1, "label").toLowerCase(), "a")
    }

    function test_theLastRowsRenameLeavesTheKeysWorking() {
        App.puzzle.newProblem()
        waitForRendering(win.contentItem)
        mouseClick(item("puzzle.problems.row.1"), 60, 10)
        keyClick(Qt.Key_Return)
        const field = item("puzzle.problems.row.1.labelField")
        tryVerify(() => field.activeFocus, 1000)
        keyClick(Qt.Key_B)
        App.puzzle.deleteProblem(1)
        tryCompare(App.puzzle.problems, "count", 1)
        const list = item("puzzle.problems.list")
        compare(list.editingRow, -1)
    }

    // row i of the problems list's actions shown and taking presses (from the event after they showed)
    function problemActionsReady(i) {
        const a = item("puzzle.problems.row." + i + ".actions")
        tryVerify(() => a.opacity === 1 && a.enabled, 1000)
    }

    function test_aProblemRowsActionsArePlacedButHiddenUntilItShows() {
        App.puzzle.newProblem()
        App.puzzle.selectProblem(0)
        mouseMove(win.contentItem, 800, 900)              // away from the list
        problemActionsReady(0)
        const hidden = item("puzzle.problems.row.1.actions")
        compare(hidden.opacity, 0)
        verify(!hidden.enabled)
        verify(item("puzzle.problems.row.1.duplicate").Accessible.ignored)
        verify(!item("puzzle.problems.row.0.duplicate").Accessible.ignored)
        const r = item("puzzle.problems.row.1")
        mouseMove(r, 60, r.height / 2)
        problemActionsReady(1)
        verify(!item("puzzle.problems.row.1.duplicate").Accessible.ignored)
        mouseClick(item("puzzle.problems.row.1.duplicate"))
        compare(App.puzzle.problems.count, 3)
    }

    function test_aClickRightAfterThePointerArrivesOnAProblemRowOnlySelects() {
        // PR 181 review, the shapes list's point 2, for the problems list: the
        // pointer lands on a row and clicks at once (a pen, a touch, a row moving
        // under a still pointer). Its buttons show under that press and must not
        // take it: wherever it lands, it selects the row -- no rename, duplicate
        // or delete.
        for (let i = 0; i < 3; i++)
            App.puzzle.newProblem()
        waitForRendering(win.contentItem)
        const before = App.puzzle.problems.count
        const list = item("puzzle.problems.list")
        // from the chip across the buttons, short of the list's scroll bar
        // (it lies over the rows' right edge and takes presses there)
        const xEnd = list.ScrollBar.vertical.mapToItem(item("puzzle.problems.row.0"), 0, 0).x
        let target = 1
        for (let x = 24; x < xEnd; x += 6) {
            App.puzzle.selectProblem(0)
            mouseMove(win.contentItem, 800, 900)          // away from the list
            waitForRendering(win.contentItem)
            const r = item("puzzle.problems.row." + target)   // a row of its own each time: no double-click
            mouseMove(r, x, r.height / 2)
            mouseClick(r, x, r.height / 2)
            waitForRendering(win.contentItem)
            const at = "row " + target + ", x " + x
            compare(App.puzzle.problems.count, before, at)
            compare(App.puzzle.problem, target, at)
            compare(list.editingRow, -1, at)
            target = target % (before - 1) + 1
            if (target === 1)
                wait(Qt.styleHints.mouseDoubleClickInterval + 50)
        }
    }

    function test_aProblemRowWithItsActionsHiddenGivesTheirRoomToTheLabel() {
        App.puzzle.newProblem()
        App.puzzle.renameProblem(1, "A label far too long to fit beside the row's actions, however wide the card is")
        App.puzzle.selectProblem(0)
        mouseMove(win.contentItem, 800, 900)              // away from the list
        waitForRendering(win.contentItem)
        const r = item("puzzle.problems.row.1")
        const label = item("puzzle.problems.row.1.label")
        const actions = item("puzzle.problems.row.1.actions")
        compare(actions.opacity, 0)
        verify(label.truncated)
        tryVerify(() => label.mapToItem(r, label.width, 0).x > actions.x + actions.width / 2, 1000)
        mouseMove(r, 60, r.height / 2)
        problemActionsReady(1)
        tryVerify(() => label.mapToItem(r, label.width, 0).x <= actions.x, 1000)
        verify(label.truncated)
    }

    // --- what a file or a user wrote ---------------------------------------------------

    function textsHolding(root, marker, found) {
        const kids = root.children
        for (let i = 0; i < kids.length; i++) {
            const k = kids[i]
            if (k.text !== undefined && k.textFormat !== undefined && String(k.text).indexOf(marker) >= 0)
                found.push(k)
            textsHolding(k, marker, found)
        }
        return found
    }

    // a name is what it says: markup in it stays text
    function test_namesAreNeverRichText() {
        App.shapes.renameShape(1, "<b>bold</b>")
        App.puzzle.renameProblem(0, "<b>bold</b>")
        App.puzzle.stepPiece(1, 1)
        App.puzzle.selectPiece(1)
        waitForRendering(win.contentItem)
        for (const view of ["chips", "table"]) {
            App.puzzle.piecesView = view
            waitForRendering(win.contentItem)
            const found = textsHolding(win.contentItem, "<b>", [])
            verify(found.length >= 3, "the names show")
            for (let i = 0; i < found.length; i++)
                compare(found[i].textFormat, Text.PlainText, "rich text: " + found[i].objectName + " " + found[i].text)
        }
        App.layout.workspace = LayoutController.Entities
        waitForRendering(win.contentItem)
        const entities = textsHolding(win.contentItem, "<b>", [])
        verify(entities.length >= 1)
        for (let i = 0; i < entities.length; i++)
            compare(entities[i].textFormat, Text.PlainText, "rich text: " + entities[i].objectName + " " + entities[i].text)
    }

    // --- the keyboard and assistive tech ------------------------------------------------

    // Tab until the item has the focus: keys as a user presses them
    function tabTo(target) {
        for (let i = 0; i < 40 && !target.activeFocus; i++) {
            keyClick(Qt.Key_Tab)
            wait(5)
        }
        verify(target.activeFocus, "Tab never reached " + target.objectName)
    }

    SignalSpy { id: announced; signalName: "announced" }

    function test_allPiecesWorkFromTheKeyboard() {
        waitForRendering(win.contentItem)
        const chips = item("puzzle.library.chips")
        tabTo(chips)
        compare(App.puzzle.selectedPiece, 1, "the first piece beside the result")
        verify(item("puzzle.library.chip.1.ring").visible, "the ring shows where the keyboard is")
        keyClick(Qt.Key_Right)
        compare(App.puzzle.selectedPiece, 2)
        keyClick(Qt.Key_Plus)
        compare(App.puzzle.copiesOf(2), 1)
        keyClick(Qt.Key_Return)
        compare(App.puzzle.copiesOf(2), 2)
        keyClick(Qt.Key_Minus)
        compare(App.puzzle.copiesOf(2), 1)
        verify(chips.activeFocus, "the keyboard stays in the list")
    }

    function test_theKeyboardArrivesOnATile() {
        App.puzzle.stepPiece(2, 1)
        App.puzzle.stepPiece(3, 1)
        App.puzzle.selectPiece(-1)
        waitForRendering(win.contentItem)
        tabTo(item("puzzle.used.tiles"))
        compare(App.puzzle.selectedPiece, 2)
        verify(item("puzzle.used.tile.2.ring").visible)
    }

    function test_theMenuKeyOpensAPiecesMenu() {
        App.puzzle.stepPiece(2, 1)
        waitForRendering(win.contentItem)
        const card = item("puzzle.left")
        tabTo(item("puzzle.used.tiles"))
        keyClick(Qt.Key_Menu)
        tryVerify(() => card.pieceContextMenu.opened, 2000)
        compare(card.pieceContextMenu.shape, 2)
        card.pieceContextMenu.close()
        tryVerify(() => !card.pieceContextMenu.visible, 1000)
    }

    function test_aDeletedGroupLeavesTheKeyboardOnItsSection() {
        App.puzzle.stepPiece(1, 1)
        App.puzzle.newGroup(1)
        waitForRendering(win.contentItem)
        const button = item("puzzle.groups.card.1.delete")
        tabTo(button)
        keyClick(Qt.Key_Space)
        tryCompare(App.puzzle.groups, "count", 0)
        const f = win.activeFocusItem
        verify(f !== null && f.visible, "the focus is on something on show")
        const section = item("puzzle.section.groups")
        let inside = false
        for (let p = f; p !== null; p = p.parent)
            if (p === section)
                inside = true
        verify(inside, "the focus went to " + f)
    }

    function test_tabBringsTheFocusIntoView() {
        win.width = 960
        win.height = 640
        addColours()
        App.puzzle.stepPiece(1, 1)
        App.puzzle.selectPiece(1)
        waitForRendering(win.contentItem)
        const clear = item("puzzle.rules.clear")
        tabTo(clear)
        waitForRendering(win.contentItem)
        const flick = item("puzzle.right.sections")
        const at = clear.mapToItem(flick, 0, 0).y
        verify(at >= 0 && at + clear.height <= flick.height, "the button is at " + at + " of " + flick.height)
    }

    function test_aRowsFieldSelectsItsPiece() {
        App.puzzle.stepPiece(1, 1)
        App.puzzle.stepPiece(2, 1)
        App.puzzle.selectPiece(-1)
        App.puzzle.piecesView = "table"
        waitForRendering(win.contentItem)
        tabTo(item("puzzle.used.row.2.min"))
        compare(App.puzzle.selectedPiece, 2)
    }

    function test_enterTicksAMatrixCellAsSpaceDoes() {
        addColours()
        const grid = item("puzzle.rules.grid")
        tabTo(grid)
        const was = App.puzzle.rules.value(0, "allowed")
        keyClick(Qt.Key_Return)
        compare(App.puzzle.rules.value(0, "allowed"), !was)
        keyClick(Qt.Key_Space)
        compare(App.puzzle.rules.value(0, "allowed"), was)
    }

    function test_theMatrixSaysEachMove() {
        addColours()
        announced.target = App.status
        announced.clear()
        tabTo(item("puzzle.rules.grid"))
        keyClick(Qt.Key_Right)
        compare(announced.count, 1)
        compare(announced.signalArguments[0][0], "Piece C1 on result C2: not allowed")
        keyClick(Qt.Key_Space)
        compare(announced.signalArguments[announced.count - 1][0], "Piece C1 on result C2: allowed")
    }

    function test_aCountAndAChangeOfFitAreSaid() {
        announced.target = App.status
        announced.clear()
        App.puzzle.stepPiece(1, 1)
        const said = []
        for (let i = 0; i < announced.count; i++)
            said.push(announced.signalArguments[i][0])
        verify(said.indexOf("S2: 1") >= 0, said.join(" | "))
        verify(said.indexOf(App.puzzle.summaryTitle) >= 0 || said.length === 1, said.join(" | "))
    }

    // WCAG 1.4.3: the accent is 3.68:1 on the light canvas
    function test_sceneCaptionsAreInTheTextColour() {
        App.puzzle.stepPiece(1, 1)
        App.puzzle.selectPiece(1)
        waitForRendering(win.contentItem)
        for (const theme of ["light", "dark"]) {
            App.settings.theme = theme
            for (let i = 0; i < App.viewport.sceneObjects.count; i++)
                compare(item("puzzle.scene.object." + i + ".caption").color, Theme.text)
        }
        App.settings.theme = "light"
    }

    function test_the3dViewSaysWhatItShows() {
        App.puzzle.stepPiece(1, 2)
        waitForRendering(win.contentItem)
        compare(App.puzzle.sceneName, "P1: result S1, 2 pieces")
        const view = item("entities.viewport.surface")
        verify(view.Accessible.name.indexOf("P1: result S1, 2 pieces") >= 0, view.Accessible.name)
    }

    // --- layout ----------------------------------------------------------------------------

    function test_aClickOnTheGroupBoxClosesItsList() {
        App.puzzle.stepPiece(1, 1)
        App.puzzle.piecesView = "table"
        waitForRendering(win.contentItem)
        const picker = item("puzzle.used.row.1.group")
        mouseClick(picker)
        tryVerify(() => picker.list.opened, 1000)
        mouseClick(picker)
        tryVerify(() => !picker.list.visible, 1000)
    }

    function test_theSummaryStaysInSightInAShortWindow() {
        win.width = 1280
        win.height = 640
        for (let i = 0; i < 5; i++)
            App.puzzle.newProblem()
        for (let i = 0; i < 12; i++)
            App.shapes.addShape()
        App.puzzle.addOneOfEach()
        waitForRendering(win.contentItem)
        const card = item("puzzle.left")
        const strip = item("puzzle.summary")
        const bottom = strip.mapToItem(card, 0, strip.height).y
        verify(bottom <= card.height + 1, "the strip ends at " + bottom + " of " + card.height)
    }

    function test_aStripWithoutAHoleLimitHasNoGap() {
        waitForRendering(win.contentItem)
        const strip = item("puzzle.summary")
        const detail = item("puzzle.summary.detail")
        const bottom = detail.mapToItem(strip, 0, detail.height).y
        verify(strip.height - bottom <= 12, "a band of " + (strip.height - bottom))
    }

    function test_aSectionFromTheRailComesIntoView() {
        win.width = 1280
        win.height = 640
        addColours()
        addColours()
        App.puzzle.stepPiece(1, 1)
        App.puzzle.selectPiece(1)
        App.layout.rightCollapsed = true
        waitForRendering(win.contentItem)
        App.puzzle.showSection("rules")
        const flick = item("puzzle.right.sections")
        tryVerify(() => flick.contentY > 0, 2000, "the card did not scroll")
        const section = item("puzzle.section.rules")
        const at = section.mapToItem(flick, 0, 0).y
        verify(at >= 0 && at < flick.height, "the section is at " + at)
    }

    function test_theGhostSitsAtThePointer() {
        waitForRendering(win.contentItem)
        const chip = item("puzzle.library.chip.2")
        const used = item("puzzle.used")
        drag(chip, used, 10, chip.height / 2)
        const ghost = item("puzzle.dragGhost")
        const end = used.mapToItem(win.contentItem, used.width / 2, used.height / 2)
        mouseMove(win.contentItem, end.x, end.y)
        wait(30)
        const after = ghost.mapToItem(win.contentItem, 0, 0)
        verify(Math.abs(after.y - end.y - 8) <= 2, "ghost at " + after.y + ", pointer at " + end.y)
        verify(Math.abs(after.x - end.x - 8) <= 2, "ghost at " + after.x + ", pointer at " + end.x)
        mouseRelease(win.contentItem, end.x, end.y)
    }

    function test_noProblemsStayAtTheTop() {
        App.document.newDocument(0)
        waitForRendering(win.contentItem)
        const list = item("puzzle.problems.list")
        const at = list.mapToItem(item("puzzle.left"), 0, 0).y
        verify(at < 100, "the list sits at " + at)
    }

    function test_minAndMaxStayInTheirBoxes() {
        App.puzzle.setRange(1, 1, 3)
        App.puzzle.selectPiece(1)
        waitForRendering(win.contentItem)
        for (const name of ["puzzle.piece.min", "puzzle.piece.max"]) {
            const stepper = item(name)
            const dec = stepper.decButton
            const label = dec.parent.children[0]
            verify(label.x + label.contentWidth <= dec.x, name + ": " + label.contentWidth + " dp of text before " + dec.x)
        }
    }

    function test_thePieceAxisFitsTwoColours() {
        App.colours.addColour()
        App.colours.addColour()
        waitForRendering(win.contentItem)
        const label = item("puzzle.rules.axis.piece")
        const matrix = item("puzzle.rules.matrix")
        const info = item("puzzle.rules.info")
        const box = label.parent
        const middle = box.mapToItem(matrix, box.width / 2, box.height / 2)
        const half = label.implicitWidth / 2
        verify(middle.y + half <= matrix.height + 1, "past the matrix by " + (middle.y + half - matrix.height))
        verify(info.x > box.x + box.width, "the info button stands clear of it")
    }

    function test_clearStaysOnTheCardInMinimal() {
        App.settings.density = "minimal"
        waitForRendering(win.contentItem)
        const row = item("puzzle.summary.holeLimit")
        row.visible = true                   // as a file with a limit shows it
        row.children[0].text = "This file limits the search to 12 holes"
        const clear = item("puzzle.summary.holeLimit.clear")
        const card = item("puzzle.left")
        waitForRendering(win.contentItem)
        const right = clear.mapToItem(card, clear.width, 0).x
        verify(right <= card.width, "Clear ends at " + right + " of " + card.width)
    }


    function test_theProblemsKeepTheirHeight() {
        const list = item("puzzle.problems.list")
        const least = 3 * (Theme.listRow + 2) - 2
        compare(App.puzzle.problems.count, 1)
        tryCompare(list, "height", least, 1000)
        // and in the smallest window, with everything below asking for room
        win.height = 640
        App.puzzle.stepPiece(1, 1)
        waitForRendering(win.contentItem)
        compare(list.height, least)
        win.height = 1000
    }

    function test_aNewProblemDoesNotMoveTheCardBelowItsHeight() {
        const result = item("puzzle.result")
        const y = result.y
        App.puzzle.newProblem()
        waitForRendering(win.contentItem)
        compare(result.y, y)
        App.puzzle.newProblem()
        waitForRendering(win.contentItem)
        compare(result.y, y, "three rows are the least it keeps")
    }

    function test_theMatrixWorksPastThreeColours() {
        addColours()
        addColours()
        addColours()
        compare(App.puzzle.colours.count, 9)
        compare(App.puzzle.rules.count, 81)
        const grid = item("puzzle.rules.grid")
        tabTo(grid)
        for (let i = 0; i < 8; i++)
            keyClick(Qt.Key_Right)
        for (let i = 0; i < 8; i++)
            keyClick(Qt.Key_Down)
        waitForRendering(win.contentItem)
        const was = App.puzzle.rules.value(80, "allowed")
        keyClick(Qt.Key_Space)
        compare(App.puzzle.rules.value(80, "allowed"), !was)
        const cell = item("puzzle.rules.cell.C9.C9")
        const matrix = item("puzzle.rules.grid")
        const at = cell.mapToItem(matrix, 0, 0).x
        verify(at + cell.width <= matrix.width + 1, "the cursor's cell ends at " + (at + cell.width) + " of " + matrix.width)
    }

    function test_aSectionShutHoldsNoMatrix() {
        addColours()
        App.puzzle.setSectionOpen("rules", false)
        waitForRendering(win.contentItem)
        compare(findChild(win.contentItem, "puzzle.rules.grid"), null)
        App.puzzle.setSectionOpen("rules", true)
        waitForRendering(win.contentItem)
        verify(item("puzzle.rules.grid").visible)
    }

    // --- the keyboard's ring --------------------------------------------------------------------

    function test_aClickShowsNoRingUntilTheKeyboardMoves() {
        App.puzzle.stepPiece(1, 1)
        App.puzzle.stepPiece(2, 1)
        addColours()
        const row = item("puzzle.problems.row.0")
        mouseClick(row, 60, 10)
        verify(item("puzzle.problems.list").activeFocus)
        verify(!App.keyboardCues.navigating)
        compare(row.border.width, 1)

        const tile = item("puzzle.used.tile.1")
        mouseClick(tile, 40, 8)
        verify(item("puzzle.used.tiles").activeFocus)
        verify(!item("puzzle.used.tile.1.ring").visible)

        const seg = item("puzzle.used.view")
        mouseClick(item("puzzle.used.view.table"))
        verify(seg.activeFocus)
        compare(seg.children[0].border.width, 1)
        mouseClick(item("puzzle.used.view.chips"))

        const cell = item("puzzle.rules.cell.C1.C2")
        mouseClick(cell)
        compare(cell.cursor, false)

        // the first Tab or arrow, and the same places ring
        waitForRendering(win.contentItem)
        mouseClick(row, 60, 10)
        keyClick(Qt.Key_Down)
        verify(App.keyboardCues.navigating)
        App.puzzle.selectProblem(0)
        tryVerify(() => row.border.width === 2, 1000)

        // and the mouse takes it back
        mouseClick(row, 60, 10)
        verify(!App.keyboardCues.navigating)
        compare(row.border.width, 1)
    }

    function test_theTilesRingFollowsTheKeyboard() {
        App.puzzle.stepPiece(1, 1)
        App.puzzle.stepPiece(2, 1)
        waitForRendering(win.contentItem)
        mouseClick(item("puzzle.used.tile.1"), 40, 8)
        keyClick(Qt.Key_Right)
        tryVerify(() => item("puzzle.used.tile.2.ring").visible)
    }

    function test_opensAtASectionWithoutARingForTheMouse() {
        App.layout.rightCollapsed = true
        waitForRendering(win.contentItem)
        mouseClick(item("puzzle.sections.rail.row.rules"))
        tryVerify(() => !App.layout.rightCollapsed)
        tryVerify(() => item("puzzle.section.rules.header").activeFocus, 1000)
        verify(!item("puzzle.section.rules.header").visualFocus)
    }

    function test_theViewportHasNoEditing() {
        // AC-C18-04
        compare(findChild(win.contentItem, "entities.viewport.toolbar.edit").visible, false)
        verify(item("puzzle.viewport.hint").visible)
        verify(!item("entities.viewport.hint.tools").visible)
    }
}
