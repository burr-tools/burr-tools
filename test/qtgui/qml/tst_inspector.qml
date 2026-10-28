import QtQuick
import QtQuick.Controls
import QtTest
import BurrTools.Ui

// The inspector under the Shapes list (P6: C03 Fit & scale, C04 Transform,
// C05 Repair & surface) through the real Main.qml: where it sits, its
// sections, its buttons by id per voxel type, why a button cannot run, the
// Transform preview, and the Minimal density.
TestCase {
    id: tc
    name: "Inspector"
    when: windowShown

    Component { id: mainComponent; Main {} }

    property var win

    function init() {
        failOnWarning(/^(?!No QRhi found for window|This plugin does not support grabbing the keyboard).*/)
        App.layout.workspace = LayoutController.Entities
        App.layout.leftCollapsed = false
        App.layout.rightCollapsed = false
        App.settings.density = "standard"
        App.inspector.transformOpen = true
        App.inspector.fitOpen = false
        App.inspector.repairOpen = false
        App.editor.applyToAll = false
        // the voxel editor as a click on the grid expects it (other files change it)
        App.editor.tool = EditorController.Fixed
        App.editor.mirror = 0
        App.editor.span = 0
        App.editor.plane = EditorController.XY
        App.editor.editMode = false
        newBricks(0)
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
        App.settings.density = "standard"
        App.inspector.clearPreview()
        App.inspector.transformOpen = true
        App.inspector.fitOpen = false
        App.inspector.repairOpen = false
    }

    // a new file of the type with one empty 3 x 3 x 3 shape, selected
    function newBricks(type) {
        App.document.newDocument(type)
        if (App.shapes.count === 0)
            App.shapes.addShape()
        App.shapes.select(0)
        for (let a = 0; a < 3; a++)
            App.editor.setSize(a, 3)
    }

    function item(name) {
        const o = findChild(win.contentItem, name)
        verify(o !== null, "no item " + name)
        return o
    }
    function clickCell(col, row) {
        const g = item("entities.editor.grid")
        const p = g.cellCentre(col, row)
        verify(p.x >= 0, "cell " + col + "," + row)
        mouseClick(g, p.x, p.y)
    }
    function status() { return item("status.text").text }

    function test_itSitsUnderTheListAndNamesTheShape() {
        // the mock: the list takes its rows (60 to 250 dp), the inspector the rest
        const list = item("entities.shapes.list")
        const insp = item("entities.inspector")
        const top = insp.mapToItem(list, 0, 0).y
        compare(top, Math.max(48, list.count * list.pitch - list.spacing) + 6)
        for (let i = 0; i < 9; i++)
            App.shapes.addShape()
        tryCompare(list, "count", 10)
        compare(insp.mapToItem(list, 0, 0).y, 238 + 6)     // 250 with the padding, then the list scrolls
        verify(insp.height > 300)
        compare(item("entities.inspector.head.chip").text, App.viewport.shapeId)
        App.shapes.select(3)
        tryCompare(item("entities.inspector.head.chip"), "text", "S4")
    }

    function test_theSectionsOpenAndRememberIt() {
        // AC-C03-01: Transform open, the others closed; a header toggles, by pointer or key
        verify(item("entities.transform.flip.x").visible)
        verify(!item("entities.fit.prune").visible)
        verify(!item("entities.repair.fillHoles").visible)
        compare(item("entities.inspector.transform.header").height, Theme.accordionHeader)
        mouseClick(item("entities.inspector.fit.header"))
        verify(App.inspector.fitOpen)
        tryVerify(() => item("entities.fit.prune").visible)
        const repair = item("entities.inspector.repair.header")
        repair.forceActiveFocus(Qt.TabFocusReason)
        keyClick(Qt.Key_Space)
        verify(App.inspector.repairOpen)
        keyClick(Qt.Key_Return)
        verify(!App.inspector.repairOpen)
        compare(repair.Accessible.name, "Repair & surface")
    }

    function test_everyControlHasItsId_data() {
        return [
            { tag: "bricks", type: 0, pads: false, scale: true,
              ids: ["transform.flip.x", "transform.rotate.y.neg", "transform.nudge.z.pos"] },
            { tag: "prism", type: 1, pads: true, scale: true,
              ids: ["transform.rotate.x", "transform.rotate.z.pos", "transform.nudge.upLeft", "transform.nudge.z.neg"] },
            { tag: "spheres", type: 2, pads: true, scale: false,
              ids: ["transform.rotate.x.pos", "transform.nudge.up.px", "transform.nudge.in.nxny", "transform.nudge.down.ny"] },
        ]
    }
    function test_everyControlHasItsId(data) {
        // AC-C04-01 / AC-C05-05 / AC-C14-03: the type's own buttons, each with its id and tooltip
        newBricks(data.type)
        App.inspector.fitOpen = true
        App.inspector.repairOpen = true
        compare(item("entities.transform.matrix").visible, !data.pads)
        compare(item("entities.transform.pads").visible, data.pads)
        compare(item("entities.fit.scale").visible, data.scale)    // AC-C03-08
        for (const id of data.ids) {
            const b = item("entities." + id)
            verify(b.visible, id)
            verify(b.Accessible.name.length > 0, id)
        }
        for (const id of ["fit.prune", "fit.center", "fit.origin", "repair.fillHoles",
                          "repair.fixed.inner", "repair.variable.outer", "repair.clear.inner"])
            verify(item("entities." + id).visible, id)
        if (data.scale)
            for (const id of ["fit.minimize", "fit.x2", "fit.x3"])
                verify(item("entities." + id).visible, id)
    }

    function test_aTransformRunsOnceAndIsBlockedAtTheEdge() {
        // AC-C04-03 / 05: a nudge to the edge goes; past it, blocked with the reason, nothing changed
        clickCell(0, 0)
        const undoable = App.document.canUndo
        mouseClick(item("entities.transform.nudge.x.neg"))
        tryVerify(() => status().indexOf("Blocked: the piece would leave the grid") >= 0)
        mouseClick(item("entities.transform.nudge.x.pos"))
        mouseClick(item("entities.transform.nudge.x.pos"))
        mouseClick(item("entities.transform.nudge.x.pos"))      // at the edge now: blocked
        tryVerify(() => status().indexOf("Blocked") >= 0)
        App.document.undo()
        App.document.undo()                                   // two steps: the two that ran
        mouseClick(item("entities.transform.nudge.x.neg"))      // back at x 0: blocked again
        tryVerify(() => status().indexOf("Blocked") >= 0)
        verify(undoable)
    }

    function test_aButtonThatCannotRunSaysWhy() {
        // PR-03 / AC-C03-02 / 04: an empty shape has nothing to fit; the hint says so
        App.inspector.fitOpen = true
        const prune = item("entities.fit.prune")
        tryVerify(() => prune.visible)
        tryVerify(() => !prune.available, 2000)              // the states settle after the new file's edits
        compare(prune.hint, "The shape is empty")
        compare(prune.opacity, 0.4)
        mouseClick(prune)                                     // still pressed: the status bar says why
        tryVerify(() => status().indexOf("The shape is empty") >= 0)

        clickCell(1, 1)
        tryVerify(() => prune.available, 2000)               // once the edits pause
        compare(prune.hint, "Shrink the grid to the piece’s bounding box")
        const minimize = item("entities.fit.minimize")
        verify(!minimize.available)
        compare(minimize.hint, "Already at minimum resolution")
        mouseClick(item("entities.fit.x2"))
        tryVerify(() => minimize.available, 2000)
        compare(minimize.hint, "Reduce resolution ÷2, same shape")
        App.editor.applyToAll = true
        verify(item("entities.fit.allShapes").visible)
    }

    function test_aTransformButtonPreviewsWhileHoveredOrFocused() {
        // user decision P6: the result in 3D under the pointer or the focus; leaving clears it
        clickCell(1, 1)
        const rot = item("entities.transform.rotate.x.pos")
        mouseMove(rot, rot.width / 2, rot.height / 2)
        tryCompare(App.inspector, "previewTask", 7)
        const flip = item("entities.transform.flip.y")
        mouseMove(flip, flip.width / 2, flip.height / 2)       // straight to the next one
        tryCompare(App.inspector, "previewTask", 13)
        mouseMove(item("entities.inspector.head"), 4, 4)
        tryCompare(App.inspector, "previewTask", -1)
        flip.forceActiveFocus(Qt.TabFocusReason)
        tryCompare(App.inspector, "previewTask", 13)
        keyClick(Qt.Key_Tab)
        tryCompare(App.inspector, "previewTask", 9)            // rotate.y.neg, the next stop
        item("entities.shapes.list").forceActiveFocus()
        tryCompare(App.inspector, "previewTask", -1)
    }

    // a solid 3 x 3 x 3 cube: Span X + Y fills a layer a click
    function fillCube() {
        App.editor.span = 3
        for (let l = 0; l < 3; l++) {
            App.editor.layer = l
            clickCell(0, 0)
        }
        App.editor.span = 0
        App.editor.layer = 0
    }
    function flashes(name, text) {
        mouseClick(item(name))
        tryVerify(() => status() === text, 1000, name + " said " + status() + ", not " + text)
    }

    function test_eachRepairButtonRunsItsOwnOperation() {
        // C05: the table's six buttons and Fill, each to its own operation
        // (T-C05-2), said in the status bar -- a solid cube: 26 outer, 1 inner
        fillCube()
        App.inspector.repairOpen = true
        tryVerify(() => item("entities.repair.fillHoles").visible)
        flashes("entities.repair.fillHoles", "No interior holes found")
        flashes("entities.repair.variable.inner", "1 inner voxel updated")
        flashes("entities.repair.variable.inner", "No inner voxel to change")
        flashes("entities.repair.fixed.inner", "1 inner voxel updated")
        flashes("entities.repair.fixed.outer", "No outer voxel to change")
        flashes("entities.repair.variable.outer", "26 outer voxels updated")
        flashes("entities.repair.fixed.outer", "26 outer voxels updated")
        flashes("entities.repair.clear.outer", "No outer voxel to change")   // Default already
        flashes("entities.repair.clear.inner", "No inner voxel to change")
    }

    function test_fitAndScaleShowTheNewSizeInTheSteppers() {
        // T-C03-4 / AC-C03-07: after each operation the Grid size steppers
        // show the shape's new dimensions; ×n then Minimize round trip (AC-C03-03)
        clickCell(1, 1)
        App.inspector.fitOpen = true
        const x = item("entities.editor.size.x")
        tryVerify(() => item("entities.fit.x2").visible)
        mouseClick(item("entities.fit.x2"))
        tryCompare(x, "value", 6)
        mouseClick(item("entities.fit.x3"))
        tryCompare(x, "value", 18)
        const minimize = item("entities.fit.minimize")
        // the states settle 120 ms after the edits pause
        tryCompare(minimize, "hint", "Reduce resolution ÷6, same shape", 2000)
        verify(minimize.available)
        mouseClick(minimize)
        tryCompare(x, "value", 3)
        tryVerify(() => status().indexOf("÷6") >= 0)
        mouseClick(item("entities.fit.prune"))                // the voxel's own box
        tryCompare(x, "value", 1)
        compare(item("entities.editor.size.z").value, 1)
        // undone one by one: back through each size
        App.document.undo()
        tryCompare(x, "value", 3)
        App.document.undo()
        tryCompare(x, "value", 18)
    }

    function test_minimalShowsIconsAndShortLabels() {
        // density.md §1-2: icon-only Transform h30, Fit's short labels h32, first-line-only rows
        App.settings.density = "minimal"
        App.inspector.fitOpen = true
        App.inspector.repairOpen = true
        const flip = item("entities.transform.flip.x")
        tryCompare(flip, "height", 30)
        verify(flip.iconOnly)
        compare(item("entities.inspector.fit.header").height, 28)
        const prune = item("entities.fit.prune")
        tryCompare(prune, "height", 32)
        tryVerify(() => prune.width < 120, 1000, "Prune is as wide as its short label: " + prune.width)
        verify(item("entities.fit.x2").horizontal)
        verify(item("entities.repair.fixed.inner").iconOnly)
        verify(item("entities.repair.fixed.inner").tip.length > 0)
        App.settings.density = "standard"
        tryVerify(() => item("entities.fit.prune").height >= 40)
        tryCompare(item("entities.transform.flip.x"), "height", 42)
    }
}
