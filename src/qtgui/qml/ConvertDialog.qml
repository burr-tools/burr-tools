pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BurrTools.Ui

// File ▸ Convert (legacy convertWindow_c), restyled: "Please Select Target
// grid" with one radio per grid this puzzle converts to, the first chosen;
// or the "no grids" notice with a single OK. Continue converts; the result
// replaces the puzzle and starts a fresh undo history (OQ-34: content kept).
BtDialog {
    id: root
    objectName: "tools.convert.dialog"
    title: qsTr("Convert puzzle")
    width: 440

    property var targets: []
    property int selectedType: -1

    onAboutToShow: {
        targets = App.tools.convertTargets()
        selectedType = targets.length > 0 ? targets[0].type : -1
    }
    onAccepted: if (selectedType >= 0) App.tools.convert(selectedType)

    contentItem: ColumnLayout {
        spacing: 8
        Text {
            Layout.fillWidth: true
            text: root.targets.length > 0
                  ? qsTr("Please select the target grid for this %1 puzzle.").arg(App.document.gridTypeName)
                  : qsTr("There are no grids that this puzzle can be converted to, sorry.")
            color: root.targets.length > 0 ? Theme.muted : Theme.text
            font.pixelSize: root.targets.length > 0 ? Theme.fontSecondary : Theme.fontBody
            wrapMode: Text.WordWrap
        }
        Repeater {
            model: root.targets
            delegate: RadioCard {
                required property var modelData
                objectName: "tools.convert.type." + modelData.type
                Layout.fillWidth: true
                text: modelData.name
                selected: root.selectedType === modelData.type
                onClicked: root.selectedType = modelData.type
                onDoubleClicked: { root.selectedType = modelData.type; root.accept() }
            }
        }
    }

    footer: Item {
        implicitHeight: 60
        Row {
            anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
            spacing: 8
            BtButton {
                objectName: "tools.convert.cancel"
                visible: root.targets.length > 0
                text: qsTr("Cancel")
                onClicked: root.reject()
            }
            BtButton {
                objectName: "tools.convert.ok"
                text: root.targets.length > 0 ? qsTr("Convert") : qsTr("OK")
                primary: true
                onClicked: root.targets.length > 0 ? root.accept() : root.reject()
            }
        }
    }
}
