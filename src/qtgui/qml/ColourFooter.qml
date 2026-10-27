pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BurrTools.Ui

// C09 Drawing colour: the footer of the Voxel editor card. The colour new
// and painted voxels get (App.colours.drawColour), the puzzle's colours to
// choose it from, and Add, Edit and Remove. Default (id 0) is the piece's
// own colour, so its swatch shows the selected shape's colour, marked with a
// small ring.
//
// Wireframe (each number marks its code below):
//
//   ┌───────────────────────────────────────────┐
//   │ ⑦COLOURS ⓘ                                 │   what colours are for, as a tooltip
//   │ ①┌──┐ ②DRAWING COLOUR          ④[+] ⑤[⋯] │   ① 32 dp; Minimal 26, the
//   │  └──┘   C1                               │      caption in its tooltip
//   │ ③(●)(■)(■)(■)(■)(■)(■)(■)                 │   the palette, full width:
//   │   (■)(■)                                  │   it wraps, 3 rows, then scrolls
//   └───────────────────────────────────────────┘
//   ⑥ the colour menu (⋯, right-click a swatch, Menu key): Edit, Remove
Rectangle {
    id: root
    objectName: "entities.editor.colour"
    implicitHeight: header.height + 8 + row.implicitHeight + 8 + palette.implicitHeight + 16
    color: Theme.panel

    readonly property int swatchSize: 26
    readonly property string paletteHelp: qsTr("Arrow keys choose, Enter edits, Delete removes, the Menu key has the rest")
    /* what the colours are for: voxels are coloured mainly to constrain
     * where pieces may go (the Puzzle workspace's colour rules) */
    readonly property string purpose: qsTr("Colours are mainly for colour constraints: in the Puzzle workspace, colour rules say which colours of the pieces may fill which colours of the result. New and painted voxels get the drawing colour; Default, the shape's own colour, constrains nothing.")

    function openMenu(id: int, at: Item, x: real, y: real) {
        colourMenu.colourId = id
        colourMenu.popup(at, x, y)
    }
    // the drawing colour's swatch scrolled into the palette's view
    function reveal() {
        flow.forceLayout()                // a colour just added is placed on the next polish
        const s = swatches.itemAt(App.colours.drawColour)
        if (s === null)
            return
        if (s.y < scroller.contentY)
            scroller.contentY = Math.max(0, s.y - flow.padding)
        else if (s.y + s.height > scroller.contentY + scroller.height)
            scroller.contentY = s.y + s.height + flow.padding - scroller.height
    }

    Connections {
        target: App.colours
        function onDrawColourChanged() { root.reveal() }
        // the menu names one colour: when the colours change under it, it goes
        function onCountChanged() { colourMenu.close() }
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 1
        color: Theme.line
    }

    // one colour, 26 x 26, radius 7; the drawing colour ringed: 2 dp panel, 4 dp accent
    component Swatch: Item {
        id: sw
        required property int index
        required property string name
        required property color rgb
        required property bool isDefault
        property int size: 26
        readonly property bool selected: index === App.colours.drawColour
        signal picked()
        signal editRequested()
        signal menuRequested(real x, real y)

        objectName: "entities.editor.colour.swatch." + index
        width: size
        height: size
        Accessible.role: Accessible.RadioButton
        Accessible.name: isDefault ? qsTr("Default (C0, the piece's colour)") : name
        Accessible.checkable: true
        Accessible.checked: selected
        Accessible.onPressAction: App.colours.select(sw.index)

        Rectangle {
            visible: sw.selected
            anchors { fill: parent; margins: -6 }
            radius: 13
            color: "transparent"
            border.width: 4
            border.color: Theme.accent
        }
        Rectangle {
            visible: sw.selected
            anchors { fill: parent; margins: -2 }
            radius: 9
            color: "transparent"
            border.width: 2
            border.color: Theme.panel
        }
        Rectangle {
            anchors.fill: parent
            radius: 7
            // Default is the selected shape's colour; panel2 when there is none
            color: !sw.isDefault ? sw.rgb : App.viewport.shapeColor.valid ? App.viewport.shapeColor : Theme.panel2
            border.width: 1
            border.color: Qt.rgba(0, 0, 0, 0.25)
            // Default: a small ring in the middle, in the text colour for the shape's colour
            Rectangle {
                visible: sw.isDefault
                anchors.centerIn: parent
                width: 10
                height: 10
                radius: 5
                color: "transparent"
                border.width: 2
                // readable on the shape's colour; on the empty fill, muted
                border.color: App.viewport.shapeColor.valid ? App.viewport.shapeTextColor : Theme.muted
            }
        }
        // click picks, double-click edits (a colour, not Default), right-click
        // opens the colour menu. A MouseArea, not TapHandlers: it gets the
        // platform's double-click as one event, which tests can send too.
        MouseArea {
            id: swMouse
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true
            onClicked: (mouse) => {
                if (mouse.button === Qt.RightButton)
                    sw.menuRequested(mouse.x, mouse.y)
                else
                    sw.picked()
            }
            onDoubleClicked: (mouse) => {
                if (mouse.button === Qt.LeftButton && !sw.isDefault)
                    sw.editRequested()
            }
        }
        BtToolTip {
            shown: swMouse.containsMouse
            text: sw.isDefault ? sw.Accessible.name : qsTr("%1 — double-click to edit").arg(sw.name)
        }
    }

    Row { // ⑦
        id: header
        objectName: "entities.editor.colour.header"
        anchors { left: parent.left; top: parent.top; leftMargin: 12; topMargin: 8 }
        height: info.height
        spacing: 4
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Colours")
            color: Theme.muted
            font.pixelSize: Theme.fontMicro
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
        }
        // the purpose, by pointer or keyboard
        AbstractButton {
            id: info
            objectName: "entities.editor.colour.info"
            anchors.verticalCenter: parent.verticalCenter
            width: 20
            height: 20
            focusPolicy: Qt.TabFocus
            hoverEnabled: true
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("About colours")
            Accessible.description: root.purpose
            readonly property alias toolTip: infoTip
            background: Rectangle {
                radius: 10
                color: info.hovered ? Theme.panel2 : "transparent"
                border.width: info.visualFocus ? 2 : 0
                border.color: Theme.accent
            }
            contentItem: Item {
                Icon {
                    anchors.centerIn: parent
                    name: "info"
                    size: 14
                    color: info.hovered || info.visualFocus ? Theme.text : Theme.muted
                }
            }
            BtToolTip { id: infoTip; shown: info.hovered || info.visualFocus; text: root.purpose }
        }
    }

    RowLayout {
        id: row
        anchors { left: parent.left; right: parent.right; top: header.bottom
                  leftMargin: 12; rightMargin: 12; topMargin: 8 }
        spacing: 12

        Rectangle { // ①
            id: current
            objectName: "entities.editor.colour.current"
            Layout.alignment: Qt.AlignTop
            implicitWidth: Theme.drawSwatch
            implicitHeight: Theme.drawSwatch
            radius: Math.round(width / 4)
            color: Theme.line2
            Accessible.ignored: true                 // the name beside it says it
            Rectangle {
                anchors { fill: parent; margins: 1 }
                radius: current.radius - 1
                color: Theme.panel
                Rectangle {
                    anchors { fill: parent; margins: 2 }
                    radius: current.radius - 3
                    color: App.colours.drawShown.valid ? App.colours.drawShown : Theme.panel2
                }
            }
            // Minimal: the caption it loses, with the name
            readonly property string tip: qsTr("Drawing colour: %1").arg(App.colours.drawName)
            HoverHandler { id: currentHover; enabled: Theme.minimal }
            BtToolTip { shown: currentHover.hovered; text: current.tip }
        }

        Column { // ②
            Layout.alignment: Theme.minimal ? Qt.AlignVCenter : Qt.AlignTop
            Layout.fillWidth: true
            Layout.minimumWidth: 78
            spacing: 2
            Text {
                objectName: "entities.editor.colour.caption"
                visible: !Theme.minimal
                text: qsTr("Drawing colour")
                font.capitalization: Font.AllUppercase
                color: Theme.muted
                font.pixelSize: Theme.fontMicro
                font.weight: Font.DemiBold
                font.letterSpacing: 0.5
            }
            Text {
                objectName: "entities.editor.colour.name"
                text: App.colours.drawName
                color: Theme.text
                font.pixelSize: Theme.fontBody
                font.weight: Font.DemiBold
            }
        }

        IconButton { // ④
            objectName: "entities.editor.colour.add"
            Layout.alignment: Qt.AlignTop
            iconName: "plus"
            enabled: App.colours.canAdd
            tip: App.colours.canAdd ? qsTr("Add colour") : qsTr("Add colour: a puzzle has at most %1 colours").arg(App.colours.maxColours)
            onClicked: App.colours.addColour()
        }
        IconButton { // ⑤
            id: moreButton
            objectName: "entities.editor.colour.more"
            Layout.alignment: Qt.AlignTop
            iconName: "more-horizontal"
            tip: qsTr("Colour options")
            onClicked: root.openMenu(App.colours.drawColour, moreButton, 0, moreButton.height)
        }
    }

    Control { // ③
        id: palette
        objectName: "entities.editor.colour.palette"
        anchors { left: parent.left; right: parent.right; top: row.bottom
                  leftMargin: 6; rightMargin: 6; topMargin: 8 }
        height: implicitHeight
        // three rows, then it scrolls
        implicitHeight: Math.min(flow.implicitHeight, 3 * root.swatchSize + 2 * flow.spacing + 2 * flow.padding)
        // a Tab stop; a click on a swatch focuses it too, without the ring
        focusPolicy: Qt.TabFocus
        Accessible.role: Accessible.List
        Accessible.name: qsTr("Drawing colour: %1").arg(App.colours.drawName)
        Accessible.description: root.paletteHelp + ". " + root.purpose
        // the palette's keys are its own, not the window's (Home is the 3D view's)
        Keys.onShortcutOverride: (event) => {
            if ([Qt.Key_Home, Qt.Key_End, Qt.Key_Return, Qt.Key_Enter, Qt.Key_Delete, Qt.Key_Backspace].indexOf(event.key) >= 0
                    && (event.modifiers & ~Qt.KeypadModifier) === Qt.NoModifier)
                event.accepted = true
        }
        Keys.onLeftPressed: App.colours.select(App.colours.drawColour - 1)
        Keys.onRightPressed: App.colours.select(App.colours.drawColour + 1)
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
                root.openMenu(App.colours.drawColour, palette, 20, 20)
            } else if (event.key === Qt.Key_Home) {
                App.colours.select(0)
            } else if (event.key === Qt.Key_End) {
                App.colours.select(App.colours.count - 1)
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                App.colours.requestEdit(App.colours.drawColour)       // Default: nothing to edit
            } else if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) {
                App.colours.requestRemove(App.colours.drawColour)
            } else {
                return
            }
            event.accepted = true
        }
        // what the keys do, for the keyboard's eyes (the swatches' own tips are the mouse's)
        BtToolTip {
            shown: palette.visualFocus
            text: qsTr("%1 — %2").arg(App.colours.drawName).arg(root.paletteHelp)
        }

        // the keyboard came here (Tab): the whole palette is ringed; a click is
        // no reason for a ring
        background: Rectangle {
            radius: Theme.radiusControl
            color: "transparent"
            border.width: palette.visualFocus ? 2 : 0
            border.color: Theme.accent
        }

        contentItem: Flickable {
            id: scroller
            contentHeight: flow.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 4 }

            Flow {
                id: flow
                width: scroller.width
                padding: 6
                spacing: 8
                Repeater {
                    id: swatches
                    model: App.colours
                    delegate: Swatch {
                        id: swatch
                        size: root.swatchSize
                        onPicked: {
                            App.colours.select(swatch.index)
                            palette.forceActiveFocus(Qt.MouseFocusReason)
                        }
                        onEditRequested: App.colours.requestEdit(swatch.index)
                        onMenuRequested: (x, y) => {
                            App.colours.select(swatch.index)
                            root.openMenu(swatch.index, swatch, x, y)
                        }
                    }
                }
            }
        }
    }

    AppMenu { // ⑥
        id: colourMenu
        objectName: "entities.editor.colour.menu"
        property int colourId: 0

        AppMenuItem {
            objectName: "entities.editor.colour.menu.edit"
            text: qsTr("Edit colour…")
            enabled: colourMenu.colourId > 0
            onTriggered: App.colours.requestEdit(colourMenu.colourId)
        }
        AppMenuItem {
            objectName: "entities.editor.colour.menu.remove"
            text: qsTr("Remove colour")
            danger: true
            enabled: colourMenu.colourId > 0
            onTriggered: App.colours.requestRemove(colourMenu.colourId)
        }
    }
}
