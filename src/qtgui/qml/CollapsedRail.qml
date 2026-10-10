import QtQuick
import BurrTools.Ui

// The rail a collapsed side card becomes (C11): expand button at the top,
// optional extra items, and the card title written vertically.
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
//   └────┘
Rectangle {
    id: root
    property string title
    property string expandIcon          // "panelL" / "panelR"
    property string expandObjectName
    default property alias extras: extraColumn.data
    signal expandClicked()

    color: Theme.panel
    radius: Theme.cardRadius
    border.color: Theme.line
    border.width: Theme.minimal ? 0 : 1
    clip: true

    Column {
        anchors { top: parent.top; topMargin: 8; horizontalCenter: parent.horizontalCenter }
        spacing: 8
        IconButton { // ①
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
}
