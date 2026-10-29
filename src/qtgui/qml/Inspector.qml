pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BurrTools.Ui

// The inspector under the Shapes list (P6): what is done to the whole
// selected shape, in three accordion sections (PR-09), closed until
// opened, whose open state persists -- C04 Transform, C03 Fit & scale, C05
// Repair & surface. App.inspector runs each operation as one undo step, or says
// in the status bar why it cannot; a greyed button still takes the press
// and its hint or tooltip names the reason.
//
// Wireframe (each number marks its code below):
//
//   ├──────────────────────────────────────────┤
//   │ ①[S3] Edit shape                         │   head: the selected shape
//   ├──────────────────────────────────────────┤
//   │ ②▾ Transform          Flip · Rotate · Nudge
//   │      FLIP   ROTATE        NUDGE           │   ③ bricks, rhombic, tetra-octa:
//   │   X  [⇋]  [↶−90][↷+90] [−1] [+1]         │      one matrix, a row per axis
//   │   Y  ...                                 │   ④ prism, spheres: Flip & Rotate,
//   │   Z  ...                                 │      then the nudge pad
//   │ ⑤▸ Fit & scale        Prune · Center · Scale
//   │      FIT TO GRID              All shapes  │   (Apply to all shapes is on)
//   │      [▣ Prune grid to fit piece        ]  │
//   │      [⊕ Center      ] [⌞ To origin     ]  │
//   │      SCALE                               │   (not for spheres)
//   │      [÷ Minimize                       ]  │
//   │      [×2 Double     ] [×3 Triple       ]  │
//   │ ⑥▸ Repair & surface   Fill · Inner / Outer
//   │      [▦ Fill interior holes            ]  │
//   │      SURFACE VOXELS                      │
//   │      Make fixed     solid      [In] [Out] │
//   │      Make variable  optional   [In] [Out] │
//   │      Clear colour   remove paint [In][Out]│
//
// A Transform button under the pointer or the keyboard focus previews its
// result in the 3D view (App.inspector.preview); leaving it clears that.
// An open section's buttons are one Tab stop (RovingFocus): the arrows
// move between them.
Item {
    id: root
    objectName: "entities.inspector"

    /* the height it needs for its open sections, without scrolling: the
     * Shapes list takes what is left (ShapesCard) */
    readonly property real neededHeight: head.height + sections.implicitHeight

    readonly property InspectorController insp: App.inspector
    // re-read whenever what may run changes
    readonly property int rev: insp.revision
    // the operations' numbers (InspectorController::Task)
    readonly property var tasks: insp.tasks
    readonly property real gap: Theme.minimal ? 4 : 6
    readonly property real twoButton: Theme.minimal ? Theme.actionButton : 46

    function allowed(task: int): bool { return root.rev >= 0 && root.insp.allowed(task) }
    function reason(task: int): string { return root.rev >= 0 ? root.insp.reason(task) : "" }

    // --- the Transform preview: one button at a time owns it -----------------
    property Item previewOwner: null
    function previewFrom(button: Item, task: int, on: bool) {
        if (on) {
            previewOwner = button
            insp.preview(task)
        } else if (previewOwner === button) {
            previewOwner = null
            insp.clearPreview()
        }
    }
    Component.onDestruction: insp.clearPreview()

    // each section's buttons, one Tab stop
    RovingFocus { id: transformKeys }
    RovingFocus { id: fitKeys }
    RovingFocus { id: repairKeys }

    // the nudge pad's rows: spheres a row per layer, the prism's in-plane
    // arrows two by two, then the buttons along Z
    function padRows(buttons: var): var {
        const nudges = buttons.filter(b => b.kind === "nudge")
        const arrows = nudges.filter(b => b.glyph === "")
        const axial = nudges.filter(b => b.glyph !== "")
        const rows = []
        if (new Set(arrows.map(b => b.layer)).size > 1) {
            const names = { "1": qsTr("Up layer"), "0": qsTr("In layer"), "-1": qsTr("Down layer") }
            for (const l of [1, 0, -1]) {
                const items = arrows.filter(b => b.layer === l)
                if (items.length > 0)
                    rows.push({ label: names[String(l)], items: items })
            }
        } else {
            for (let i = 0; i < arrows.length; i += 2)
                rows.push({ label: i === 0 ? qsTr("In plane") : "", items: arrows.slice(i, i + 2) })
        }
        if (axial.length > 0)
            rows.push({ label: "Z", items: axial })
        return rows
    }
    function matrixButton(buttons: var, kind: string, axis: int, sign: int): var {
        return buttons.find(b => b.kind === kind && b.axis === axis && b.sign === sign) ?? null
    }

    // a sub-heading (11/600 uppercase muted)
    component SubHeading: Text {
        topPadding: 8
        bottomPadding: 6
        color: Theme.muted
        font.pixelSize: Theme.fontMicro
        font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 0.4
    }

    // a Transform button: a glyph and caption, previewing while hovered or focused
    component TransformButton: GlyphButton {
        id: tb
        required property var spec
        objectName: "entities.transform." + spec.id
        roving: transformKeys
        glyph: spec.glyph
        caption: spec.caption
        tip: spec.tip
        glyphSize: Theme.minimal ? 22 : 26
        iconOnly: Theme.minimal
        implicitWidth: 40
        implicitHeight: Theme.transformButton
        Accessible.role: Accessible.Button
        Accessible.checkable: false
        readonly property bool previewing: hovered || visualFocus
        onPreviewingChanged: root.previewFrom(tb, spec.task, previewing)
        onClicked: root.insp.run(spec.task)
    }

    // a nudge-pad button: an arrow in the layer, or a glyph along Z
    component PadButton: AbstractButton {
        id: pb
        required property var spec
        objectName: "entities.transform." + spec.id
        implicitWidth: 40
        implicitHeight: Math.min(32, Theme.transformButton)
        // in the Transform section's one Tab stop, as a GlyphButton with `roving`
        focusPolicy: transformKeys.stop === pb ? Qt.StrongFocus : Qt.ClickFocus
        Component.onCompleted: transformKeys.join(pb)
        Component.onDestruction: transformKeys.leave(pb)
        onActiveFocusChanged: if (activeFocus) transformKeys.current = pb
        Keys.onPressed: (event) => {
            if ((event.modifiers & ~Qt.KeypadModifier) === Qt.NoModifier && transformKeys.navigate(pb, event.key))
                event.accepted = true
        }
        hoverEnabled: true
        Accessible.role: Accessible.Button
        Accessible.name: spec.tip
        BtToolTip { shown: pb.hovered || pb.visualFocus; text: pb.spec.tip }
        readonly property bool previewing: hovered || visualFocus
        onPreviewingChanged: root.previewFrom(pb, spec.task, previewing)
        onClicked: root.insp.run(spec.task)
        background: Rectangle {
            radius: Theme.radiusControl
            color: pb.hovered ? Theme.accentSoft : Theme.panel2
            border.width: pb.visualFocus ? 2 : 1
            border.color: pb.visualFocus || pb.hovered ? Theme.accent : Theme.line
        }
        contentItem: Item {
            Glyph {
                anchors.centerIn: parent
                visible: pb.spec.glyph !== ""
                name: pb.spec.glyph
                size: 22
            }
            Text {
                anchors.centerIn: parent
                visible: pb.spec.glyph === ""
                text: "→"
                rotation: -pb.spec.arrowDeg       // counter-clockwise from +X
                color: Theme.text
                font.pixelSize: 15
                font.weight: Font.Bold
                Accessible.ignored: true
            }
        }
    }

    /* Transform buttons by axis: the axis letters in a 22 dp column, then
     * equal columns under their heads ([title, columns]); `cells` holds a
     * row of specs per axis, null for an empty cell */
    component AxisMatrix: Column {
        id: am
        property var heads: []
        property var cells: []
        readonly property int columns: heads.reduce((n, h) => n + h[1], 0)
        readonly property real gapX: Theme.minimal ? 3 : 4
        readonly property real cellW: (width - 22 - columns * gapX) / Math.max(1, columns)
        spacing: gapX
        Row {
            spacing: am.gapX
            Item { width: 22; height: 1 }
            Repeater {
                model: am.heads
                delegate: SubHeading {
                    required property var modelData
                    width: modelData[1] * am.cellW + (modelData[1] - 1) * am.gapX
                    text: modelData[0]
                    horizontalAlignment: Text.AlignHCenter
                    topPadding: 0
                    bottomPadding: 0
                }
            }
        }
        Repeater {
            model: am.cells.length
            delegate: Row {
                id: axisRow
                required property int index
                spacing: am.gapX
                Text {
                    width: 22
                    height: Theme.transformButton
                    text: "XYZ"[axisRow.index]
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: [Theme.axisX, Theme.axisY, Theme.axisZ][axisRow.index]
                    font.pixelSize: Theme.fontBody
                    font.weight: Font.Bold
                    Accessible.ignored: true
                }
                Repeater {
                    model: am.columns
                    delegate: Loader {
                        id: cell
                        required property int index
                        readonly property var spec: am.cells[axisRow.index][index] ?? null
                        width: am.cellW
                        height: Theme.transformButton
                        active: spec !== null
                        sourceComponent: TransformButton { spec: cell.spec }
                    }
                }
            }
        }
    }

    Rectangle { // ①
        id: head
        objectName: "entities.inspector.head"
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: Theme.cardHeader
        color: "transparent"
        Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top } height: 1; color: Theme.line }
        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: Theme.line }
        Row {
            anchors { left: parent.left; leftMargin: Theme.minimal ? 10 : 14; verticalCenter: parent.verticalCenter }
            spacing: 8
            Chip {
                objectName: "entities.inspector.head.chip"
                kind: Chip.Header
                visible: App.viewport.hasShape
                text: App.viewport.shapeId
                fill: App.viewport.shapeColor
                textColor: App.viewport.shapeTextColor
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Edit shape")
                color: Theme.text
                font.pixelSize: Theme.fontBody
                font.weight: Font.DemiBold
            }
        }
    }

    Flickable {
        id: scroller
        objectName: "entities.inspector.scroll"
        anchors { left: parent.left; right: parent.right; top: head.bottom; bottom: parent.bottom }
        clip: true
        contentWidth: width
        contentHeight: sections.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
            id: sections
            width: scroller.width

            AccordionSection { // ②
                id: transformSection
                objectName: "entities.inspector.transform"
                width: parent.width
                title: qsTr("Transform")
                summary: qsTr("Flip · Rotate · Nudge")
                open: root.insp.transformOpen
                onToggled: root.insp.transformOpen = !root.insp.transformOpen

                AxisMatrix { // ③
                    objectName: "entities.transform.matrix"
                    visible: !root.insp.pads
                    width: parent.width
                    heads: [[qsTr("Flip"), 1], [qsTr("Rotate"), 2], [qsTr("Nudge"), 2]]
                    // per axis: Flip, Rotate −, Rotate +, Nudge −, Nudge +
                    cells: {
                        const b = root.insp.pads ? [] : root.insp.transformButtons
                        return [0, 1, 2].map(a => [root.matrixButton(b, "flip", a, 0),
                                                   root.matrixButton(b, "rotate", a, -1), root.matrixButton(b, "rotate", a, 1),
                                                   root.matrixButton(b, "nudge", a, -1), root.matrixButton(b, "nudge", a, 1)])
                    }
                }

                Column { // ④
                    objectName: "entities.transform.pads"
                    visible: root.insp.pads
                    width: parent.width
                    spacing: root.gap

                    AxisMatrix {
                        width: parent.width
                        heads: [[qsTr("Flip"), 1], [qsTr("Rotate"), 2]]
                        // per axis: Flip, then its rotations -- one 180° button, or − and +
                        cells: {
                            const b = root.insp.pads ? root.insp.transformButtons : []
                            return [0, 1, 2].map(a => {
                                const rotations = b.filter(x => x.kind === "rotate" && x.axis === a)
                                return [root.matrixButton(b, "flip", a, 0)].concat(
                                    rotations.length === 1 ? [rotations[0], null]
                                                           : [root.matrixButton(b, "rotate", a, -1), root.matrixButton(b, "rotate", a, 1)])
                            })
                        }
                    }

                    SubHeading { text: qsTr("Nudge"); topPadding: 4; bottomPadding: 0 }

                    Repeater {
                        model: root.insp.pads ? root.padRows(root.insp.transformButtons) : []
                        delegate: Row {
                            id: padRow
                            required property var modelData
                            readonly property real gapX: Theme.minimal ? 3 : 4
                            readonly property real cellW: (width - 72 - modelData.items.length * gapX) / modelData.items.length
                            width: parent.width
                            spacing: gapX
                            Text {
                                width: 72                // the mock's 62 elides "Down layer"
                                height: Math.min(32, Theme.transformButton)
                                verticalAlignment: Text.AlignVCenter
                                text: padRow.modelData.label
                                color: Theme.muted
                                font.pixelSize: Theme.fontCaption
                                font.weight: Font.DemiBold
                                font.capitalization: Font.AllUppercase
                                font.letterSpacing: 0.3
                                elide: Text.ElideRight
                            }
                            Repeater {
                                model: padRow.modelData.items
                                delegate: PadButton {
                                    required property var modelData
                                    spec: modelData
                                    width: padRow.cellW
                                }
                            }
                        }
                    }
                }
            }

            AccordionSection { // ⑤
                objectName: "entities.inspector.fit"
                width: parent.width
                title: qsTr("Fit & scale")
                summary: qsTr("Prune · Center · Scale")
                open: root.insp.fitOpen
                onToggled: root.insp.fitOpen = !root.insp.fitOpen

                Item {
                    width: parent.width
                    implicitHeight: fitHeading.implicitHeight
                    SubHeading { id: fitHeading; text: qsTr("Fit to grid"); topPadding: 2 }
                    Text {
                        objectName: "entities.fit.allShapes"
                        anchors { right: parent.right; baseline: fitHeading.baseline }
                        visible: App.editor.applyToAll
                        text: qsTr("All shapes")
                        color: Theme.accent
                        font.pixelSize: Theme.fontMicro
                        font.weight: Font.DemiBold
                        HoverHandler { id: allHover }
                        BtToolTip {
                            shown: allHover.hovered
                            text: qsTr("Apply to all shapes is on (Grid size): these act on every shape")
                        }
                    }
                }
                Flow {
                    id: fitFlow
                    width: parent.width
                    spacing: root.gap
                    readonly property real half: (width - spacing) / 2
                    ActionButton {
                        objectName: "entities.fit.prune"
                        roving: fitKeys
                        width: Theme.minimal ? implicitWidth : parent.width
                        glyph: "prune-grid"
                        title: qsTr("Prune grid to fit piece")
                        shortTitle: qsTr("Prune")
                        available: root.allowed(root.tasks.prune)
                        hint: available ? qsTr("Shrink the grid to the piece’s bounding box") : root.reason(root.tasks.prune)
                        onClicked: root.insp.run(root.tasks.prune)
                    }
                    ActionButton {
                        objectName: "entities.fit.center"
                        roving: fitKeys
                        width: Theme.minimal ? implicitWidth : fitFlow.half
                        glyph: "center-on-grid"
                        title: qsTr("Center")
                        available: root.allowed(root.tasks.center)
                        hint: available ? qsTr("Centre on the grid") : root.reason(root.tasks.center)
                        onClicked: root.insp.run(root.tasks.center)
                    }
                    ActionButton {
                        objectName: "entities.fit.origin"
                        roving: fitKeys
                        width: Theme.minimal ? implicitWidth : fitFlow.half
                        glyph: "move-to-origin"
                        title: qsTr("To origin")
                        shortTitle: qsTr("Origin")
                        available: root.allowed(root.tasks.origin)
                        hint: available ? qsTr("Corner at 0, 0, 0") : root.reason(root.tasks.origin)
                        onClicked: root.insp.run(root.tasks.origin)
                    }
                }
                Column {
                    objectName: "entities.fit.scale"
                    visible: root.insp.scaleShown
                    width: parent.width
                    SubHeading { text: qsTr("Scale") }
                    Flow {
                        id: scaleFlow
                        width: parent.width
                        spacing: root.gap
                        readonly property real half: (width - spacing) / 2
                        ActionButton {
                            objectName: "entities.fit.minimize"
                            roving: fitKeys
                            width: Theme.minimal ? implicitWidth : parent.width
                            glyph: "minimize"
                            title: qsTr("Minimize")
                            available: root.allowed(root.tasks.minimize)
                            hint: available ? qsTr("Reduce resolution ÷%1, same shape").arg(root.rev >= 0 ? root.insp.minimizeFactor : 1)
                                            : root.reason(root.tasks.minimize)
                            onClicked: root.insp.run(root.tasks.minimize)
                        }
                        Repeater {
                            model: [{ n: 2, caption: qsTr("Double ×2"), task: root.tasks.scale2 },
                                    { n: 3, caption: qsTr("Triple ×3"), task: root.tasks.scale3 }]
                            delegate: GlyphButton {
                                id: scaleButton
                                required property var modelData
                                objectName: "entities.fit.x" + modelData.n
                                roving: fitKeys
                                width: Theme.minimal ? implicitWidth : scaleFlow.half
                                // Minimal: the glyph and the caption side by side, as wide as they are
                                implicitWidth: Theme.minimal ? glyphSize + 6 + captionWidth.advanceWidth + 16 : 62
                                TextMetrics {
                                    id: captionWidth
                                    text: scaleButton.caption
                                    font.pixelSize: Theme.fontCaption
                                    font.weight: Font.DemiBold
                                }
                                implicitHeight: root.twoButton
                                glyph: "multiply-" + modelData.n
                                glyphSize: Theme.minimal ? 24 : 30
                                caption: modelData.caption
                                horizontal: Theme.minimal
                                available: root.allowed(modelData.task)
                                tip: available ? qsTr("Multiply every voxel %1× per axis (piece grows)").arg(modelData.n)
                                               : root.reason(modelData.task)
                                Accessible.role: Accessible.Button
                                Accessible.checkable: false
                                onClicked: root.insp.run(modelData.task)
                            }
                        }
                    }
                }
            }

            AccordionSection { // ⑥
                objectName: "entities.inspector.repair"
                width: parent.width
                title: qsTr("Repair & surface")
                summary: qsTr("Fill · Inner / Outer")
                open: root.insp.repairOpen
                onToggled: root.insp.repairOpen = !root.insp.repairOpen

                Item { width: 1; height: 2 }
                ActionButton {
                    objectName: "entities.repair.fillHoles"
                    roving: repairKeys
                    width: Theme.minimal ? implicitWidth : parent.width
                    glyph: "fill-holes"
                    title: qsTr("Fill interior holes")
                    shortTitle: qsTr("Fill holes")
                    hint: qsTr("Fill empty voxels not visible from outside with fixed voxels")
                    onClicked: root.insp.run(root.tasks.fillHoles)
                }
                SubHeading { text: qsTr("Surface voxels") }
                GridLayout {
                    objectName: "entities.repair.surface"
                    width: parent.width
                    columns: 3
                    columnSpacing: root.gap
                    rowSpacing: root.gap
                    Repeater {
                        model: [{ key: "fixed", title: qsTr("Make fixed"), sub: qsTr("solid") },
                                { key: "variable", title: qsTr("Make variable"), sub: qsTr("optional") },
                                { key: "clear", title: qsTr("Clear colour"), sub: qsTr("remove paint") }]
                        delegate: Column {
                            id: surfaceRow
                            required property var modelData
                            required property int index
                            Layout.row: index
                            Layout.column: 0
                            Layout.fillWidth: true
                            Text {
                                width: surfaceRow.width
                                text: surfaceRow.modelData.title
                                color: Theme.text
                                font.pixelSize: Theme.minimal ? Theme.fontSecondary : Theme.fontActionTitle
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Text {
                                visible: !Theme.minimal
                                text: surfaceRow.modelData.sub
                                color: Theme.muted
                                font.pixelSize: Theme.fontMicro
                            }
                        }
                    }
                    Repeater {
                        model: 6
                        delegate: GlyphButton {
                            id: surfaceButton
                            required property int index
                            readonly property int rowNo: Math.floor(index / 2)
                            readonly property bool outer: index % 2 === 1
                            readonly property var row: [["fixed", qsTr("Make fixed"), root.tasks.fixedInner],
                                                        ["variable", qsTr("Make variable"), root.tasks.variableInner],
                                                        ["clear", qsTr("Clear colour"), root.tasks.clearInner]][rowNo]
                            objectName: "entities.repair." + row[0] + (outer ? ".outer" : ".inner")
                            roving: repairKeys
                            Layout.row: rowNo
                            Layout.column: outer ? 2 : 1
                            Layout.preferredWidth: Theme.minimal ? 52 : 66
                            implicitHeight: root.twoButton
                            glyph: "surface-" + row[0] + (outer ? "-outer" : "-inner")
                            glyphSize: Theme.minimal ? 24 : 30
                            caption: outer ? qsTr("Outer") : qsTr("Inner")
                            iconOnly: Theme.minimal
                            tip: row[1] + " — " + (outer ? qsTr("voxels on the outer surface") : qsTr("voxels not visible from outside"))
                            Accessible.role: Accessible.Button
                            Accessible.checkable: false
                            onClicked: root.insp.run(row[2] + (outer ? 1 : 0))
                        }
                    }
                }
            }
        }
    }
}
