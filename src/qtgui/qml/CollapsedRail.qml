import QtQuick
import BurrTools.Ui

// The rail a collapsed side card becomes (C11): expand button at the top,
// optional extra items, the card title written vertically, and an optional
// list below it (the shape chips, CollapsedShapes).
//
// Wireframe (each number marks its code below):
//
//   ┌────┐
//   │①[◧]│   expand
//   │ ②  │   extras, if any
//   │ ③S │   the title, turned
//   │  h │
//   │  a │
//   │  p │
//   │  e │
//   │  s │
//   │ ④  │   list, if any
//   │[S1]│
//   │[S2]│
//   └────┘
Rectangle {
    id: root
    property string title
    property string expandIcon          // "panelL" / "panelR"
    property string expandObjectName
    default property alias extras: extraColumn.data
    property alias list: listSlot.data          // below the title, filling the rest
    readonly property Item expandButton: expandBtn   // Main keeps the keyboard on it (or the card's collapse)
    signal expandClicked()

    /* The rail shows at once while its column narrows from the card's width
     * to its own (Main.qml animates it): what it holds keeps the rail's width,
     * at the column's fixed edge -- the left for the left card, the right for
     * the right -- so nothing stretches or slides meanwhile. */
    readonly property real contentX: expandIcon === "panelR" ? width - Theme.collapsedRail : 0

    color: Theme.panel
    radius: Theme.cardRadius
    border.color: Theme.line
    border.width: Theme.minimal ? 0 : 1
    clip: true

    Column {
        id: header
        x: root.contentX
        width: Theme.collapsedRail
        anchors { top: parent.top; topMargin: 8 }
        spacing: 8
        IconButton { // ①
            id: expandBtn
            objectName: root.expandObjectName
            anchors.horizontalCenter: parent.horizontalCenter
            iconName: root.expandIcon
            tip: qsTr("Expand sidebar")
            onClicked: root.expandClicked()
        }
        Column { // ②
            id: extraColumn
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8
        }
        Item { // ③
            anchors.horizontalCenter: parent.horizontalCenter
            width: titleText.implicitHeight
            height: titleText.implicitWidth
            Text {
                id: titleText
                text: root.title
                color: Theme.muted
                font.pixelSize: Theme.fontSecondary
                font.weight: Font.DemiBold
                rotation: 90
                anchors.centerIn: parent
            }
        }
    }

    Item { // ④
        id: listSlot
        x: root.contentX
        width: Theme.collapsedRail
        anchors { top: header.bottom; topMargin: 10
                  bottom: parent.bottom; bottomMargin: 6 }
    }
}
