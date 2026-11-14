pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BurrTools.Ui

// PR-31 Docked drawer (C21): the step-by-step explorer over the bottom of
// the Solver's 3D view, in place of legacy's two browser windows. The 3D
// view stays usable above it, and the cards keep their layout.
//
// Placements lists each piece with the places the assembler's search has
// for it (legacy placementBrowser_c), marked when the assembler's state
// places it; Step advances the assembler (C19's Step), Run continues the
// search. Movements lists the selected solution's disassembly, piece by
// piece (its level: the moves to take each one out), the reached ones
// filled; its controls drive the player. Legacy's movement tree (analyse a
// node, prune, add a movement) waits: OQ-28.
//
// Wireframe (each number marks its code below):
//
//   ┌──────────────────────────────────────────────────────────────┐
//   │ ①[Placements|Movements]  Assembly search tree            [×] │
//   ├──────────────────┬───────────────────────────────────────────┤
//   │ ②S1        24 ●  │ ③[◀ Back] [Step ▶] [Run ▶▶] [Reset]       │
//   │  S2         8    │ ④Assembler state — 2 of 6 pieces placed    │
//   │  S3.1      16 ●  │ ⑤(1)(2)(3)(4)(5)(6)   reached ones filled  │
//   └──────────────────┴───────────────────────────────────────────┘
Rectangle {
    id: root
    color: Theme.panel
    radius: Theme.radiusDialog
    border.color: Theme.line
    border.width: 1

    readonly property bool placements: App.solver.explorer === "placements"
    // the selected solution's level, "8.2.1": the moves to remove each piece
    readonly property list<int> levels: {
        const t = App.solver.selectedLevel
        const out = []
        if (t.length > 0 && t !== "—")
            for (const part of t.split("."))
                out.push(parseInt(part))
        return out
    }
    // how many pieces are out at the player's position
    readonly property int reached: {
        let sum = 0
        let n = 0
        for (let i = 0; i < levels.length; i++) {
            sum += levels[i]
            if (App.solver.position + 1e-6 >= sum)
                n++
        }
        return n
    }
    readonly property int placedCount: {
        let n = 0
        for (let i = 0; i < App.solver.placements.count; i++)
            if (App.solver.placements.revision >= 0 && Boolean(App.solver.placements.value(i, "placed")))
                n++
        return n
    }

    Accessible.role: Accessible.Pane
    Accessible.name: placements ? qsTr("Placements explorer") : qsTr("Movements explorer")

    ColumnLayout {
        anchors { fill: parent; margins: 10 }
        spacing: 8

        RowLayout { // ①
            Layout.fillWidth: true
            spacing: 10
            Segmented {
                objectName: "solver.drawer.kind"
                options: [ { value: "placements", label: qsTr("Placements") }, { value: "movements", label: qsTr("Movements") } ]
                value: App.solver.explorer
                Accessible.name: qsTr("Explore")
                onActivated: (v) => { if (v !== App.solver.explorer) App.solver.toggleExplorer(v) }
            }
            Text {
                Layout.fillWidth: true
                text: root.placements ? qsTr("Assembly search tree") : qsTr("Disassembly search (breadth-first)")
                color: Theme.muted
                font.pixelSize: Theme.fontSecondary
                elide: Text.ElideRight
            }
            IconButton {
                objectName: "solver.drawer.close"
                iconName: "close"
                tip: qsTr("Close the explorer")
                onClicked: App.solver.closeExplorer()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            ListView { // ②
                id: list
                objectName: "solver.drawer.list"
                Layout.preferredWidth: 230
                Layout.fillHeight: true
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                cacheBuffer: 0
                // read-only: no Tab stop (its rows are read out as they change)
                activeFocusOnTab: false
                Accessible.role: Accessible.List
                Accessible.name: root.placements ? qsTr("Pieces and their placements") : qsTr("Disassembly, piece by piece")
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                model: root.placements ? App.solver.placements : root.levels.length
                delegate: Rectangle {
                    id: item
                    required property int index
                    readonly property bool mark: root.placements
                                                 ? (App.solver.placements.revision >= 0 && Boolean(App.solver.placements.value(index, "placed")))
                                                 : index < root.reached
                    readonly property string name: root.placements
                                                   ? (App.solver.placements.revision >= 0 ? String(App.solver.placements.value(index, "pid")) : "")
                                                   : qsTr("Piece %1 out").arg(index + 1)
                    readonly property int count: root.placements
                                                 ? (App.solver.placements.revision >= 0 ? Number(App.solver.placements.value(index, "count")) : 0)
                                                 : root.levels[index]
                    width: ListView.view.width
                    height: Theme.listRow - 4
                    radius: Theme.radiusChip
                    color: mark ? Theme.accentSoft : "transparent"
                    border.width: mark ? 1 : 0
                    border.color: Theme.accent
                    Accessible.role: Accessible.ListItem
                    // the mark in words too: its fill alone is no state to a screen reader
                    Accessible.name: root.placements
                                     ? (count >= 0 ? qsTr("%1: %2 placements").arg(name).arg(count) : name)
                                       + (mark ? qsTr(", placed") : "")
                                     : qsTr("%1: %2 moves").arg(name).arg(count) + (mark ? qsTr(", out") : "")
                    Text {
                        anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                        text: item.name
                        color: Theme.text
                        font.pixelSize: Theme.fontSecondary
                        font.weight: Font.DemiBold
                        textFormat: Text.PlainText
                    }
                    Text {
                        anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                        text: item.count >= 0 ? (root.placements ? item.count : (item.count === 1 ? qsTr("1 move") : qsTr("%1 moves").arg(item.count))) : "—"
                        color: Theme.muted
                        font.pixelSize: Theme.fontSecondary
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8
                RowLayout { // ③
                    spacing: 6
                    BtButton {
                        objectName: "solver.drawer.back"
                        small: true
                        text: qsTr("Back")
                        enabled: !root.placements && App.solver.position > 0
                        onClicked: App.solver.transport("back")
                    }
                    BtButton {
                        objectName: "solver.drawer.step"
                        small: true
                        primary: true
                        text: qsTr("Step")
                        enabled: root.placements ? App.solver.canStep : App.solver.position < App.solver.totalMoves
                        onClicked: root.placements ? App.solver.step() : App.solver.transport("fwd")
                    }
                    BtButton {
                        objectName: "solver.drawer.run"
                        small: true
                        text: qsTr("Run")
                        enabled: root.placements ? App.solver.canContinue : App.solver.position < App.solver.totalMoves
                        onClicked: root.placements ? App.solver.resume() : App.solver.togglePlay()
                    }
                    BtButton {
                        objectName: "solver.drawer.reset"
                        small: true
                        text: qsTr("Reset")
                        enabled: !root.placements && App.solver.position > 0
                        onClicked: App.solver.transport("start")
                    }
                }
                Text { // ④
                    objectName: "solver.drawer.info"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.text
                    font.pixelSize: Theme.fontSecondary
                    textFormat: Text.PlainText
                    text: root.placements
                          ? (App.solver.assemblerStateShown ? App.solver.assemblerCaption
                             : App.solver.canStep ? qsTr("The search is prepared. Step places the pieces one decision at a time; Run continues the search from here.")
                             : qsTr("The search has ended: the places it had for each piece."))
                          : root.levels.length === 0 ? qsTr("The selected solution has no stored disassembly.")
                          : qsTr("%1 of %2 pieces out at move %3 of %4.").arg(root.reached).arg(root.levels.length)
                              .arg(Number(App.solver.position).toFixed(1)).arg(Math.round(App.solver.totalMoves))
                }
                Flow { // ⑤
                    objectName: "solver.drawer.depth"
                    Layout.fillWidth: true
                    spacing: 4
                    Repeater {
                        model: root.placements ? App.solver.placements.count : root.levels.length
                        delegate: Rectangle {
                            id: chip
                            required property int index
                            readonly property bool done: root.placements ? index < root.placedCount : index < root.reached
                            width: 24
                            height: 24
                            radius: 12
                            color: done ? Theme.accent : "transparent"
                            border.width: 1
                            border.color: done ? Theme.accent : Theme.line2
                            Accessible.ignored: true
                            Text {
                                anchors.centerIn: parent
                                text: chip.index + 1
                                color: chip.done ? Theme.onAccent : Theme.muted
                                font.pixelSize: Theme.fontMicro
                                font.weight: Font.Bold
                            }
                        }
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }
    }
}
