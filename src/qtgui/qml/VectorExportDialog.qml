import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import BurrTools.Ui

// Export ▸ Vector Image (legacy vectorExportWindow_c "Parameters for Vector
// Export"), restyled with its content kept (OQ-34): the six file types in
// legacy's grid, SVG chosen. It writes the 3D view as it is now. The file
// is chosen in the OS save dialog (legacy had a name field and a chooser).
BtDialog {
    id: root
    objectName: "export.vector.dialog"
    title: qsTr("Export vector image")
    width: 480

    property int format: 4       // btui::VectorFormat::SVG, legacy's default

    // in legacy's order, which is btui::VectorFormat's
    readonly property var formats: [
        { id: 0, label: qsTr("Postscript") },
        { id: 1, label: qsTr("Encapsulated Postscript") },
        { id: 2, label: qsTr("TeX") },
        { id: 3, label: qsTr("PDF") },
        { id: 4, label: qsTr("SVG") },
        { id: 5, label: qsTr("PGF") }
    ]

    function suggestedFile() {
        const doc = App.document
        const folder = doc.filePath.length > 0 ? doc.folder : App.stl.folder
        const base = doc.fileName.length > 0 ? doc.fileName.replace(/\.xmpuzzle$/, "") : "out"
        return folder + "/" + base + "." + App.viewport.vectorExtension(format)
    }

    contentItem: ColumnLayout {
        spacing: 10
        Text {
            Layout.fillWidth: true
            text: qsTr("Writes the 3D view as it is now. File type:")
            color: Theme.muted
            font.pixelSize: Theme.fontSecondary
            wrapMode: Text.WordWrap
        }
        GridLayout {
            columns: 2
            columnSpacing: 24
            rowSpacing: 4
            Repeater {
                model: root.formats
                delegate: BtCheckBox {
                    required property var modelData
                    objectName: "export.vector.format." + modelData.id
                    radio: true
                    text: modelData.label
                    selected: root.format === modelData.id
                    onClicked: root.format = modelData.id
                }
            }
        }
        Text {
            Layout.fillWidth: true
            visible: root.format === 2
            text: qsTr("TeX output holds the picture frame only; it includes the graphic of the same name, exported separately as EPS or PDF.")
            color: Theme.muted
            font.pixelSize: Theme.fontSecondary
            wrapMode: Text.WordWrap
        }
    }

    footer: Item {
        implicitHeight: 60
        Row {
            anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
            spacing: 8
            BtButton { objectName: "export.vector.cancel"; text: qsTr("Cancel"); onClicked: root.reject() }
            BtButton {
                objectName: "export.vector.export"
                text: qsTr("Export…")
                primary: true
                onClicked: {
                    const ext = App.viewport.vectorExtension(root.format)
                    saveDialog.nameFilters = [root.formats[root.format].label + " (*." + ext + ")"]
                    saveDialog.defaultSuffix = ext
                    saveDialog.selectedFile = root.suggestedFile()
                    saveDialog.open()
                }
            }
        }
    }

    FileDialog {
        id: saveDialog
        title: qsTr("File to save image to")
        fileMode: FileDialog.SaveFile
        onAccepted: {
            const url = selectedFile
            if (App.viewport.exportVector(url, root.format)) {
                const parts = url.toString().split("/")
                App.status.flash(qsTr("Exported %1").arg(decodeURIComponent(parts[parts.length - 1])))
                root.close()
            } else {
                failure.open()
            }
        }
    }

    MessageDialog {
        id: failure
        title: qsTr("Export vector image")
        text: qsTr("The file could not be written.")
        buttons: MessageDialog.Ok
    }
}
