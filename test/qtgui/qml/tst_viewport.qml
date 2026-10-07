import QtQuick
import QtTest
import BurrTools.Ui

// The 3D viewport card (C06, C13) through the real Main.qml.
TestCase {
    id: tc
    name: "Viewport"
    when: windowShown

    Component { id: mainComponent; Main {} }

    property var win

    function init() {
        failOnWarning(/^(?!No QRhi found for window|This plugin does not support grabbing the keyboard).*/)
        App.layout.workspace = LayoutController.Entities
        App.layout.leftCollapsed = false
        App.layout.rightCollapsed = false
        App.viewport.navMode = "orbit"
        App.settings.showViewCube = true
        win = createTemporaryObject(mainComponent, tc)
        verify(win !== null)
        win.width = 1600
        win.height = 1000
        App.document.openFile(Qt.resolvedUrl("../../../examples/PelikanBurr.xmpuzzle"))
        waitForRendering(win.contentItem)
    }

    function item(name) {
        const o = findChild(win.contentItem, name)
        verify(o !== null, "no item " + name)
        return o
    }

    function test_toolbarHasItsFiveButtonsInOrder() {
        // AC-C06-01: Orbit, Pan, Edit, Display, Focus -- no Zoom, Views or Fit
        const names = ["orbit", "pan", "edit", "display", "focus"]
        let lastX = -1
        for (const n of names) {
            const b = item("entities.viewport.toolbar." + n)
            verify(b.visible, n)
            const x = b.mapToItem(null, 0, 0).x
            verify(x > lastX, n + " out of order")
            lastX = x
        }
        for (const n of ["zoom", "views", "fit"])
            compare(findChild(win.contentItem, "entities.viewport.toolbar." + n), null)
        // editing in 3D arrives with the editing phase
        verify(!item("entities.viewport.toolbar.edit").enabled)
    }

    function test_orbitAndPanAreARadioPair() {
        mouseClick(item("entities.viewport.toolbar.pan"))
        compare(App.viewport.navMode, "pan")
        verify(item("entities.viewport.toolbar.pan").on)
        verify(!item("entities.viewport.toolbar.orbit").on)
        mouseClick(item("entities.viewport.toolbar.orbit"))
        compare(App.viewport.navMode, "orbit")
    }

    function test_displayMenuStaysOpenAndPersists() {
        // T-C06-8
        mouseClick(item("entities.viewport.toolbar.display"))
        const menu = findChild(item("entities.viewport.toolbar.display"), "entities.viewport.display.menu")
        verify(menu !== null)
        tryVerify(() => menu.opened, 1000)

        const axes = findChild(menu.contentItem, "entities.viewport.display.axes")
        verify(axes.selected)
        mouseClick(axes)
        verify(!App.viewport.displayAxes)
        verify(menu.opened, "the menu closed on a toggle")
        mouseClick(axes)
        verify(App.viewport.displayAxes)

        const ortho = findChild(menu.contentItem, "entities.viewport.display.ortho")
        mouseClick(ortho)
        compare(App.viewport.projection, "orthographic")
        mouseClick(findChild(menu.contentItem, "entities.viewport.display.persp"))
        compare(App.viewport.projection, "perspective")
        menu.close()
    }

    function test_slabRowsNeedTheVoxelEditor() {
        mouseClick(item("entities.viewport.toolbar.display"))
        const menu = findChild(item("entities.viewport.toolbar.display"), "entities.viewport.display.menu")
        tryVerify(() => menu.opened, 1000)
        const slab = findChild(menu.contentItem, "entities.viewport.display.layer")
        verify(slab.enabled)
        App.layout.rightCollapsed = true
        verify(!slab.enabled)
        compare(slab.hint, "needs Voxel editor")
        App.layout.rightCollapsed = false
        verify(slab.enabled)
        menu.close()
    }

    function test_viewCubeVisibilityAndPlacement() {
        // AC-C13-01 / AC-C13-15
        const cube = item("entities.viewport.cube")
        const card = item("entities.viewport")
        verify(cube.visible)
        compare(cube.y, -11)
        compare(cube.x, card.width - cube.width - 2)
        App.settings.showViewCube = false
        verify(!cube.visible)
        App.settings.showViewCube = true
        App.layout.toggleFocus2d()
        verify(!cube.visible)
        App.layout.escape()
        verify(cube.visible)
    }

    function test_tagAndHintFollowTheView() {
        const tag = item("entities.viewport.tag")
        verify(tag.visible)
        App.shapes.select(1)
        compare(App.viewport.shapeId, "S2")
        App.viewport.navMode = "pan"
        App.viewport.navMode = "orbit"
        App.layout.toggleFocus2d()
        verify(!tag.visible)
        verify(!item("entities.viewport.hint").visible)
        App.layout.escape()
    }

    function test_compactToolbarInFocus2d() {
        // T-C06-6
        App.layout.toggleFocus2d()
        compare(item("entities.viewport.toolbar.orbit").width, 34)
        App.layout.escape()
        compare(item("entities.viewport.toolbar.orbit").width, Theme.toolbarButtonWidth)
    }
}
