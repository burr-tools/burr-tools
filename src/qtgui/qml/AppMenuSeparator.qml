import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// A PR-11 menu divider: 1 dp line, 3 dp above and below.
MenuSeparator {
    topPadding: 3
    bottomPadding: 3
    contentItem: Rectangle { implicitHeight: 1; color: Theme.line }
}
