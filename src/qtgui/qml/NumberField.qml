import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// A non-negative whole number input (legacy LFl_Int_Input), h30, radius 8.
// `value` follows what is typed; an empty field reads as 0, as atoi() did.
TextField {
    id: root
    property int value: 0
    property int maximum: 2147483647

    implicitWidth: 110
    implicitHeight: 30
    leftPadding: 10
    rightPadding: 10
    verticalAlignment: TextInput.AlignVCenter
    color: Theme.text
    selectionColor: Theme.accent
    selectedTextColor: "white"
    font.pixelSize: Theme.fontBody
    selectByMouse: true
    inputMethodHints: Qt.ImhDigitsOnly
    validator: IntValidator { bottom: 0; top: root.maximum }

    Component.onCompleted: text = String(value)
    onValueChanged: if (parseInt(text || "0") !== value) text = String(value)
    onTextEdited: value = text.length > 0 ? parseInt(text) : 0
    onEditingFinished: if (text.length === 0) text = "0"

    background: Rectangle {
        radius: Theme.radiusControl
        color: Theme.panel
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? Theme.accent : Theme.line2
    }
    opacity: enabled ? 1 : 0.4
}
