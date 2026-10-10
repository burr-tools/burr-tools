import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// A menu. On macOS the system's own (the menu bar there is the system's).
// Elsewhere drawn in the theme -- PR-11: panel fill, 1 dp line2 border,
// radius 10, padding 6 -- in a window of its own, so it may reach past the
// main window like a system menu. On Windows 11 that window gets the
// system's rounded corners, shadow and border (PopupWindowStyle), so the
// menu leaves its corners and border to it.
Menu {
    id: root
    popupType: Qt.platform.os === "osx" ? Popup.Native : Popup.Window
    padding: 6
    overlap: 4

    // as wide as the widest item (Qt's Menu sizes to its background)
    implicitWidth: {
        let widest = 220
        for (let i = 0; i < count; i++) {
            const item = itemAt(i)
            if (item)
                widest = Math.max(widest, item.implicitWidth)
        }
        return widest + leftPadding + rightPadding
    }

    delegate: AppMenuItem {}

    // an open menu takes Esc before the layout does (Main.qml's Esc ladder)
    readonly property var appWindow: parent && parent.ApplicationWindow ? parent.ApplicationWindow.window : null
    onAboutToShow: if (appWindow && appWindow.openPopups !== undefined) appWindow.openPopups++
    onAboutToHide: if (appWindow && appWindow.openPopups !== undefined) appWindow.openPopups--

    background: Rectangle {
        implicitWidth: 220
        color: Theme.panel
        border.color: Theme.line2
        border.width: App.popupStyle.roundsCorners ? 0 : 1
        radius: App.popupStyle.roundsCorners ? 0 : Theme.radiusPopup
    }
}
