import QtQuick
import QtQuick.Layouts
import BurrTools.Ui

// PR-10 Card: rounded panel with an optional header (title, extra controls,
// collapse button). Minimal density: flush, no radius, no border.
Rectangle {
    id: root
    property string title
    property string collapseIcon        // "panelL" / "panelR"; empty = no collapse button
    property string collapseObjectName
    property alias headerExtras: extras.data       // after the title
    property alias headerActions: actions.data     // at the right, before the collapse button
    readonly property Item collapseButton: collapseBtn   // Main keeps the keyboard on it (or the rail's expand)
    default property alias content: body.data
    property bool showHeader: title.length > 0
    signal collapseClicked()

    color: Theme.panel
    radius: Theme.cardRadius
    border.color: Theme.line
    border.width: Theme.minimal ? 0 : 1
    clip: true

    Item {
        id: header
        // C01's "entities.shapes.header", and the like for every card
        objectName: root.objectName.length > 0 ? root.objectName + ".header" : ""
        visible: root.showHeader
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: visible ? Theme.cardHeader : 0

        RowLayout {
            anchors { fill: parent; leftMargin: 14; rightMargin: 8 }
            spacing: 6
            Text {
                text: root.title
                color: Theme.text
                font.pixelSize: Theme.fontBody
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Row {
                id: extras
                spacing: 6
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
            }
            Row {
                id: actions
                spacing: 2
                Layout.alignment: Qt.AlignVCenter
            }
            IconButton {
                id: collapseBtn
                visible: root.collapseIcon.length > 0
                objectName: root.collapseObjectName
                iconName: root.collapseIcon
                tip: qsTr("Collapse sidebar")
                onClicked: root.collapseClicked()
            }
        }
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 1
            color: Theme.line
        }
    }

    Item {
        id: body
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom }
    }
}
