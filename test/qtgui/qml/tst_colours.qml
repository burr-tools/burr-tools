import QtQuick
import QtQuick.Controls
import QtTest
import BurrTools.Ui

// The Drawing colour footer (C09) through the real Main.qml: the current
// colour and its name, the palette, the colour menu, Add through the
// chooser, Remove that asks first, and the collapsed rail's swatch.
TestCase {
    id: tc
    name: "Colours"
    when: windowShown

    Component { id: mainComponent; Main {} }

    property var win

    SignalSpy { id: chooser; target: App.colours; signalName: "chooserRequested" }

    function init() {
        // as tst_shell: the headless platform's two warnings are expected
        failOnWarning(/^(?!No QRhi found for window|This plugin does not support grabbing the keyboard).*/)
        App.layout.workspace = LayoutController.Entities
        App.layout.leftCollapsed = false
        App.layout.rightCollapsed = false
        App.document.openFile(Qt.resolvedUrl("../../../examples/PelikanBurr.xmpuzzle"))
        win = createTemporaryObject(mainComponent, tc)
        verify(win !== null, "Main.qml failed to load")
        win.width = 1600
        win.height = 1000
        waitForRendering(win.contentItem)
        tryVerify(() => win.contentReady, 5000)
        for (let i = 0; i < 5; i++) {
            wait(50)
            const m = findChild(win, "shell.message")
            if (m === null || !m.visible)
                break
            m.close()
        }
        win.requestActivate()
        tryVerify(() => win.active, 3000)
        chooser.clear()
    }

    // the layout is the App's, shared with the other test files
    function cleanup() {
        App.layout.workspace = LayoutController.Entities
        App.layout.leftCollapsed = false
        App.layout.rightCollapsed = false
    }

    function item(name) {
        const o = findChild(win.contentItem, name)
        verify(o !== null, "no item " + name)
        return o
    }
    function swatch(i) {
        let s = null
        tryVerify(() => (s = findChild(win.contentItem, "entities.editor.colour.swatch." + i)) !== null, 1000)
        return s
    }
    // + adds the next preset at once (the mock's quick add): no chooser
    function addColour() {
        const n = App.colours.count
        mouseClick(item("entities.editor.colour.add"))
        compare(chooser.count, 0)
        compare(App.colours.count, n + 1)
        // the palette places its new swatch on the next frame
        waitForRendering(win.contentItem)
    }
    // the chooser (Edit) put away, its answer already in or cancelled
    function closeChooser() {
        const d = findChild(win, "entities.editor.colour.chooser")
        verify(d !== null, "the colour chooser was made")
        tryVerify(() => d.visible, 1000)
        d.close()
        tryVerify(() => !d.visible, 1000)
        // Headless, the chooser is Qt's QML one (the platform's own on a
        // desktop), in a window of its own: it hands the focus back after
        // its exit transition, which would land after the test's next click,
        // and leaves the main window to be activated again, as the desktop
        // does when a native chooser closes
        wait(400)
        win.requestActivate()
        tryVerify(() => win.active, 3000)
    }
    function menuItem(menu, key) {
        for (let i = 0; i < menu.count; i++)
            if (menu.itemAt(i).objectName === "entities.editor.colour.menu." + key)
                return menu.itemAt(i)
        return null
    }

    function test_theFooterShowsDefaultFirst() {
        // AC-C09-01
        compare(item("entities.editor.colour.name").text, "Default")
        verify(swatch(0) !== null)
        compare(swatch(0).Accessible.name, "Default (C0, the piece's colour)")
        verify(swatch(0).selected)
        // Default draws in the shape's colour
        compare(item("entities.editor.colour.current").children[0].children[0].color, App.viewport.shapeColor)
        verify(item("entities.editor.colour.add").enabled)
    }

    function test_addingSelectsTheNewColour() {
        addColour()
        compare(App.colours.count, 2)
        compare(item("entities.editor.colour.name").text, "C1")
        verify(swatch(1).selected)
        verify(!swatch(0).selected)
        // one undo step
        App.document.undo()
        compare(App.colours.count, 1)
        compare(item("entities.editor.colour.name").text, "Default")
    }

    function test_clickAndKeysChooseTheColour() {
        // AC-C09-02 (the colour later painting uses)
        addColour()
        addColour()
        mouseClick(swatch(0))
        compare(App.colours.drawColour, 0)
        verify(item("entities.editor.colour.palette").activeFocus)
        keyClick(Qt.Key_Right)
        compare(App.colours.drawColour, 1)
        keyClick(Qt.Key_Right)
        keyClick(Qt.Key_Right)                          // stops at the last
        compare(App.colours.drawColour, 2)
        keyClick(Qt.Key_Left)
        compare(App.colours.drawColour, 1)
    }

    function test_theColourMenu() {
        // AC-C09-04/05: Edit and Remove, not for Default; right-click is for that swatch
        addColour()
        const footer = item("entities.editor.colour")
        mouseClick(swatch(0), 13, 13, Qt.RightButton)
        let menu = null
        for (const c of footer.data)
            if (c.objectName === "entities.editor.colour.menu")
                menu = c
        verify(menu !== null)
        tryVerify(() => menu.opened, 1000)
        verify(!menuItem(menu, "edit").enabled)
        verify(!menuItem(menu, "remove").enabled)
        menu.close()
        tryVerify(() => !menu.visible, 1000)

        mouseClick(swatch(1), 13, 13, Qt.RightButton)
        tryVerify(() => menu.opened, 1000)
        compare(App.colours.drawColour, 1)
        verify(menuItem(menu, "edit").enabled)
        menuItem(menu, "edit").triggered()
        tryVerify(() => !menu.visible, 1000)
        compare(chooser.count, 1)                       // the chooser, starting at the colour
        compare(chooser.signalArguments[0][1], Qt.color("#ef4444"))      // the first preset
        closeChooser()                                  // its Cancel

        // ⋯ is for the drawing colour; Remove of an unused colour goes at once
        mouseClick(item("entities.editor.colour.more"))
        tryVerify(() => menu.opened, 1000)
        menuItem(menu, "remove").triggered()
        tryVerify(() => !menu.visible, 1000)
        compare(App.colours.count, 1)
        compare(App.colours.drawColour, 0)
    }

    function test_aRemoveThatLosesSolutionsAsksFirst() {
        App.document.openFile(Qt.resolvedUrl("../../../examples/SolidSixPieceBurrs.xmpuzzle"))
        tryVerify(() => App.shapes.count > 0)
        addColour()
        // the colour on a voxel of every shape the problems use -- through the
        // model's own paths only: the grid paints with it in P4, so here a
        // C++ test does the painting; this checks the question and its answer
        // on a colour that asks (removeConfirmRequested), sent as the model would
        App.colours.removeConfirmRequested(1, "C1")
        let dialog = null
        tryVerify(() => (dialog = findChild(win, "entities.editor.colour.removeDialog")) !== null && dialog.opened, 1000)
        verify(dialog.title.indexOf("C1") >= 0)
        verify(App.commands.blocked)                     // counted as modal
        mouseClick(findChild(dialog.footer, "entities.editor.colour.removeDialog.cancel"))
        tryVerify(() => !dialog.visible, 1000)
        compare(App.colours.count, 2)
        App.colours.removeConfirmRequested(1, "C1")
        tryVerify(() => dialog.opened, 1000)
        mouseClick(findChild(dialog.footer, "entities.editor.colour.removeDialog.confirm"))
        tryVerify(() => !dialog.visible, 1000)
        compare(App.colours.count, 1)
    }

    function test_theRailShowsTheDrawingColour() {
        // AC-C09-06
        addColour()
        App.layout.rightCollapsed = true
        const rail = item("entities.editor.colour.rail")
        tryVerify(() => rail.visible, 1000)
        compare(rail.color, Qt.color("#ef4444"))
        App.colours.select(0)
        compare(rail.color, App.viewport.shapeColor)
        App.layout.workspace = LayoutController.Puzzle
        verify(!rail.visible)
    }

    function test_aClickFocusesThePaletteWithoutTheRing() {
        // the ring is for the keyboard (Tab); a click only gives it the keys
        addColour()
        mouseClick(swatch(1))
        const palette = item("entities.editor.colour.palette")
        verify(palette.activeFocus)
        verify(!palette.visualFocus)
        compare(palette.background.border.width, 0)
    }

    function test_doubleClickEditsAColour() {
        addColour()
        mouseDoubleClickSequence(swatch(1))
        compare(chooser.count, 1)                        // Edit colour, starting at it
        compare(chooser.signalArguments[0][1], App.colours.colourOf(1))
        compare(App.colours.drawColour, 1)
        closeChooser()
        chooser.clear()
        mouseDoubleClickSequence(swatch(0))              // Default has no colour to edit
        compare(chooser.count, 0)
        compare(App.colours.drawColour, 0)
    }

    function test_thePalettesKeys() {
        // the keyboard's way to everything the mouse does: Home / End, Enter
        // edits, Delete removes (Default has neither)
        addColour()
        addColour()
        mouseClick(swatch(0))
        keyClick(Qt.Key_End)
        compare(App.colours.drawColour, 2)
        keyClick(Qt.Key_Home)
        compare(App.colours.drawColour, 0)
        keyClick(Qt.Key_Return)                          // Default: nothing to edit
        compare(chooser.count, 0)
        keyClick(Qt.Key_Delete)
        compare(App.colours.count, 3)
        keyClick(Qt.Key_End)
        keyClick(Qt.Key_Delete)                          // unused: goes at once
        compare(App.colours.count, 2)
        compare(App.colours.drawColour, 0)
        verify(item("status.text").text.indexOf("C2") >= 0)   // and is announced
        keyClick(Qt.Key_End)
        keyClick(Qt.Key_Return)                          // Edit: the chooser -- last, as focus after it is unreliable in a test window
        compare(chooser.count, 1)
        closeChooser()
    }

    function test_theKeyboardSelectionScrollsIntoView() {
        // the palette scrolls after three rows; the chosen swatch stays in sight
        for (let i = 0; i < 40; i++)
            App.colours.addColour()
        waitForRendering(win.contentItem)
        const palette = item("entities.editor.colour.palette")
        const flick = palette.contentItem
        verify(flick.contentHeight > flick.height, "the palette scrolls")
        // the colour just added (drawn with) is in view, and again after a trip to the top
        let s = swatch(40)
        verify(s.y + s.height <= flick.contentY + flick.height + 1, "the new swatch is in view")
        App.colours.select(0)
        compare(flick.contentY, 0)
        App.colours.select(40)
        waitForRendering(win.contentItem)
        s = swatch(40)
        verify(s.y + s.height <= flick.contentY + flick.height + 1, "the last swatch is in view")
        App.colours.select(0)
        compare(flick.contentY, 0)
    }

    function test_defaultsMarkStaysVisibleWithoutAShape() {
        // no shape selected: the Default swatch's ring is muted, not white on a pale fill
        App.document.newDocument(0)
        tryCompare(App.shapes, "count", 0)
        const ring = swatch(0).children[2].children[0]
        verify(ring.visible)
        compare(ring.border.color, Theme.muted)
    }

    function test_theLimitDisablesAdd() {
        for (let i = 0; i < 63; i++)
            App.colours.addColour()
        compare(App.colours.count, 64)
        verify(!item("entities.editor.colour.add").enabled)
    }

    function test_thePaletteRunsAcrossTheCard() {
        // the swatches side by side, the palette as wide as the card: one row
        // holds several (it showed one per row beside the name)
        for (let i = 0; i < 5; i++)
            addColour()
        waitForRendering(win.contentItem)
        for (let i = 1; i < 6; i++) {
            compare(swatch(i).y, swatch(0).y, "swatch " + i + " in the first row")
            verify(swatch(i).x > swatch(i - 1).x)
        }
        const card = item("entities.editor")
        verify(item("entities.editor.colour.palette").width > card.width - 40)
    }
}
