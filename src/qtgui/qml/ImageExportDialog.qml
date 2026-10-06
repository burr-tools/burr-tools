import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import BurrTools.Ui

// Export ▸ Image (legacy imageExport_c "Export Images"), restyled with its
// content kept (OQ-34): what to export (shape, problem, assembly, solution
// as assembly or disassembly steps) from which shape or problem; the
// background, supersampling, colours and dimming of static pieces; the page
// (paper size and DPI, or pixels) and the number of files; and a preview
// whose orientation the pictures take. Pages are written as <name>000.png,
// <name>001.png, ... next to the file chosen in the OS save dialog.
//
// Legacy's unlabelled "Number of images" field was never read and is gone.
BtDialog {
    id: root
    objectName: "export.image.dialog"
    title: qsTr("Export images")
    width: Math.min(1180, (parent ? parent.width : 1200) - 32)
    height: Math.min(660, (parent ? parent.height : 800) - 32)
    closePolicy: root.ex.busy ? Popup.NoAutoClose : Popup.CloseOnEscape

    readonly property var ex: App.images

    onAboutToShow: {
        problemList.problems = App.tools.problems()
        ex.begin()
    }
    onClosed: ex.end()

    component SectionTitle: Text {
        Layout.fillWidth: true
        Layout.topMargin: 6
        color: Theme.muted
        font.pixelSize: Theme.fontMicro
        font.weight: Font.Bold
        font.capitalization: Font.AllUppercase
    }
    component Label: Text {
        color: parent && parent.enabled === false ? Theme.muted : Theme.text
        font.pixelSize: Theme.fontBody
    }

    contentItem: RowLayout {
        spacing: 20
        enabled: !root.ex.busy

        // --- what ---
        ColumnLayout {
            Layout.preferredWidth: 300
            Layout.maximumWidth: 300
            Layout.fillHeight: true
            spacing: 4

            SectionTitle { text: qsTr("Export"); Layout.topMargin: 0 }
            Repeater {
                model: [
                    { mode: "shape", label: qsTr("Shape"), on: root.ex.canShape },
                    { mode: "problem", label: qsTr("Problem"), on: root.ex.canProblem },
                    { mode: "assembly", label: qsTr("Assembly"), on: root.ex.canAssembly },
                    { mode: "solution", label: qsTr("Solution (assembly)"), on: root.ex.canSolution },
                    { mode: "disassembly", label: qsTr("Solution (disassembly)"), on: root.ex.canSolution }
                ]
                delegate: BtCheckBox {
                    required property var modelData
                    objectName: "export.image.mode." + modelData.mode
                    radio: true
                    text: modelData.label
                    enabled: modelData.on
                    selected: root.ex.mode === modelData.mode
                    onClicked: root.ex.mode = modelData.mode
                }
            }

            SectionTitle { text: qsTr("Shape") }
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 70
                radius: Theme.radiusControl
                color: Theme.panel
                border.color: Theme.line2
                enabled: root.ex.mode === "shape"
                opacity: enabled ? 1 : 0.5
                ListView {
                    objectName: "export.image.shapes"
                    anchors { fill: parent; margins: 4 }
                    clip: true
                    model: App.shapes
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}
                    delegate: AbstractButton {
                        id: shapeRow
                        required property int index
                        required property string idText
                        required property string label
                        required property color chipColor
                        required property color chipTextColor
                        objectName: "export.image.shape." + index
                        width: ListView.view.width
                        height: 26
                        hoverEnabled: true
                        onClicked: root.ex.shape = index
                        background: Rectangle {
                            radius: Theme.radiusChip
                            color: root.ex.shape === shapeRow.index ? Theme.accentSoft
                                 : shapeRow.hovered ? Theme.panel2 : "transparent"
                            border.width: root.ex.shape === shapeRow.index ? 1 : 0
                            border.color: Theme.accent
                        }
                        contentItem: Row {
                            leftPadding: 6
                            spacing: 8
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: Math.max(30, chip.implicitWidth + 12)
                                height: 20
                                radius: Theme.radiusChip
                                color: shapeRow.chipColor
                                Text {
                                    id: chip
                                    anchors.centerIn: parent
                                    text: shapeRow.idText
                                    color: shapeRow.chipTextColor
                                    font.pixelSize: Theme.fontSecondary
                                    font.weight: Font.DemiBold
                                }
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                width: shapeRow.width - 60
                                text: shapeRow.label
                                elide: Text.ElideRight
                                color: Theme.text
                                font.pixelSize: Theme.fontSecondary
                            }
                        }
                    }
                }
            }

            SectionTitle { text: qsTr("Problem") }
            ProblemList {
                id: problemList
                objectName: "export.image.problems"
                Layout.fillWidth: true
                implicitHeight: 96
                enabled: root.ex.mode !== "shape"
                opacity: enabled ? 1 : 0.5
                currentProblem: root.ex.problem
                onCurrentProblemChanged: root.ex.problem = currentProblem
            }
        }

        // --- how, and the page ---
        ColumnLayout {
            Layout.preferredWidth: 300
            Layout.maximumWidth: 300
            Layout.alignment: Qt.AlignTop
            spacing: 6

            SectionTitle { text: qsTr("Image"); Layout.topMargin: 0 }
            GridLayout {
                columns: 2
                columnSpacing: 10
                rowSpacing: 6
                Label { text: qsTr("Background") }
                Segmented {
                    objectName: "export.image.background"
                    options: [{ value: false, label: qsTr("White") }, { value: true, label: qsTr("Transparent") }]
                    value: root.ex.transparent
                    onActivated: (v) => root.ex.transparent = v
                }
                Label { text: qsTr("Supersampling") }
                Segmented {
                    objectName: "export.image.supersampling"
                    options: [
                        { value: 1, label: qsTr("Off") }, { value: 2, label: "2×2" }, { value: 3, label: "3×3" },
                        { value: 4, label: "4×4" }, { value: 5, label: "5×5" }
                    ]
                    value: root.ex.supersampling
                    onActivated: (v) => root.ex.supersampling = v
                }
                Label { text: qsTr("Colours") }
                Segmented {
                    objectName: "export.image.colours"
                    options: [{ value: false, label: qsTr("Pieces") }, { value: true, label: qsTr("Constraints") }]
                    value: root.ex.constraintColours
                    onActivated: (v) => root.ex.constraintColours = v
                }
            }
            BtCheckBox {
                objectName: "export.image.dim"
                text: qsTr("Dim static pieces")
                selected: root.ex.dimStatic
                onClicked: root.ex.dimStatic = selected
            }

            SectionTitle { text: qsTr("Page") }
            GridLayout {
                columns: 2
                columnSpacing: 16
                rowSpacing: 2
                Repeater {
                    model: [
                        { id: "a4p", label: qsTr("A4 portrait") }, { id: "a4l", label: qsTr("A4 landscape") },
                        { id: "letterp", label: qsTr("Letter portrait") }, { id: "letterl", label: qsTr("Letter landscape") },
                        { id: "manual", label: qsTr("Manual") }
                    ]
                    delegate: BtCheckBox {
                        required property var modelData
                        objectName: "export.image.paper." + modelData.id
                        radio: true
                        text: modelData.label
                        selected: root.ex.paper === modelData.id
                        onClicked: root.ex.paper = modelData.id
                    }
                }
            }
            GridLayout {
                columns: 4
                columnSpacing: 8
                rowSpacing: 6
                Label { text: qsTr("Size mm") }
                NumberField {
                    objectName: "export.image.mmX"
                    implicitWidth: 80
                    enabled: root.ex.paper === "manual"
                    value: root.ex.sizeXmm
                    onTextEdited: root.ex.sizeXmm = value
                }
                Label { text: "×" }
                NumberField {
                    objectName: "export.image.mmY"
                    implicitWidth: 80
                    enabled: root.ex.paper === "manual"
                    value: root.ex.sizeYmm
                    onTextEdited: root.ex.sizeYmm = value
                }
                Label { text: qsTr("DPI") }
                NumberField {
                    objectName: "export.image.dpi"
                    implicitWidth: 80
                    value: root.ex.dpi
                    onTextEdited: root.ex.dpi = value
                }
                Item { Layout.columnSpan: 2; implicitHeight: 1 }
                Label { text: qsTr("Pixels") }
                NumberField {
                    objectName: "export.image.pxX"
                    implicitWidth: 80
                    value: root.ex.pixelX
                    onEditingFinished: root.ex.pixelX = value
                }
                Label { text: "×" }
                NumberField {
                    objectName: "export.image.pxY"
                    implicitWidth: 80
                    value: root.ex.pixelY
                    onEditingFinished: root.ex.pixelY = value
                }
                Label { text: qsTr("Files") }
                NumberField {
                    objectName: "export.image.pages"
                    implicitWidth: 80
                    value: root.ex.pages
                    onEditingFinished: root.ex.pages = value
                }
            }
        }

        // --- the preview; its orientation is the pictures' ---
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 260
            radius: Theme.cardRadius
            color: Theme.canvas
            border.color: Theme.line
            clip: true
            VoxelViewport {
                objectName: "export.image.preview"
                anchors { fill: parent; margins: 1 }
                controller: root.ex
            }
            IconButton {
                anchors { top: parent.top; left: parent.left; margins: 8 }
                iconName: "fit"
                tip: qsTr("Reset the view")
                onClicked: root.ex.home()
            }
            Text {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 10 }
                text: qsTr("The pictures take this view's orientation.")
                color: Theme.muted
                font.pixelSize: Theme.fontMicro
                wrapMode: Text.WordWrap
            }
        }
    }

    footer: Item {
        implicitHeight: 60
        Text {
            objectName: "export.image.progress"
            anchors { left: parent.left; leftMargin: 20; verticalCenter: parent.verticalCenter }
            text: root.ex.progressText
            color: Theme.muted
            font.pixelSize: Theme.fontSecondary
        }
        Row {
            anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
            spacing: 8
            BtButton {
                objectName: "export.image.close"
                text: root.ex.busy ? qsTr("Cancel") : qsTr("Close")
                onClicked: root.ex.busy ? root.ex.cancel() : root.close()
            }
            BtButton {
                objectName: "export.image.export"
                text: qsTr("Export images…")
                primary: true
                enabled: !root.ex.busy
                onClicked: {
                    saveDialog.currentFolder = root.ex.folder
                    saveDialog.selectedFile = root.ex.folder + "/" + root.ex.suggestedName
                    saveDialog.open()
                }
            }
        }
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Name for the image files")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("PNG images (*.png)")]
        defaultSuffix: "png"
        onAccepted: root.ex.start(selectedFile)
    }

    Connections {
        target: root.ex
        function onFinished(pages, firstFile) {
            App.status.flash(pages === 1 ? qsTr("Exported %1").arg(firstFile)
                                         : qsTr("Exported %1 pages from %2").arg(pages).arg(firstFile))
        }
        function onFailed(message) { failure.text = message; failure.open() }
    }

    MessageDialog {
        id: failure
        title: qsTr("Export images")
        buttons: MessageDialog.Ok
    }
}
