pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BurrTools.Ui

// C07 / C08 / C02 Voxel editor card: header with the read-only voxel type
// chip (C14), Focus 2D and collapse; what a click on the grid does -- the
// tools, Area, and voxel placement (Mirror, Span) --, the 2D grid with its
// layer strip, the plane and layer, the grid size and the Drawing colour
// footer (C09). Sizes per density (density.md section 1); Minimal shows the
// tools, Mirror and Span and the planes as icons alone, their names in the
// tooltips. The click controls have fixed widths and flow: a wider card
// takes the placement group up beside the tools, the grid gets the height.
//
// Wireframe (each number marks its code below):
//
//   ┌──────────────────────────────────────────────┐
//   │ Voxel editor  ①(▪ Brick)           ②[⤢] [◨] │   title, collapse: Card
//   ├──────────────────────────────────────────────┤
//   │ ③ [■ Fixed 1][▨ Variable 2][⌫ Erase 3][◆ Paint 4] [⬚ Area]
//   │ ④ VOXEL PLACEMENT                            │   (beside ③ at the right
//   │                                              │    edge when wide)
//   │   Mirror [X][Y][Z]      Span [X][Y][Z]       │
//   │   also places the       fills the whole      │
//   │   mirror image          line                 │
//   ├────┬─────────────────────────────────────────┤
//   │ ⑥  │ ⑦                                        │
//   │Z ly│   3 ▢ ▢ ▢ ▢                              │
//   │ 4  │   2 ▢ ■ ■ ▢      the 2D grid             │
//   │ 3• │   1 ▢ ■ ▢ ▢                              │
//   │ 2• │   0 ▢ ▢ ▢ ▢                              │
//   │ 1  │     0 1 2 3                              │
//   ├────┴─────────────────────────────────────────┤
//   │ ⑤ EDITING PLANE                              │   (⑧ beside it at the right
//   │   [XY] [XZ] [YZ]                 Layer 1 of 4 │    edge when wide)
//   ├──────────────────────────────────────────────┤
//   │ ⑧ GRID SIZE              Apply to all shapes ◯ │
//   │   [X − 4 ＋] [Y − 4 ＋] [Z − 4 ＋]              │
//   ├──────────────────────────────────────────────┤
//   │ ⑨ Drawing colour: [■] C2  (●)(■)(■) [+][⋯]   │
//   └──────────────────────────────────────────────┘
Card {
    id: root
    objectName: "entities.editor"
    title: qsTr("Voxel editor")
    collapseIcon: "panelR"
    collapseObjectName: "entities.editor.collapse"
    onCollapseClicked: App.layout.rightCollapsed = true
    /* hidden (collapsed to its rail), it keeps no focus: a focus scope
     * remembers its focused child, and Qt before 6.12 gives the window's
     * focus back to it, unseen, before anything else */
    onVisibleChanged: if (!visible) grid.focus = false

    readonly property var editor: App.editor
    readonly property var axisColours: [Theme.axisX, Theme.axisY, Theme.axisZ]
    readonly property var axisNames: ["X", "Y", "Z"]

    headerExtras: [
        Rectangle { // ①
            objectName: "entities.editor.voxelType"
            // read-only, so no Tab stop: assistive tech reads its note from here
            Accessible.role: Accessible.StaticText
            Accessible.name: qsTr("Voxel type: %1").arg(App.document.gridTypeName)
            Accessible.description: typeTip.text
            height: 22
            width: typeRow.implicitWidth + 16
            radius: 11
            color: Theme.panel2
            border.color: Theme.line
            Row {
                id: typeRow
                anchors.centerIn: parent
                spacing: 4
                // a padlock, drawn: an emoji would need a fallback font,
                // which Qt finds by reading every installed one (App)
                Item {
                    width: 8
                    height: 10
                    anchors.verticalCenter: parent.verticalCenter
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 6
                        height: 7
                        radius: 3
                        color: "transparent"
                        border.color: Theme.muted
                        border.width: 1.5
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: 8
                        height: 6
                        radius: 1.5
                        color: Theme.muted
                    }
                }
                Text {
                    text: App.document.gridTypeName
                    color: Theme.text
                    font.pixelSize: Theme.fontMicro
                    font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            HoverHandler { id: typeHover }
            BtToolTip {
                id: typeTip
                shown: typeHover.hovered
                text: qsTr("Voxel type: %1 — fixed when the file is created (File › New); it cannot be changed afterwards").arg(App.document.gridTypeName)
            }
        }
    ]
    headerActions: [
        IconButton { // ②
            objectName: "entities.editor.focus2d"
            toggle: true
            iconName: App.layout.focus === LayoutController.Focus2d ? "unfocus" : "focus"
            on: App.layout.focus === LayoutController.Focus2d
            tip: qsTr("Focus 2D editor (Esc to exit)")
            onClicked: App.layout.toggleFocus2d()
        }
    ]

    // a row of the card, with the 1 dp rule below it
    component Section: Item {
        default property alias content: inner.data
        property real padH: 12
        property real padV: 8
        Layout.fillWidth: true
        implicitHeight: inner.childrenRect.height + 2 * padV + 1
        Item {
            id: inner
            anchors { left: parent.left; right: parent.right; top: parent.top
                      leftMargin: parent.padH; rightMargin: parent.padH; topMargin: parent.padV }
            height: childrenRect.height
        }
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 1
            color: Theme.line
        }
    }

    component MicroLabel: Text {
        color: Theme.muted
        font.pixelSize: Theme.fontMicro
        font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
    }

    /* A radio group as one Tab stop (C07 Keyboard: the strip is a roving
     * tab index): ← / → select the neighbour that is enabled; the selected
     * button shows the focus ring. Its buttons take no focus themselves, as
     * the palette's swatches (ColourFooter). */
    component RadioStrip: Control {
        id: rs
        property int count
        property int current
        property var buttons: []
        property string groupName
        signal picked(int index)
        focusPolicy: Qt.StrongFocus
        Accessible.role: Accessible.RadioButton
        Accessible.name: groupName
        Accessible.description: buttons[current] ? buttons[current].tip : ""
        Keys.onLeftPressed: move(-1)
        Keys.onRightPressed: move(+1)
        function move(d: int): void {
            for (let i = current + d; i >= 0 && i < count; i += d)
                if (buttons[i] && buttons[i].enabled) {
                    picked(i)
                    return
                }
        }
        background: null
    }

    ColumnLayout {
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: footer.top }
        spacing: 0

        Section { // what a click on the grid does: the tool, Area, and where voxels go
            objectName: "entities.editor.click"
            padH: 12
            /* Fixed-width buttons in two blocks: stacked, or -- a card wide
             * enough for both (Focus 2D) -- side by side, the placement group
             * at the right edge, leaving the height to the grid */
            Item {
                id: clickFlow
                width: parent.width
                readonly property real gap: 10
                readonly property bool wide: width >= toolsBlock.implicitWidth + gap + placementBlock.implicitWidth
                height: wide ? Math.max(toolsBlock.height, placementBlock.height)
                             : toolsBlock.height + gap + placementBlock.height
                readonly property real toolWidth: Theme.minimal ? 44 : 58
                FontMetrics {
                    id: groupLabelMetrics
                    font.pixelSize: Theme.fontSecondary
                    font.weight: Font.DemiBold
                }
                readonly property real axisWidth: Theme.minimal ? 34 : 46

                Column { // ③
                  id: toolsBlock
                  objectName: "entities.editor.tools"
                  spacing: 4
                  /* Beside the placement group (a wide card) the tools take a
                   * heading too, and the room of its group labels, so the two
                   * rows of buttons line up; stacked they need neither */
                  readonly property bool besidePlacement: clickFlow.wide && !Theme.minimal
                  MicroLabel {
                      objectName: "entities.editor.tools.label"
                      text: qsTr("Editing tool")
                      visible: toolsBlock.besidePlacement
                  }
                  Item {
                      width: 1
                      height: groupLabelMetrics.height
                      visible: toolsBlock.besidePlacement
                  }
                  Row {
                    spacing: 8
                    RadioStrip {
                      id: toolStrip
                      objectName: "entities.editor.tools.group"
                      width: 4 * clickFlow.toolWidth + 3 * 6
                      height: Theme.toolStrip
                      count: 4
                      current: root.editor.tool
                      buttons: toolRow.buttons
                      groupName: qsTr("Tool")
                      onPicked: (i) => root.editor.tool = i
                      contentItem: Row {
                        id: toolRow
                        spacing: 6
                        readonly property var specs: [
                            { glyph: "tool-fixed", caption: qsTr("Fixed"), name: "fixed",
                              tip: qsTr("Place a fixed (solid) voxel (1)") },
                            { glyph: "tool-variable", caption: qsTr("Variable"), name: "variable",
                              tip: qsTr("Place a variable (optional) voxel (2)") },
                            { glyph: "tool-erase", caption: qsTr("Erase"), name: "erase",
                              tip: qsTr("Erase the voxel (3)") },
                            { glyph: "tool-paint", caption: qsTr("Paint"), name: "paint",
                              tip: qsTr("Apply the drawing colour to an existing voxel (4)") }
                        ]
                        property var buttons: []
                        Repeater {
                            model: toolRow.specs
                            GlyphButton {
                                required property var modelData
                                required property int index
                                objectName: "entities.editor.tool." + modelData.name
                                focusPolicy: Qt.NoFocus
                                ring: toolStrip.visualFocus && on
                                width: clickFlow.toolWidth
                                height: Theme.toolStrip
                                glyphSize: Theme.minimal ? 24 : 30
                                iconOnly: Theme.minimal
                                glyph: modelData.glyph
                                caption: modelData.caption
                                keyBadge: String(index + 1)
                                tip: modelData.tip
                                on: root.editor.tool === index
                                onClicked: root.editor.tool = index
                                Component.onCompleted: { toolRow.buttons[index] = this; toolRow.buttons = toolRow.buttons }
                            }
                        }
                      }
                    }
                    // legacy's rubber band: a drag spans an area, applied on release
                    GlyphButton {
                        objectName: "entities.editor.tool.area"
                        toggle: true
                        width: clickFlow.toolWidth
                        height: Theme.toolStrip
                        glyphSize: Theme.minimal ? 24 : 30
                        iconOnly: Theme.minimal
                        glyph: "tool-area"
                        caption: qsTr("Area")
                        on: root.editor.areaMode
                        tip: root.editor.areaMode
                             ? qsTr("Area is on: drag over the grid to cover a whole area, placed when you let go (Esc drops it)")
                             : qsTr("Area is off: each cell the pointer passes is placed. On, a drag covers a whole area")
                        onClicked: root.editor.areaMode = !root.editor.areaMode
                    }
                  }
                }

                Column { // ④
                    id: placementBlock
                    objectName: "entities.editor.modifiers"
                    x: clickFlow.wide ? clickFlow.width - width : 0
                    y: clickFlow.wide ? 0 : toolsBlock.height + clickFlow.gap
                    spacing: 4
                    MicroLabel {
                        objectName: "entities.editor.placement.label"
                        text: qsTr("Voxel placement")
                        visible: !Theme.minimal
                    }
                    Row {
                        spacing: 14
                        Repeater {
                            model: [
                                { label: qsTr("Mirror"), name: "mirror", glyph: "mirror-",
                                  hint: qsTr("also places the mirror image") },
                                { label: qsTr("Span"), name: "span", glyph: "span-",
                                  hint: qsTr("fills the whole line") }
                            ]
                            Column {
                                id: group
                                required property var modelData
                                width: 3 * clickFlow.axisWidth + 2 * 6
                                spacing: 4
                                readonly property bool mirror: modelData.name === "mirror"
                                Text {
                                    objectName: "entities.editor." + group.modelData.name + ".label"
                                    text: group.modelData.label
                                    visible: !Theme.minimal
                                    color: Theme.text
                                    font.pixelSize: Theme.fontSecondary
                                    font.weight: Font.DemiBold
                                }
                                Row {
                                    spacing: 6
                                    Repeater {
                                        model: 3
                                        GlyphButton {
                                            required property int index
                                            readonly property string axis: root.axisNames[index]
                                            objectName: "entities.editor." + group.modelData.name + "." + axis.toLowerCase()
                                            toggle: true
                                            width: clickFlow.axisWidth
                                            height: Theme.mirrorSpan
                                            glyphSize: Theme.minimal ? 22 : 28
                                            iconOnly: Theme.minimal
                                            glyph: group.modelData.glyph + axis.toLowerCase()
                                            caption: axis
                                            captionColor: root.axisColours[index]
                                            on: ((group.mirror ? root.editor.mirror : root.editor.span) & (1 << index)) !== 0
                                            // names the group too: Minimal hides its label
                                            tip: group.mirror
                                                 ? qsTr("Mirror %1: each voxel placed also places its mirror image across the %1 centre plane").arg(axis)
                                                 : qsTr("Span %1: each voxel placed fills the whole line along %1").arg(axis)
                                            onClicked: group.mirror ? root.editor.toggleMirror(index) : root.editor.toggleSpan(index)
                                        }
                                    }
                                }
                                Text {
                                    objectName: "entities.editor." + group.modelData.name + ".hint"
                                    width: parent.width
                                    visible: !Theme.minimal
                                    text: group.modelData.hint
                                    color: Theme.muted
                                    font.pixelSize: Theme.fontMicro
                                    wrapMode: Text.Wrap
                                }
                            }
                        }
                    }
                }
            }
        }

        Item { // the layer strip beside the grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 120

            Rectangle { // ⑥
                id: strip
                objectName: "entities.editor.layers"
                width: Theme.layerStrip
                anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                color: Theme.panel
                Rectangle {
                    anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                    width: 1
                    color: Theme.line
                }
                Text { // Minimal: the chips' tooltips name the axis
                    id: stripHeader
                    objectName: "entities.editor.layers.caption"
                    visible: !Theme.minimal
                    height: visible ? implicitHeight : 0
                    anchors { top: parent.top; topMargin: visible ? 8 : 0; horizontalCenter: parent.horizontalCenter }
                    text: qsTr("%1 layer").arg(root.editor.layerAxisName)
                    color: root.axisColours[root.editor.layerAxis]
                    font.pixelSize: Theme.fontMicro
                    font.weight: Font.Bold
                }
                ListView {
                    id: layers
                    anchors { top: stripHeader.bottom; topMargin: 6; bottom: parent.bottom; bottomMargin: 6
                              left: parent.left; right: parent.right }
                    clip: true
                    spacing: 4
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.editor.layerCount
                    // highest layer at the top
                    delegate: Rectangle {
                        id: chip
                        required property int index
                        readonly property int layerNo: root.editor.layerCount - 1 - index
                        readonly property bool current: layerNo === root.editor.layer
                        objectName: "entities.editor.layer." + (layerNo + 1)
                        x: (layers.width - width) / 2 - 0.5
                        width: Theme.layerChipWidth
                        height: Theme.layerChipHeight
                        radius: 6
                        color: current ? Theme.accent : (chipMouse.containsMouse ? Theme.panel2 : "transparent")
                        border.width: 1
                        border.color: current || chipMouse.containsMouse ? Theme.accent : Theme.line
                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("%1 layer %2").arg(root.editor.layerAxisName).arg(layerNo + 1)
                        Text {
                            anchors.centerIn: parent
                            anchors.horizontalCenterOffset: dot.visible ? -3 : 0
                            text: chip.layerNo + 1
                            color: chip.current ? "white" : Theme.text
                            font.pixelSize: Theme.fontSecondary
                            font.weight: Font.DemiBold
                        }
                        Rectangle {
                            id: dot
                            objectName: chip.objectName + ".dot"
                            visible: root.editor.layerDots[chip.layerNo] === true
                            anchors { right: parent.right; rightMargin: 5; verticalCenter: parent.verticalCenter }
                            width: 5
                            height: 5
                            radius: 2.5
                            color: chip.current ? "white" : Theme.muted
                        }
                        MouseArea {
                            id: chipMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: root.editor.layer = chip.layerNo
                        }
                        BtToolTip { shown: chipMouse.containsMouse; text: chip.Accessible.name }
                    }
                    // keep the current layer's chip in view
                    function follow(): void {
                        positionViewAtIndex(root.editor.layerCount - 1 - root.editor.layer, ListView.Contain)
                    }
                    Connections {
                        target: root.editor
                        function onLayerChanged() { layers.follow() }
                    }
                }
            }

            Rectangle { // ⑦
                anchors { left: strip.right; right: parent.right; top: parent.top; bottom: parent.bottom }
                color: Theme.panel2
                VoxelGridItem {
                    id: grid
                    objectName: "entities.editor.grid"
                    anchors.fill: parent
                    editor: root.editor
                    // a Tab stop: arrows move a cell cursor, Space paints, Delete erases (VoxelGridItem)
                    tabStop: true
                    Accessible.role: Accessible.Table
                    Accessible.name: qsTr("2D grid: layer %1 of %2").arg(root.editor.layer + 1).arg(root.editor.layerCount)
                    Accessible.description: qsTr("Arrow keys move the cell cursor, Space or Enter applies the tool (Shift+Space: fixed), Delete erases, Page Up and Page Down change the layer")
                    HoverHandler { id: gridHover }
                    BtToolTip {
                        shown: gridHover.hovered && !root.editor.stroking
                        text: qsTr("Click or drag: active tool (Shift fixed · Alt variable · Ctrl erase) · Right-click or right-drag: erase")
                    }
                }
                // the keyboard's focus on the grid
                Rectangle {
                    objectName: "entities.editor.grid.focusRing"
                    anchors.fill: parent
                    visible: grid.activeFocus
                    color: "transparent"
                    border.width: 2
                    border.color: Theme.accent
                }
                Text {
                    anchors.centerIn: parent
                    visible: !root.editor.hasShape
                    text: qsTr("No shape selected")
                    color: Theme.muted
                    font.pixelSize: Theme.fontSecondary
                }
            }
        }

        Rectangle { // the rule above the plane row
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.line
        }
        /* The plane and the grid size: stacked with a rule between, or -- a
         * card wide enough for both (Focus 2D) -- on one row, the grid size
         * at the right edge */
        Item {
            id: lowerPair
            objectName: "entities.editor.planeAndSize"
            Layout.fillWidth: true
            readonly property real pad: 12
            readonly property real gap: 24
            readonly property real inner: width - 2 * pad
            readonly property real sizeWidth: 360          // its steppers' width beside the planes
            readonly property bool wide: inner >= planeBlock.naturalWidth + gap + sizeWidth
            implicitHeight: wide ? 8 + Math.max(planeBlock.height, sizeBlock.height) + 10
                                 : 8 + planeBlock.height + 8 + 1 + 8 + sizeBlock.height + 10

            Item { // ⑤ the plane and layer the grid shows, below it
                id: planeBlock
                objectName: "entities.editor.plane"
                x: lowerPair.pad
                y: 8
                width: lowerPair.wide ? naturalWidth : lowerPair.inner
                height: childrenRect.height
                // the buttons, a gap and "Layer n of m"
                readonly property real naturalWidth: 3 * Theme.planeWidth + 2 * 6 + 24 + layerLabel.implicitWidth
                MicroLabel {
                    id: planeHeading
                    objectName: "entities.editor.plane.label"
                    text: qsTr("Editing plane")
                    visible: !Theme.minimal
                }
                Item {
                    y: planeHeading.visible ? planeHeading.height + 6 : 0
                    width: parent.width
                    height: Theme.planeHeight
                    RadioStrip {
                      id: planeStrip
                      objectName: "entities.editor.plane.group"
                      width: 3 * Theme.planeWidth + 2 * 6
                      height: Theme.planeHeight
                      count: 3
                      current: root.editor.plane
                      buttons: planeRow.buttons
                      groupName: qsTr("Plane")
                      onPicked: (i) => root.editor.plane = i
                      contentItem: Row {
                        id: planeRow
                        spacing: 6
                        property var buttons: []
                        Repeater {
                            model: ["XY", "XZ", "YZ"]
                            Item {
                                id: planeCell
                                required property string modelData
                                required property int index
                                width: Theme.planeWidth
                                height: Theme.planeHeight
                                readonly property bool available: root.editor.allPlanes || index === 0
                                GlyphButton {
                                    anchors.fill: parent
                                    objectName: "entities.editor.plane." + planeCell.modelData.toLowerCase()
                                    focusPolicy: Qt.NoFocus
                                    ring: planeStrip.visualFocus && on
                                    enabled: planeCell.available
                                    glyphSize: Theme.minimal ? 24 : 30
                                    iconOnly: Theme.minimal
                                    glyph: "plane-" + planeCell.modelData.toLowerCase()
                                    caption: planeCell.modelData
                                    tip: qsTr("Edit the %1 plane").arg(planeCell.modelData)
                                    on: root.editor.plane === planeCell.index
                                    onClicked: root.editor.plane = planeCell.index
                                    Component.onCompleted: { planeRow.buttons[planeCell.index] = this; planeRow.buttons = planeRow.buttons }
                                }
                                // a disabled button has no hover: say why from here
                                HoverHandler { id: planeHover; enabled: !planeCell.available }
                                BtToolTip {
                                    shown: planeHover.hovered
                                    text: qsTr("%1 shapes are edited one XY layer at a time: across the prisms their layers are not alike").arg(App.document.gridTypeName)
                                }
                            }
                        }
                      }
                    }
                    Text {
                        id: layerLabel
                        objectName: "entities.editor.layerLabel"
                        anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                        text: root.editor.layerCount > 0
                              ? qsTr("Layer %1 of %2").arg(root.editor.layer + 1).arg(root.editor.layerCount) : ""
                        color: Theme.muted
                        font.pixelSize: Theme.fontSecondary
                    }
                }
            }

            Rectangle { // the rule between them, stacked
                visible: !lowerPair.wide
                x: 0
                y: planeBlock.y + planeBlock.height + 8
                width: lowerPair.width
                height: 1
                color: Theme.line
            }

            Item { // ⑧
                id: sizeBlock
                objectName: "entities.editor.size"
                x: lowerPair.wide ? lowerPair.width - lowerPair.pad - width : lowerPair.pad
                y: lowerPair.wide ? 8 : planeBlock.y + planeBlock.height + 8 + 1 + 8
                width: lowerPair.wide ? lowerPair.sizeWidth : lowerPair.inner
                height: sizeCol.implicitHeight
                Column {
                    id: sizeCol
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    spacing: 6
                    Item {
                        width: parent.width
                        height: applyAll.height
                        MicroLabel {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Grid size")
                        }
                        Row {
                            anchors.right: parent.right
                            spacing: 8
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: qsTr("Apply to all shapes")
                                color: Theme.muted
                                font.pixelSize: Theme.fontSecondary
                            }
                            BtSwitch {
                                id: applyAll
                                objectName: "entities.editor.size.applyAll"
                                anchors.verticalCenter: parent.verticalCenter
                                checked: root.editor.applyToAll
                                onToggled: root.editor.applyToAll = checked
                                Accessible.name: qsTr("Apply to all shapes")
                                BtToolTip { shown: applyAll.hovered || applyAll.visualFocus; text: qsTr("Size changes apply to every shape") }
                            }
                        }
                    }
                    Row {
                        width: parent.width
                        spacing: 6
                        Repeater {
                            model: 3
                            Stepper {
                                required property int index
                                readonly property string axisName: root.axisNames[index]
                                objectName: "entities.editor.size." + axisName.toLowerCase()
                                width: (parent.width - 12) / 3
                                enabled: root.editor.hasShape
                                axis: axisName
                                axisColor: root.axisColours[index]
                                value: index === 0 ? root.editor.sizeX : index === 1 ? root.editor.sizeY : root.editor.sizeZ
                                from: 1
                                to: root.editor.maxSize
                                tip: qsTr("Size along %1").arg(axisName)
                                onCommitted: (v) => root.editor.setSize(index, v)
                            }
                        }
                    }
                }
            }
        }
    }

    ColourFooter { // ⑨
        id: footer
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
    }
}
