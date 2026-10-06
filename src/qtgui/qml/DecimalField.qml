import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// A number input (legacy LFl_Float_Input, or LFl_Int_Input with `integer`),
// h30, radius 8. `committed` fires with the parsed value when editing
// finishes -- Enter or leaving the field -- which is when legacy's inputs
// called back. An unparsable entry reads as 0, as atof() did.
TextField {
    id: root
    property bool allowNegative: true
    property bool integer: false        // whole numbers only (LFl_Int_Input)
    signal committed(real value)

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
    inputMethodHints: Qt.ImhFormattedNumbersOnly
    validator: integer ? intValidator : doubleValidator
    readonly property var doubleValidator: DoubleValidator {
        bottom: root.allowNegative ? -1e9 : 0
        top: 1e9
        notation: DoubleValidator.StandardNotation
        locale: "C"
    }
    readonly property var intValidator: IntValidator { bottom: root.allowNegative ? -1000000000 : 0; top: 1000000000 }

    onEditingFinished: {
        const v = parseFloat(text)
        committed(isNaN(v) ? 0 : v)
    }

    background: Rectangle {
        radius: Theme.radiusControl
        color: Theme.panel
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? Theme.accent : Theme.line2
    }
    opacity: enabled ? 1 : 0.4
}
