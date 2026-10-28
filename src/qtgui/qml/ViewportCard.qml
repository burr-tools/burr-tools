pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// C06 3D viewport card: the canvas, the floating toolbar, the Display
// menu, the view tag, the hint, the empty-shape message and the view cube
// (C13). The canvas edits (P5): a quick tool held, or Edit mode (the Edit
// button, E) -- ViewportController picks the target, EditorController
// applies the tool. Tab reaches the canvas, which then has keys of its own
// (the command table's viewport scope) and a ring on the card's edge.
//
// Wireframe (each number marks its code below):
//
//   ┌───────────────────────────────────────────────────────┐
//   │ ②[S1] Result  ③┌Orbit│Pan│Edit│Display│Focus┐   ④┌──┐ │
//   │                └───────────┬─⑤──────────────┘    └──┘ │
//   │                    ┌───────┴──────┐              cube │
//   │                    │ Display menu │                   │
//   │                    └──────────────┘                   │
//   │        ① the 3D canvas, the whole card                │
//   │        ⑦ No shape — open a puzzle ... (when empty)    │
//   │                                                       │
//   │ ⑥ [Shift] click 1 fixed · [Alt] click 2 variable · …  │
//   │   Drag orbit · [Shift] / middle-drag pan · Wheel zoom │
//   └───────────────────────────────────────────────────────┘
//   ⑧ Minimal: the toolbar hidden, a handle where it is (density.md
//     section 3); the pointer near the top, Tab, its menu or O P E F show it
Rectangle {
    id: root
    objectName: "entities.viewport"
    /* transparent: the canvas (z -1, after the toolbar in Tab order) is
     * drawn behind this item's own fill, which would hide it; the fill is
     * the child below, behind the canvas */
    color: "transparent"
    radius: Theme.cardRadius
    border.color: Theme.line
    border.width: Theme.minimal ? 0 : 1
    clip: true

    Rectangle {
        objectName: "entities.viewport.fill"
        anchors.fill: parent
        z: -2
        radius: root.radius
        color: Theme.canvas
    }

    // --- Minimal's auto-hiding toolbar (density.md section 3, AC-DEN-05) ---
    readonly property bool autoHide: Theme.toolbarRevealZone > 0
    // the pointer within the reveal zone of the top edge, held for the hide delay after it leaves
    property bool pointerNear: false
    // a mode key (O P E F) shows it a moment
    property bool modeFlash: false
    readonly property bool toolbarShown: !autoHide || pointerNear || modeFlash || toolbar.activeFocus
                                         || toolbarHover.hovered || displayMenu.visible
    // hide later: 700 ms after the pointer leaves the zone, 500 ms after it leaves the view
    function pointerAway(leftView: bool) {
        if (!pointerNear)
            return
        hideTimer.interval = leftView ? Theme.toolbarHideDelay - 200 : Theme.toolbarHideDelay
        hideTimer.restart()
    }
    Timer { id: hideTimer; onTriggered: root.pointerNear = false }
    Timer { id: flashTimer; interval: 1400; onTriggered: root.modeFlash = false }
    /* The reveal zone along the top edge, over the canvas (which takes the
     * hover events below it): a non-blocking HoverHandler, so the canvas
     * still gets hover and presses. Left through the view's own edge, the
     * pointer has left the view: the shorter delay. */
    Item {
        id: revealZone
        objectName: "entities.viewport.revealZone"
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: Theme.toolbarRevealZone
        visible: root.autoHide
        z: 5
        property point last
        HoverHandler {
            id: zoneHover
            blocking: false
            onPointChanged: if (hovered) revealZone.last = point.position
            onHoveredChanged: {
                if (hovered) {
                    hideTimer.stop()
                    root.pointerNear = true
                } else {
                    const p = revealZone.last
                    root.pointerAway(p.y < 4 || p.x < 4 || p.x > revealZone.width - 4)
                }
            }
        }
    }
    // a press in the view lets go of the toolbar's keyboard focus, so it can hide
    PointHandler {
        enabled: root.autoHide
        onActiveChanged: if (active && toolbar.activeFocus && !toolbarHover.hovered) toolbar.focus = false
    }
    Connections {
        target: App.commands
        enabled: root.autoHide
        function onToolbarModeUsed() {
            root.modeFlash = true
            flashTimer.restart()
        }
    }
    // a density switch starts it afresh
    onAutoHideChanged: {
        pointerNear = false
        modeFlash = false
        hideTimer.stop()
        flashTimer.stop()
    }

    readonly property bool entities: App.layout.workspace === LayoutController.Entities
    readonly property bool focus2d: App.layout.focus === LayoutController.Focus2d
    readonly property bool compact: focus2d || Theme.minimal

    // --- view tag (top left) ---
    Row { // ②
        objectName: "entities.viewport.tag"
        visible: root.entities && !root.focus2d && App.viewport.hasShape
        anchors { left: parent.left; top: parent.top; margins: 14 }
        spacing: 8
        Rectangle {
            width: Math.max(28, tagId.implicitWidth + 10)
            height: 22
            radius: Theme.radiusChip
            color: App.viewport.shapeColor
            Text {
                id: tagId
                anchors.centerIn: parent
                text: App.viewport.shapeId
                color: App.viewport.shapeTextColor
                font.pixelSize: Theme.fontMicro
                font.weight: Font.Bold
            }
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: App.viewport.shapeLabel
            color: Theme.muted
            font.pixelSize: Theme.fontSecondary
        }
    }

    // --- floating toolbar (top centre) ---
    Rectangle { // ③
        id: toolbar
        objectName: "entities.viewport.toolbar"
        anchors { top: parent.top; topMargin: 8; horizontalCenter: parent.horizontalCenter }
        width: buttons.implicitWidth + 6
        height: buttons.implicitHeight + 6
        radius: 10
        color: Theme.panel
        border.color: Theme.line
        // hidden: faded out and up 10 dp (density.md section 3)
        opacity: root.toolbarShown ? 1 : 0
        transform: Translate {
            y: root.toolbarShown ? 0 : -10
            Behavior on y { NumberAnimation { duration: Theme.toolbarFade; easing.type: Easing.OutCubic } }
        }
        Behavior on opacity { NumberAnimation { duration: Theme.toolbarFade; easing.type: Easing.OutCubic } }
        HoverHandler { id: toolbarHover }
        // Esc lets go of it (Minimal), before the window's Esc ladder
        Keys.onShortcutOverride: (event) => { if (root.autoHide && event.key === Qt.Key_Escape) event.accepted = true }
        Keys.onEscapePressed: (event) => {
            if (root.autoHide) {
                toolbar.focus = false             // let it hide
                event.accepted = true
            } else {
                event.accepted = false
            }
        }

        /* One roving Tab stop (overview/04 section 5): <- / -> move between
         * the shown buttons, Enter or Space presses the one ringed; the
         * buttons take no focus themselves */
        activeFocusOnTab: true
        property int current: 0
        function shown(): list<Item> {
            const out = []
            for (let i = 0; i < buttons.children.length; i++) {
                const b = buttons.children[i]
                if (b.toolbarButton === true && b.visible)
                    out.push(b)
            }
            return out
        }
        function ringed(b: Item): bool {
            const list = shown()
            return toolbar.activeFocus && list[Math.min(current, list.length - 1)] === b
        }
        Keys.onLeftPressed: current = Math.max(0, Math.min(current, shown().length - 1) - 1)
        Keys.onRightPressed: current = Math.min(shown().length - 1, current + 1)
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                const list = shown()
                list[Math.min(current, list.length - 1)].clicked()
                event.accepted = true
            }
        }
        Accessible.role: Accessible.ToolBar
        Accessible.name: qsTr("3D view toolbar")

        component Separator: Rectangle {
            width: 1
            height: root.compact ? 20 : 28
            anchors.verticalCenter: parent ? parent.verticalCenter : undefined
            color: Theme.line
        }

        Row {
            id: buttons
            anchors.centerIn: parent
            spacing: 2

            ToolbarButton {
                objectName: "entities.viewport.toolbar.orbit"
                focusPolicy: Qt.NoFocus
                ring: toolbar.ringed(this)
                toggle: true
                iconName: "orbit"
                caption: qsTr("Orbit")
                tip: qsTr("Orbit: drag to rotate the view (O)")
                compact: root.compact
                on: App.viewport.navMode === "orbit"
                onClicked: App.viewport.navMode = "orbit"
            }
            ToolbarButton {
                objectName: "entities.viewport.toolbar.pan"
                focusPolicy: Qt.NoFocus
                ring: toolbar.ringed(this)
                toggle: true
                iconName: "pan"
                caption: qsTr("Pan")
                tip: qsTr("Pan: drag to move the view (P)")
                compact: root.compact
                on: App.viewport.navMode === "pan"
                onClicked: App.viewport.navMode = "pan"
            }
            Separator { visible: root.entities }
            ToolbarButton {
                objectName: "entities.viewport.toolbar.edit"
                focusPolicy: Qt.NoFocus
                ring: toolbar.ringed(this)
                visible: root.entities
                readonly property var toolNames: [qsTr("Fixed"), qsTr("Variable"), qsTr("Erase"), qsTr("Paint")]
                readonly property string toolName: toolNames[App.editor.toolInEffect]
                readonly property int toolNo: App.editor.toolInEffect + 1
                toggle: true
                // the cursor's own drawing, its badge or drop in the drawing colour
                // (Default: the theme's contrast), as the 3D cursor is (C06)
                glyphSource: "image://editglyph/" + App.editor.toolInEffect
                             + "?color=" + (App.colours.drawColour > 0 ? App.colours.drawRgb.toString().replace("#", "").slice(-6) : "")
                             + "&dark=" + (Theme.dark ? 1 : 0)
                caption: qsTr("Edit")
                badge: String(toolNo)
                // C06 Edit button: on in Edit mode; override while a quick tool is held
                on: App.editor.editMode
                override: App.editor.quickHeld3d
                tip: override ? qsTr("Quick tool — %1 (%2).").arg(toolName).arg(toolNo)
                   : App.editor.editMode ? qsTr("Click edits with the active tool. Press E to leave edit mode.")
                   : qsTr("3D edit mode — %1 (%2). Press E to turn edit mode on; or hold Shift/Alt/Ctrl while clicking.").arg(toolName).arg(toolNo)
                compact: root.compact
                onClicked: App.editor.toggleEditMode()
            }
            Separator {}
            ToolbarButton {
                id: displayButton
                objectName: "entities.viewport.toolbar.display"
                focusPolicy: Qt.NoFocus
                ring: toolbar.ringed(this)
                toggle: true
                iconName: "eye"
                caption: qsTr("Display")
                tip: qsTr("Display options")
                compact: root.compact
                on: displayMenu.visible
                onClicked: displayMenu.visible ? displayMenu.close() : displayMenu.open()

                DisplayMenu { // ⑤
                    id: displayMenu
                    y: displayButton.height + 8
                    x: displayButton.width / 2 - width / 2
                }
            }
            Separator {}
            ToolbarButton {
                objectName: "entities.viewport.toolbar.focus"
                focusPolicy: Qt.NoFocus
                ring: toolbar.ringed(this)
                toggle: true
                iconName: App.layout.focus === LayoutController.Focus3d ? "unfocus" : "focus"
                caption: App.layout.focus === LayoutController.Focus3d ? qsTr("Exit") : qsTr("Focus")
                tip: qsTr("Focus the 3D view (Ctrl+Shift+F, Esc to exit)")
                compact: root.compact
                on: App.layout.focus === LayoutController.Focus3d
                onClicked: App.layout.toggleFocus3d()
            }
        }
    }

    Rectangle { // ⑧
        objectName: "entities.viewport.toolbar.handle"
        anchors { top: parent.top; topMargin: 4; horizontalCenter: parent.horizontalCenter }
        width: 40
        height: 4
        radius: 2
        color: Theme.muted
        opacity: root.toolbarShown ? 0 : 0.45
        visible: root.autoHide
        Behavior on opacity { NumberAnimation { duration: Theme.toolbarFade } }
    }

    VoxelViewport { // ①
        id: canvas
        objectName: "entities.viewport.surface"
        anchors.fill: parent
        // after the toolbar in Tab order (the spec's: toolbar, then canvas), drawn below everything
        z: -1
        controller: App.viewport
        // to assistive tech: what it shows, and the gestures the hint line (hidden
        // in Minimal and Focus 2D) explains
        Accessible.role: Accessible.Graphic
        Accessible.name: App.viewport.hasShape ? qsTr("3D view of %1 %2").arg(App.viewport.shapeId).arg(App.viewport.shapeLabel) : qsTr("3D view")
        Accessible.description: App.viewport.navMode === "orbit"
                                ? qsTr("Drag to orbit, Shift-drag or middle-drag to pan, wheel to zoom. With the view focused: arrows orbit, Shift+arrows pan, + and − zoom. O orbit mode, P pan mode, Home home view, F fit.")
                                : qsTr("Drag to pan, Shift-drag to orbit, middle-drag to pan, wheel to zoom. With the view focused: arrows orbit, Shift+arrows pan, + and − zoom. O orbit mode, P pan mode, Home home view, F fit.")
        // Tab reaches it (a click does not take the focus: C20); its keys are
        // the command table's viewport scope (Help ▸ Keyboard shortcuts)
        tabStop: true
        Keys.onShortcutOverride: (event) => {
            if (App.commands.keyInScope("viewport", event.key, event.modifiers) !== "")
                event.accepted = true
        }
        Keys.onPressed: (event) => {
            const key = App.commands.keyInScope("viewport", event.key, event.modifiers)
            if (key !== "") {
                App.commands.trigger(key)
                event.accepted = true
            }
        }
    }

    // the keyboard's focus on the canvas: a ring on the card's edge
    Rectangle {
        objectName: "entities.viewport.focusRing"
        anchors.fill: parent
        visible: canvas.activeFocus
        color: "transparent"
        radius: root.radius
        border.width: 2
        border.color: Theme.accent
        z: 10
    }

    // --- view cube (top right, C13 placement) ---
    ViewCubeItem { // ④
        id: cube
        objectName: "entities.viewport.cube"
        controller: App.viewport
        // the design's 156 x 170 plus room for the axis labels at the left
        // and bottom (ViewCube::kPadLeft / kPadBottom)
        width: implicitWidth
        height: implicitHeight
        // the visible content 16 dp from the top and right edges; below the
        // toolbar when the card is narrower than 640 dp
        x: root.width - width - 2
        y: root.width < 640 ? 76 : -11
        visible: App.viewport.showViewCube && !root.focus2d
    }

    // --- hint (bottom left) ---
    Column { // ⑥
        objectName: "entities.viewport.hint"
        visible: !root.focus2d && !Theme.minimal
        anchors { left: parent.left; bottom: parent.bottom; margins: 14 }
        spacing: 4

        /* Each pair reads "key + gesture" then what it does: the gesture
         * muted, its effect in the text colour, pairs apart by a dot (user
         * feedback 2026-10-28: C06's "Shift click 1 fixed" read as a riddle) */
        component HintText: Text {
            property bool effect: false
            anchors.verticalCenter: parent ? parent.verticalCenter : undefined
            color: effect ? Theme.text : Theme.muted
            font.pixelSize: Theme.fontSecondary
        }
        component Dot: HintText { text: "  ·  " }

        // line 1, fixed: the quick tools (Entities)
        Row {
            objectName: "entities.viewport.hint.tools"
            visible: root.entities
            spacing: 4
            KeyBadge { text: "Shift" }
            HintText { text: qsTr("+click") }
            HintText { effect: true; text: qsTr("fixed voxel") }
            Dot {}
            KeyBadge { text: Qt.platform.os === "osx" ? "Command" : "Alt" }
            HintText { text: qsTr("+click") }
            HintText { effect: true; text: qsTr("variable voxel") }
            Dot {}
            KeyBadge { text: Qt.platform.os === "osx" ? "Option" : "Ctrl" }
            HintText { text: qsTr("+click") }
            HintText { effect: true; text: qsTr("erase") }
        }
        // line 2 follows the navigation mode: what a plain drag does first
        Row {
            objectName: "entities.viewport.hint.navigation"
            readonly property bool orbit: App.viewport.navMode === "orbit"
            spacing: 4
            HintText { text: parent.orbit ? qsTr("Drag") : qsTr("Drag or middle-drag") }
            HintText { effect: true; text: parent.orbit ? qsTr("orbit") : qsTr("pan") }
            Dot {}
            KeyBadge { text: "Shift" }
            HintText { text: parent.orbit ? qsTr("+drag or middle-drag") : qsTr("+drag") }
            HintText { effect: true; text: parent.orbit ? qsTr("pan") : qsTr("orbit") }
            Dot {}
            HintText { text: qsTr("Wheel") }
            HintText { effect: true; text: qsTr("zoom") }
        }
    }

    // --- empty states ---
    Text { // ⑦
        objectName: "entities.viewport.empty"
        anchors.centerIn: parent
        visible: text.length > 0
        text: !root.entities ? (App.layout.workspace === LayoutController.Puzzle
                                  ? qsTr("The Puzzle scene arrives with the Puzzle phase")
                                  : qsTr("The Solver scene arrives with the Solver phase"))
             : !App.viewport.hasShape ? qsTr("No shape — open a puzzle or add a shape")
             : !App.viewport.emptyShape ? ""
             : App.viewport.editorVisible && App.viewport.displayLayerSlab
               ? qsTr("Empty shape — draw in the Voxel editor, or Shift-click the highlighted layer here")
             : qsTr("Empty shape — Shift-click here (or press E, then click) to place the first voxel at the origin")
        color: Theme.muted
        font.pixelSize: Theme.fontSecondary
    }
}
