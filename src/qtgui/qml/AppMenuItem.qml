import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// A PR-11 menu row: h30, hover panel2, a 14 dp check column for checkable
// items (PR-16), the shortcuts right-aligned in muted text, a chevron for a
// submenu. The shortcuts come from the item's action (CommandMenu).
MenuItem {
    id: root
    property string shortcutText: root.action && root.action.shortcutText !== undefined ? root.action.shortcutText : ""

    implicitHeight: 30
    implicitWidth: Math.max(200, leftPadding + rightPadding + contentItem.implicitWidth)
    leftPadding: 10
    rightPadding: 10
    hoverEnabled: true
    readonly property string plainText: root.text.replace(/&(.)/g, "$1")
    Accessible.role: Accessible.MenuItem
    Accessible.name: plainText

    background: Rectangle {
        radius: Theme.radiusChip
        color: root.highlighted ? Theme.panel2 : "transparent"
    }
    contentItem: Item {
        implicitWidth: 24 + label.implicitWidth + (keys.text.length > 0 ? keys.implicitWidth + 32 : 0)
                       + (root.subMenu ? 20 : 0)
        implicitHeight: 30
        Icon {
            visible: root.checkable && root.checked
            anchors { left: parent.left; verticalCenter: parent.verticalCenter }
            name: "check"
            size: 14
            color: Theme.accent
        }
        Text {
            id: label
            objectName: "label"
            anchors { left: parent.left; leftMargin: 24; verticalCenter: parent.verticalCenter }
            // the access-key marker is for the menu bar's mnemonics; not drawn here
            text: root.plainText
            color: Theme.text
            font.pixelSize: Theme.fontBody
        }
        Text {
            id: keys
            objectName: "keys"
            anchors { right: parent.right; rightMargin: root.subMenu ? 20 : 0; verticalCenter: parent.verticalCenter }
            text: root.shortcutText
            color: Theme.muted
            font.pixelSize: Theme.fontMicro
        }
        Icon {
            visible: root.subMenu !== null
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            name: "chevron-down"
            rotation: -90
            size: 14
            color: Theme.muted
        }
    }
    indicator: null
    arrow: null
    opacity: enabled ? 1 : 0.4
}
