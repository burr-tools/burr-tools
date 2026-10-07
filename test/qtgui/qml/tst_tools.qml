import QtQuick
import QtTest
import BurrTools.Ui

// The legacy menu tools ported as dialogs (OQ-34): Convert, Import
// assemblies and Status, opened through their real commands in Main.qml.
TestCase {
    id: tc
    name: "Tools"
    when: windowShown

    Component { id: mainComponent; Main {} }

    property var win

    function init() {
        // see tst_shell.qml for why these two warnings are allowed
        failOnWarning(/^(?!No QRhi found for window|This plugin does not support grabbing the keyboard).*/)
        App.layout.workspace = LayoutController.Entities
        win = createTemporaryObject(mainComponent, tc)
        verify(win !== null, "Main.qml failed to load")
        win.width = 1400
        win.height = 900
        App.document.newDocument(0)
        waitForRendering(win.contentItem)
    }

    function cleanup() {
        App.settings.tooltips = true
        // leave no dialog open for the next case
        for (const name of ["tools.convert.dialog", "tools.import.dialog", "tools.status.dialog", "export.stl.dialog", "export.vector.dialog", "export.image.dialog"]) {
            const d = findChild(win, name)
            if (d && d.visible)
                d.close()
        }
    }

    function open(command, name) {
        App.commands.trigger(command)
        const d = findChild(win, name)
        verify(d !== null, "no dialog " + name)
        tryVerify(() => d.opened, 2000)
        // onAboutToShow changes which buttons show; let the layout settle
        // before clicking, as a user's first click always comes later
        waitForRendering(d.contentItem)
        return d
    }

    function inside(dialog, name) {
        const o = findChild(dialog.contentItem, name) || findChild(dialog.footer, name)
        verify(o !== null, "no item " + name)
        return o
    }

    function load(file) {
        App.document.openFile(Qt.resolvedUrl("../../../examples/" + file))
        compare(App.document.fileName, file)
    }

    function test_convertListsTheTargetsAndConverts() {
        load("PelikanBurr.xmpuzzle")
        const targets = App.tools.convertTargets()
        verify(targets.length > 0)
        const d = open("file.convert", "tools.convert.dialog")
        const first = inside(d, "tools.convert.type." + targets[0].type)
        verify(first.selected)
        mouseClick(inside(d, "tools.convert.ok"))
        tryVerify(() => !d.visible, 2000)
        compare(App.document.gridType, targets[0].type)
        verify(App.document.modified)
    }

    function test_convertCancelChangesNothing() {
        load("PelikanBurr.xmpuzzle")
        const d = open("file.convert", "tools.convert.dialog")
        mouseClick(inside(d, "tools.convert.cancel"))
        tryVerify(() => !d.visible, 2000)
        compare(App.document.gridType, 0)
        verify(!App.document.modified)
    }

    function test_importWithoutProblemsExplains() {
        const d = open("file.importAssemblies", "tools.import.dialog")
        verify(!d.hasProblems)
        verify(!inside(d, "tools.import.ok").visible)
        mouseClick(inside(d, "tools.import.cancel"))
        tryVerify(() => !d.visible, 2000)
    }

    function test_importAddsShapesToANewProblem() {
        load("SolidSixPieceBurrs.xmpuzzle")
        const before = App.shapes.count
        const d = open("file.importAssemblies", "tools.import.dialog")
        verify(d.hasProblems)
        verify(inside(d, "tools.import.dropNonMillable").visible)     // a brick puzzle
        verify(!inside(d, "tools.import.target").enabled)
        mouseClick(inside(d, "tools.import.dest.new"))
        compare(d.destination, "new")
        verify(inside(d, "tools.import.dest.new").selected)
        verify(!inside(d, "tools.import.dest.shapes").selected)
        mouseClick(inside(d, "tools.import.ok"))
        tryVerify(() => !d.visible, 2000)
        verify(App.shapes.count > before)
        verify(App.document.canUndo)
    }

    function test_importToolFiltersAreBricksOnly() {
        App.document.newDocument(2)
        const d = open("file.importAssemblies", "tools.import.dialog")
        verify(!d.bricks)
    }

    function test_statusShowsEveryShapeAndRemovesSelected() {
        load("PelikanBurr.xmpuzzle")
        const n = App.shapes.count
        const d = open("status", "tools.status.dialog")
        const model = App.tools.shapeStatus
        tryVerify(() => !model.busy, 5000)
        compare(model.rowCount(), n)
        const table = inside(d, "tools.status.table")
        tryCompare(table, "count", n)
        verify(!inside(d, "tools.status.remove").enabled)

        // the view makes its delegates on its next polish
        tryVerify(() => findChild(table.contentItem, "tools.status.select.0") !== null, 2000)
        mouseClick(findChild(table.contentItem, "tools.status.select.0"))
        compare(model.selectedCount, 1)
        verify(inside(d, "tools.status.remove").enabled)
        mouseClick(inside(d, "tools.status.remove"))
        compare(App.shapes.count, n - 1)
        tryVerify(() => !model.busy, 5000)
        compare(model.rowCount(), n - 1)
        verify(d.visible)       // stays open with the new table, as legacy reopens

        mouseClick(inside(d, "tools.status.close"))
        tryVerify(() => !d.visible, 2000)
        compare(model.rowCount(), 0)
    }

    function test_statusOfAnEmptyPuzzle() {
        const d = open("status", "tools.status.dialog")
        tryVerify(() => !App.tools.shapeStatus.busy, 2000)
        verify(!inside(d, "tools.status.selectHoles").enabled)
    }

    function test_dialogsBlockTheShortcuts() {
        load("PelikanBurr.xmpuzzle")
        const d = open("status", "tools.status.dialog")
        verify(win.modalOpen)
        const shortcuts = findChild(win, "shell.shortcuts")
        verify(shortcuts !== null && shortcuts.count > 0)
        for (let i = 0; i < shortcuts.count; i++)
            verify(!shortcuts.objectAt(i).enabled, "shortcut " + i + " is live under a dialog")
        d.close()
        tryVerify(() => !win.modalOpen, 2000)
        for (let i = 0; i < shortcuts.count; i++)
            verify(shortcuts.objectAt(i).enabled)
    }

    function test_stlExportPreviewsAndEditsParameters() {
        load("PelikanBurr.xmpuzzle")
        App.shapes.select(0)
        const d = open("export.stl", "export.stl.dialog")
        const stl = App.stl
        // S1 has variable voxels: no mesh, the reason shows, nothing to export
        verify(inside(d, "export.stl.error").visible)
        verify(!inside(d, "export.stl.export").enabled)

        const list = inside(d, "export.stl.shapes")
        tryVerify(() => findChild(list.contentItem, "export.stl.shape.2") !== null, 2000)
        mouseClick(findChild(list.contentItem, "export.stl.shape.2"))
        compare(stl.shape, 2)
        verify(stl.hasMesh)
        verify(!inside(d, "export.stl.error").visible)
        verify(inside(d, "export.stl.export").enabled)
        verify(inside(d, "export.stl.volume").text.startsWith("Volume"))

        // one field per parameter; a committed edit reaches the exporter
        compare(stl.parameterCount, 6)
        const field = inside(d, "export.stl.param.field.0")
        const before = stl.volumeText
        // as typing "20" and pressing Enter does (synthesised keys do not
        // reliably reach a second top-level window on the offscreen platform)
        field.text = "20"
        field.editingFinished()
        compare(stl.parameter(0).value, 20)
        compare(field.text, "20.00")
        verify(stl.volumeText !== before)
        verify(inside(d, "export.stl.param.field.0") === field)    // the field stayed

        mouseClick(inside(d, "export.stl.close"))
        tryVerify(() => !d.visible, 2000)
        verify(!stl.hasMesh)
    }

    function test_stlParameterTooltipsFollowTheSetting() {
        // PR-14: the parameters' tooltips used to show with Settings ▸ Show
        // tooltips off
        load("PelikanBurr.xmpuzzle")
        App.shapes.select(2)
        const d = open("export.stl", "export.stl.dialog")
        const field = inside(d, "export.stl.param.field.0")
        const tip = field.toolTip
        compare(tip.text, App.stl.parameter(0).tooltip)
        verify(tip.text.length > 0)

        mouseMove(field, field.width / 2, field.height / 2)
        tryVerify(() => tip.opened, 2000)
        App.settings.tooltips = false
        tryVerify(() => !tip.visible, 1000)
        mouseMove(field, 6, 6)
        wait(Theme.tooltipDelay + 200)
        verify(!tip.visible, "shown with tooltips off")
    }

    function test_stlExportIsOffForAnEmptyPuzzle() {
        App.commands.trigger("export.stl")
        const d = findChild(win, "export.stl.dialog")
        wait(50)
        verify(!d.visible)
    }

    function test_vectorExportOffersTheSixLegacyTypes() {
        load("PelikanBurr.xmpuzzle")
        const d = open("export.vector", "export.vector.dialog")
        for (let i = 0; i <= 5; i++)
            verify(inside(d, "export.vector.format." + i).visible)
        verify(inside(d, "export.vector.format.4").selected)        // SVG, as legacy
        mouseClick(inside(d, "export.vector.format.3"))
        compare(d.format, 3)
        verify(d.suggestedFile().endsWith("/PelikanBurr.pdf"))
        mouseClick(inside(d, "export.vector.cancel"))
        tryVerify(() => !d.visible, 2000)
    }

    function test_imageExportFollowsWhatThePuzzleHas() {
        load("PelikanBurr.xmpuzzle")
        const d = open("export.image", "export.image.dialog")
        const ex = App.images
        compare(ex.mode, "solution")
        verify(inside(d, "export.image.mode.solution").selected)
        verify(inside(d, "export.image.mode.disassembly").enabled)

        mouseClick(inside(d, "export.image.mode.shape"))
        compare(ex.mode, "shape")
        verify(!inside(d, "export.image.problems").enabled)

        // a paper size fills the millimetres and the pixels; manual frees them
        verify(inside(d, "export.image.mmX").enabled)
        mouseClick(inside(d, "export.image.paper.a4l"))
        compare(ex.pixelX, 3508)
        compare(inside(d, "export.image.pxX").text, "3508")
        verify(!inside(d, "export.image.mmX").enabled)

        mouseClick(inside(d, "export.image.close"))
        tryVerify(() => !d.visible, 2000)
    }

    function test_imageExportOfAnEmptyPuzzleIsOff() {
        verify(!App.commands.isEnabled("export.image"))
    }
}
