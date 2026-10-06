import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// PR-19 Dropdown: h30, min-w 120, radius 8, 1 dp line2, panel fill, value
// text 13, chevron at the right; opens a PR-11 popup listing the options
// with the current one checked. Up/Down change the value when focused,
// Space/Enter open it.
ComboBox {
    id: root
    implicitWidth: 120
    implicitHeight: 30
    leftPadding: 10
    rightPadding: 30
    font.pixelSize: Theme.fontBody
    // ComboBox follows the platform's hover hint; the hover fill is ours
    hoverEnabled: true

    contentItem: Text {
        text: root.displayText
        color: Theme.text
        font: root.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Icon {
        x: root.width - width - 10
        anchors.verticalCenter: parent.verticalCenter
        name: "chevron-down"
        size: 14
        color: Theme.muted
    }

    background: Rectangle {
        radius: Theme.radiusControl
        color: root.hovered ? Theme.panel2 : Theme.panel
        border.width: root.visualFocus ? 2 : 1
        border.color: root.visualFocus ? Theme.accent : Theme.line2
    }

    delegate: CheckRow {
        required property int index
        required property var modelData
        width: ListView.view ? ListView.view.width : implicitWidth
        text: root.textRole ? modelData[root.textRole] : modelData
        selected: root.currentIndex === index
        onClicked: {
            root.currentIndex = index
            root.activated(index)
            root.popup.close()
        }
    }

    popup: Popup {
        y: root.height + 4
        width: Math.max(root.width, 140)
        padding: 6
        implicitHeight: contentItem.implicitHeight + 12
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
        }
        background: Rectangle {
            color: Theme.panel
            border.color: Theme.line2
            radius: Theme.radiusPopup
        }
    }
    opacity: enabled ? 1 : 0.4
}
