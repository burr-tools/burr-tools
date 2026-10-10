import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-12 Dialog: modal, centred over a scrim, radius 14; Esc and a scrim click
// close it; focus stays inside while it is open.
Dialog {
    id: root
    modal: true
    // keyboard focus moves into the dialog while it is open
    focus: true
    // over the whole window, wherever the dialog was made (Main.qml makes
    // them in Loaders when first opened): its size follows the window's
    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 20
    topPadding: 16

    Overlay.modal: Rectangle { color: Theme.scrim }

    background: Rectangle {
        color: Theme.panel
        radius: Theme.radiusDialog
        border.color: Theme.line
    }

    header: Item {
        implicitHeight: root.title.length > 0 ? 52 : 0
        Text {
            visible: root.title.length > 0
            anchors { left: parent.left; leftMargin: 20; verticalCenter: parent.verticalCenter; verticalCenterOffset: 4 }
            text: root.title
            color: Theme.text
            font.pixelSize: Theme.fontDialogTitle
            font.weight: Font.Bold
        }
    }
}
