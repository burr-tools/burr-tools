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
        tryVerify(() => win.contentReady, 5000)       // made after the first frame
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
        // Edit mode (P5): a toggle
        verify(item("entities.viewport.toolbar.edit").enabled)
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

    function cleanup() {
        App.editor.editMode = false
        App.editor.tool = EditorController.Fixed
        App.settings.singleKeyShortcuts = true
    }

    // keys reach the window's shortcuts only while it is the active one
    // and while no modal is up: the puzzle's comment comes up as a message
    function activate() {
        win.requestActivate()
        tryVerify(() => win.active, 3000)
        for (let i = 0; i < 5; i++) {
            wait(50)
            const m = findChild(win, "shell.message")
            if (m === null || !m.visible)
                break
            m.close()
        }
        tryVerify(() => !win.modalOpen, 1000)
    }

    function test_theEditButtonAndEToggleEditMode() {
        // AC-C06-07 / 09: the button and E latch Edit mode; the button shows
        // the tool, on in Edit mode, and its tooltip says what a click does
        activate()
        const edit = item("entities.viewport.toolbar.edit")
        verify(!App.editor.editMode)
        verify(edit.tip.indexOf("Press E to turn edit mode on") >= 0)
        compare(edit.badge, String(App.editor.toolInEffect + 1))
        mouseClick(edit)
        verify(App.editor.editMode)
        verify(edit.on)
        verify(edit.tip.indexOf("Press E to leave edit mode") >= 0)
        keyClick(Qt.Key_E)
        verify(!App.editor.editMode)
        keyClick(Qt.Key_E)
        verify(App.editor.editMode)
        App.editor.tool = EditorController.Erase
        compare(edit.badge, "3")
        compare(edit.glyphName, "edit-tool-erase")
    }

    function test_singleKeyShortcutsCanBeTurnedOff() {
        // WCAG 2.1.4: Settings ▸ General ▸ Single-key shortcuts
        activate()
        App.settings.singleKeyShortcuts = false
        keyClick(Qt.Key_P)
        compare(App.viewport.navMode, "orbit")
        keyClick(Qt.Key_E)
        verify(!App.editor.editMode)
        keyClick(Qt.Key_2)
        compare(App.editor.tool, EditorController.Fixed)
        App.settings.singleKeyShortcuts = true
        keyClick(Qt.Key_P)
        compare(App.viewport.navMode, "pan")
        keyClick(Qt.Key_E)
        verify(App.editor.editMode)
    }

    function test_tabReachesTheCanvasAndRingsTheCard() {
        // WCAG 2.1.1 / 2.4.7: the canvas is a Tab stop after the toolbar,
        // ringed on the card's edge, and its keys are the viewport's
        activate()
        const canvas = item("entities.viewport.surface")
        const ring = item("entities.viewport.focusRing")
        verify(canvas.activeFocusOnTab)
        verify(!ring.visible)
        item("entities.viewport.toolbar.focus").forceActiveFocus(Qt.TabFocusReason)
        keyClick(Qt.Key_Tab)
        verify(canvas.activeFocus)
        verify(ring.visible)
        // a key of its own, not the window's: O stays the window's orbit mode
        App.viewport.navMode = "pan"
        keyClick(Qt.Key_O)
        compare(App.viewport.navMode, "orbit")
        keyClick(Qt.Key_Left)                              // orbits; the shapes list is not involved
        compare(App.shapes.selected, App.shapes.selected)
        // a click in the view takes no focus (C20)
        item("entities.viewport.toolbar.orbit").forceActiveFocus(Qt.TabFocusReason)
        mouseClick(canvas, 300, 300)
        verify(!canvas.activeFocus)
    }

    function test_theHintNamesTheQuickTools() {
        // C06 hint line 1, in Entities
        verify(item("entities.viewport.hint.tools").visible)
        App.layout.workspace = LayoutController.Puzzle
        verify(!item("entities.viewport.hint.tools").visible)
    }
}
