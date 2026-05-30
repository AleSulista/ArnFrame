import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import Drift
import "components"

PanelFrame {
    id: root

    Component.onCompleted: AssetLibrary.ensureAllMedia()

    function addAssetToTimeline(assetIndex) {
        if (typeof Window !== "undefined" && Window.window && Window.window.configureAndAddAsset) {
            Window.window.configureAndAddAsset(assetIndex, () => EditorState.addClipFromAsset(assetIndex))
        } else {
            EditorState.addClipFromAsset(assetIndex)
        }
    }

    function importMedia() {
        var urls = FileDialogs.openFiles(qsTr("Import Media"), [
            qsTr("Media files (*.mp4 *.mov *.mkv *.avi *.webm *.m4v *.mp3 *.wav *.aac *.flac *.ogg *.m4a *.png *.jpg *.jpeg *.gif *.webp *.bmp)")
        ])
        if (urls.length > 0)
            AssetLibrary.importUrls(urls)
    }

    function kindsForTab(tabId) {
        if (tabId === "media") return ["video", "image", "audio"]
        return []
    }

    function assetVisible(kind) {
        const tabId = tabsModel.get(activeTab).tabId
        if (tabId === "text" || tabId === "stickers" || tabId === "effects"
                || tabId === "adjustment" || tabId === "settings" || tabId === "sounds"
                || tabId === "transitions")
            return false
        const kinds = kindsForTab(tabId)
        return kinds.length === 0 || kinds.indexOf(kind) >= 0
    }

    ListModel {
        id: tabsModel
        ListElement { tabId: "media"; icon: 0; label: "Media" }
        ListElement { tabId: "sounds"; icon: 1; label: "Sounds" }
        ListElement { tabId: "text"; icon: 2; label: "Text" }
        ListElement { tabId: "stickers"; icon: 3; label: "Stickers" }
        ListElement { tabId: "effects"; icon: 4; label: "Effects" }
        ListElement { tabId: "transitions"; icon: 5; label: "Transitions" }
        ListElement { tabId: "settings"; icon: 6; label: "Settings" }
    }
    property var tabIcons: [
        Theme.icons.folder,
        Theme.icons.headphones,
        Theme.icons.type,
        Theme.icons.smile,
        Theme.icons.wand,
        Theme.icons.chevronsRight,
        Theme.icons.settings
    ]
    property int activeTab: 0
    property bool sortByKind: false

    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: (drop) => {
            if (drop.hasUrls)
                AssetLibrary.importUrls(drop.urls)
        }
    }

    Row {
        anchors.fill: parent
        spacing: 0

        Column {
            width: Theme.tabRailWidth
            height: parent.height
            topPadding: 4
            spacing: 2

            Repeater {
                model: tabsModel
                delegate: IconButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    icon: root.tabIcons[model.icon]
                    variant: "ghost"
                    tooltip: model.label
                    active: root.activeTab === index
                    onClicked: root.activeTab = index
                }
            }
        }

        Rectangle {
            width: 1
            height: parent.height
            color: Theme.panelBorder
        }

        Column {
            id: assetsContent
            width: parent.width - Theme.tabRailWidth - 1
            height: parent.height
            property bool gridMode: true

            Rectangle {
                width: parent.width
                height: Theme.panelHeaderHeight
                color: Theme.appBackground

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.panelBorder
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: tabsModel.get(root.activeTab).label
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                }

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6
                    visible: kindsForTab(tabsModel.get(root.activeTab).tabId).length > 0

                    IconButton {
                        icon: Theme.icons.grid
                        variant: "ghost"
                        tooltip: qsTr("Grid view")
                        active: assetsContent.gridMode
                        onClicked: assetsContent.gridMode = true
                    }
                    IconButton {
                        icon: Theme.icons.list
                        variant: "ghost"
                        tooltip: qsTr("List view")
                        active: !assetsContent.gridMode
                        onClicked: assetsContent.gridMode = false
                    }
                    IconButton {
                        icon: Theme.icons.sort
                        variant: "ghost"
                        tooltip: qsTr("Sort assets")
                        onClicked: {
                            if (root.sortByKind)
                                AssetLibrary.sortByName()
                            else
                                AssetLibrary.sortByKind()
                            root.sortByKind = !root.sortByKind
                        }
                    }

                    ThemedButton {
                        text: "Import"
                        variant: "ghost"
                        glyph: Theme.icons.upload
                        anchors.verticalCenter: parent.verticalCenter
                        onClicked: root.importMedia()
                    }
                }
            }

            // Text tab panel
            Column {
                visible: tabsModel.get(activeTab).tabId === "text"
                width: parent.width
                height: parent.height - Theme.panelHeaderHeight
                spacing: 8
                padding: 12

                ThemedTextField {
                    id: textClipInput
                    width: parent.width - 24
                    placeholderText: "Enter text for timeline clip"
                    font.family: Theme.fontFamily
                }

                ThemedButton {
                    text: "Add to timeline"
                    variant: "primary"
                    glyph: Theme.icons.type
                    onClicked: {
                        EditorState.addTextClip(textClipInput.text, -1)
                        textClipInput.clear()
                    }
                }
            }

            // Sounds tab panel
            Item {
                visible: tabsModel.get(activeTab).tabId === "sounds"
                width: parent.width
                height: parent.height - Theme.panelHeaderHeight

                Text {
                    anchors.centerIn: parent
                    width: parent.width - 48
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("This tab is for royalty free sound effects")
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                }
            }

            Item {
                visible: tabsModel.get(activeTab).tabId === "stickers"
                width: parent.width
                height: parent.height - Theme.panelHeaderHeight

                Flickable {
                    anchors.fill: parent
                    contentHeight: stickersContent.height + 24
                    clip: true
                    ScrollBar.vertical: AppScrollBar { }

                    Column {
                        id: stickersContent
                        x: 12
                        y: 12
                        width: parent.width - 24
                        spacing: 16

                        Text {
                            text: "Stickers"
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                        }

                        Grid {
                            width: parent.width
                            columns: Math.max(1, Math.floor((width + Theme.assetCardGap) / (Theme.assetCardWidth + Theme.assetCardGap)))
                            columnSpacing: Theme.assetCardGap
                            rowSpacing: Theme.assetCardGap

                            Repeater {
                                model: EditorState.builtinStickers()
                                delegate: Column {
                                    required property var modelData
                                    width: Theme.assetCardWidth
                                    spacing: 4

                                    Rectangle {
                                        width: Theme.assetCardWidth
                                        height: Theme.assetCardWidth
                                        radius: Theme.radiusSm
                                        color: Theme.panelAccent
                                        clip: true

                                        Image {
                                            anchors.fill: parent
                                            anchors.margins: 12
                                            source: EditorState.imageUrl(modelData.path)
                                            fillMode: Image.PreserveAspectFit
                                            asynchronous: true
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: EditorState.addStickerClip(modelData.id, -1)
                                        }
                                    }

                                    Text {
                                        width: parent.width
                                        text: modelData.label
                                        color: Theme.mutedForeground
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeCard
                                        horizontalAlignment: Text.AlignHCenter
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }

                        Text {
                            text: "Shapes"
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                            topPadding: 4
                        }

                        Grid {
                            width: parent.width
                            columns: Math.max(1, Math.floor((width + Theme.assetCardGap) / (Theme.assetCardWidth + Theme.assetCardGap)))
                            columnSpacing: Theme.assetCardGap
                            rowSpacing: Theme.assetCardGap

                            Repeater {
                                model: EditorState.builtinShapes()
                                delegate: Column {
                                    id: shapeCard
                                    required property var modelData
                                    width: Theme.assetCardWidth
                                    spacing: 4

                                    Drag.active: shapeDrag.active
                                    Drag.dragType: Drag.Automatic
                                    Drag.supportedActions: Qt.CopyAction
                                    Drag.keys: ["application/x-drift-shape"]
                                    Drag.mimeData: { "application/x-drift-shape": shapeCard.modelData.id }

                                    Rectangle {
                                        width: Theme.assetCardWidth
                                        height: Theme.assetCardWidth
                                        radius: Theme.radiusSm
                                        color: Theme.panelAccent

                                        ShapePreview {
                                            anchors.fill: parent
                                            anchors.margins: 16
                                            shapeKind: shapeCard.modelData.id
                                        }

                                        TapHandler {
                                            onTapped: EditorState.addShapeClip(shapeCard.modelData.id, -1)
                                        }
                                        DragHandler { id: shapeDrag }
                                    }

                                    Text {
                                        width: parent.width
                                        text: modelData.label
                                        color: Theme.mutedForeground
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeCard
                                        horizontalAlignment: Text.AlignHCenter
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Item {
                visible: tabsModel.get(activeTab).tabId === "settings"
                width: parent.width
                height: parent.height - Theme.panelHeaderHeight

                Column {
                    id: settingsColumn
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Text {
                        text: "Preview quality"
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                    }

                    ThemedComboBox {
                        model: ["full", "half", "quarter"]
                        currentIndex: Math.max(0, model.indexOf(EditorState.playback.previewQuality))
                        onActivated: EditorState.playback.previewQuality = model[currentIndex]
                    }

                    Text {
                        text: "Playback renders at half this scale, then snaps back on pause."
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        opacity: 0.7
                        width: settingsColumn.width
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        text: "Preview guides"
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        topPadding: 6
                    }

                    Row {
                        spacing: 8
                        CheckBox {
                            checked: EditorState.guidesEnabled
                            text: "Enabled"
                            onToggled: EditorState.guidesEnabled = checked
                        }
                        ThemedComboBox {
                            model: ["thirds", "crosshair", "safe"]
                            currentIndex: Math.max(0, model.indexOf(EditorState.guideType))
                            onActivated: EditorState.guideType = model[currentIndex]
                        }
                    }

                    Text {
                        text: "Canvas background"
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        topPadding: 6
                    }

                    ThemedComboBox {
                        id: bgKindCombo
                        model: ["color", "blur"]
                        currentIndex: Math.max(0, model.indexOf(EditorState.background.kind))
                        onActivated: EditorState.setBackground({ kind: model[currentIndex] })
                    }

                    Flow {
                        width: parent.width
                        spacing: 6
                        visible: EditorState.background.kind === "color"
                        Repeater {
                            model: ["#ff000000", "#ffffffff", "#ff808080", "#ff1e293b",
                                    "#ff2563eb", "#ff059669", "#ffdc2626", "#ffd97706"]
                            delegate: Rectangle {
                                required property string modelData
                                width: 22
                                height: 22
                                radius: 4
                                color: modelData
                                property bool selected: (EditorState.background.color || "").toLowerCase()
                                                        === modelData.toLowerCase()
                                border.width: selected ? 2 : 1
                                border.color: selected ? Theme.accent : Theme.panelBorder
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: EditorState.setBackground({ kind: "color", color: parent.modelData })
                                }
                            }
                        }
                    }

                    ThemedTextField {
                        width: 120
                        visible: EditorState.background.kind === "color"
                        text: EditorState.background.color || "#ff000000"
                        onEditingFinished: EditorState.setBackground({ kind: "color", color: text })
                    }

                    Column {
                        width: parent.width
                        spacing: 4
                        visible: EditorState.background.kind === "blur"

                        Text {
                            text: "Blur strength"
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                        }
                        ThemedSlider {
                            width: parent.width
                            from: 1
                            to: 100
                            stepSize: 1
                            value: EditorState.background.blurStrength || 20
                            onPressedChanged: {
                                if (!pressed)
                                    EditorState.setBackground({ kind: "blur", blurStrength: value })
                            }
                        }
                    }

                    Text {
                        text: "Keybindings"
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        topPadding: 6
                    }

                    Flickable {
                        width: parent.width
                        height: Math.max(80, settingsColumn.height - y)
                        contentHeight: shortcutColumn.height
                        clip: true
                        ScrollBar.vertical: AppScrollBar { }

                        Column {
                            id: shortcutColumn
                            width: parent.width
                            spacing: 6

                            Text {
                                width: parent.width
                                wrapMode: Text.WordWrap
                                text: "Click a shortcut, then press the keys. Esc cancels, Backspace clears."
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                bottomPadding: 4
                            }

                            Repeater {
                                model: EditorState.actions
                                delegate: Row {
                                    required property var modelData
                                    width: shortcutColumn.width
                                    spacing: 8

                                    Text {
                                        width: Math.max(90, shortcutColumn.width - 128)
                                        text: modelData.label
                                        color: Theme.panelForeground
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeXs
                                        wrapMode: Text.WordWrap
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    ShortcutCaptureField {
                                        width: 120
                                        actionId: modelData.id
                                        shortcut: modelData.shortcut
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Effects browser
            EffectBrowser {
                visible: tabsModel.get(activeTab).tabId === "effects"
                width: parent.width
                height: parent.height - Theme.panelHeaderHeight
            }

            // Transitions browser
            Item {
                id: transitionsBrowser
                visible: tabsModel.get(activeTab).tabId === "transitions"
                width: parent.width
                height: parent.height - Theme.panelHeaderHeight

                readonly property var categories: EditorState.transitionCategories()
                property string activeCategory: categories.length > 0 ? categories[0].id : ""

                Column {
                    anchors.fill: parent
                    spacing: 0

                    Text {
                        id: transitionTip
                        width: parent.width - 24
                        leftPadding: 12
                        rightPadding: 12
                        topPadding: 8
                        bottomPadding: 4
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("Drag onto an overlapping region between two clips. Overlaps default to crossfade.")
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                    }

                    Flickable {
                        id: transitionCategoryFlick
                        width: parent.width
                        height: 34
                        contentWidth: transitionCategoryRow.width + 24
                        clip: true

                        Row {
                            id: transitionCategoryRow
                            x: 12
                            height: parent.height
                            spacing: 6

                            Repeater {
                                model: transitionsBrowser.categories
                                delegate: ThemedChip {
                                    required property var modelData
                                    text: modelData.label
                                    variant: "secondary"
                                    selected: modelData.id === transitionsBrowser.activeCategory
                                    onClicked: transitionsBrowser.activeCategory = modelData.id
                                }
                            }
                        }
                    }

                    Flickable {
                        width: parent.width
                        height: Math.max(0, parent.height - transitionTip.height - transitionCategoryFlick.height)
                        contentHeight: transitionGrid.height + 24
                        clip: true
                        ScrollBar.vertical: AppScrollBar { }

                        Grid {
                            id: transitionGrid
                            x: 12
                            y: 12
                            width: parent.width - 24
                            columns: Math.max(1, Math.floor((width + Theme.assetCardGap) / (Theme.assetCardWidth + Theme.assetCardGap)))
                            columnSpacing: Theme.assetCardGap
                            rowSpacing: Theme.assetCardGap

                            Repeater {
                                model: EditorState.transitionKinds()
                                delegate: Column {
                                    id: transitionCard
                                    required property var modelData
                                    visible: transitionCard.modelData.category === transitionsBrowser.activeCategory
                                    width: visible ? Theme.assetCardWidth : 0
                                    spacing: 4
                                    opacity: transitionDrag.active ? 0.85 : 1

                                    readonly property string strip: transitionCard.modelData.previewStripPath || ""
                                    readonly property int frameCount: Math.max(1, transitionCard.modelData.previewFrames || 1)

                                    // Cards rest on a frame partway through the transition; hovering
                                    // scrubs the whole strip, which is the only way to tell many of
                                    // these apart (a crossfade and a dip look the same at p = 0.5).
                                    property real scrub: 0.45
                                    readonly property int frameIndex:
                                        Math.max(0, Math.min(frameCount - 1, Math.round(scrub * (frameCount - 1))))

                                    NumberAnimation on scrub {
                                        running: transitionHover.hovered && transitionCard.frameCount > 1
                                        from: 0
                                        to: 1
                                        duration: 1400
                                        loops: Animation.Infinite
                                    }

                                    Connections {
                                        target: transitionHover
                                        function onHoveredChanged() {
                                            if (!transitionHover.hovered)
                                                transitionCard.scrub = 0.45
                                        }
                                    }

                                    Drag.active: transitionDrag.active
                                    Drag.dragType: Drag.Automatic
                                    Drag.supportedActions: Qt.CopyAction
                                    Drag.keys: ["application/x-drift-transition"]
                                    Drag.mimeData: ({ "application/x-drift-transition": transitionCard.modelData.kind })
                                    Drag.hotSpot.x: width / 2
                                    Drag.hotSpot.y: Theme.assetCardWidth / 2

                                    Rectangle {
                                        width: Theme.assetCardWidth
                                        height: Theme.assetCardWidth
                                        radius: Theme.radiusSm
                                        color: transitionHover.hovered ? Theme.panelSecondaryBg : Theme.panelAccent
                                        border.width: transitionDrag.active ? 1 : 0
                                        border.color: Theme.transitionOverlap
                                        clip: true

                                        HoverHandler { id: transitionHover }

                                        DragHandler {
                                            id: transitionDrag
                                            target: null
                                            acceptedButtons: Qt.LeftButton
                                        }

                                        // The strip is one row of square cells; slide it rather than
                                        // re-decoding a sourceClipRect per frame.
                                        Image {
                                            visible: transitionCard.strip.length > 0
                                            source: transitionCard.strip.length > 0
                                                    ? EditorState.imageUrl(transitionCard.strip) : ""
                                            height: parent.height
                                            width: parent.height * transitionCard.frameCount
                                            x: -transitionCard.frameIndex * parent.height
                                            fillMode: Image.Stretch
                                            asynchronous: true
                                            smooth: true
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            visible: transitionCard.strip.length === 0
                                            text: "≫"
                                            color: Theme.transitionOverlap
                                            font.family: Theme.fontFamily
                                            font.pixelSize: 22
                                            font.weight: Font.Bold
                                        }
                                    }

                                    Text {
                                        width: parent.width
                                        text: transitionCard.modelData.label
                                        color: Theme.panelForeground
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeCard
                                        font.weight: Font.Medium
                                        horizontalAlignment: Text.AlignHCenter
                                        wrapMode: Text.WordWrap
                                        maximumLineCount: 2
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Flickable {
                id: flick
                visible: kindsForTab(tabsModel.get(activeTab).tabId).length > 0
                width: parent.width
                height: parent.height - Theme.panelHeaderHeight
                contentHeight: assetsContent.gridMode ? grid.height + 24 : listColumn.height + 24
                clip: true
                ScrollBar.vertical: AppScrollBar { }

                Grid {
                    id: grid
                    visible: assetsContent.gridMode
                    x: 12
                    y: 12
                    width: flick.width - 24
                    columns: Math.max(1, Math.floor((width + Theme.assetCardGap) / (Theme.assetCardWidth + Theme.assetCardGap)))
                    columnSpacing: Theme.assetCardGap
                    rowSpacing: Theme.assetCardGap

                    Repeater {
                        model: AssetLibrary
                        delegate: Column {
                            width: Theme.assetCardWidth
                            spacing: 4
                            visible: root.assetVisible(kind)

                            required property int index
                            required property string name
                            required property string kind
                            required property string duration
                            required property double durationSeconds
                            required property string path
                            required property string thumbnailPath
                            required property string filmstripPath

                            property int assetIndex: index

                            Drag.active: assetDrag.active
                            Drag.dragType: Drag.Automatic
                            Drag.supportedActions: Qt.CopyAction
                            Drag.keys: ["text/plain"]
                            Drag.mimeData: { "text/plain": assetIndex.toString() }

                            TapHandler { onTapped: root.addAssetToTimeline(assetIndex) }
                            DragHandler {
                                id: assetDrag
                                onActiveChanged: {
                                    if (active) {
                                        EditorState.draggingAssetIndex = assetIndex
                                    } else {
                                        Qt.callLater(function() {
                                            if (!assetDrag.active)
                                                EditorState.draggingAssetIndex = -1
                                        })
                                    }
                                }
                            }

                            Rectangle {
                                width: Theme.assetCardWidth
                                height: Theme.assetCardWidth * 9 / 16
                                radius: Theme.radiusSm
                                color: Theme.panelAccent
                                clip: true

                                Image {
                                    anchors.fill: parent
                                    visible: thumbnailPath.length > 0
                                    source: thumbnailPath.length > 0 ? EditorState.imageUrl(thumbnailPath) : ""
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                }

                                IconGlyph {
                                    anchors.centerIn: parent
                                    visible: thumbnailPath.length === 0
                                    glyph: kind === "audio" ? Theme.icons.music
                                         : kind === "image" ? Theme.icons.image
                                         : Theme.icons.film
                                    iconSize: 24
                                    iconColor: Theme.mutedForeground
                                }

                                Rectangle {
                                    visible: duration.length > 0
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 4
                                    color: "#000000b3"
                                    radius: 3
                                    width: durationLabel.implicitWidth + 8
                                    height: durationLabel.implicitHeight + 4
                                    Text {
                                        id: durationLabel
                                        anchors.centerIn: parent
                                        text: duration
                                        color: "white"
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                }
                            }

                            Text {
                                width: parent.width
                                text: name
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeCard
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                Column {
                    id: listColumn
                    visible: !assetsContent.gridMode
                    x: 12
                    y: 12
                    width: flick.width - 24
                    spacing: 6

                    Repeater {
                        model: AssetLibrary
                        delegate: Rectangle {
                            width: listColumn.width
                            height: 48
                            radius: Theme.radiusSm
                            color: Theme.panelAccent
                            visible: root.assetVisible(kind)

                            required property int index
                            required property string name
                            required property string kind
                            required property string duration
                            required property string thumbnailPath

                            property int assetIndex: index

                            Row {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 10

                                Rectangle {
                                    width: 56
                                    height: 32
                                    radius: 4
                                    color: Theme.panelBackground
                                    clip: true
                                    Image {
                                        anchors.fill: parent
                                        visible: thumbnailPath.length > 0
                                        source: thumbnailPath.length > 0 ? EditorState.imageUrl(thumbnailPath) : ""
                                        fillMode: Image.PreserveAspectCrop
                                    }
                                    IconGlyph {
                                        anchors.centerIn: parent
                                        visible: thumbnailPath.length === 0
                                        glyph: kind === "audio" ? Theme.icons.music : Theme.icons.film
                                        iconSize: 16
                                        iconColor: Theme.mutedForeground
                                    }
                                }

                                Column {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - 66
                                    Text {
                                        text: name
                                        color: Theme.panelForeground
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        elide: Text.ElideRight
                                        width: parent.width
                                    }
                                    Text {
                                        text: kind + (duration.length > 0 ? " · " + duration : "")
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.addAssetToTimeline(assetIndex)
                            }
                        }
                    }
                }
            }
        }
    }
}
