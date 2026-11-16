import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// Set range for all pieces (C16; legacy's bulk range window): gives every
// piece in the puzzle one minimum and maximum. Max is at least 1 and never
// below Min, as the two steppers keep it.
//
// Wireframe (each number marks its code below):
//
//   ┌ Set range for all pieces ─────────────────┐
//   │ ① Every piece in this puzzle gets ...     │
//   │ ② [ Min − 0 + ]   [ Max − 1 + ]           │
//   ├───────────────────────────────────────────┤
//   │ ③                     [Cancel]  [Apply]   │
//   └───────────────────────────────────────────┘
BtDialog {
    id: root
    objectName: "puzzle.setRange.dialog"
    title: qsTr("Set range for all pieces")
    defaultButton: applyButton
    width: 400

    property int minValue: 0
    property int maxValue: 1

    onAboutToShow: { minValue = 0; maxValue = 1 }
    onAccepted: {
        // Enter presses Apply without leaving the field
        minField.commitText()
        maxField.commitText()
        App.puzzle.setRangeForAll(minValue, maxValue)
    }

    contentItem: Column {
        spacing: 12
        Text { // ①
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("Every piece in this puzzle gets this minimum and maximum.")
            color: Theme.text
            font.pixelSize: Theme.fontBody
        }
        Row { // ②
            spacing: 8
            Stepper {
                id: minField
                objectName: "puzzle.setRange.min"
                implicitWidth: 150
                axis: qsTr("Min")
                axisColor: Theme.muted
                from: 0
                to: App.puzzle.maxCount
                value: root.minValue
                tip: qsTr("Minimum")
                onCommitted: (v) => {
                    root.minValue = v
                    root.maxValue = Math.max(root.maxValue, v)
                }
            }
            Stepper {
                id: maxField
                objectName: "puzzle.setRange.max"
                implicitWidth: 150
                axis: qsTr("Max")
                axisColor: Theme.muted
                from: Math.max(1, root.minValue)
                to: App.puzzle.maxCount
                value: root.maxValue
                tip: qsTr("Maximum")
                onCommitted: (v) => root.maxValue = v
            }
        }
    }

    footer: Item { // ③
        implicitHeight: 60
        Row {
            anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
            spacing: 8
            BtButton { objectName: "puzzle.setRange.cancel"; text: qsTr("Cancel"); onClicked: root.reject() }
            BtButton { id: applyButton; objectName: "puzzle.setRange.apply"; text: qsTr("Apply"); primary: true; onClicked: root.accept() }
        }
    }
}
