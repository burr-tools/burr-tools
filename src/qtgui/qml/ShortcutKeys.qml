import QtQuick
import BurrTools.Ui

// The keys of one shortcut row (C22): alternatives separated by "/", key
// caps of a combination joined with "+", mouse gestures as dashed tokens.
Row {
    id: root
    property var keys: []      // [[{ text, mouse }]] from CommandController::shortcutHelp
    spacing: 6

    Repeater {
        model: root.keys
        delegate: Row {
            id: alternative
            required property var modelData
            required property int index
            spacing: 4
            Text {
                visible: alternative.index > 0
                anchors.verticalCenter: parent.verticalCenter
                text: "/"
                color: Theme.muted
                font.pixelSize: Theme.fontSecondary
            }
            Repeater {
                model: alternative.modelData
                delegate: Row {
                    id: partRow
                    required property var modelData
                    required property int index
                    spacing: 4
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        visible: partRow.index > 0
                        anchors.verticalCenter: parent.verticalCenter
                        text: "+"
                        color: Theme.muted
                        font.pixelSize: Theme.fontMicro
                    }
                    KeyBadge {
                        visible: !partRow.modelData.mouse
                        anchors.verticalCenter: parent.verticalCenter
                        text: partRow.modelData.text
                    }
                    // a mouse input: dashed outline
                    Rectangle {
                        visible: partRow.modelData.mouse
                        anchors.verticalCenter: parent.verticalCenter
                        implicitWidth: mouseLabel.implicitWidth + 12
                        implicitHeight: mouseLabel.implicitHeight + 4
                        width: visible ? implicitWidth : 0
                        radius: 4
                        color: "transparent"
                        Canvas {
                            id: dash
                            anchors.fill: parent
                            Connections { target: Theme; function onChanged() { dash.requestPaint() } }
                            onPaint: {
                                const c = getContext("2d")
                                c.reset()
                                c.setLineDash([3, 2])
                                c.strokeStyle = Theme.line2
                                c.lineWidth = 1
                                c.beginPath()
                                c.roundedRect(0.5, 0.5, width - 1, height - 1, 4, 4)
                                c.stroke()
                            }
                        }
                        Text {
                            id: mouseLabel
                            anchors.centerIn: parent
                            text: partRow.modelData.text
                            color: Theme.text
                            font.pixelSize: Theme.fontMicro
                        }
                    }
                }
            }
        }
    }
}
