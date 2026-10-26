import QtQuick
import QtQuick.Controls
import QtTest
import BurrTools.Ui

// The Shapes card (C01) through the real Main.qml: the row's actions, inline
// rename, the list's keys, the context menu, drag to reorder, the rail's +
// and the delete that asks first (test plan T-C01-*).
TestCase {
    id: tc
    name: "Shapes"
    when: windowShown

    Component { id: mainComponent; Main {} }

    property var win

    function init() {
        // as tst_shell: the headless platform's two warnings are expected
        failOnWarning(/^(?!No QRhi found for window|This plugin does not support grabbing the keyboard).*/)
        App.layout.workspace = LayoutController.Entities
        App.layout.leftCollapsed = false
        App.layout.rightCollapsed = false
        // the puzzle before the window, so its list is made once, for it
        App.document.openFile(Qt.resolvedUrl("../../../examples/PelikanBurr.xmpuzzle"))
        win = createTemporaryObject(mainComponent, tc)
        verify(win !== null, "Main.qml failed to load")
        win.width = 1600
        win.height = 1000
        waitForRendering(win.contentItem)
        tryVerify(() => win.contentReady, 5000)
        settle()
    }

    function open(name) {
        App.document.openFile(Qt.resolvedUrl("../../../examples/" + name + ".xmpuzzle"))
        settle()
    }

    function settle() {
        // a file can bring its comment up: out of the way
        for (let i = 0; i < 5; i++) {
            wait(50)
            const m = findChild(win, "shell.message")
            if (m === null || !m.visible)
                break
            m.close()
        }
        tryVerify(() => list().count === App.shapes.count && App.shapes.count > 2, 2000)
        win.requestActivate()
        tryVerify(() => win.active, 3000)
    }

    function item(name) {
        const o = findChild(win.contentItem, name)
        verify(o !== null, "no item " + name)
        return o
    }
    function list() { return findChild(win.contentItem, "entities.shapes.list") }
    function row(i) {
        let r = null
        tryVerify(() => (r = findChild(list().contentItem, "entities.shapes.row." + i)) !== null, 1000)
        return r
    }
    function label(i) { return App.shapes.data(App.shapes.index(i, 0), ShapesModel.LabelRole) }
    function focusList() {
        mouseClick(row(0), 60, row(0).height / 2)
        verify(list().activeFocus, "focus is on " + win.activeFocusItem)
    }
    // a drag of row 0's grip by dy, in steps; in the list's coordinates,
    // which do not move with the dragged row (it follows the pointer)
    function dragRow0(dy, steps, beforeRelease) {
        const l = list()
        const y0 = row(0).height / 2
        mousePress(l, 10, y0)
        for (let i = 1; i <= steps; i++)
            mouseMove(l, 10, y0 + i * dy / steps)
        if (beforeRelease)
            beforeRelease()
        mouseRelease(l, 10, y0 + dy)
    }
    function contextMenu() { return item("entities.shapes").contextMenu }

    function test_actionsShowOnTheSelectedAndTheHoveredRow() {
        // AC-C01-01
        App.shapes.select(0)
        mouseMove(win.contentItem, 800, 500)             // away from the list
        verify(item("entities.shapes.row.0.duplicate").visible)
        verify(!item("entities.shapes.row.1.duplicate").visible)
        mouseMove(row(1), 60, row(1).height / 2)
        tryVerify(() => item("entities.shapes.row.1.duplicate").visible)
        // the row's buttons are no Tab stops: the list is one
        verify(!item("entities.shapes.row.1.delete").activeFocusOnTab)
    }

    function test_newAndDuplicateAppendAndSelect() {
        // AC-C01-04, New: appended, selected
        const n = App.shapes.count
        mouseClick(item("entities.shapes.new"))
        compare(App.shapes.count, n + 1)
        compare(App.shapes.selected, n)
        App.shapes.select(1)
        mouseMove(row(1), 60, row(1).height / 2)
        mouseClick(item("entities.shapes.row.1.duplicate"))
        compare(App.shapes.count, n + 2)
        compare(App.shapes.selected, n + 1)
        compare(label(n + 1), label(1))
        compare(item("entities.shapes.count").text, String(n + 2))
    }

    function test_renameInlineEnterCommitsEscReverts() {
        // AC-C01-03 (F2 stays Save: Enter and double-click start it)
        const r = row(1)
        // on a selected row, laid out: a row's actions show (and move into place) as it is selected
        App.shapes.select(1)
        waitForRendering(win.contentItem)
        // A double-click opens it (the row's TapHandler: onDoubleTapped ->
        // startRename). QtTest's double-click reaches a TapHandler as two single
        // taps -- their timestamps are too far apart -- and its doubleTapped
        // cannot be sent from QML (a QEventPoint argument), so the test starts
        // where that handler goes; Enter, below, starts it by the real key.
        verify(findChild(r, "entities.shapes.row.1.tap") !== null)
        list().startRename(1)
        let field = null
        tryVerify(() => (field = findChild(r, "entities.shapes.row.1.labelField")) !== null && field.activeFocus, 1000)
        field.text = "Arm"
        keyClick(Qt.Key_Return)
        compare(label(1), "Arm")
        tryVerify(() => findChild(r, "entities.shapes.row.1.labelField") === null)
        verify(list().activeFocus)                       // back to the list

        // Enter in the list starts it; Esc cancels and leaves the label alone
        keyClick(Qt.Key_Return)
        tryVerify(() => (field = findChild(r, "entities.shapes.row.1.labelField")) !== null && field.activeFocus, 1000)
        field.text = "x"
        keyClick(Qt.Key_Escape)
        compare(label(1), "Arm")
        compare(App.layout.focus, LayoutController.FocusNone)
        verify(list().activeFocus)
    }

    function test_theListsKeys() {
        // AC-C01-K: arrows select, Alt+arrows move and renumber, Ctrl+D, Del
        focusList()
        const first = label(0), second = label(1)
        keyClick(Qt.Key_Down)
        compare(App.shapes.selected, 1)
        keyClick(Qt.Key_Up, Qt.AltModifier)
        compare(App.shapes.selected, 0)
        compare(label(0), second)
        compare(label(1), first)
        verify(item("status.text").text.indexOf("renumbered") >= 0)
        keyClick(Qt.Key_Down, Qt.AltModifier)
        compare(label(1), second)

        const n = App.shapes.count
        keyClick(Qt.Key_D, Qt.ControlModifier)
        compare(App.shapes.count, n + 1)
        compare(App.shapes.selected, n)
        keyClick(Qt.Key_Delete)                          // the copy: no problem uses it
        compare(App.shapes.count, n)
        compare(App.shapes.selected, n - 1)
    }

    function test_theContextMenu() {
        // AC-C01-08: at the pointer, the row selected, items enabled as they may be
        const r = row(0)
        mouseClick(r, 60, r.height / 2, Qt.RightButton)
        const menu = contextMenu()
        compare(menu.objectName, "entities.shapes.contextMenu")
        tryVerify(() => menu.opened, 1000)
        compare(App.shapes.selected, 0)
        compare(menu.row, 0)
        const it = (k) => {
            for (let i = 0; i < menu.count; i++)
                if (menu.itemAt(i).objectName === "entities.shapes.contextMenu." + k)
                    return menu.itemAt(i)
            return null
        }
        for (const k of ["header", "rename", "duplicate", "moveEarlier", "moveLater", "weightUp", "weightDown", "delete"])
            verify(it(k) !== null, k)
        verify(!it("header").enabled)
        verify(!it("moveEarlier").enabled)               // the first shape
        verify(it("moveLater").enabled)
        verify(it("delete").enabled)
        it("moveLater").triggered()
        tryVerify(() => !menu.visible, 1000)
        compare(App.shapes.selected, 1)

        // the Menu key opens it for the selected shape
        focusList()
        keyClick(Qt.Key_Menu)
        tryVerify(() => menu.opened, 1000)
        compare(menu.row, 0)
        menu.close()
    }

    function test_dragReordersAndRenumbers() {
        // AC-C01-07: drop line, reorder, selection follows, one undo
        const first = label(0)
        const r = row(0)
        const pitch = r.height + list().spacing
        // into the lower half of row 2: rows 1 and 2 slide up into the gap
        // the dragged row leaves, and row 3 stays
        dragRow0(2 * pitch + r.height / 4, 100, () => {
            tryCompare(row(1).transform[0], "y", -pitch, 1000)
            tryCompare(row(2).transform[0], "y", -pitch, 1000)
            compare(row(3).transform[0].y, 0)
            verify(row(0).transform[0].y > pitch)      // the dragged row is under the pointer
        })
        compare(label(2), first)
        // after the drop every row stands in its own place again
        for (let i = 0; i < 4; i++)
            compare(row(i).transform[0].y, 0)
        compare(App.shapes.selected, 2)
        App.document.undo()
        compare(label(0), first)
    }

    function test_escCancelsADrag() {
        const first = label(0)
        const pitch = row(0).height + list().spacing
        dragRow0(2 * pitch, 10, () => {
            tryCompare(row(1).transform[0], "y", -pitch, 1000)
            keyClick(Qt.Key_Escape)
            tryCompare(row(1).transform[0], "y", 0, 1000)      // the others slide back
        })
        compare(label(0), first)
        compare(App.layout.focus, LayoutController.FocusNone)
    }

    function test_theRailsNewButton() {
        // AC-C01-09: + on the rail
        const n = App.shapes.count
        App.layout.leftCollapsed = true
        tryCompare(item("workspace.left"), "width", Theme.collapsedRail, 1000)
        mouseClick(item("entities.shapes.rail.new"))
        compare(App.shapes.count, n + 1)
        compare(App.shapes.selected, n)
        verify(item("entities.shapes.rail.list").activeFocusOnTab)
    }

    function test_aDeleteThatLosesSolutionsAsksFirst() {
        open("SolidSixPieceBurrs")
        const n = App.shapes.count
        App.shapes.requestDelete(0)
        let dialog = null
        tryVerify(() => (dialog = findChild(win, "entities.shapes.deleteDialog")) !== null && dialog.opened, 1000)
        compare(App.shapes.count, n)
        verify(dialog.title.indexOf("S1") >= 0)
        verify(App.commands.blocked)                     // counted as modal
        mouseClick(findChild(dialog.footer, "entities.shapes.deleteDialog.cancel"))
        tryVerify(() => !dialog.visible, 1000)
        compare(App.shapes.count, n)

        App.shapes.requestDelete(0)
        tryVerify(() => dialog.opened, 1000)
        mouseClick(findChild(dialog.footer, "entities.shapes.deleteDialog.delete"))
        tryVerify(() => !dialog.visible, 1000)
        compare(App.shapes.count, n - 1)
    }
}
