import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-12 Dialog: modal, centred over a scrim, radius 14; Esc and a scrim click
// close it; focus stays inside while it is open, starting on its first field
// or default button, and Enter presses the default button.
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

    /* Keyboard: the control focused when the dialog opens (a dialog's first
     * field; else its default button), and the button Enter presses -- not
     * while a multi-line text field has the focus, where Enter is a new line.
     * The Shortcut sits in the dialog, so it lives only while this is the
     * dialog that is open. */
    property Item initialFocus: null
    property AbstractButton defaultButton: null
    onOpened: {
        const f = initialFocus ? initialFocus : defaultButton
        if (f)
            f.forceActiveFocus(Qt.TabFocusReason)
    }
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: root.opened && root.defaultButton !== null && root.defaultButton.enabled
                 && root.defaultButton.visible && !(root.activeFocusItemIsMultiLine())
        onActivated: root.defaultButton.clicked()
    }
    function activeFocusItemIsMultiLine(): bool {
        const w = root.contentItem ? root.contentItem.Window.window : null
        return w !== null && (w.activeFocusItem as TextEdit) !== null
    }

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
