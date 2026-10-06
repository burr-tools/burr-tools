import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import BurrTools.Ui

// Export ▸ STL (legacy stlExport_c), restyled with its content kept (OQ-34):
// the shape to export, the grid exporter's own parameters with their legacy
// tooltips, Binary STL, and a preview of the exporter's mesh that handles
// like the main view, with the Normal / Insides view and the volume.
//
// The file is chosen in the OS save dialog when exporting (legacy had a
// name and a path field plus a chooser); the dialog stays open after an
// export so more shapes can follow, as legacy's did.
BtDialog {
    id: root
    objectName: "export.stl.dialog"
    title: qsTr("Export STL")
    width: Math.min(1040, (parent ? parent.width : 1200) - 32)
    height: Math.min(700, (parent ? parent.height : 800) - 32)
    closePolicy: Popup.CloseOnEscape

    readonly property var stl: App.stl

    onAboutToShow: stl.begin()
    onClosed: stl.end()

    component SectionTitle: Text {
        Layout.fillWidth: true
        Layout.topMargin: 6
        color: Theme.muted
        font.pixelSize: Theme.fontMicro
        font.weight: Font.Bold
        font.capitalization: Font.AllUppercase
    }

    contentItem: RowLayout {
        spacing: 20

        ColumnLayout {
            Layout.preferredWidth: 330
            Layout.maximumWidth: 330
            Layout.fillHeight: true
            spacing: 6

            SectionTitle { text: qsTr("Shape"); Layout.topMargin: 0 }
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 90
                radius: Theme.radiusControl
                color: Theme.panel
                border.color: Theme.line2
                ListView {
                    id: shapeList
                    objectName: "export.stl.shapes"
                    anchors { fill: parent; margins: 4 }
                    clip: true
                    model: App.shapes
                    currentIndex: root.stl.shape
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}
                    onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
                    delegate: AbstractButton {
                        id: shapeRow
                        required property int index
                        required property string idText
                        required property string label
                        required property color chipColor
                        required property color chipTextColor
                        objectName: "export.stl.shape." + index
                        width: ListView.view.width
                        height: 28
                        hoverEnabled: true
                        Accessible.role: Accessible.RadioButton
                        Accessible.name: idText + (label.length ? " " + label : "")
                        Accessible.checked: root.stl.shape === index
                        onClicked: root.stl.shape = index
                        background: Rectangle {
                            radius: Theme.radiusChip
                            color: root.stl.shape === shapeRow.index ? Theme.accentSoft
                                 : shapeRow.hovered ? Theme.panel2 : "transparent"
                            border.width: root.stl.shape === shapeRow.index ? 1 : 0
                            border.color: Theme.accent
                        }
                        contentItem: Row {
                            leftPadding: 6
                            spacing: 8
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: Math.max(30, chipText.implicitWidth + 12)
                                height: 20
                                radius: Theme.radiusChip
                                color: shapeRow.chipColor
                                Text {
                                    id: chipText
                                    anchors.centerIn: parent
                                    text: shapeRow.idText
                                    color: shapeRow.chipTextColor
                                    font.pixelSize: Theme.fontSecondary
                                    font.weight: Font.DemiBold
                                }
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: shapeRow.label
                                color: Theme.text
                                font.pixelSize: Theme.fontSecondary
                                elide: Text.ElideRight
                                width: shapeRow.width - 60
                            }
                        }
                    }
                }
            }

            SectionTitle { text: qsTr("Parameters") }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: root.stl.parameterCount
                    delegate: Item {
                        id: param
                        required property int index
                        // re-read whenever a value changes; the field itself stays
                        readonly property var info: { root.stl.revision; return root.stl.parameter(index) }
                        readonly property bool isSwitch: info.type === "switch"
                        Layout.fillWidth: true
                        implicitHeight: 30
                        objectName: "export.stl.param." + index

                        Text {
                            visible: !param.isSwitch
                            anchors { left: parent.left; verticalCenter: parent.verticalCenter; right: field.left; rightMargin: 8 }
                            text: param.info.name
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            elide: Text.ElideRight
                        }
                        DecimalField {
                            id: field
                            objectName: "export.stl.param.field." + param.index
                            visible: !param.isSwitch
                            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                            width: 120
                            text: param.info.text
                            allowNegative: param.info.type === "double"
                            integer: param.info.type === "posInt"
                            onCommitted: (value) => {
                                root.stl.setParameter(param.index, value)
                                // show the value as the exporter took it
                                text = Qt.binding(() => param.info.text)
                            }
                            hoverEnabled: true
                            readonly property alias toolTip: fieldTip
                            BtToolTip {
                                id: fieldTip
                                shown: field.hovered
                                text: param.info.tooltip
                            }
                        }
                        BtCheckBox {
                            id: paramSwitch
                            visible: param.isSwitch
                            objectName: "export.stl.param.switch." + param.index
                            anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                            text: param.info.name
                            selected: param.info.value !== 0
                            onClicked: root.stl.setParameter(param.index, selected ? 1 : 0)
                            readonly property alias toolTip: switchTip
                            BtToolTip {
                                id: switchTip
                                shown: paramSwitch.hovered
                                text: param.info.tooltip
                            }
                        }
                    }
                }
            }

            SectionTitle { text: qsTr("File") }
            BtCheckBox {
                objectName: "export.stl.binary"
                text: qsTr("Binary STL")
                selected: root.stl.binary
                onClicked: root.stl.binary = selected
            }
        }

        // the preview
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 300
            radius: Theme.cardRadius
            color: Theme.canvas
            border.color: Theme.line
            clip: true

            VoxelViewport {
                id: preview
                objectName: "export.stl.preview"
                anchors { fill: parent; margins: 1 }
                controller: root.stl
            }

            Segmented {
                objectName: "export.stl.mode"
                anchors { top: parent.top; right: parent.right; margins: 10 }
                options: [
                    { value: false, label: qsTr("Normal") },
                    { value: true, label: qsTr("Insides") }
                ]
                value: root.stl.insides
                onActivated: (v) => root.stl.insides = v
            }

            IconButton {
                objectName: "export.stl.home"
                anchors { top: parent.top; left: parent.left; margins: 8 }
                iconName: "fit"
                tip: qsTr("Reset the view")
                onClicked: root.stl.home()
            }

            Text {
                objectName: "export.stl.volume"
                anchors { left: parent.left; bottom: parent.bottom; margins: 10 }
                text: root.stl.volumeText
                color: Theme.muted
                font.pixelSize: Theme.fontSecondary
            }

            Text {
                objectName: "export.stl.error"
                visible: root.stl.error.length > 0
                anchors.centerIn: parent
                width: parent.width - 60
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: root.stl.error
                color: Theme.danger
                font.pixelSize: Theme.fontBody
            }
        }
    }

    footer: Item {
        implicitHeight: 60
        Row {
            anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
            spacing: 8
            BtButton { objectName: "export.stl.close"; text: qsTr("Close"); onClicked: root.close() }
            BtButton {
                objectName: "export.stl.export"
                text: qsTr("Export STL…")
                primary: true
                enabled: root.stl.hasMesh
                onClicked: {
                    saveDialog.currentFolder = root.stl.folder
                    saveDialog.selectedFile = root.stl.folder + "/" + root.stl.suggestedName
                    saveDialog.open()
                }
            }
        }
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Choose STL file to write")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("STL files (*.stl)")]
        defaultSuffix: "stl"
        onAccepted: root.stl.exportTo(selectedFile)
    }

    Connections {
        target: root.stl
        function onExported(fileName) { App.status.flash(qsTr("Exported %1").arg(fileName)) }
        function onFailed(message) { failure.text = message; failure.open() }
    }

    MessageDialog {
        id: failure
        title: qsTr("Export STL")
        buttons: MessageDialog.Ok
    }
}
