import QtQuick
import BurrTools.Ui

// PR-25 Summary strip: the bottom of a card: an 18 dp icon, a bold line, a
// muted detail line; ok (the check, in the ok colour) or warn (the triangle
// and the title in amber). The icon and the words carry the state, not the
// hue alone. 1 dp top border, padding 6 x 14.
Rectangle {
    id: root
    property bool ok: true
    property string title
    property string detail
    default property alias extra: extraSlot.data

    implicitHeight: textColumn.implicitHeight + 12
    color: "transparent"

    Accessible.role: Accessible.StaticText
    Accessible.name: qsTr("%1. %2").arg(title).arg(detail)

    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 1
        color: Theme.line
    }
    Icon {
        id: icon
        anchors { left: parent.left; leftMargin: 14; top: parent.top; topMargin: 7 }
        name: root.ok ? "check-circle" : "alert-triangle"
        size: 16
        color: root.ok ? Theme.ok : Theme.warn
    }
    Column {
        id: textColumn
        anchors { left: icon.right; leftMargin: 8; right: parent.right; rightMargin: 14; top: parent.top; topMargin: 6 }
        spacing: 1
        Text {
            objectName: root.objectName + ".title"
            width: parent.width
            text: root.title
            Accessible.ignored: true
            wrapMode: Text.WordWrap
            color: root.ok ? Theme.text : Theme.warn
            font.pixelSize: Theme.fontBody
            font.weight: Font.DemiBold
        }
        Text {
            objectName: root.objectName + ".detail"
            width: parent.width
            text: root.detail
            textFormat: Text.PlainText
            Accessible.ignored: true
            wrapMode: Text.WordWrap
            color: Theme.muted
            font.pixelSize: Theme.fontSecondary
        }
        Item {
            id: extraSlot
            width: parent.width
            visible: shown
            // only what is on show: a hidden row keeps its height in childrenRect
            readonly property bool shown: {
                for (let i = 0; i < children.length; i++)
                    if (children[i].visible)
                        return true
                return false
            }
            implicitHeight: shown ? childrenRect.height : 0
            height: implicitHeight
        }
    }
}
