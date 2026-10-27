import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// A search box: h32, magnifier at the left, placeholder in muted text
// (C12 header, C22). Esc with text clears it; Esc when empty is left to the
// dialog, which closes.
TextField {
    id: root
    implicitWidth: 260
    implicitHeight: 32
    leftPadding: 32
    rightPadding: 10
    verticalAlignment: TextInput.AlignVCenter
    color: Theme.text
    placeholderTextColor: Theme.muted
    selectionColor: Theme.accent
    selectedTextColor: "white"
    font.pixelSize: Theme.fontBody
    selectByMouse: true

    Keys.onEscapePressed: (event) => {
        if (text.length > 0) {
            text = ""
            event.accepted = true
        } else {
            event.accepted = false
        }
    }

    Icon {
        anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
        name: "search"
        size: 15
        color: Theme.muted
    }

    background: Rectangle {
        radius: Theme.radiusControl
        color: Theme.panel
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? Theme.accent : Theme.line2
    }
}
