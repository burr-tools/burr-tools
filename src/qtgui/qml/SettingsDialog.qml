import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BurrTools.Ui

// C12 Settings (File ▸ Settings…, Ctrl+,; BurrTools ▸ Preferences…, ⌘, on
// macOS): every legacy setting once, grouped into General, 3D view and
// Performance, searchable across pages, each change applied and saved at
// once. Search also lists matching keyboard shortcuts (C22), whose own
// window is Help ▸ Keyboard shortcuts.
BtDialog {
    id: root
    objectName: "settings.dialog"
    width: Math.min(780, (parent ? parent.width : 800) - 32)
    height: Math.min(580, (parent ? parent.height : 700) - 32)
    padding: 0
    topPadding: 0

    // the page shown, kept for the session (C12: remember the last page)
    property string page: "general"
    property string query: ""
    readonly property bool searching: query.trim().length > 0

    onAboutToShow: search.text = ""
    onOpened: search.forceActiveFocus()

    readonly property var pages: [
        { id: "general", name: qsTr("General"), rows: [
            { key: "density", prop: "density", type: "seg", title: qsTr("Interface density"),
              desc: qsTr("Standard is the default. Minimal shows icons without captions, moves helper text into tooltips, hides the viewport toolbar until you point at the top of the 3D view and narrows the side cards. Every function stays available in both."),
              opts: [{ value: "standard", label: qsTr("Standard") }, { value: "minimal", label: qsTr("Minimal") }] },
            { key: "theme", prop: "theme", type: "seg", title: qsTr("Theme"),
              desc: qsTr("Light (the default), Dark, or System. System follows the operating system’s light/dark choice when the OS provides one, otherwise Light."),
              opts: [{ value: "light", label: qsTr("Light") }, { value: "dark", label: qsTr("Dark") }, { value: "system", label: qsTr("System") }] },
            { key: "undoDepth", prop: "undoDepth", type: "dropdown", title: qsTr("Undo history depth"),
              desc: qsTr("Number of undo steps kept in memory. Higher values use more RAM for large puzzles."),
              opts: [25, 50, 100, 200, 500] },
            { key: "tooltips", prop: "tooltips", type: "switch", title: qsTr("Show tooltips"),
              desc: qsTr("Show short help text when the mouse rests on buttons and other controls.") }
        ] },
        { id: "view3d", name: qsTr("3D view"), rows: [
            { key: "viewCube", prop: "showViewCube", type: "switch", title: qsTr("Show view cube"),
              desc: qsTr("Display the 3D orientation cube in the corner of the 3D preview. Click it to snap to a face, edge, or corner view.") },
            { key: "reverseScroll", prop: "reverseScroll", type: "switch", title: qsTr("Reverse scroll zoom direction"),
              desc: qsTr("Reverse the direction of the preview zoom when the mouse wheel is used.") },
            { key: "rotationMethod", prop: "rotationMethod", type: "seg", title: qsTr("Rotation method"),
              desc: qsTr("Drag is the newer click-and-drag rotation. Arc-ball is the original method."),
              opts: [{ value: "drag", label: qsTr("Drag") }, { value: "arcball", label: qsTr("Arc-ball") }] },
            { key: "voxelStyle", prop: "voxelStyle", type: "seg", title: qsTr("Voxel style"),
              desc: qsTr("Flat shows plain faces with a thin outline round every voxel face. Classic is the original look: bevelled voxels in alternating light and dark shades."),
              opts: [{ value: "flat", label: qsTr("Flat") }, { value: "legacy", label: qsTr("Classic") }] },
            { key: "lighting", prop: "lighting", type: "switch", title: qsTr("Lighting"),
              desc: qsTr("Light the 3D preview so pieces look solid and shaded. Turn off for a flatter, unlit appearance.") },
            { key: "fadePieces", prop: "fadePieces", type: "switch", title: qsTr("Fade out removed pieces"),
              desc: qsTr("During a disassembly animation, pieces that have been removed fade away instead of vanishing at once.") }
        ] },
        { id: "performance", name: qsTr("Performance"), rows: [
            { key: "workerThreads", prop: "workerThreads", type: "slider", title: qsTr("Worker threads"),
              desc: qsTr("Threads used for assembling and disassembling. Both stages share this pool, so raising it speeds up both.") },
            { key: "displayLists", prop: "displayLists", type: "switch", title: qsTr("Use OpenGL display lists"),
              desc: qsTr("Cache 3D geometry in OpenGL display lists. The redesigned 3D view draws without display lists, so this setting has no effect here; it is kept so the classic program’s choice carries over.") }
        ] }
    ]

    function rowMatches(row) {
        const q = root.query.trim().toLowerCase()
        return row.title.toLowerCase().indexOf(q) >= 0 || row.desc.toLowerCase().indexOf(q) >= 0
    }

    // the keyboard shortcuts the search also finds
    readonly property var shortcutMatches: {
        if (!searching)
            return []
        const q = query.trim().toLowerCase()
        const out = []
        for (const g of App.commands.shortcutHelp())
            for (const r of g.rows) {
                const keys = r.keys.map(a => a.map(p => p.text).join("+")).join(" / ")
                if (r.action.toLowerCase().indexOf(q) >= 0 || keys.toLowerCase().indexOf(q) >= 0)
                    out.push(r)
            }
        return out
    }

    // --- pieces --------------------------------------------------------------

    component PageTitle: RowLayout {
        id: pageTitle
        property string text
        property string page
        property bool resettable: true
        width: parent ? parent.width : 0
        Text {
            text: pageTitle.text.toUpperCase()
            color: Theme.muted
            font.pixelSize: Theme.fontMicro
            font.weight: Font.DemiBold
            font.letterSpacing: 0.5
            Layout.fillWidth: true
        }
        BtButton {
            visible: pageTitle.resettable
            objectName: "settings.reset." + pageTitle.page
            small: true
            text: qsTr("Reset section")
            onClicked: App.settings.resetSection(pageTitle.page)
        }
    }

    component SettingRow: Item {
        id: settingRow
        property var row
        objectName: "settings.row." + row.key
        width: parent ? parent.width : 0
        implicitHeight: Math.max(texts.implicitHeight, control.implicitHeight) + 28

        ColumnLayout {
            id: texts
            anchors { left: parent.left; right: control.left; rightMargin: 28; verticalCenter: parent.verticalCenter }
            spacing: 2
            Text {
                text: settingRow.row.title
                color: Theme.text
                font.pixelSize: Theme.fontBody
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                text: settingRow.row.desc
                color: Theme.muted
                font.pixelSize: Theme.fontSecondary
                wrapMode: Text.WordWrap
            }
        }
        Loader {
            id: control
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            property var row: settingRow.row
            sourceComponent: row.type === "switch" ? switchControl
                           : row.type === "seg" ? segControl
                           : row.type === "dropdown" ? dropdownControl : sliderControl
        }
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 1
            color: Theme.line
        }
    }

    Component {
        id: switchControl
        BtSwitch {
            objectName: "settings." + parent.row.key
            Accessible.name: parent.row.title
            checked: App.settings[parent.row.prop]
            onToggled: App.settings[parent.row.prop] = checked
        }
    }
    Component {
        id: segControl
        Segmented {
            objectName: "settings." + parent.row.key
            Accessible.name: parent.row.title
            options: parent.row.opts
            value: App.settings[parent.row.prop]
            onActivated: (v) => App.settings[parent.row.prop] = v
        }
    }
    Component {
        id: dropdownControl
        Dropdown {
            objectName: "settings." + parent.row.key
            Accessible.name: parent.row.title
            model: parent.row.opts
            currentIndex: parent.row.opts.indexOf(App.settings[parent.row.prop])
            onActivated: (i) => App.settings[parent.row.prop] = parent.row.opts[i]
        }
    }
    Component {
        id: sliderControl
        BtSlider {
            objectName: "settings." + parent.row.key
            Accessible.name: parent.row.title
            from: 1
            to: App.settings.maxWorkerThreads
            value: App.settings[parent.row.prop]
            onMoved: (v) => App.settings[parent.row.prop] = v
        }
    }

    // --- frame ------------------------------------------------------------------

    header: Item {
        implicitHeight: 56
        RowLayout {
            anchors { fill: parent; leftMargin: 20; rightMargin: 12 }
            spacing: 16
            Text {
                text: qsTr("Settings")
                color: Theme.text
                font.pixelSize: Theme.fontDialogTitle
                font.weight: Font.Bold
            }
            SearchField {
                id: search
                objectName: "settings.search"
                placeholderText: qsTr("Search settings")
                onTextChanged: root.query = text
            }
            Item { Layout.fillWidth: true }
            IconButton {
                objectName: "settings.close"
                // C12 Tab order is search, nav, rows, footer; Esc closes from the keyboard
                focusPolicy: Qt.NoFocus
                iconName: "close"
                tip: qsTr("Close (Esc)")
                onClicked: root.close()
            }
        }
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 1
            color: Theme.line
        }
    }

    // the footer is part of the content, after the page, so Tab runs
    // search -> nav -> rows -> footer (C12); a Dialog footer would come first
    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // left navigation
            Rectangle {
                Layout.preferredWidth: 200
                Layout.fillHeight: true
                color: Theme.panel2
                Column {
                    anchors { fill: parent; margins: 10 }
                    spacing: 4
                    Repeater {
                        model: root.pages
                        delegate: AbstractButton {
                            id: navItem
                            required property var modelData
                            readonly property bool current: !root.searching && root.page === modelData.id
                            objectName: "settings.nav." + modelData.id
                            width: parent.width
                            height: 34
                            hoverEnabled: true
                            focusPolicy: Qt.StrongFocus
                            Accessible.role: Accessible.PageTab
                            Accessible.name: modelData.name
                            onClicked: {
                                search.text = ""
                                root.page = modelData.id
                            }
                            background: Rectangle {
                                radius: Theme.radiusControl
                                color: navItem.current ? Theme.panel : (navItem.hovered ? Qt.darker(Theme.panel2, 1.04) : "transparent")
                                border.width: navItem.visualFocus ? 2 : 0
                                border.color: Theme.accent
                            }
                            contentItem: Text {
                                leftPadding: 12
                                verticalAlignment: Text.AlignVCenter
                                text: navItem.modelData.name
                                color: navItem.current ? Theme.text : Theme.muted
                                font.pixelSize: Theme.fontBody
                                font.weight: navItem.current ? Font.DemiBold : Font.Normal
                            }
                        }
                    }
                }
            }

            // the page, or the search results
            ScrollView {
                id: pageArea
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth

                Column {
                    x: 26
                    width: pageArea.availableWidth - 52
                    topPadding: 16
                    bottomPadding: 16
                    spacing: 4

                    Repeater {
                        model: root.pages
                        delegate: Column {
                            id: pageColumn
                            required property var modelData
                            readonly property var shown: root.searching ? modelData.rows.filter(r => root.rowMatches(r))
                                                                        : (root.page === modelData.id ? modelData.rows : [])
                            width: parent.width
                            visible: shown.length > 0
                            spacing: 0
                            bottomPadding: 12

                            PageTitle {
                                text: pageColumn.modelData.name
                                page: pageColumn.modelData.id
                                resettable: !root.searching
                            }
                            Repeater {
                                model: pageColumn.shown
                                delegate: SettingRow {
                                    required property var modelData
                                    row: modelData
                                }
                            }
                        }
                    }

                    // matching shortcuts (AC-C22-02)
                    Column {
                        width: parent.width
                        visible: root.shortcutMatches.length > 0
                        spacing: 0
                        PageTitle { text: qsTr("Keyboard shortcuts"); resettable: false }
                        Repeater {
                            model: root.shortcutMatches
                            delegate: Item {
                                id: shortcutRow
                                required property var modelData
                                width: parent.width
                                implicitHeight: 40
                                Text {
                                    anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                                    text: shortcutRow.modelData.action
                                    color: Theme.text
                                    font.pixelSize: Theme.fontBody
                                }
                                ShortcutKeys {
                                    anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                                    keys: shortcutRow.modelData.keys
                                }
                            }
                        }
                    }

                    Text {
                        objectName: "settings.noResults"
                        visible: root.searching && root.shortcutMatches.length === 0
                                 && root.pages.every(p => p.rows.filter(r => root.rowMatches(r)).length === 0)
                        text: qsTr("No settings match “%1”").arg(root.query.trim())
                        color: Theme.muted
                        font.pixelSize: Theme.fontBody
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            implicitHeight: 56
            Rectangle {
                anchors { left: parent.left; right: parent.right; top: parent.top }
                height: 1
                color: Theme.line
            }
            Text {
                anchors { left: parent.left; leftMargin: 20; verticalCenter: parent.verticalCenter }
                text: qsTr("Changes apply immediately")
                color: Theme.muted
                font.pixelSize: Theme.fontSecondary
            }
            Row {
                anchors { right: parent.right; rightMargin: 20; verticalCenter: parent.verticalCenter }
                spacing: 8
                BtButton { objectName: "settings.restoreAll"; text: qsTr("Restore all defaults"); onClicked: App.settings.restoreAllDefaults() }
                BtButton { objectName: "settings.done"; text: qsTr("Done"); primary: true; onClicked: root.close() }
            }
        }
    }
}
