import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import QtQuick.Dialogs
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

    // Imports and reports the outcome. `importUrls` skips anything it cannot
    // probe, so a bad file used to just never appear with no explanation at all.
    // Comparing the row count before and after tells us how many were rejected.
    function importUrlsReporting(urls) {
        if (!urls || urls.length === 0)
            return
        root.importing = true
        const before = AssetLibrary.count
        AssetLibrary.importUrls(urls)
        const added = AssetLibrary.count - before
        root.importing = false

        const skipped = urls.length - added
        if (added > 0 && skipped > 0)
            Toasts.warning(qsTr("Imported %1 of %2 files. %3 could not be read.")
                           .arg(added).arg(urls.length).arg(skipped))
        else if (added > 0)
            Toasts.success(qsTr("Imported %n file(s).", "", added))
        else if (urls.length === 1)
            Toasts.error(qsTr("Could not import that file — the format may be unsupported."))
        else
            Toasts.error(qsTr("Could not import any of the %n selected file(s).", "", urls.length))
    }

    // True while an import is running, so the panel can show progress.
    property bool importing: false

    function importMedia() {
        var urls = FileDialogs.openFiles(qsTr("Import Media"), [
            qsTr("Media files (*.mp4 *.mov *.mkv *.avi *.webm *.m4v *.mp3 *.wav *.aac *.flac *.ogg *.m4a *.png *.jpg *.jpeg *.gif *.webp *.bmp)")
        ])
        root.importUrlsReporting(urls)
    }

    // Selects a tab by id. Used by cross-panel jumps such as the properties
    // panel's "Browse effects" empty-state action.
    function showTab(tabId) {
        for (var i = 0; i < tabsModel.count; ++i) {
            if (tabsModel.get(i).tabId === tabId) {
                root.activeTab = i
                return
            }
        }
    }

    function kindsForTab(tabId) {
        if (tabId === "media") return ["video", "image", "audio"]
        return []
    }

    function assetVisible(kind) {
        const tabId = tabsModel.get(activeTab).tabId
        if (tabId === "text" || tabId === "stickers" || tabId === "effects"
                || tabId === "adjustment" || tabId === "settings" || tabId === "sounds"
                || tabId === "transitions" || tabId === "shortcuts")
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
        ListElement { tabId: "shortcuts"; icon: 7; label: "Shortcuts" }
    }
    property var tabIcons: [
        Theme.icons.film,
        Theme.icons.headphones,
        Theme.icons.type,
        Theme.icons.smile,
        Theme.icons.wand,
        Theme.icons.blend,
        Theme.icons.settings,
        Theme.icons.keyboard
    ]
    property int activeTab: 0
    property bool sortByKind: false

    DropArea {
        id: assetDropArea
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: (drop) => {
            if (drop.hasUrls)
                root.importUrlsReporting(drop.urls)
        }
    }

    // Drag feedback. Dropping files onto the panel used to give no visual
    // confirmation that it was even a valid target.
    Rectangle {
        anchors.fill: parent
        z: 50
        radius: Theme.radiusMd
        color: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.08)
        border.width: Theme.borderWidthFocus
        border.color: Theme.primary
        visible: opacity > 0
        opacity: assetDropArea.containsDrag ? 1 : 0

        Behavior on opacity {
            NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
        }

        EmptyState {
            anchors.centerIn: parent
            glyph: Theme.icons.upload
            title: qsTr("Drop to import")
            hint: qsTr("Video, audio and image files")
        }
    }

    // Import progress. Probing and thumbnailing a large selection blocks for a
    // while; the panel used to simply appear frozen.
    Rectangle {
        anchors.fill: parent
        z: 60
        color: Theme.panelBackground
        opacity: root.importing ? 0.92 : 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
        }

        EmptyState {
            anchors.centerIn: parent
            glyph: Theme.icons.spinner
            title: qsTr("Importing…")
            hint: qsTr("Reading media and generating thumbnails.")
        }
    }

    Row {
        anchors.fill: parent
        spacing: 0

        // Vertical tab rail. Up/Down move between tabs once it has focus.
        Column {
            id: tabRail
            width: Theme.tabRailWidth
            height: parent.height
            topPadding: Theme.spacingSm
            spacing: Theme.spacingXs

            Accessible.role: Accessible.PageTabList

            Keys.onUpPressed: function(event) {
                root.activeTab = (root.activeTab - 1 + tabsModel.count) % tabsModel.count
                event.accepted = true
            }
            Keys.onDownPressed: function(event) {
                root.activeTab = (root.activeTab + 1) % tabsModel.count
                event.accepted = true
            }

            Repeater {
                model: tabsModel
                delegate: IconButton {
                    required property int index
                    required property var model

                    anchors.horizontalCenter: parent.horizontalCenter
                    glyph: root.tabIcons[model.icon]
                    variant: "ghost"
                    tooltip: model.label
                    active: root.activeTab === index
                    onClicked: root.activeTab = index

                    Accessible.role: Accessible.PageTab
                    Accessible.name: model.label
                    Accessible.checked: root.activeTab === index
                }
            }
        }

        Rectangle {
            width: Theme.borderWidth
            height: parent.height
            color: Theme.panelBorder
        }

        Column {
            id: assetsContent
            width: parent.width - Theme.tabRailWidth - Theme.borderWidth
            height: parent.height
            property bool gridMode: true

            Rectangle {
                width: parent.width
                height: Theme.panelHeaderHeight
                // Matches the surrounding PanelFrame; it used to paint the app
                // background, so the header read as a different surface than the
                // panel it belongs to.
                color: Theme.panelBackground

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: Theme.borderWidth
                    color: Theme.panelBorder
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.pagePadding
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
                        glyph: Theme.icons.grid
                        variant: "ghost"
                        tooltip: qsTr("Grid view")
                        active: assetsContent.gridMode
                        onClicked: assetsContent.gridMode = true
                    }
                    IconButton {
                        glyph: Theme.icons.list
                        variant: "ghost"
                        tooltip: qsTr("List view")
                        active: !assetsContent.gridMode
                        onClicked: assetsContent.gridMode = false
                    }
                    IconButton {
                        glyph: root.sortByKind ? Theme.icons.sortByKind : Theme.icons.sortByName
                        variant: "ghost"
                        tooltip: root.sortByKind ? qsTr("Sort by name") : qsTr("Sort by kind")
                        onClicked: {
                            if (root.sortByKind)
                                AssetLibrary.sortByName()
                            else
                                AssetLibrary.sortByKind()
                            root.sortByKind = !root.sortByKind
                        }
                    }

                    ThemedButton {
                        text: qsTr("Import")
                        variant: "ghost"
                        glyph: Theme.icons.upload
                        tooltip: qsTr("Import video, audio or image files")
                        enabled: !root.importing
                        anchors.verticalCenter: parent.verticalCenter
                        onClicked: root.importMedia()
                    }
                }
            }

            // Text tab panel
            Column {
                id: textTab
                visible: tabsModel.get(activeTab).tabId === "text"
                width: parent.width
                height: parent.height - Theme.panelHeaderHeight
                spacing: Theme.spacingLg
                padding: Theme.pagePadding

                // Width available inside the Column's padding. Was open-coded as
                // `textTab.contentWidth` at eleven separate sites.
                readonly property real contentWidth: width - padding * 2

                ThemedTextField {
                    id: textClipInput
                    width: textTab.contentWidth
                    placeholderText: qsTr("Enter text (optional)")
                    font.family: Theme.fontFamily
                }

                ThemedButton {
                    text: qsTr("Add to timeline")
                    variant: "primary"
                    glyph: Theme.icons.type
                    onClicked: {
                        EditorState.addTextClip(textClipInput.text, -1)
                        textClipInput.clear()
                    }
                }

                Text {
                    width: textTab.contentWidth
                    wrapMode: Text.WordWrap
                    text: qsTr("Leave it empty and just add — the clip lands on the preview ready to type. You can always double-click text on the preview to edit it.")
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }

                Rectangle {
                    width: textTab.contentWidth
                    height: Theme.borderWidth
                    color: Theme.panelBorder
                }

                Text {
                    width: textTab.contentWidth
                    wrapMode: Text.WordWrap
                    text: qsTr("Subtitle track — one clip holds many timed captions. Place it on the timeline, trim its duration, then type cues at each moment in the inspector.")
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }

                ThemedButton {
                    text: qsTr("Add subtitle clip")
                    variant: "secondary"
                    glyph: Theme.icons.messageSquare
                    onClicked: EditorState.addSubtitleClip(-1)
                }

                Rectangle {
                    width: textTab.contentWidth
                    height: Theme.borderWidth
                    color: Theme.panelBorder
                }

                // Same transcriber as the clip inspector's Audio tab, surfaced here so
                // auto captions sit next to the manual subtitle route. It transcribes the
                // selected clip, so it stays disabled until a video or audio clip is picked.
                Text {
                    width: textTab.contentWidth
                    text: qsTr("Add auto caption")
                    color: Theme.foreground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                }

                Text {
                    width: textTab.contentWidth
                    wrapMode: Text.WordWrap
                    text: textTab.captionTargetReady
                          ? qsTr("Transcribes the selected clip and drops the cues onto a subtitle clip.")
                          : qsTr("Select a video (or audio) clip on the timeline first — auto caption runs on the selected clip.")
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }

                // The transcriber ships as an addon; without it there are no languages to
                // list and nothing to run, so offer the download in place of the controls.
                property bool whisperReady: Addons.hasKind("whisper-model")

                readonly property string captionClipKind: {
                    const data = EditorState.selectedClipData
                    return (data && data.kind) ? data.kind : ""
                }
                readonly property bool captionTargetReady: captionClipKind === "video"
                                                           || captionClipKind === "audio"

                Connections {
                    target: Addons
                    function onKindChanged(kind) {
                        if (kind === "whisper-model")
                            textTab.whisperReady = Addons.hasKind("whisper-model")
                    }
                }

                ThemedComboBox {
                    id: captionLanguageBox
                    visible: textTab.whisperReady
                    width: textTab.contentWidth
                    enabled: textTab.captionTargetReady && !EditorState.subtitleGenerating
                    textRole: "label"
                    valueRole: "code"
                    model: EditorState.whisperLanguages()
                    Component.onCompleted: currentIndex = 0
                }

                ThemedButton {
                    visible: textTab.whisperReady && !EditorState.subtitleGenerating
                    width: textTab.contentWidth
                    text: qsTr("Add auto caption")
                    variant: "secondary"
                    glyph: Theme.icons.messageSquare
                    tooltip: textTab.captionTargetReady
                             ? qsTr("Transcribe the selected clip into subtitle cues")
                             : qsTr("Select a video or audio clip first")
                    enabled: textTab.captionTargetReady
                    onClicked: {
                        const lang = captionLanguageBox.currentValue !== undefined
                                     ? captionLanguageBox.currentValue
                                     : ""
                        EditorState.generateSubtitlesForClip(
                            EditorState.selectedTrack, EditorState.selectedClip, lang)
                    }
                }

                // Real progress for a multi-minute operation. This used to be a
                // percentage baked into the button's own label, with no bar and
                // no way to stop.
                Column {
                    visible: textTab.whisperReady && EditorState.subtitleGenerating
                    width: textTab.contentWidth
                    spacing: Theme.spacingMd

                    Text {
                        width: parent.width
                        text: EditorState.subtitleGenStatus.length > 0
                              ? EditorState.subtitleGenStatus
                              : qsTr("Transcribing… %1%").arg(Math.round(EditorState.subtitleGenProgress * 100))
                        color: Theme.panelForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        width: parent.width
                        height: Theme.spacingMd
                        radius: height / 2
                        color: Theme.panelMuted

                        Rectangle {
                            width: parent.width * Math.max(0, Math.min(1, EditorState.subtitleGenProgress))
                            height: parent.height
                            radius: parent.radius
                            color: Theme.primary

                            Behavior on width {
                                NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easing }
                            }
                        }
                    }

                    ThemedButton {
                        text: qsTr("Cancel")
                        variant: "destructive"
                        glyph: Theme.icons.x
                        tooltip: qsTr("Stop transcribing")
                        onClicked: EditorState.cancelSubtitleGeneration()
                    }
                }

                ThemedButton {
                    visible: !textTab.whisperReady
                    width: textTab.contentWidth
                    text: qsTr("Install speech model (≈670 MB)")
                    variant: "primary"
                    glyph: Theme.icons.download
                    tooltip: qsTr("Downloads the Whisper speech model used for auto captions")
                    onClicked: root.Window.window.openAddonManager("whisper-model")
                }
            }

            // Sounds tab panel
            Item {
                visible: tabsModel.get(activeTab).tabId === "sounds"
                width: parent.width
                height: parent.height - Theme.panelHeaderHeight

                // Uses the shared empty state rather than bare centered text, so
                // it reads the same as every other "nothing here" surface.
                EmptyState {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - Theme.spacing3xl * 2, 260)
                    glyph: Theme.icons.headphones
                    title: qsTr("Sound effects")
                    hint: qsTr("Royalty-free sound effects will appear here.")
                }
            }

            Item {
                id: stickersTab
                visible: tabsModel.get(activeTab).tabId === "stickers"
                width: parent.width
                height: parent.height - Theme.panelHeaderHeight

                // Stickers come from an addon, so these are refreshed on install rather than
                // being fixed at load: a fresh install has no packs and only the Shapes page.
                property var categories: EditorState.builtinStickerCategories()
                property var allStickers: EditorState.builtinStickers()
                readonly property var pages: categories.concat([{ id: "shapes", label: "Shapes" }])
                readonly property bool hasStickers: allStickers.length > 0
                property int pageIndex: 0
                readonly property string currentPageId: pages[pageIndex] ? pages[pageIndex].id : ""

                Connections {
                    target: Addons
                    function onKindChanged(kind) {
                        if (kind !== "stickers")
                            return
                        stickersTab.categories = EditorState.builtinStickerCategories()
                        stickersTab.allStickers = EditorState.builtinStickers()
                        stickersTab.pageIndex = 0
                    }
                }

                Flow {
                    id: stickerPageBar
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 12
                    spacing: 6

                    Repeater {
                        model: stickersTab.pages
                        delegate: ThemedChip {
                            required property var modelData
                            required property int index
                            text: modelData.label
                            variant: "secondary"
                            selected: stickersTab.pageIndex === index
                            onClicked: stickersTab.pageIndex = index
                        }
                    }
                }

                Flickable {
                    anchors.top: stickerPageBar.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.topMargin: 12
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    contentHeight: stickerPageContent.height + 24
                    clip: true
                    ScrollBar.vertical: AppScrollBar { }

                    Column {
                        id: stickerPageContent
                        width: parent.width - 12
                        spacing: 16

                        EmptyState {
                            visible: !stickersTab.hasStickers
                            width: parent.width
                            compact: true
                            glyph: Theme.icons.smile
                            title: qsTr("No sticker packs installed")
                            hint: qsTr("Install the emoji pack to add stickers.")
                            actionText: qsTr("Open addon manager")
                            actionVariant: "primary"
                            onActionTriggered: root.Window.window.openAddonManager("stickers")
                        }

                        // A category page with no matching stickers used to render
                        // as a silently blank grid.
                        readonly property var currentStickers:
                            stickersTab.currentPageId === "shapes"
                            ? []
                            : stickersTab.allStickers.filter(function(s) { return s.category === stickersTab.currentPageId })

                        EmptyState {
                            visible: stickersTab.hasStickers
                                     && stickersTab.currentPageId !== "shapes"
                                     && stickerPageContent.currentStickers.length === 0
                            width: parent.width
                            compact: true
                            glyph: Theme.icons.smile
                            title: qsTr("Nothing in this category")
                            hint: qsTr("Pick another category above.")
                        }

                        // Sticker grid for the selected category page.
                        Grid {
                            visible: stickersTab.currentPageId !== "shapes"
                            width: parent.width
                            columns: Math.max(1, Math.floor((width + Theme.assetCardGap) / (Theme.assetCardWidth + Theme.assetCardGap)))
                            columnSpacing: Theme.assetCardGap
                            rowSpacing: Theme.assetCardGap

                            Repeater {
                                model: stickerPageContent.currentStickers
                                delegate: Column {
                                    required property var modelData
                                    width: Theme.assetCardWidth
                                    spacing: Theme.spacingSm

                                    Rectangle {
                                        width: Theme.assetCardWidth
                                        height: Theme.assetCardWidth
                                        radius: Theme.radiusSm
                                        color: stickerMouse.containsMouse ? Theme.popoverHover : Theme.panelAccent
                                        clip: true

                                        Behavior on color {
                                            ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                                        }

                                        SkeletonBox {
                                            anchors.fill: parent
                                            anchors.margins: Theme.pagePadding
                                            visible: stickerImage.status === Image.Loading
                                        }

                                        Image {
                                            id: stickerImage
                                            anchors.fill: parent
                                            anchors.margins: Theme.pagePadding
                                            source: EditorState.imageUrl(modelData.path)
                                            fillMode: Image.PreserveAspectFit
                                            asynchronous: true
                                            opacity: status === Image.Ready ? 1 : 0

                                            Behavior on opacity {
                                                NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easing }
                                            }
                                        }

                                        IconGlyph {
                                            anchors.centerIn: parent
                                            visible: stickerImage.status === Image.Error
                                            glyph: Theme.icons.error
                                            iconSize: Theme.iconSizeLg
                                            iconColor: Theme.mutedForeground
                                        }

                                        ThemedToolTip {
                                            text: modelData.label
                                            visible: stickerMouse.containsMouse
                                        }

                                        MouseArea {
                                            id: stickerMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
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

                        // Shapes page.
                        Grid {
                            visible: stickersTab.currentPageId === "shapes"
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
                                    spacing: Theme.spacingSm

                                    Drag.active: shapeDrag.active
                                    Drag.dragType: Drag.Automatic
                                    Drag.supportedActions: Qt.CopyAction
                                    Drag.keys: ["application/x-drift-shape"]
                                    Drag.mimeData: { "application/x-drift-shape": shapeCard.modelData.id }

                                    Rectangle {
                                        width: Theme.assetCardWidth
                                        height: Theme.assetCardWidth
                                        radius: Theme.radiusSm
                                        // Shape cards had neither hover feedback
                                        // nor a cursor, unlike the sticker cards
                                        // in the very same grid.
                                        color: shapeHover.hovered ? Theme.popoverHover : Theme.panelAccent

                                        Behavior on color {
                                            ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                                        }

                                        // Matches the sticker cards' inset, which
                                        // was 12 here and 16 there for no reason.
                                        ShapePreview {
                                            anchors.fill: parent
                                            anchors.margins: Theme.pagePadding
                                            shapeKind: shapeCard.modelData.id
                                        }

                                        HoverHandler {
                                            id: shapeHover
                                            cursorShape: Qt.PointingHandCursor
                                        }

                                        ThemedToolTip {
                                            text: qsTr("%1 — click to add, or drag to the timeline").arg(shapeCard.modelData.label)
                                            visible: shapeHover.hovered
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

                // The whole settings page scrolls, so short panel heights no
                // longer push the keybindings list out of reach.
                Flickable {
                    anchors.fill: parent
                    contentHeight: settingsColumn.height + Theme.spacing3xl
                    clip: true
                    ScrollBar.vertical: AppScrollBar { }

                Column {
                    id: settingsColumn
                    x: Theme.pagePadding
                    width: parent.width - Theme.pagePadding * 2
                    spacing: Theme.spacingLg
                    topPadding: Theme.pagePadding

                    // Live project size, re-read whenever the timeline changes so
                    // undo/redo of a crop is reflected back into these fields.
                    property int canvasW: { void EditorState.tracks; return EditorState.projectWidth() }
                    property int canvasH: { void EditorState.tracks; return EditorState.projectHeight() }

                    readonly property var canvasPresets: [
                        { label: qsTr("Custom"), w: 0, h: 0 },
                        { label: "1920×1080 (16:9)", w: 1920, h: 1080 },
                        { label: "3840×2160 (4K)", w: 3840, h: 2160 },
                        { label: "1080×1920 (9:16)", w: 1080, h: 1920 },
                        { label: "1080×1080 (1:1)", w: 1080, h: 1080 },
                        { label: "1440×1080 (4:3)", w: 1440, h: 1080 }
                    ]

                    Text {
                        text: qsTr("Canvas resolution")
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                    }

                    ThemedComboBox {
                        width: parent.width
                        enabled: !EditorState.canvasCropMode
                        model: settingsColumn.canvasPresets.map(function (p) { return p.label })
                        tooltip: qsTr("Resize the output frame. Clips keep their current size and position.")
                        currentIndex: {
                            const presets = settingsColumn.canvasPresets
                            for (var i = 1; i < presets.length; ++i) {
                                if (presets[i].w === settingsColumn.canvasW
                                        && presets[i].h === settingsColumn.canvasH)
                                    return i
                            }
                            return 0
                        }
                        onActivated: {
                            const preset = settingsColumn.canvasPresets[currentIndex]
                            if (preset.w > 0)
                                EditorState.setProjectResolution(preset.w, preset.h)
                        }
                    }

                    Row {
                        width: parent.width
                        spacing: Theme.spacingLg

                        Column {
                            width: (parent.width - parent.spacing) / 2
                            spacing: Theme.spacingSm
                            ThemedLabel { text: qsTr("Width") }
                            ThemedNumberField {
                                width: parent.width
                                enabled: !EditorState.canvasCropMode
                                from: 16
                                to: 7680
                                step: 2
                                unit: "px"
                                value: settingsColumn.canvasW
                                onEdited: v => EditorState.setProjectResolution(v, settingsColumn.canvasH)
                            }
                        }

                        Column {
                            width: (parent.width - parent.spacing) / 2
                            spacing: Theme.spacingSm
                            ThemedLabel { text: qsTr("Height") }
                            ThemedNumberField {
                                width: parent.width
                                enabled: !EditorState.canvasCropMode
                                from: 16
                                to: 4320
                                step: 2
                                unit: "px"
                                value: settingsColumn.canvasH
                                onEdited: v => EditorState.setProjectResolution(settingsColumn.canvasW, v)
                            }
                        }
                    }

                    ThemedButton {
                        width: parent.width
                        variant: EditorState.canvasCropMode ? "primary" : "secondary"
                        glyph: Theme.icons.crop
                        text: EditorState.canvasCropMode ? qsTr("Cancel crop") : qsTr("Crop canvas")
                        tooltip: qsTr("Drag the preview edges to reframe the canvas")
                        onClicked: EditorState.canvasCropMode = !EditorState.canvasCropMode
                    }

                    Text {
                        text: qsTr("Clips are never rescaled by a canvas change — anything outside the new frame is cropped away.")
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        opacity: 0.7
                        width: settingsColumn.width
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        text: qsTr("Preview guides")
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        topPadding: Theme.spacingMd
                    }

                    Row {
                        spacing: Theme.spacingLg
                        ThemedCheckBox {
                            anchors.verticalCenter: parent.verticalCenter
                            checked: EditorState.guidesEnabled
                            text: qsTr("Enabled")
                            tooltip: qsTr("Show composition guides over the preview")
                            onToggled: EditorState.guidesEnabled = checked
                        }
                        ThemedComboBox {
                            anchors.verticalCenter: parent.verticalCenter
                            model: ["thirds", "crosshair", "safe"]
                            enabled: EditorState.guidesEnabled
                            tooltip: qsTr("Which guide overlay to draw")
                            currentIndex: Math.max(0, model.indexOf(EditorState.guideType))
                            onActivated: EditorState.guideType = model[currentIndex]
                        }
                    }

                    Text {
                        text: qsTr("Canvas background")
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        topPadding: Theme.spacingMd
                    }

                    ThemedComboBox {
                        id: bgKindCombo
                        model: ["color", "blur"]
                        tooltip: qsTr("Fill behind clips that do not cover the whole frame")
                        currentIndex: Math.max(0, model.indexOf(EditorState.background.kind))
                        onActivated: EditorState.setBackground({ kind: model[currentIndex] })
                    }


                    Row {
                        spacing: Theme.spacingMd
                        visible: EditorState.background.kind === "color"

                        Rectangle {
                            width: Theme.spacing3xl
                            height: Theme.spacing3xl
                            radius: Theme.radiusSm
                            anchors.verticalCenter: parent.verticalCenter
                            color: EditorState.background.color || "#ff000000"
                            border.width: swatchMouse.containsMouse ? Theme.borderWidthFocus : Theme.borderWidth
                            border.color: swatchMouse.containsMouse ? Theme.primary : Theme.panelBorder

                            Behavior on border.color {
                                ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                            }

                            ThemedToolTip {
                                text: qsTr("Choose canvas colour")
                                visible: swatchMouse.containsMouse
                            }

                            MouseArea {
                                id: swatchMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    canvasColorDialog.selectedColor = EditorState.background.color || "#ff000000"
                                    canvasColorDialog.open()
                                }
                            }
                        }

                        ThemedTextField {
                            id: canvasHexField
                            width: 92
                            text: EditorState.background.color || "#ff000000"
                            // Rejects malformed input instead of silently passing
                            // a typo through to the compositor.
                            readonly property bool valid: /^#([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$/.test(text)
                            errorText: valid || text.length === 0 ? "" : qsTr("Use #RRGGBB or #AARRGGBB")
                            onEditingFinished: {
                                if (valid)
                                    EditorState.setBackground({ kind: "color", color: text })
                            }
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: Theme.spacingSm
                        visible: EditorState.background.kind === "blur"

                        Text {
                            text: qsTr("Blur strength")
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                        }
                        ThemedSlider {
                            width: parent.width
                            from: 1
                            to: 100
                            stepSize: 1
                            valueFormatter: function (v) { return Math.round(v) }
                            value: EditorState.background.blurStrength || 20
                            onPressedChanged: {
                                if (!pressed)
                                    EditorState.setBackground({ kind: "blur", blurStrength: value })
                            }
                        }
                    }
                }
                }
            }

            // Shortcuts tab panel. Split out of Settings, which had grown long
            // enough that the binding list was permanently below the fold.
            Item {
                visible: tabsModel.get(activeTab).tabId === "shortcuts"
                width: parent.width
                height: parent.height - Theme.panelHeaderHeight

                Flickable {
                    anchors.fill: parent
                    contentHeight: shortcutColumn.height + Theme.spacing3xl
                    clip: true
                    ScrollBar.vertical: AppScrollBar { }

                    Column {
                        id: shortcutColumn
                        x: Theme.pagePadding
                        width: parent.width - Theme.pagePadding * 2
                        spacing: Theme.spacingMd
                        topPadding: Theme.pagePadding

                        Text {
                            width: parent.width
                            wrapMode: Text.WordWrap
                            text: qsTr("Click a shortcut, then press the keys. Esc cancels, Backspace clears.")
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                            bottomPadding: Theme.spacingSm
                        }

                        EmptyState {
                            visible: !EditorState.actions || EditorState.actions.length === 0
                            width: parent.width
                            compact: true
                            glyph: Theme.icons.keyboard
                            title: qsTr("No bindable actions")
                        }

                        Repeater {
                            model: EditorState.actions
                            delegate: Row {
                                required property var modelData
                                width: shortcutColumn.width
                                spacing: Theme.spacingLg

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
                        width: parent.width
                        leftPadding: Theme.pagePadding
                        rightPadding: Theme.pagePadding
                        topPadding: Theme.spacingLg
                        bottomPadding: Theme.spacingSm
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        // Capped so a wrapping tip cannot grow until it swallows
                        // the grid below it at narrow panel widths.
                        maximumLineCount: 3
                        elide: Text.ElideRight
                        text: qsTr("Drag onto an overlapping region between two clips. Overlaps default to crossfade.")
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                    }

                    Flickable {
                        id: transitionCategoryFlick
                        width: parent.width
                        height: 34
                        contentWidth: transitionCategoryRow.width + Theme.spacing3xl
                        clip: true

                        Row {
                            id: transitionCategoryRow
                            x: Theme.pagePadding
                            height: parent.height
                            spacing: Theme.spacingMd

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

                    Item {
                        width: parent.width
                        height: Math.max(0, parent.height - transitionTip.height - transitionCategoryFlick.height)

                        // A category whose filter matches nothing used to leave a
                        // blank scroll area with no explanation.
                        EmptyState {
                            anchors.centerIn: parent
                            width: Math.min(parent.width - Theme.spacing3xl, 260)
                            visible: transitionsBrowser.categories.length === 0
                            glyph: Theme.icons.blend
                            title: qsTr("No transitions available")
                            hint: qsTr("Install a transitions pack to add more.")
                            actionText: qsTr("Open addon manager")
                            onActionTriggered: root.Window.window.openAddonManager()
                        }

                    Flickable {
                        anchors.fill: parent
                        visible: transitionsBrowser.categories.length > 0
                        contentHeight: transitionGrid.height + Theme.spacing3xl
                        clip: true
                        ScrollBar.vertical: AppScrollBar { }

                        Grid {
                            id: transitionGrid
                            x: Theme.pagePadding
                            y: Theme.pagePadding
                            width: parent.width - Theme.pagePadding * 2
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
                                        border.width: transitionDrag.active ? Theme.borderWidth : 0
                                        border.color: Theme.transitionOverlap
                                        clip: true

                                        // The card already had a considered hover
                                        // scrub animation but no transition on its
                                        // own colours.
                                        Behavior on color {
                                            ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                                        }
                                        Behavior on border.width {
                                            NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                                        }

                                        HoverHandler {
                                            id: transitionHover
                                            cursorShape: Qt.PointingHandCursor
                                        }

                                        ThemedToolTip {
                                            text: qsTr("%1 — drag onto an overlap between two clips").arg(transitionCard.modelData.label)
                                            visible: transitionHover.hovered
                                        }

                                        DragHandler {
                                            id: transitionDrag
                                            target: null
                                            acceptedButtons: Qt.LeftButton
                                        }

                                        SkeletonBox {
                                            anchors.fill: parent
                                            visible: transitionCard.strip.length > 0
                                                     && transitionStrip.status === Image.Loading
                                        }

                                        // The strip is one row of square cells; slide it rather than
                                        // re-decoding a sourceClipRect per frame.
                                        Image {
                                            id: transitionStrip
                                            visible: transitionCard.strip.length > 0
                                                     && status === Image.Ready
                                            source: transitionCard.strip.length > 0
                                                    ? EditorState.imageUrl(transitionCard.strip) : ""
                                            height: parent.height
                                            width: parent.height * transitionCard.frameCount
                                            x: -transitionCard.frameIndex * parent.height
                                            fillMode: Image.Stretch
                                            asynchronous: true
                                            smooth: true
                                        }

                                        IconGlyph {
                                            anchors.centerIn: parent
                                            visible: transitionCard.strip.length === 0
                                                     || transitionStrip.status === Image.Error
                                            glyph: Theme.icons.blend
                                            iconSize: Theme.iconSizeXl
                                            iconColor: Theme.transitionOverlap
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
            }

            // First-run screen for a project with no media. This area used to
            // render as a blank rectangle, with no hint that the panel accepts
            // drops or that an Import button exists.
            EmptyState {
                width: parent.width
                height: parent.height - Theme.panelHeaderHeight
                visible: kindsForTab(tabsModel.get(activeTab).tabId).length > 0
                         && AssetLibrary.count === 0 && !root.importing
                glyph: Theme.icons.film
                title: qsTr("No media yet")
                hint: qsTr("Drag video, audio or images here, or use Import.")
                actionText: qsTr("Import media")
                actionVariant: "primary"
                onActionTriggered: root.importMedia()
            }

            Flickable {
                id: flick
                visible: kindsForTab(tabsModel.get(activeTab).tabId).length > 0
                         && AssetLibrary.count > 0
                width: parent.width
                height: parent.height - Theme.panelHeaderHeight
                contentHeight: assetsContent.gridMode ? grid.height + Theme.spacing3xl
                                                      : listColumn.height + Theme.spacing3xl
                clip: true
                ScrollBar.vertical: AppScrollBar { }

                Grid {
                    id: grid
                    visible: assetsContent.gridMode
                    x: Theme.pagePadding
                    y: Theme.pagePadding
                    width: flick.width - Theme.pagePadding * 2
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

                            // Grid cards had neither hover feedback nor a pointing
                            // cursor, while the list rows had both.
                            HoverHandler {
                                id: cardHover
                                cursorShape: Qt.PointingHandCursor
                            }

                            ThemedToolTip {
                                text: name
                                visible: cardHover.hovered
                            }

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
                                border.width: cardHover.hovered ? Theme.borderWidth : 0
                                border.color: Theme.primary
                                scale: cardHover.hovered ? 1.03 : 1.0

                                Behavior on scale {
                                    NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                                }
                                Behavior on border.width {
                                    NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                                }

                                // Placeholder while the thumbnail decodes. The
                                // card used to sit empty, indistinguishable from
                                // a failed load.
                                SkeletonBox {
                                    anchors.fill: parent
                                    radius: parent.radius
                                    visible: thumbnailPath.length > 0
                                             && gridThumb.status === Image.Loading
                                }

                                Image {
                                    id: gridThumb
                                    anchors.fill: parent
                                    visible: thumbnailPath.length > 0 && status === Image.Ready
                                    source: thumbnailPath.length > 0 ? EditorState.imageUrl(thumbnailPath) : ""
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                    // Fades in rather than popping at full opacity.
                                    opacity: status === Image.Ready ? 1 : 0

                                    Behavior on opacity {
                                        NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easing }
                                    }
                                }

                                IconGlyph {
                                    anchors.centerIn: parent
                                    // Also covers Image.Error, so a missing
                                    // thumbnail file falls back to the kind icon
                                    // instead of staying blank forever.
                                    visible: thumbnailPath.length === 0
                                             || gridThumb.status === Image.Error
                                    glyph: kind === "audio" ? Theme.icons.music
                                         : kind === "image" ? Theme.icons.image
                                         : Theme.icons.film
                                    iconSize: Theme.spacing3xl
                                    iconColor: Theme.mutedForeground
                                }

                                Rectangle {
                                    visible: duration.length > 0
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: Theme.spacingSm
                                    color: Theme.scrimStrong
                                    radius: Theme.radiusXs
                                    width: durationLabel.implicitWidth + Theme.spacingLg
                                    height: durationLabel.implicitHeight + Theme.spacingSm
                                    Text {
                                        id: durationLabel
                                        anchors.centerIn: parent
                                        text: duration
                                        color: Theme.onMedia
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
                    x: Theme.pagePadding
                    y: Theme.pagePadding
                    width: flick.width - Theme.pagePadding * 2
                    spacing: Theme.spacingMd

                    Repeater {
                        model: AssetLibrary
                        delegate: Rectangle {
                            id: listRow
                            width: listColumn.width
                            height: 48
                            radius: Theme.radiusSm
                            color: rowMouse.containsMouse ? Theme.popoverHover : Theme.panelAccent
                            visible: root.assetVisible(kind)

                            Behavior on color {
                                ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                            }

                            required property int index
                            required property string name
                            required property string kind
                            required property string duration
                            required property string thumbnailPath

                            property int assetIndex: index

                            Row {
                                id: listRowContent
                                anchors.fill: parent
                                anchors.margins: Theme.spacingLg
                                spacing: Theme.spacingLg + Theme.spacingXs

                                Rectangle {
                                    id: listThumbFrame
                                    width: 56
                                    height: 32
                                    radius: Theme.radiusSm
                                    color: Theme.panelBackground
                                    clip: true

                                    SkeletonBox {
                                        anchors.fill: parent
                                        radius: parent.radius
                                        visible: thumbnailPath.length > 0
                                                 && listThumb.status === Image.Loading
                                    }

                                    Image {
                                        id: listThumb
                                        anchors.fill: parent
                                        visible: thumbnailPath.length > 0 && status === Image.Ready
                                        source: thumbnailPath.length > 0 ? EditorState.imageUrl(thumbnailPath) : ""
                                        fillMode: Image.PreserveAspectFit
                                        // Was missing, so list thumbnails decoded
                                        // on the UI thread and stalled scrolling.
                                        asynchronous: true
                                    }

                                    IconGlyph {
                                        anchors.centerIn: parent
                                        visible: thumbnailPath.length === 0
                                                 || listThumb.status === Image.Error
                                        glyph: kind === "audio" ? Theme.icons.music : Theme.icons.film
                                        iconSize: Theme.iconSizeBase
                                        iconColor: Theme.mutedForeground
                                    }
                                }

                                Column {
                                    anchors.verticalCenter: parent.verticalCenter
                                    // Derived from the actual thumbnail width
                                    // rather than a magic constant.
                                    width: parent.width - listThumbFrame.width - listRowContent.spacing
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
                                        // Was unbounded, so it overflowed the row
                                        // at narrow panel widths.
                                        elide: Text.ElideRight
                                        width: parent.width
                                    }
                                }
                            }

                            ThemedToolTip {
                                text: name
                                visible: rowMouse.containsMouse
                            }

                            MouseArea {
                                id: rowMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.addAssetToTimeline(assetIndex)
                            }
                        }
                    }
                }
            }
        }
    }

    ColorDialog {
        id: canvasColorDialog
        title: qsTr("Select Canvas Color")

        function colorToHex(c) {
            var toHex = function(v) {
                var h = Math.round(v * 255).toString(16);
                return h.length === 1 ? "0" + h : h;
            }
            return "#" + toHex(c.a) + toHex(c.r) + toHex(c.g) + toHex(c.b);
        }

        onAccepted: {
            EditorState.setBackground({ kind: "color", color: colorToHex(selectedColor) })
        }
    }
}
