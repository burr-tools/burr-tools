import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BurrTools.Ui

// C07 / C08 / C02 Voxel editor card: header with the read-only voxel type
// chip (C14), Focus 2D and collapse; the tools, Mirror and Span, the plane
// and layer, the 2D grid, the grid size and the Drawing colour footer (C09).
//
// Wireframe (each number marks its code below):
//
//   ┌──────────────────────────────────────────────┐
//   │ Voxel editor  ①(▪ Brick)           ②[⤢] [◨] │   title, collapse: Card
//   ├──────────────────────────────────────────────┤
//   │ ③ [■ Fixed 1] [▨ Variable 2] [⌫ Erase 3] [◆ Paint 4] │
//   ├──────────────────────────────────────────────┤
//   │ ④ MIRROR [X][Y][Z]        SPAN [X][Y][Z]     │
//   ├──────────────────────────────────────────────┤
//   │ ⑤ [XY] [XZ] [YZ]                 Layer 1 of 4 │
//   ├────┬─────────────────────────────────────────┤
//   │ ⑥  │ ⑦                                        │
//   │Z ly│   3 ▢ ▢ ▢ ▢                              │
//   │ 4  │   2 ▢ ■ ■ ▢      the 2D grid             │
//   │ 3• │   1 ▢ ■ ▢ ▢                              │
//   │ 2• │   0 ▢ ▢ ▢ ▢                              │
//   │ 1  │     0 1 2 3                              │
//   ├────┴─────────────────────────────────────────┤
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
        function move(d) {
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

        Section { // ③
            objectName: "entities.editor.tools"
            padH: 10
            RadioStrip {
              id: toolStrip
              objectName: "entities.editor.tools.group"
              width: parent.width
              height: 58
              count: 4
              current: root.editor.tool
              buttons: toolRow.buttons
              groupName: qsTr("Tool")
              onPicked: (i) => root.editor.tool = i
              contentItem: Row {
                id: toolRow
                spacing: 6
                readonly property real cell: (width - 3 * spacing) / 4
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
                        width: toolRow.cell
                        height: 58
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
        }

        Section { // ④
            objectName: "entities.editor.modifiers"
            Row {
                width: parent.width
                spacing: 14
                Repeater {
                    model: [
                        { label: qsTr("Mirror"), name: "mirror", glyph: "mirror-" },
                        { label: qsTr("Span"), name: "span", glyph: "span-" }
                    ]
                    Column {
                        id: group
                        required property var modelData
                        width: (parent.width - 14) / 2
                        spacing: 4
                        readonly property bool mirror: modelData.name === "mirror"
                        MicroLabel { text: group.modelData.label }
                        Row {
                            spacing: 6
                            Repeater {
                                model: 3
                                GlyphButton {
                                    required property int index
                                    readonly property string axis: root.axisNames[index]
                                    objectName: "entities.editor." + group.modelData.name + "." + axis.toLowerCase()
                                    toggle: true
                                    width: (group.width - 12) / 3
                                    height: 54
                                    glyphSize: 28
                                    glyph: group.modelData.glyph + axis.toLowerCase()
                                    caption: axis
                                    captionColor: root.axisColours[index]
                                    on: ((group.mirror ? root.editor.mirror : root.editor.span) & (1 << index)) !== 0
                                    tip: group.mirror
                                         ? qsTr("Also place a mirrored voxel across the %1 centre plane").arg(axis)
                                         : qsTr("Fill the whole grid along %1 on each click").arg(axis)
                                    onClicked: group.mirror ? root.editor.toggleMirror(index) : root.editor.toggleSpan(index)
                                }
                            }
                        }
                    }
                }
            }
        }

        Section { // ⑤
            objectName: "entities.editor.plane"
            Item {
                width: parent.width
                height: 56
                RadioStrip {
                  id: planeStrip
                  objectName: "entities.editor.plane.group"
                  width: 3 * 62 + 2 * 6
                  height: 56
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
                            width: 62
                            height: 56
                            readonly property bool available: root.editor.allPlanes || index === 0
                            GlyphButton {
                                anchors.fill: parent
                                objectName: "entities.editor.plane." + planeCell.modelData.toLowerCase()
                                focusPolicy: Qt.NoFocus
                                ring: planeStrip.visualFocus && on
                                enabled: planeCell.available
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
                                text: qsTr("Only brick shapes are edited in the XZ and YZ planes; %1 shapes are edited one XY layer at a time").arg(App.document.gridTypeName)
                            }
                        }
                    }
                  }
                }
                Text {
                    objectName: "entities.editor.layerLabel"
                    anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                    text: root.editor.layerCount > 0
                          ? qsTr("Layer %1 of %2").arg(root.editor.layer + 1).arg(root.editor.layerCount) : ""
                    color: Theme.muted
                    font.pixelSize: Theme.fontSecondary
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
                width: Theme.minimal ? 40 : 48
                anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                color: Theme.panel
                Rectangle {
                    anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                    width: 1
                    color: Theme.line
                }
                Text {
                    id: stripHeader
                    anchors { top: parent.top; topMargin: 8; horizontalCenter: parent.horizontalCenter }
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
                        width: 36
                        height: 28
                        radius: 7
                        color: current ? Theme.accent : (chipMouse.containsMouse ? Theme.panel2 : "transparent")
                        border.width: 1
                        border.color: current || chipMouse.containsMouse ? Theme.accent : Theme.line
                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Layer %1").arg(layerNo + 1)
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
                        BtToolTip { shown: chipMouse.containsMouse; text: qsTr("Layer %1").arg(chip.layerNo + 1) }
                    }
                    // keep the current layer's chip in view
                    function follow() {
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
                    Accessible.role: Accessible.Table
                    Accessible.name: qsTr("2D grid: layer %1 of %2").arg(root.editor.layer + 1).arg(root.editor.layerCount)
                    HoverHandler { id: gridHover }
                    BtToolTip {
                        shown: gridHover.hovered && !root.editor.stroking
                        text: qsTr("Click or drag: active tool (Shift fixed · Alt variable · Ctrl erase) · Right-click or right-drag: erase")
                    }
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

        Rectangle { // the rule above the grid size
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.line
        }
        Item { // ⑧
            objectName: "entities.editor.size"
            Layout.fillWidth: true
            implicitHeight: sizeCol.implicitHeight + 18
            Column {
                id: sizeCol
                anchors { left: parent.left; right: parent.right; top: parent.top
                          leftMargin: 12; rightMargin: 12; topMargin: 8 }
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

    ColourFooter { // ⑨
        id: footer
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
    }
}
