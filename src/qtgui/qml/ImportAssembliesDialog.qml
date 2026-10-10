import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BurrTools.Ui

// File ▸ Import assemblies (legacy assmImportWindow_c "Convert Assemblies to
// Shapes"), restyled with its content kept (OQ-34): Source problem;
// Destination (just add shapes / new problem / existing problem); the count
// Range given to the new shapes in that problem; the Filter, with the two
// tool filters for brick puzzles only; Shape min/max in voxels.
//
// Wireframe (each number marks its code below):
//
//   ┌ Import assemblies as shapes ─────────────────────────────────┐
//   │ ② Source                    │ ⑦ Filter                       │
//   │ ③ ┌──────────────────────┐  │   [ ] Drop disconnected shapes │
//   │   │ problems with        │  │   [ ] Drop mirror symmetry     │
//   │   │ solutions            │  │   [ ] Drop any symmetry        │
//   │   └──────────────────────┘  │   [ ] Drop non-millable  brick │
//   │ ④ Destination               │   [ ] Drop non-notchable  only │
//   │   (•) Just add shapes       │   [ ] Remove identical shapes  │
//   │   ( ) To a new problem      │ ⑧ Shape min [  ] max [  ]      │
//   │   ( ) To existing problem   │   Shape sizes count the fixed  │
//   │ ⑤ ┌──────────────────────┐  │   voxels.                      │
//   │   │ problems             │  │                                │
//   │   └──────────────────────┘  │                                │
//   │ ⑥ Range  Min [  ] Max [  ]  │                                │
//   ├─────────────────────────────┴────────────────────────────────┤
//   │ ⑨                                       [Cancel] [Import]    │
//   └──────────────────────────────────────────────────────────────┘
//   ① instead of it all: "You need a problem with solutions ..."
BtDialog {
    id: root
    objectName: "tools.import.dialog"
    title: qsTr("Import assemblies as shapes")
    width: 740

    property var problems: []
    property bool bricks: false
    property string destination: "shapes"
    readonly property bool hasProblems: problems.length > 0

    onAboutToShow: {
        problems = App.tools.problems()
        bricks = App.tools.isBricks()
        destination = "shapes"
        source.currentProblem = 0
        target.currentProblem = 0
        rangeMin.value = 0
        rangeMax.value = 1
        dropDisconnected.selected = true
        dropMirror.selected = false
        dropSymmetric.selected = false
        dropNonMillable.selected = false
        dropNonNotchable.selected = false
        dropIdentical.selected = true
        shapeMin.value = 0
        shapeMax.value = 1000000
    }

    function options() {
        return {
            source: source.currentProblem,
            destination: destination,
            target: target.currentProblem,
            rangeMin: rangeMin.value,
            rangeMax: rangeMax.value,
            dropDisconnected: dropDisconnected.selected,
            dropMirror: dropMirror.selected,
            dropSymmetric: dropSymmetric.selected,
            dropNonMillable: bricks && dropNonMillable.selected,
            dropNonNotchable: bricks && dropNonNotchable.selected,
            dropIdentical: dropIdentical.selected,
            shapeMin: shapeMin.value,
            shapeMax: shapeMax.value
        }
    }

    onAccepted: {
        if (!hasProblems)
            return
        const n = App.tools.importAssemblies(options())
        App.status.flash(n === 1 ? qsTr("Imported 1 shape") : qsTr("Imported %1 shapes").arg(n))
    }

    component SectionTitle: Text {
        Layout.fillWidth: true
        Layout.topMargin: 6
        color: Theme.muted
        font.pixelSize: Theme.fontMicro
        font.weight: Font.Bold
        font.capitalization: Font.AllUppercase
    }

    contentItem: Item {
        implicitHeight: root.hasProblems ? form.implicitHeight : none.implicitHeight

        Text { // ①
            id: none
            visible: !root.hasProblems
            width: parent.width
            text: qsTr("You need a problem with solutions to import assemblies.")
            color: Theme.text
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
        }

        RowLayout {
            id: form
            visible: root.hasProblems
            width: parent.width
            spacing: 24

            ColumnLayout { // ②
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.alignment: Qt.AlignTop
                spacing: 6

                SectionTitle { text: qsTr("Source"); Layout.topMargin: 0 }
                ProblemList { // ③
                    id: source
                    objectName: "tools.import.source"
                    Layout.fillWidth: true
                    problems: root.problems
                }

                SectionTitle { text: qsTr("Destination") } // ④
                BtCheckBox {
                    objectName: "tools.import.dest.shapes"
                    radio: true
                    text: qsTr("Just add shapes to puzzle")
                    selected: root.destination === "shapes"
                    onClicked: root.destination = "shapes"
                }
                BtCheckBox {
                    objectName: "tools.import.dest.new"
                    radio: true
                    text: qsTr("Add shapes to a new problem")
                    selected: root.destination === "new"
                    onClicked: root.destination = "new"
                }
                BtCheckBox {
                    objectName: "tools.import.dest.existing"
                    radio: true
                    text: qsTr("Add shapes to existing problem")
                    selected: root.destination === "existing"
                    onClicked: root.destination = "existing"
                }
                ProblemList { // ⑤
                    id: target
                    objectName: "tools.import.target"
                    Layout.fillWidth: true
                    implicitHeight: 84
                    problems: root.problems
                    showSolutions: false
                    enabled: root.destination === "existing"
                }

                SectionTitle { text: qsTr("Range") } // ⑥
                RowLayout {
                    spacing: 8
                    enabled: root.destination !== "shapes"
                    // the fields dim themselves when disabled
                    Text { text: qsTr("Min"); color: parent.enabled ? Theme.text : Theme.muted; font.pixelSize: Theme.fontBody }
                    NumberField { id: rangeMin; objectName: "tools.import.rangeMin" }
                    Text { text: qsTr("Max"); color: parent.enabled ? Theme.text : Theme.muted; font.pixelSize: Theme.fontBody; Layout.leftMargin: 8 }
                    NumberField { id: rangeMax; objectName: "tools.import.rangeMax" }
                }
            }

            ColumnLayout { // ⑦
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.alignment: Qt.AlignTop
                spacing: 6

                SectionTitle { text: qsTr("Filter"); Layout.topMargin: 0 }
                BtCheckBox { id: dropDisconnected; objectName: "tools.import.dropDisconnected"; text: qsTr("Drop disconnected shapes") }
                BtCheckBox { id: dropMirror; objectName: "tools.import.dropMirror"; text: qsTr("Drop shapes with mirror symmetry") }
                BtCheckBox { id: dropSymmetric; objectName: "tools.import.dropSymmetric"; text: qsTr("Drop all shapes with a symmetry") }
                BtCheckBox { id: dropNonMillable; objectName: "tools.import.dropNonMillable"; visible: root.bricks; text: qsTr("Drop non millable shapes") }
                BtCheckBox { id: dropNonNotchable; objectName: "tools.import.dropNonNotchable"; visible: root.bricks; text: qsTr("Drop non notchable shapes") }
                BtCheckBox { id: dropIdentical; objectName: "tools.import.dropIdentical"; text: qsTr("Remove identical shapes") }
                RowLayout { // ⑧
                    spacing: 8
                    Text { text: qsTr("Shape min"); color: Theme.text; font.pixelSize: Theme.fontBody }
                    NumberField { id: shapeMin; objectName: "tools.import.shapeMin" }
                    Text { text: qsTr("max"); color: Theme.text; font.pixelSize: Theme.fontBody; Layout.leftMargin: 8 }
                    NumberField { id: shapeMax; objectName: "tools.import.shapeMax" }
                }
                Text { text: qsTr("Shape sizes count the fixed voxels."); color: Theme.muted; font.pixelSize: Theme.fontSecondary }
            }
        }
    }

    footer: Item { // ⑨
        implicitHeight: 60
        Row {
            anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
            spacing: 8
            BtButton {
                objectName: "tools.import.cancel"
                text: root.hasProblems ? qsTr("Cancel") : qsTr("OK")
                primary: !root.hasProblems
                onClicked: root.reject()
            }
            BtButton {
                objectName: "tools.import.ok"
                visible: root.hasProblems
                text: qsTr("Import")
                primary: true
                onClicked: root.accept()
            }
        }
    }
}
