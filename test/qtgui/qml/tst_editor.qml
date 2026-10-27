import QtQuick
import QtQuick.Controls
import QtTest
import BurrTools.Ui

// The voxel editor card (C07, C08, C02) through the real Main.qml: the tool
// strip and its keys, Mirror and Span, the planes and the layer strip, the
// 2D grid painting with the pointer, and the grid size steppers.
TestCase {
    id: tc
    name: "Editor"
    when: windowShown

    Component { id: mainComponent; Main {} }

    property var win

    function init() {
        // as tst_shell: the headless platform's two warnings are expected
        failOnWarning(/^(?!No QRhi found for window|This plugin does not support grabbing the keyboard).*/)
        App.layout.workspace = LayoutController.Entities
        App.layout.leftCollapsed = false
        App.layout.rightCollapsed = false
        App.document.newDocument(0)                       // bricks
        App.shapes.addShape()
        App.shapes.select(0)
        App.editor.tool = EditorController.Fixed
        App.editor.mirror = 0
        App.editor.span = 0
        App.editor.plane = EditorController.XY
        App.editor.applyToAll = false
        win = createTemporaryObject(mainComponent, tc)
        verify(win !== null, "Main.qml failed to load")
        win.width = 1600
        win.height = 1000
        waitForRendering(win.contentItem)
        tryVerify(() => win.contentReady, 5000)
        win.requestActivate()
        tryVerify(() => win.active, 3000)
    }

    function cleanup() {
        App.layout.workspace = LayoutController.Entities
        App.layout.leftCollapsed = false
        App.layout.rightCollapsed = false
    }

    function item(name) {
        let o = null
        tryVerify(() => (o = findChild(win.contentItem, name)) !== null, 1000, "no item " + name)
        return o
    }
    function voxels() {
        return App.shapes.data(App.shapes.index(0, 0), ShapesModel.FixedRole)
    }
    function variables() {
        return App.shapes.data(App.shapes.index(0, 0), ShapesModel.VariableRole)
    }
    function clickCell(col, row, button, modifiers) {
        const g = item("entities.editor.grid")
        const p = g.cellCentre(col, row)
        verify(p.x >= 0, "cell " + col + "," + row)
        mouseClick(g, p.x, p.y, button === undefined ? Qt.LeftButton : button,
                   modifiers === undefined ? Qt.NoModifier : modifiers)
    }

    function test_keysAndButtonsPickTheTool() {
        // AC-C07-01: four buttons, keys 1-4, one selected
        for (const name of ["fixed", "variable", "erase", "paint"])
            item("entities.editor.tool." + name)
        mouseClick(item("entities.editor.tool.erase"))
        compare(App.editor.tool, EditorController.Erase)
        verify(item("entities.editor.tool.erase").on)
        verify(!item("entities.editor.tool.fixed").on)
        mouseClick(item("entities.editor.grid"), 2, 2)      // the keys work from anywhere in Entities
        keyClick(Qt.Key_2)
        compare(App.editor.tool, EditorController.Variable)
        keyClick(Qt.Key_4)
        compare(App.editor.tool, EditorController.Paint)
        // the 3D Edit button shows the tool in place (AC-C07-09)
        compare(item("entities.viewport.toolbar.edit").badge, "4")
        compare(item("entities.viewport.toolbar.edit").glyphName, "edit-tool-paint")
        keyClick(Qt.Key_1)
        compare(App.editor.tool, EditorController.Fixed)
    }

    function test_theToolStripIsOneTabStopMovedWithArrows() {
        // C07 Keyboard: a roving tab index, ← / → move and select
        const strip = item("entities.editor.tools.group")
        strip.forceActiveFocus(Qt.TabFocusReason)
        verify(item("entities.editor.tool.fixed").ring)
        keyClick(Qt.Key_Right)
        compare(App.editor.tool, EditorController.Variable)
        verify(strip.activeFocus)
        verify(item("entities.editor.tool.variable").ring)
        verify(!item("entities.editor.tool.fixed").ring)
        keyClick(Qt.Key_Right)
        keyClick(Qt.Key_Right)
        keyClick(Qt.Key_Right)                                // stays at the end
        compare(App.editor.tool, EditorController.Paint)
        keyClick(Qt.Key_Left)
        compare(App.editor.tool, EditorController.Erase)
        // the planes: a disabled one is skipped
        App.document.newDocument(2)
        App.shapes.addShape()
        App.shapes.select(0)
        const planes = item("entities.editor.plane.group")
        planes.forceActiveFocus(Qt.TabFocusReason)
        keyClick(Qt.Key_Right)
        compare(App.editor.plane, EditorController.XY)
    }

    function test_clickingAndDraggingTheGridPaints() {
        // T-C07-4 / AC-C08-05 / AC-C08-RC through the pointer
        compare(voxels(), 0)
        const g = item("entities.editor.grid")
        const a = g.cellCentre(0, 0), b = g.cellCentre(3, 0)
        mousePress(g, a.x, a.y)
        mouseMove(g, b.x, b.y)
        mouseRelease(g, b.x, b.y)
        compare(voxels(), 4)
        App.document.undo()
        compare(voxels(), 0)
        App.document.redo()
        clickCell(1, 0, Qt.RightButton)                      // right-click erases
        compare(voxels(), 3)
        clickCell(1, 1, Qt.LeftButton, Qt.AltModifier)       // Alt: variable
        compare(variables(), 1)
        verify(item("entities.editor.layer.1.dot").visible)
    }

    function test_mirrorAndSpanToggle() {
        // AC-C07-03 / 04 through the buttons
        mouseClick(item("entities.editor.mirror.x"))
        verify(item("entities.editor.mirror.x").on)
        compare(App.editor.mirror, 1)
        clickCell(0, 0)
        compare(voxels(), 2)
        mouseClick(item("entities.editor.mirror.x"))
        mouseClick(item("entities.editor.span.y"))
        compare(App.editor.span, 2)
        clickCell(1, 0)
        compare(voxels(), 2 + App.editor.sizeY)
    }

    function test_planesAndLayers() {
        // AC-C08-01 / 02: the label, the strip, the chips
        const label = item("entities.editor.layerLabel")
        compare(label.text, "Layer 1 of " + App.editor.layerCount)
        mouseClick(item("entities.editor.plane.xz"))
        compare(App.editor.plane, EditorController.XZ)
        compare(label.text, "Layer 1 of " + App.editor.sizeY)
        const top = App.editor.layerCount
        mouseClick(item("entities.editor.layer." + top))
        compare(App.editor.layer, top - 1)
        compare(label.text, "Layer " + top + " of " + top)
        keyClick(Qt.Key_PageDown)
        compare(App.editor.layer, top - 2)
    }

    function test_otherVoxelTypesEditXyOnly() {
        // product decision 2026-10-27: XZ and YZ for bricks only
        App.document.newDocument(2)                          // spheres
        App.shapes.addShape()
        App.shapes.select(0)
        waitForRendering(win.contentItem)
        verify(!item("entities.editor.plane.xz").enabled)
        verify(!item("entities.editor.plane.yz").enabled)
        verify(item("entities.editor.plane.xy").on)
        mouseClick(item("entities.editor.plane.yz"))
        compare(App.editor.plane, EditorController.XY)
    }

    function test_theSteppersResizeAndRefuseToCutVoxels() {
        // T-C02-1 / AC-C02-02 / 04
        const x = item("entities.editor.size.x")
        const before = App.editor.sizeX
        mouseClick(item("entities.editor.size.x.inc"))
        compare(App.editor.sizeX, before + 1)
        compare(x.value, before + 1)
        const input = item("entities.editor.size.x.input")
        input.forceActiveFocus()
        input.selectAll()
        keyClick(Qt.Key_2)
        keyClick(Qt.Key_Return)
        compare(App.editor.sizeX, 2)
        keyClick(Qt.Key_Up)
        compare(App.editor.sizeX, 3)
        // a voxel at x = 2: a shrink to 2 is blocked, with a message
        clickCell(2, 0)
        input.forceActiveFocus()
        input.selectAll()
        keyClick(Qt.Key_2)
        keyClick(Qt.Key_Return)
        compare(App.editor.sizeX, 3)
        compare(input.text, "3")
        verify(App.status.plainText.indexOf("Blocked") >= 0, App.status.plainText)
        // invalid text reverts
        input.selectAll()
        keyClick(Qt.Key_A)
        keyClick(Qt.Key_Return)
        compare(input.text, "3")
    }

    function test_hoverShowsTheCursorReadout() {
        // T-C08-4
        const g = item("entities.editor.grid")
        const p = g.cellCentre(2, 1)
        mouseMove(g, p.x, p.y)
        tryCompare(App.status, "cursorText", "Cursor X2 Y1 · Z0")
        mouseMove(item("entities.editor.tool.fixed"), 5, 5)
        tryCompare(App.status, "cursorText", "")
    }
}
