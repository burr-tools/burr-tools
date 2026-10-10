pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import BurrTools.Ui

// The component gallery (spec P0): every primitive of the redesigned GUI in
// each of its states, on the theme's card colours, plus contact sheets of
// the glyphs and UI icons. `burrtools-qt --gallery` opens it;
// tst_gallery.qml checks it against design-tokens §4-5 and against reference
// images, in both themes and at device-pixel ratios 1, 1.5 and 2.
//
// Hover and keyboard focus cannot be forced from QML: the "hover" and
// "focus" cells hold an ordinary instance that the test -- or you, with the
// pointer and Tab -- puts into that state. Every other state is a property.
//
// Each row is "gallery.<row>" and each cell "gallery.<row>.<state>"; a
// cell's `control` is the instance it shows.
ApplicationWindow {
    id: win
    objectName: "gallery.window"
    title: qsTr("Component gallery - BurrTools")
    width: 1120
    // no taller than the screen, or the title bar starts above its top edge
    // and the window cannot be moved, resized or closed; the rows scroll
    height: Math.min(1500, Screen.desktopAvailableHeight * 0.92)
    visible: true
    color: Theme.bg

    // one instance in one state, captioned with the state's name
    component Cell: Column {
        id: cell
        property string kind
        // for an instance that takes no room of its own (a tooltip popup)
        property size area: Qt.size(0, 0)
        // a popup shown in the cell, which is not one of its items
        property QtObject popup: null
        default property alias content: holder.data
        readonly property Item control: holder.children.length > 0 ? holder.children[0] : null
        padding: 6
        spacing: 4
        Text {
            text: cell.kind
            color: Theme.muted
            font.pixelSize: Theme.fontMicro
        }
        Item {
            id: holder
            width: cell.area.width > 0 ? cell.area.width : childrenRect.width
            height: cell.area.height > 0 ? cell.area.height : childrenRect.height
        }
    }

    // one primitive (or variant): its name, then its cells
    component GalleryRow: Item {
        id: row
        property string name
        property string label
        default property alias cells: cellRow.data
        objectName: "gallery." + name
        width: parent ? parent.width : 0
        height: Math.max(cellRow.implicitHeight, 36)
        Text {
            x: 14
            width: 150
            anchors.verticalCenter: parent.verticalCenter
            text: row.label
            color: Theme.text
            font.pixelSize: Theme.fontBody
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
        }
        Row {
            id: cellRow
            x: 170
            spacing: 8
        }
        Component.onCompleted: {
            for (const c of cellRow.children)
                if (c.kind !== undefined)
                    c.objectName = "gallery." + name + "." + c.kind
        }
    }

    // a titled card of rows
    component Section: Column {
        id: section
        property string title
        default property alias rows: list.data
        width: parent ? parent.width : 0
        spacing: 6
        Text {
            text: section.title.toUpperCase()
            color: Theme.muted
            font.pixelSize: Theme.fontMicro
            font.weight: Font.DemiBold
            font.letterSpacing: 0.5
        }
        Rectangle {
            width: parent.width
            height: list.implicitHeight + 12
            radius: Theme.cardRadius
            color: Theme.panel
            border.color: Theme.line
            Column {
                id: list
                y: 6
                width: parent.width
            }
        }
    }

    // a contact sheet of SVG assets
    component Sheet: Flow {
        id: sheet
        property var names: []
        property bool glyphs: false
        width: parent ? parent.width - 28 : 0
        x: 14
        spacing: 6
        padding: 6
        Repeater {
            model: sheet.names
            delegate: Column {
                id: cell
                required property string modelData
                width: 76
                spacing: 2
                Glyph {
                    visible: sheet.glyphs
                    anchors.horizontalCenter: parent.horizontalCenter
                    name: sheet.glyphs ? cell.modelData : ""
                    size: 32
                }
                Icon {
                    visible: !sheet.glyphs
                    anchors.horizontalCenter: parent.horizontalCenter
                    name: sheet.glyphs ? "" : cell.modelData
                    size: 20
                    color: Theme.text
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: cell.modelData
                    color: Theme.muted
                    font.pixelSize: 10
                    elide: Text.ElideMiddle
                }
            }
        }
    }

    header: Rectangle {
        height: 48
        color: Theme.panel
        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 1; color: Theme.line }
        Row {
            anchors { left: parent.left; leftMargin: 14; verticalCenter: parent.verticalCenter }
            spacing: 12
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Component gallery")
                color: Theme.text
                font.pixelSize: Theme.fontDialogTitle
                font.weight: Font.Bold
            }
            Segmented {
                objectName: "gallery.theme"
                anchors.verticalCenter: parent.verticalCenter
                options: [ { value: "light", label: qsTr("Light") }, { value: "dark", label: qsTr("Dark") } ]
                value: Theme.dark ? "dark" : "light"
                onActivated: (v) => Theme.mode = v
            }
            Segmented {
                objectName: "gallery.density"
                anchors.verticalCenter: parent.verticalCenter
                options: [ { value: "standard", label: qsTr("Standard") }, { value: "minimal", label: qsTr("Minimal") } ]
                value: Theme.density
                onActivated: (v) => Theme.density = v
            }
        }
    }

    Flickable {
        id: flick
        objectName: "gallery.body"
        anchors.fill: parent
        contentHeight: body.implicitHeight + 32
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        Column {
            id: body
            x: 16
            y: 16
            width: flick.width - 32
            spacing: 16

            Section {
                title: qsTr("Buttons")
                GalleryRow {
                    name: "iconButton"; label: "PR-01 Icon button"
                    Cell { kind: "normal"; IconButton { iconName: "trash"; tip: "Delete" } }
                    Cell { kind: "hover"; IconButton { iconName: "trash"; tip: "Delete" } }
                    Cell { kind: "on"; IconButton { iconName: "eye"; tip: "Show"; on: true } }
                    Cell { kind: "focus"; IconButton { iconName: "trash"; tip: "Delete" } }
                    Cell { kind: "disabled"; IconButton { iconName: "trash"; tip: "Delete"; enabled: false } }
                    Cell { kind: "dangerHover"; IconButton { iconName: "trash"; tip: "Delete"; dangerHover: true } }
                }
                GalleryRow {
                    name: "button"; label: "PR-04 Button"
                    Cell { kind: "normal"; BtButton { text: "Reset section" } }
                    Cell { kind: "hover"; BtButton { text: "Reset section" } }
                    Cell { kind: "focus"; BtButton { text: "Reset section" } }
                    Cell { kind: "disabled"; BtButton { text: "Reset section"; enabled: false } }
                    Cell { kind: "small"; BtButton { text: "New shape"; small: true } }
                }
                GalleryRow {
                    name: "buttonPrimary"; label: "PR-04 Button, primary"
                    Cell { kind: "normal"; BtButton { text: "Done"; primary: true } }
                    Cell { kind: "hover"; BtButton { text: "Done"; primary: true } }
                    Cell { kind: "focus"; BtButton { text: "Done"; primary: true } }
                    Cell { kind: "disabled"; BtButton { text: "Done"; primary: true; enabled: false } }
                    Cell { kind: "danger"; BtButton { text: "Delete"; danger: true } }
                }
                GalleryRow {
                    name: "toolbarButton"; label: "3D toolbar button"
                    Cell { kind: "normal"; ToolbarButton { iconName: "orbit"; caption: "Orbit" } }
                    Cell { kind: "hover"; ToolbarButton { iconName: "orbit"; caption: "Orbit" } }
                    Cell { kind: "on"; ToolbarButton { iconName: "pan"; caption: "Pan"; on: true } }
                    Cell { kind: "focus"; ToolbarButton { iconName: "orbit"; caption: "Orbit" } }
                    Cell { kind: "disabled"; ToolbarButton { glyphName: "edit-tool-fixed"; caption: "Edit"; badge: "1"; enabled: false } }
                    Cell { kind: "compact"; ToolbarButton { iconName: "eye"; caption: "Display"; compact: true } }
                }
                GalleryRow {
                    name: "railButton"; label: "PR-21 Rail button"
                    Cell { kind: "normal"; RailButton { text: "Puzzle"; iconName: "rail-puzzle" } }
                    Cell { kind: "hover"; RailButton { text: "Puzzle"; iconName: "rail-puzzle" } }
                    Cell { kind: "selected"; RailButton { text: "Entities"; iconName: "rail-entities"; selected: true } }
                    Cell { kind: "focus"; RailButton { text: "Solver"; iconName: "rail-solver" } }
                }
            }

            Section {
                title: qsTr("Choices")
                GalleryRow {
                    name: "segmented"; label: "PR-05 Segmented"
                    Cell { kind: "normal"; Segmented { options: [ { value: "a", label: "Trackball" }, { value: "b", label: "Turntable" } ]; value: "a" } }
                    Cell { kind: "focus"; Segmented { options: [ { value: "a", label: "Trackball" }, { value: "b", label: "Turntable" } ]; value: "b" } }
                    Cell { kind: "disabled"; Segmented { options: [ { value: "a", label: "Trackball" }, { value: "b", label: "Turntable" } ]; value: "a"; enabled: false } }
                }
                GalleryRow {
                    name: "switch"; label: "PR-07 Switch"
                    Cell { kind: "off"; BtSwitch {} }
                    Cell { kind: "on"; BtSwitch { checked: true } }
                    Cell { kind: "focus"; BtSwitch { checked: true } }
                    Cell { kind: "disabled"; BtSwitch { enabled: false } }
                }
                GalleryRow {
                    name: "checkBox"; label: "Check box"
                    Cell { kind: "off"; BtCheckBox { text: "Binary STL" } }
                    Cell { kind: "on"; BtCheckBox { text: "Binary STL"; selected: true } }
                    Cell { kind: "hover"; BtCheckBox { text: "Binary STL" } }
                    Cell { kind: "focus"; BtCheckBox { text: "Binary STL" } }
                    Cell { kind: "disabled"; BtCheckBox { text: "Binary STL"; selected: true; enabled: false } }
                }
                GalleryRow {
                    name: "radio"; label: "Radio button"
                    Cell { kind: "off"; BtCheckBox { radio: true; text: "Brick" } }
                    Cell { kind: "on"; BtCheckBox { radio: true; text: "Brick"; selected: true } }
                    Cell { kind: "focus"; BtCheckBox { radio: true; text: "Brick" } }
                }
                GalleryRow {
                    name: "radioCard"; label: "PR-27 Radio card"
                    Cell { kind: "normal"; RadioCard { width: 200; text: "Cubes"; description: "Square grid" } }
                    Cell { kind: "hover"; RadioCard { width: 200; text: "Cubes"; description: "Square grid" } }
                    Cell { kind: "selected"; RadioCard { width: 200; text: "Cubes"; description: "Square grid"; selected: true } }
                    Cell { kind: "focus"; RadioCard { width: 200; text: "Cubes"; description: "Square grid" } }
                }
                GalleryRow {
                    name: "checkRow"; label: "PR-16 Check row"
                    Cell { kind: "normal"; CheckRow { width: 170; text: "Axes" } }
                    Cell { kind: "hover"; CheckRow { width: 170; text: "Axes" } }
                    Cell { kind: "selected"; CheckRow { width: 170; text: "Axes"; selected: true } }
                    Cell { kind: "disabled"; CheckRow { width: 210; text: "Slab"; hint: "editor hidden"; enabled: false } }
                }
                GalleryRow {
                    name: "dropdown"; label: "PR-19 Dropdown"
                    Cell { kind: "normal"; Dropdown { model: [ "25", "50", "100" ] } }
                    Cell { kind: "hover"; Dropdown { model: [ "25", "50", "100" ] } }
                    Cell { kind: "focus"; Dropdown { model: [ "25", "50", "100" ] } }
                    Cell { kind: "disabled"; Dropdown { model: [ "25", "50", "100" ]; enabled: false } }
                }
            }

            Section {
                title: qsTr("Input")
                GalleryRow {
                    name: "searchField"; label: "Search field"
                    Cell { kind: "empty"; SearchField { width: 200; placeholderText: "Search settings" } }
                    Cell { kind: "text"; SearchField { width: 200; text: "zoom" } }
                    Cell { kind: "focus"; SearchField { width: 200; placeholderText: "Search settings" } }
                }
                GalleryRow {
                    name: "numberField"; label: "Number field"
                    Cell { kind: "normal"; NumberField { value: 42 } }
                    Cell { kind: "focus"; NumberField { value: 42 } }
                    Cell { kind: "disabled"; NumberField { value: 42; enabled: false } }
                }
                GalleryRow {
                    name: "slider"; label: "Slider"
                    Cell { kind: "normal"; BtSlider { from: 1; to: 8; value: 3 } }
                    Cell { kind: "focus"; BtSlider { from: 1; to: 8; value: 3 } }
                    Cell { kind: "disabled"; BtSlider { from: 1; to: 8; value: 3; enabled: false } }
                }
            }

            Section {
                title: qsTr("Menus, tooltips and keys")
                GalleryRow {
                    name: "menuItem"; label: "PR-11 Menu rows"
                    Cell { kind: "normal"; AppMenuItem { text: "&Open…"; shortcutText: "Ctrl+O / F3" } }
                    Cell { kind: "highlighted"; AppMenuItem { text: "&Open…"; shortcutText: "Ctrl+O / F3"; highlighted: true } }
                    Cell { kind: "checked"; AppMenuItem { text: "Show &menu bar"; checkable: true; checked: true } }
                    Cell { kind: "disabled"; AppMenuItem { text: "&Undo"; shortcutText: "Ctrl+Z"; enabled: false } }
                }
                GalleryRow {
                    name: "menuPanel"; label: "PR-11 Popup panel"
                    Cell {
                        kind: "open"
                        Rectangle {
                            width: panelItems.width + 12
                            height: panelItems.height + 12
                            color: Theme.panel
                            border.color: Theme.line2
                            radius: Theme.radiusPopup
                            Column {
                                id: panelItems
                                x: 6
                                y: 6
                                AppMenuItem { text: "&New"; shortcutText: "Ctrl+N" }
                                AppMenuItem { text: "&Open…"; shortcutText: "Ctrl+O / F3"; highlighted: true }
                                Rectangle { width: parent.width; height: 9; color: "transparent"; Rectangle { y: 4; width: parent.width; height: 1; color: Theme.line } }
                                AppMenuItem { text: "&Save"; shortcutText: "Ctrl+S / F2"; enabled: false }
                            }
                        }
                    }
                }
                GalleryRow {
                    id: tipRow
                    name: "tooltip"; label: "PR-14 Tooltip"
                    // Tooltips are popups: they would not scroll out with the
                    // body but be pushed back into the window, over other
                    // rows. Shown only while the whole row is in view.
                    // takes the scroll position so the binding follows it
                    function fits(contentY: real, viewHeight: real): bool {
                        const top = tipRow.mapToItem(flick, 0, 0).y
                        return top >= 0 && top + tipRow.height <= viewHeight
                    }
                    readonly property bool inView: fits(flick.contentY, flick.height)
                    Cell {
                        kind: "short"
                        area: Qt.size(200, 34)
                        popup: shortTip
                        BtToolTip {
                            id: shortTip
                            shown: tipRow.inView; delay: 0; closePolicy: Popup.NoAutoClose; x: 0; y: 4
                            enter: null; exit: null
                            text: "Undo (Ctrl+Z)"
                        }
                    }
                    Cell {
                        kind: "long"
                        area: Qt.size(300, 64)
                        popup: longTip
                        BtToolTip {
                            id: longTip
                            shown: tipRow.inView; delay: 0; closePolicy: Popup.NoAutoClose; x: 0; y: 4
                            enter: null; exit: null
                            text: "Voxel type: Cubes — fixed when the file is created (File ▸ New); it cannot be changed afterwards"
                        }
                    }
                }
                GalleryRow {
                    name: "keyBadge"; label: "PR-15 Key badge"
                    // ⌘ only where shortcuts show it: elsewhere the monospace font
                    // (Consolas) has no such glyph, and whichever font stands in
                    // differs between machines -- the reference images with it
                    Cell { kind: "keys"; Row { spacing: 6; KeyBadge { text: "Ctrl" } KeyBadge { text: "Shift" } KeyBadge { text: "F2" } KeyBadge { text: Qt.platform.os === "osx" ? "⌘" : "Esc" } } }
                    Cell {
                        kind: "shortcut"
                        ShortcutKeys { keys: [ [ { text: "Ctrl", mouse: false }, { text: "Z", mouse: false } ], [ { text: "Middle-drag", mouse: true } ] ] }
                    }
                }
            }

            Section {
                title: qsTr("Surfaces")
                GalleryRow {
                    name: "card"; label: "PR-10 Card"
                    Cell {
                        kind: "normal"
                        Rectangle {
                            // the card sits on the workspace background, as in the shell
                            width: 280; height: 110; color: Theme.bg
                            Card {
                                anchors { fill: parent; margins: 8 }
                                title: "Shapes"
                                collapseIcon: "panelL"
                                Text { x: 14; y: 10; text: "Card body"; color: Theme.muted; font.pixelSize: Theme.fontSecondary }
                            }
                        }
                    }
                }
            }

            Section {
                title: qsTr("Glyphs (PR-17), %1 theme").arg(Theme.dark ? "dark" : "light")
                Sheet { objectName: "gallery.glyphs"; glyphs: true; names: App.assetNames("glyphs/" + Theme.glyphFolder) }
            }

            Section {
                title: qsTr("UI icons")
                Sheet { objectName: "gallery.icons"; names: App.assetNames("icons") }
            }
        }
    }
}
