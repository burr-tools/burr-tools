import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-04 Button: h30 (small 26), padding 0x12, radius 8; secondary (panel fill,
// line2 border) or primary (accent fill, white text).
AbstractButton {
    id: root
    property bool primary: false
    property bool danger: false
    property bool small: false

    implicitHeight: small ? 26 : 30
    implicitWidth: Math.max(64, label.implicitWidth + 24)
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: text

    background: Rectangle {
        radius: Theme.radiusControl
        color: root.danger ? Theme.danger
             : root.primary ? (root.hovered ? Qt.lighter(Theme.accent, 1.08) : Theme.accent)
             : (root.hovered ? Theme.panel2 : Theme.panel)
        border.width: (root.primary || root.danger) ? 0 : 1
        border.color: Theme.line2
        Rectangle {
            anchors { fill: parent; margins: -3 }
            visible: root.visualFocus
            radius: Theme.radiusControl + 3
            color: "transparent"
            border.width: 2
            border.color: Theme.accent
        }
    }
    contentItem: Text {
        id: label
        text: root.text
        color: (root.primary || root.danger) ? "white" : Theme.text
        font.pixelSize: Theme.fontBody
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    opacity: enabled ? 1 : 0.4
}
