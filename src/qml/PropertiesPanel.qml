import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Window
import Drift
import "components"

PanelFrame {
    id: root

    // Raised by the Effects tab's empty state; Main wires it to the assets
    // panel so "Browse effects" actually takes the user somewhere.
    signal browseEffectsRequested()

    // Human label for a clip kind. The raw id was shown to the user.
    function clipKindLabel(kind) {
        switch (kind) {
        case "video": return qsTr("Video")
        case "audio": return qsTr("Audio")
        case "image": return qsTr("Image")
        case "text": return qsTr("Text")
        case "subtitle": return qsTr("Subtitle")
        case "shape": return qsTr("Shape")
        case "sticker": return qsTr("Sticker")
        }
        return kind.length > 0 ? kind : "—"
    }

    // selectedClipData is a QVariantMap; key the binding on an explicit revision
    // so nested fields such as effects refresh after project edits.
    property int clipDataRevision: 0
    property int transitionDataRevision: 0
    property bool suppressTransitionKindUpdate: false
    property int previousTab: 0
    readonly property var clip: {
        void clipDataRevision
        return EditorState.selectedClipData
    }
    readonly property bool hasSelection: !!clip && Object.keys(clip).length > 0
    readonly property int canvasW: {
        void EditorState.tracks
        return Math.max(1, EditorState.projectWidth())
    }
    readonly property int canvasH: {
        void EditorState.tracks
        return Math.max(1, EditorState.projectHeight())
    }
    readonly property var transition: EditorState.selectedTransitionData
    readonly property bool hasTransitionSelection: !!transition && Object.keys(transition).length > 0
    readonly property int transitionEditTrack: EditorState.selectedTransitionTrack >= 0
                                                 ? EditorState.selectedTransitionTrack
                                                 : EditorState.selectedTrack
    readonly property int transitionEditLeftClip: EditorState.selectedTransitionLeftClip >= 0
                                                    ? EditorState.selectedTransitionLeftClip
                                                    : EditorState.selectedClip
    readonly property var activeTransition: {
        void transitionDataRevision
        const selected = EditorState.selectedTransitionData
        if (selected && Object.keys(selected).length > 0)
            return selected
        return EditorState.transitionBetweenClips(
                   root.transitionEditTrack, root.transitionEditLeftClip)
    }
    readonly property bool hasActiveTransition: !!activeTransition && Object.keys(activeTransition).length > 0
    readonly property bool canAddOutgoingTransition: {
        if (!root.hasSelection)
            return false
        const tracks = EditorState.tracks
        const t = EditorState.selectedTrack
        const c = EditorState.selectedClip
        if (t < 0 || !tracks || t >= tracks.length)
            return false
        const track = tracks[t]
        if (track.type !== "video" && track.type !== "shape")
            return false
        if (c < 0 || c >= track.clips.length)
            return false
        const left = track.clips[c]
        for (let i = 0; i < track.clips.length; i++) {
            if (i === c)
                continue
            const right = track.clips[i]
            if (right.start < left.start)
                continue
            const gap = right.start - (left.start + left.duration)
            if (gap <= 0.001)
                return true
        }
        return false
    }
    readonly property int transitionTabIndex: 5
    readonly property var selectedEffects: EditorState.selectedClipEffects
    readonly property string clipKind: hasSelection ? (clip.kind || "") : ""
    readonly property bool hasTextStyle: hasSelection
                                         && (clipKind === "text" || clipKind === "subtitle")
                                         && !!clip.textStyle
    readonly property var textStyle: hasTextStyle ? clip.textStyle : ({
                                                                       "fontFamily": "Inter",
                                                                       "pixelSize": 64,
                                                                       "fontWeight": 700,
                                                                       "italic": false,
                                                                       "color": "#ffffffff",
                                                                       "align": "center",
                                                                       "valign": "middle",
                                                                       "wordWrap": true,
                                                                       "lineHeight": 1.2,
                                                                       "letterSpacing": 0,
                                                                       "outlineWidth": 0,
                                                                       "outlineColor": "#ff000000",
                                                                       "shadowEnabled": false,
                                                                       "shadowOffsetX": 0,
                                                                       "shadowOffsetY": 4,
                                                                       "shadowBlur": 8,
                                                                       "shadowOpacity": 0.6,
                                                                       "shadowColor": "#ff000000",
                                                                       "boxEnabled": false,
                                                                       "boxColor": "#80000000",
                                                                       "boxPadding": 8,
                                                                       "boxRadius": 0,
                                                                       "animIn": { "kind": "none", "duration": 0.4, "ease": "easeOut" },
                                                                       "animOut": { "kind": "none", "duration": 0.4, "ease": "easeOut" }
                                                                   })

    // The selected family's real weight ladder — never an invented one. Single-weight display faces
    // (Anton, Bebas Neue, Pacifico...) expose exactly one entry and no italic.
    readonly property var fontFamilyInfo: {
        void clipDataRevision
        const catalog = EditorState.fontCatalog()
        for (let i = 0; i < catalog.length; ++i) {
            if (catalog[i].family === root.textStyle.fontFamily)
                return catalog[i]
        }
        return null
    }
    readonly property var availableWeights: fontFamilyInfo ? fontFamilyInfo.weights
                                                           : [100, 200, 300, 400, 500, 600, 700, 800, 900]
    readonly property bool familyHasItalic: fontFamilyInfo ? fontFamilyInfo.hasItalic : true

    readonly property var weightLabels: ({
                                             100: "Thin", 200: "ExtraLight", 300: "Light",
                                             400: "Regular", 500: "Medium", 600: "SemiBold",
                                             700: "Bold", 800: "ExtraBold", 900: "Black"
                                         })
    readonly property var animKinds: ["none", "fade", "slideUp", "slideDown", "slideLeft", "slideRight", "pop", "blur"]
    readonly property var animKindLabels: ["None", "Fade", "Slide up", "Slide down", "Slide left", "Slide right", "Pop", "Blur"]
    readonly property var easeKinds: ["linear", "easeOut", "easeInOut", "back"]
    readonly property var easeLabels: ["Linear", "Ease out", "Ease in-out", "Back"]

    function setTextStyleKey(key, value) {
        const patch = {}
        patch[key] = value
        EditorState.setTextStyle(EditorState.selectedTrack, EditorState.selectedClip, patch)
    }

    function setTextAnim(which, key, value) {
        const anim = {}
        anim[key] = value
        const patch = {}
        patch[which] = anim
        EditorState.setTextStyle(EditorState.selectedTrack, EditorState.selectedClip, patch)
    }
    property int activeTab: 0
    readonly property string currentTabId: tabsModel.get(activeTab).tabId

    onActiveTabChanged: {
        if (tabsModel.get(root.previousTab).tabId === "transition")
            root.commitTransitionEdits()
        root.previousTab = activeTab
        root.refreshTransitionFields()
    }

    // Keep inspector fields synced when selection/project changes without fighting active edits.
    function formatSeconds(value) {
        return Number(value || 0).toFixed(2)
    }

    function refreshInspectorFields() {
        if (!root.hasSelection)
            return
        if (startField && !startField.activeFocus)
            startField.value = root.clip.start
        if (durationField && !durationField.activeFocus)
            durationField.value = root.clip.duration
        if (inPointField && !inPointField.activeFocus)
            inPointField.value = root.clip.inPoint
        if (outPointField && !outPointField.activeFocus)
            outPointField.value = root.clip.outPoint
        if (textContentField && !textContentField.activeFocus)
            textContentField.text = root.clip.textContent || ""
        if (blendModeBox)
            blendModeBox.currentIndex = Math.max(0, blendModeBox.model.indexOf(root.clip.blendMode || "normal"))
        if (root.hasTextStyle) {
            const s = root.textStyle
            if (pixelSizeField && !pixelSizeField.activeFocus)
                pixelSizeField.value = s.pixelSize
            if (textColorField && !textColorField.activeFocus)
                textColorField.text = s.color
            if (lineHeightField && !lineHeightField.activeFocus)
                lineHeightField.value = s.lineHeight
            if (letterSpacingField && !letterSpacingField.activeFocus)
                letterSpacingField.value = s.letterSpacing
            if (outlineWidthField && !outlineWidthField.activeFocus)
                outlineWidthField.value = s.outlineWidth
            if (outlineColorField && !outlineColorField.activeFocus)
                outlineColorField.text = s.outlineColor
            if (shadowOffsetXField && !shadowOffsetXField.activeFocus)
                shadowOffsetXField.value = s.shadowOffsetX
            if (shadowOffsetYField && !shadowOffsetYField.activeFocus)
                shadowOffsetYField.value = s.shadowOffsetY
            if (shadowBlurField && !shadowBlurField.activeFocus)
                shadowBlurField.value = s.shadowBlur
            if (shadowOpacityField && !shadowOpacityField.activeFocus)
                shadowOpacityField.value = s.shadowOpacity
            if (shadowColorField && !shadowColorField.activeFocus)
                shadowColorField.text = s.shadowColor
            if (boxColorField && !boxColorField.activeFocus)
                boxColorField.text = s.boxColor
            if (boxPaddingField && !boxPaddingField.activeFocus)
                boxPaddingField.value = s.boxPadding
            if (boxRadiusField && !boxRadiusField.activeFocus)
                boxRadiusField.value = s.boxRadius
            if (animInDurationField && !animInDurationField.activeFocus)
                animInDurationField.value = s.animIn.duration
            if (animOutDurationField && !animOutDurationField.activeFocus)
                animOutDurationField.value = s.animOut.duration
        }
    }

    function refreshTransitionFields() {
        if (!root.hasActiveTransition)
            return
        if (transitionDurationField && !transitionDurationField.activeFocus)
            transitionDurationField.value = root.activeTransition.duration || 0.5
        if (transitionKindBox) {
            const kinds = EditorState.transitionKinds()
            const active = root.activeTransition.kind || "crossfade"
            let idx = 0
            for (let i = 0; i < kinds.length; ++i) {
                if (kinds[i].kind === active) {
                    idx = i
                    break
                }
            }
            root.suppressTransitionKindUpdate = true
            transitionKindBox.currentIndex = idx
            root.suppressTransitionKindUpdate = false
        }
    }

    function commitTransitionEdits() {
        if (!root.hasActiveTransition)
            return
        const transitionId = root.activeTransition.id
        if (!transitionId)
            return
        if (transitionDurationField) {
            const v = transitionDurationField.value
            const current = Number(root.activeTransition.duration || 0.5)
            if (Math.abs(v - current) > 0.0001)
                EditorState.setTransitionDuration(
                    root.transitionEditTrack, transitionId, v)
        }
        if (transitionKindBox && transitionKindBox.currentIndex >= 0) {
            const kinds = EditorState.transitionKinds()
            const item = kinds[transitionKindBox.currentIndex]
            if (item && item.kind !== (root.activeTransition.kind || "crossfade"))
                EditorState.setTransitionKind(
                    root.transitionEditTrack, transitionId, item.kind)
        }
    }

    Connections {
        target: EditorState
        function onSelectionChanged() {
            root.clipDataRevision++
            root.transitionDataRevision++
            root.refreshInspectorFields()
            root.refreshTransitionFields()
            root.syncSubtitlesTab()
        }
        function onSelectedClipDataChanged() {
            root.clipDataRevision++
            root.refreshInspectorFields()
            root.syncSubtitlesTab()
        }
        function onSelectedTransitionDataChanged() {
            root.transitionDataRevision++
            if (root.hasTransitionSelection)
                root.activeTab = root.transitionTabIndex
            root.refreshTransitionFields()
        }
        function onTracksChanged() {
            root.clipDataRevision++
            root.transitionDataRevision++
            root.refreshInspectorFields()
            root.refreshTransitionFields()
        }
    }

    Component.onCompleted: {
        root.refreshInspectorFields()
        root.refreshTransitionFields()
        root.syncSubtitlesTab()
    }

    ListModel {
        id: tabsModel
        ListElement { tabId: "general"; icon: 0; label: "General" }
        ListElement { tabId: "transform"; icon: 1; label: "Transform" }
        ListElement { tabId: "audio"; icon: 2; label: "Audio" }
        ListElement { tabId: "speed"; icon: 3; label: "Speed & Fade" }
        ListElement { tabId: "blending"; icon: 4; label: "Blending" }
        ListElement { tabId: "transition"; icon: 5; label: "Transition" }
        ListElement { tabId: "masks"; icon: 6; label: "Masks" }
        ListElement { tabId: "effects"; icon: 7; label: "Effects" }
        ListElement { tabId: "subtitles"; icon: 8; label: "Subtitles" }
    }
    property var tabIcons: [
        Theme.icons.info,
        Theme.icons.maximize,
        Theme.icons.headphones,
        Theme.icons.gauge,
        Theme.icons.layers,
        Theme.icons.blend,
        Theme.icons.mask,
        Theme.icons.wand,
        Theme.icons.messageSquare
    ]
    readonly property int subtitlesTabIndex: {
        for (let i = 0; i < tabsModel.count; i++) {
            if (tabsModel.get(i).tabId === "subtitles")
                return i
        }
        return -1
    }

    function syncSubtitlesTab() {
        if (root.subtitlesTabIndex < 0)
            return
        if (root.clipKind === "subtitle")
            root.activeTab = root.subtitlesTabIndex
        else if (root.activeTab === root.subtitlesTabIndex)
            root.activeTab = 0
    }

    // Tell the timeline to show its subtitle-cue lane only while the Subtitles tab is open.
    Binding {
        target: EditorState
        property: "subtitleEditing"
        value: root.currentTabId === "subtitles" && root.clipKind === "subtitle"
    }

    Column {
        anchors.centerIn: parent
        width: Math.min(260, root.width - 32)
        visible: !root.hasSelection
        spacing: 16

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 48
            height: 48
            radius: Theme.radiusMd
            color: "transparent"
            border.width: 1
            border.color: Theme.panelBorder

            IconGlyph {
                anchors.centerIn: parent
                glyph: Theme.icons.sliders
                iconSize: 22
                iconColor: Theme.mutedForeground
            }
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("It's empty here")
            color: Theme.panelForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBase
            font.weight: Font.Medium
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Click a clip on the timeline to edit its properties")
            color: Theme.mutedForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
        }
    }

    Item {
        id: content
        anchors.fill: parent
        visible: root.hasSelection

        // === tab rail + tab content, similar UX to AssetsPanel =========================
        Row {
            id: tabsRow
            anchors.fill: parent
            spacing: 0

            // Up/Down move between tabs once the rail has focus.
            Column {
                id: propertiesTabRail
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
                        visible: model.tabId !== "subtitles" || root.clipKind === "subtitle"
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

            Flickable {
                id: tabFlick
                width: parent.width - Theme.tabRailWidth - Theme.borderWidth
                height: parent.height
                visible: root.currentTabId !== "subtitles"
                contentWidth: width
                contentHeight: tabColumn.height + Theme.spacing3xl
                clip: true
                ScrollBar.vertical: AppScrollBar { }

                Column {
                    id: tabColumn
                    x: Theme.pagePadding
                    y: Theme.pagePadding
                    width: parent.width - Theme.pagePadding * 2
                    spacing: Theme.spacingXl

                    Text {
                        text: tabsModel.get(root.activeTab).label
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        font.weight: Font.Medium
                    }

                    // ----- General -----------------------------------------------------------
                    Column {
                        width: tabColumn.width
                        spacing: Theme.spacingXl
                        visible: root.currentTabId === "general"

                        Text {
                            text: clip.name || "Untitled clip"
                            color: Theme.panelForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeBase
                            font.weight: Font.Medium
                            width: parent.width
                            elide: Text.ElideRight
                        }

                        Column {
                            width: tabColumn.width
                            spacing: 4
                            Text {
                                text: qsTr("Kind")
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }
                            Text {
                                // Human label rather than the raw internal id.
                                text: root.clipKindLabel(root.clipKind)
                                color: Theme.panelForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                                elide: Text.ElideRight
                                width: parent.width - x
                            }
                        }

                        Row {
                            width: parent.width
                            spacing: 8

                            Column {
                                width: (parent.width - parent.spacing) / 2
                                spacing: 4
                                Text {
                                    text: qsTr("Start (s)")
                                    color: Theme.mutedForeground
                                    font.pixelSize: Theme.fontSizeXs
                                    font.family: Theme.fontFamily
                                }
                                ThemedNumberField {
                                    id: startField
                                    to: 86400
                                    unit: "s"
                                    width: parent.width
                                    decimals: 2
                                    step: 0.1
                                    from: 0
                                    onEdited: v => EditorState.setClipStart(
                                                      EditorState.selectedTrack, EditorState.selectedClip, v)
                                }
                            }

                            Column {
                                width: (parent.width - parent.spacing) / 2
                                spacing: 4
                                Text {
                                    text: qsTr("Duration (s)")
                                    color: Theme.mutedForeground
                                    font.pixelSize: Theme.fontSizeXs
                                    font.family: Theme.fontFamily
                                }
                                ThemedNumberField {
                                    id: durationField
                                    to: 86400
                                    unit: "s"
                                    width: parent.width
                                    decimals: 2
                                    step: 0.1
                                    from: 0.1
                                    onEdited: v => EditorState.setClipDuration(
                                                        EditorState.selectedTrack, EditorState.selectedClip, v)
                                }
                            }
                        }

                        Column {
                            width: tabColumn.width
                            spacing: 4
                            visible: root.clipKind === "text"
                            Text {
                                text: qsTr("Text content")
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }
                            ThemedTextArea {
                                width: parent.width
                                height: 80
                                text: clip.textContent || ""
                                onEditingFinished: EditorState.setClipTextContent(
                                                       EditorState.selectedTrack, EditorState.selectedClip, text)
                            }
                        }

                        Column {
                            width: tabColumn.width
                            spacing: 8
                            visible: root.hasTextStyle

                            Text {
                                text: qsTr("Presets")
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }

                            Flow {
                                width: parent.width
                                spacing: 6
                                Repeater {
                                    model: EditorState.textPresets()
                                    delegate: ThemedChip {
                                        required property var modelData
                                        text: modelData.label
                                        variant: "outline"
                                        onClicked: EditorState.applyTextPreset(
                                                       EditorState.selectedTrack, EditorState.selectedClip,
                                                       modelData.id)
                                    }
                                }
                            }

                            Text {
                                text: qsTr("Font")
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }

                            FontPicker {
                                width: parent.width
                                family: root.textStyle.fontFamily
                                onFamilyPicked: family => root.setTextStyleKey("fontFamily", family)
                            }

                            Row {
                                width: parent.width
                                spacing: 8

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: qsTr("Weight")
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedComboBox {
                                        id: fontWeightBox
                                        width: parent.width
                                        // Only the weights this family actually ships.
                                        model: root.availableWeights.map(
                                                   w => root.weightLabels[w] || String(w))
                                        currentIndex: Math.max(0, root.availableWeights.indexOf(root.textStyle.fontWeight))
                                        onActivated: root.setTextStyleKey("fontWeight", root.availableWeights[currentIndex])
                                    }
                                }

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: qsTr("Size")
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedNumberField {
                                        id: pixelSizeField
                                        unit: "px"
                                        width: parent.width
                                        decimals: 0
                                        step: 1
                                        from: 1
                                        to: 500
                                        onEdited: v => root.setTextStyleKey("pixelSize", v)
                                    }
                                }
                            }

                            Row {
                                width: parent.width
                                spacing: 8

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: qsTr("Color")
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    Row {
                                        spacing: 6
                                        Rectangle {
                                            width: Theme.spacing3xl
                                            height: Theme.spacing3xl
                                            radius: Theme.radiusSm
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: root.textStyle.color
                                            border.width: swatch_color.containsMouse ? Theme.borderWidthFocus : Theme.borderWidth
                                            border.color: swatch_color.containsMouse ? Theme.primary : Theme.panelBorder

                                            Behavior on border.color {
                                                ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                                            }

                                            ThemedToolTip {
                                                text: qsTr("Choose text colour")
                                                visible: swatch_color.containsMouse
                                            }

                                            MouseArea {
                                                id: swatch_color
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    styleColorDialog.targetStyleKey = "color"
                                                    styleColorDialog.selectedColor = root.textStyle.color
                                                    styleColorDialog.open()
                                                }
                                            }
                                        }
                                        ThemedTextField {
                                            id: textColorField
                                            width: 92
                                            text: root.textStyle.color
                                            color: Theme.panelForeground
                                            font.family: Theme.monoFontFamily
                                            font.pixelSize: Theme.fontSizeSm
                                            // Rejects malformed input instead of silently applying a typo.
                                            readonly property bool validHex:
                                                /^#([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$/.test(text)
                                            errorText: validHex || text.length === 0 ? "" : qsTr("Use #RRGGBB or #AARRGGBB")
                                            onEditingFinished: if (validHex) root.setTextStyleKey("color", text)
                                        }
                                    }
                                }

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: qsTr("Style")
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedToggleButton {
                                        width: 60
                                        text: qsTr("Italic")
                                        checked: root.textStyle.italic
                                        // Single-face display fonts have no italic.
                                        enabled: root.familyHasItalic
                                        tooltip: root.familyHasItalic
                                                 ? qsTr("Italicise the text")
                                                 : qsTr("%1 has no italic face").arg(root.textStyle.fontFamily)
                                        onClicked: root.setTextStyleKey("italic", !root.textStyle.italic)
                                    }
                                }
                            }

                            // Alignment groups. These were two visually identical
                            // rows of L/C/R and T/M/B boxes, distinguishable only
                            // by their tooltips; they now carry real icons and are
                            // keyboard-operable.
                            Row {
                                width: parent.width
                                spacing: Theme.spacingMd

                                Repeater {
                                    model: [
                                        { value: "left",   glyph: Theme.icons.alignLeft,   label: qsTr("Align left") },
                                        { value: "center", glyph: Theme.icons.alignCenter, label: qsTr("Align centre") },
                                        { value: "right",  glyph: Theme.icons.alignRight,  label: qsTr("Align right") }
                                    ]
                                    delegate: ThemedToggleButton {
                                        required property var modelData
                                        width: 34
                                        glyph: modelData.glyph
                                        checked: root.textStyle.align === modelData.value
                                        tooltip: modelData.label
                                        onClicked: root.setTextStyleKey("align", modelData.value)
                                    }
                                }

                                Rectangle {
                                    width: Theme.borderWidth
                                    height: Theme.controlHeightSm
                                    color: Theme.panelBorder
                                }

                                Repeater {
                                    model: [
                                        { value: "top",    glyph: Theme.icons.alignTop,    label: qsTr("Align top") },
                                        { value: "middle", glyph: Theme.icons.alignMiddle, label: qsTr("Align middle") },
                                        { value: "bottom", glyph: Theme.icons.alignBottom, label: qsTr("Align bottom") }
                                    ]
                                    delegate: ThemedToggleButton {
                                        required property var modelData
                                        width: 34
                                        glyph: modelData.glyph
                                        checked: root.textStyle.valign === modelData.value
                                        tooltip: modelData.label
                                        onClicked: root.setTextStyleKey("valign", modelData.value)
                                    }
                                }
                            }

                            Row {
                                width: parent.width
                                spacing: 8

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: qsTr("Line height")
                                        HoverHandler { id: tipHover761 }
                                        ThemedToolTip { text: qsTr("Vertical spacing between lines, as a multiple of the font size"); visible: tipHover761.hovered }
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedNumberField {
                                        id: lineHeightField
                                        width: parent.width
                                        decimals: 2
                                        step: 0.05
                                        from: 0.5
                                        to: 4
                                        onEdited: v => root.setTextStyleKey("lineHeight", v)
                                    }
                                }

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: qsTr("Letter spacing")
                                        HoverHandler { id: tipHover781 }
                                        ThemedToolTip { text: qsTr("Extra space between characters, in pixels"); visible: tipHover781.hovered }
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedNumberField {
                                        id: letterSpacingField
                                        to: 200
                                        from: -100
                                        unit: "px"
                                        width: parent.width
                                        decimals: 1
                                        step: 0.5
                                        onEdited: v => root.setTextStyleKey("letterSpacing", v)
                                    }
                                }
                            }

                            ThemedToggleButton {
                                width: 96
                                text: qsTr("Word wrap")
                                checked: root.textStyle.wordWrap
                                tooltip: qsTr("Wrap long lines inside the text box instead of overflowing")
                                onClicked: root.setTextStyleKey("wordWrap", !root.textStyle.wordWrap)
                            }

                            Text {
                                text: qsTr("Outline")
                                HoverHandler { id: tipHover808 }
                                ThemedToolTip { text: qsTr("Stroke drawn around each glyph"); visible: tipHover808.hovered }
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }

                            Row {
                                width: parent.width
                                spacing: 8

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: qsTr("Width")
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedNumberField {
                                        id: outlineWidthField
                                        to: 100
                                        unit: "px"
                                        width: parent.width
                                        decimals: 1
                                        step: 0.5
                                        from: 0
                                        onEdited: v => root.setTextStyleKey("outlineWidth", v)
                                    }
                                }

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: qsTr("Color")
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    Row {
                                        spacing: 6
                                        Rectangle {
                                            width: Theme.spacing3xl
                                            height: Theme.spacing3xl
                                            radius: Theme.radiusSm
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: root.textStyle.outlineColor
                                            border.width: swatch_outlineColor.containsMouse ? Theme.borderWidthFocus : Theme.borderWidth
                                            border.color: swatch_outlineColor.containsMouse ? Theme.primary : Theme.panelBorder

                                            Behavior on border.color {
                                                ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                                            }

                                            ThemedToolTip {
                                                text: qsTr("Choose outline colour")
                                                visible: swatch_outlineColor.containsMouse
                                            }

                                            MouseArea {
                                                id: swatch_outlineColor
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    styleColorDialog.targetStyleKey = "outlineColor"
                                                    styleColorDialog.selectedColor = root.textStyle.outlineColor
                                                    styleColorDialog.open()
                                                }
                                            }
                                        }
                                        ThemedTextField {
                                            id: outlineColorField
                                            width: 92
                                            text: root.textStyle.outlineColor
                                            color: Theme.panelForeground
                                            font.family: Theme.monoFontFamily
                                            font.pixelSize: Theme.fontSizeSm
                                            // Rejects malformed input instead of silently applying a typo.
                                            readonly property bool validHex:
                                                /^#([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$/.test(text)
                                            errorText: validHex || text.length === 0 ? "" : qsTr("Use #RRGGBB or #AARRGGBB")
                                            onEditingFinished: if (validHex) root.setTextStyleKey("outlineColor", text)
                                        }
                                    }
                                }
                            }

                            ThemedToggleButton {
                                width: 96
                                text: qsTr("Shadow")
                                checked: root.textStyle.shadowEnabled
                                tooltip: qsTr("Draw a drop shadow behind the text")
                                onClicked: root.setTextStyleKey("shadowEnabled", !root.textStyle.shadowEnabled)
                            }

                            Row {
                                width: parent.width
                                spacing: 8
                                visible: root.textStyle.shadowEnabled

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: qsTr("Offset X")
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedNumberField {
                                        id: shadowOffsetXField
                                        to: 500
                                        from: -500
                                        unit: "px"
                                        width: parent.width
                                        decimals: 1
                                        step: 1
                                        onEdited: v => root.setTextStyleKey("shadowOffsetX", v)
                                    }
                                }

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: qsTr("Offset Y")
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedNumberField {
                                        id: shadowOffsetYField
                                        to: 500
                                        from: -500
                                        unit: "px"
                                        width: parent.width
                                        decimals: 1
                                        step: 1
                                        onEdited: v => root.setTextStyleKey("shadowOffsetY", v)
                                    }
                                }
                            }

                            Row {
                                width: parent.width
                                spacing: 8
                                visible: root.textStyle.shadowEnabled

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: qsTr("Blur")
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedNumberField {
                                        id: shadowBlurField
                                        to: 100
                                        unit: "px"
                                        width: parent.width
                                        decimals: 1
                                        step: 1
                                        from: 0
                                        onEdited: v => root.setTextStyleKey("shadowBlur", v)
                                    }
                                }

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: qsTr("Opacity")
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedNumberField {
                                        id: shadowOpacityField
                                        width: parent.width
                                        decimals: 2
                                        step: 0.05
                                        from: 0
                                        to: 1
                                        onEdited: v => root.setTextStyleKey("shadowOpacity", v)
                                    }
                                }
                            }

                            Row {
                                width: parent.width
                                spacing: 6
                                visible: root.textStyle.shadowEnabled

                                Text {
                                    text: qsTr("Shadow color")
                                    color: Theme.mutedForeground
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Rectangle {
                                    width: Theme.spacing3xl
                                    height: Theme.spacing3xl
                                    radius: Theme.radiusSm
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: root.textStyle.shadowColor
                                    border.width: swatch_shadowColor.containsMouse ? Theme.borderWidthFocus : Theme.borderWidth
                                    border.color: swatch_shadowColor.containsMouse ? Theme.primary : Theme.panelBorder

                                    Behavior on border.color {
                                        ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                                    }

                                    ThemedToolTip {
                                        text: qsTr("Choose shadow colour")
                                        visible: swatch_shadowColor.containsMouse
                                    }

                                    MouseArea {
                                        id: swatch_shadowColor
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            styleColorDialog.targetStyleKey = "shadowColor"
                                            styleColorDialog.selectedColor = root.textStyle.shadowColor
                                            styleColorDialog.open()
                                        }
                                    }
                                }
                                ThemedTextField {
                                    id: shadowColorField
                                    width: 92
                                    text: root.textStyle.shadowColor
                                    color: Theme.panelForeground
                                    font.family: Theme.monoFontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    // Rejects malformed input instead of silently applying a typo.
                                    readonly property bool validHex:
                                        /^#([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$/.test(text)
                                    errorText: validHex || text.length === 0 ? "" : qsTr("Use #RRGGBB or #AARRGGBB")
                                    onEditingFinished: if (validHex) root.setTextStyleKey("shadowColor", text)
                                }
                            }

                            ThemedToggleButton {
                                width: 96
                                text: qsTr("Background")
                                checked: root.textStyle.boxEnabled
                                tooltip: qsTr("Draw a filled box behind the text")
                                onClicked: root.setTextStyleKey("boxEnabled", !root.textStyle.boxEnabled)
                            }

                            Row {
                                width: parent.width
                                spacing: 6
                                visible: root.textStyle.boxEnabled

                                Rectangle {
                                    width: Theme.spacing3xl
                                    height: Theme.spacing3xl
                                    radius: Theme.radiusSm
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: root.textStyle.boxColor
                                    border.width: swatch_boxColor.containsMouse ? Theme.borderWidthFocus : Theme.borderWidth
                                    border.color: swatch_boxColor.containsMouse ? Theme.primary : Theme.panelBorder

                                    Behavior on border.color {
                                        ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                                    }

                                    ThemedToolTip {
                                        text: qsTr("Choose background colour")
                                        visible: swatch_boxColor.containsMouse
                                    }

                                    MouseArea {
                                        id: swatch_boxColor
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            styleColorDialog.targetStyleKey = "boxColor"
                                            styleColorDialog.selectedColor = root.textStyle.boxColor
                                            styleColorDialog.open()
                                        }
                                    }
                                }
                                ThemedTextField {
                                    id: boxColorField
                                    width: 92
                                    text: root.textStyle.boxColor
                                    color: Theme.panelForeground
                                    font.family: Theme.monoFontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    // Rejects malformed input instead of silently applying a typo.
                                    readonly property bool validHex:
                                        /^#([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$/.test(text)
                                    errorText: validHex || text.length === 0 ? "" : qsTr("Use #RRGGBB or #AARRGGBB")
                                    onEditingFinished: if (validHex) root.setTextStyleKey("boxColor", text)
                                }
                            }

                            Row {
                                width: parent.width
                                spacing: 8
                                visible: root.textStyle.boxEnabled

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: qsTr("Padding")
                                        HoverHandler { id: tipHover1088 }
                                        ThemedToolTip { text: qsTr("Space between the text and the edge of its background box"); visible: tipHover1088.hovered }
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedNumberField {
                                        id: boxPaddingField
                                        to: 500
                                        unit: "px"
                                        width: parent.width
                                        decimals: 1
                                        step: 1
                                        from: 0
                                        onEdited: v => root.setTextStyleKey("boxPadding", v)
                                    }
                                }

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: qsTr("Corner radius")
                                        HoverHandler { id: tipHover1109 }
                                        ThemedToolTip { text: qsTr("Roundness of the background box corners"); visible: tipHover1109.hovered }
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedNumberField {
                                        id: boxRadiusField
                                        to: 500
                                        unit: "px"
                                        width: parent.width
                                        decimals: 1
                                        step: 1
                                        from: 0
                                        onEdited: v => root.setTextStyleKey("boxRadius", v)
                                    }
                                }
                            }

                            Text {
                                text: qsTr("Animation")
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }

                            Row {
                                width: parent.width
                                spacing: 6

                                Text {
                                    text: qsTr("In")
                                    width: 20
                                    color: Theme.mutedForeground
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                ThemedComboBox {
                                    id: animInKindBox
                                    width: Math.max(40, (parent.width - 20 - parent.spacing * 3 - 56) / 2)
                                    model: root.animKindLabels
                                    currentIndex: Math.max(0, root.animKinds.indexOf(root.textStyle.animIn.kind))
                                    onActivated: root.setTextAnim("animIn", "kind", root.animKinds[currentIndex])
                                }
                                ThemedComboBox {
                                    id: animInEaseBox
                                    width: Math.max(40, (parent.width - 20 - parent.spacing * 3 - 56) / 2)
                                    model: root.easeLabels
                                    currentIndex: Math.max(0, root.easeKinds.indexOf(root.textStyle.animIn.ease))
                                    onActivated: root.setTextAnim("animIn", "ease", root.easeKinds[currentIndex])
                                }
                                ThemedNumberField {
                                    id: animInDurationField
                                    to: 60
                                    unit: "s"
                                    width: 56
                                    decimals: 2
                                    step: 0.05
                                    from: 0
                                    onEdited: v => root.setTextAnim("animIn", "duration", v)
                                }
                            }

                            Row {
                                width: parent.width
                                spacing: 6

                                Text {
                                    text: qsTr("Out")
                                    width: 20
                                    color: Theme.mutedForeground
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                ThemedComboBox {
                                    id: animOutKindBox
                                    width: Math.max(40, (parent.width - 20 - parent.spacing * 3 - 56) / 2)
                                    model: root.animKindLabels
                                    currentIndex: Math.max(0, root.animKinds.indexOf(root.textStyle.animOut.kind))
                                    onActivated: root.setTextAnim("animOut", "kind", root.animKinds[currentIndex])
                                }
                                ThemedComboBox {
                                    id: animOutEaseBox
                                    width: Math.max(40, (parent.width - 20 - parent.spacing * 3 - 56) / 2)
                                    model: root.easeLabels
                                    currentIndex: Math.max(0, root.easeKinds.indexOf(root.textStyle.animOut.ease))
                                    onActivated: root.setTextAnim("animOut", "ease", root.easeKinds[currentIndex])
                                }
                                ThemedNumberField {
                                    id: animOutDurationField
                                    to: 60
                                    unit: "s"
                                    width: 56
                                    decimals: 2
                                    step: 0.05
                                    from: 0
                                    onEdited: v => root.setTextAnim("animOut", "duration", v)
                                }
                            }
                        }

                        Column {
                            width: tabColumn.width
                            spacing: 8
                            visible: root.clipKind !== "text" && root.clipKind !== "subtitle"

                            Text {
                                text: qsTr("Trim")
                                HoverHandler { id: tipHover1217 }
                                ThemedToolTip { text: qsTr("Which part of the source media this clip plays"); visible: tipHover1217.hovered }
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }

                            Row {
                                width: parent.width
                                spacing: 8

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: qsTr("In point (s)")
                                        HoverHandler { id: tipHover1231 }
                                        ThemedToolTip { text: qsTr("Seconds into the source where this clip starts"); visible: tipHover1231.hovered }
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedNumberField {
                                        id: inPointField
                                        to: 86400
                                        unit: "s"
                                        width: parent.width
                                        decimals: 2
                                        step: 0.1
                                        from: 0
                                        onEdited: v => applyTrim(v, root.clip.outPoint)
                                    }
                                }

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: qsTr("Out point (s)")
                                        HoverHandler { id: tipHover1252 }
                                        ThemedToolTip { text: qsTr("Seconds into the source where this clip ends"); visible: tipHover1252.hovered }
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedNumberField {
                                        id: outPointField
                                        to: 86400
                                        unit: "s"
                                        width: parent.width
                                        decimals: 2
                                        step: 0.1
                                        from: 0
                                        onEdited: v => applyTrim(root.clip.inPoint, v)
                                    }
                                }
                            }
                        }

                        Column {
                            width: tabColumn.width
                            spacing: 4
                            visible: clip.path !== undefined && clip.path.length > 0

                            Text {
                                text: qsTr("Path")
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }

                            Text {
                                id: clipPathLabel
                                text: clip.path || "—"
                                color: Theme.panelForeground
                                font.family: Theme.monoFontFamily
                                font.pixelSize: Theme.fontSizeSm
                                width: parent.width
                                wrapMode: Text.WrapAnywhere
                                // Capped: a deep path used to wrap unbounded and
                                // dominate the whole General tab.
                                maximumLineCount: 3
                                elide: Text.ElideRight

                                HoverHandler { id: pathHover }

                                ThemedToolTip {
                                    text: clip.path || ""
                                    visible: pathHover.hovered && (clip.path || "").length > 0
                                }
                            }
                        }
                    }

                    // ----- Transform -------------------------------------------------------
                    Column {
                        width: tabColumn.width
                        spacing: Theme.spacingXl
                        visible: root.currentTabId === "transform"

                        EmptyState {
                            visible: root.clipKind === "audio"
                            width: parent.width
                            compact: true
                            glyph: Theme.icons.film
                            title: qsTr("Video only")
                            hint: qsTr("This tab does not apply to audio clips.")
                        }

                        Column {
                            width: tabColumn.width
                            spacing: 10
                            visible: root.clipKind !== "audio"

                            Text {
                                width: parent.width
                                wrapMode: Text.WordWrap
                                text: qsTr("Scrub the playhead, set a value, then click the diamond to key it. With Auto-key on, dragging a slider or the preview also writes keys.")
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }

                            ThemedChip {
                                text: qsTr("Auto-key")
                                selected: EditorState.autoKeyEnabled
                                onClicked: EditorState.autoKeyEnabled = !EditorState.autoKeyEnabled
                            }

                            Text {
                                text: qsTr("Position (px)")
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                font.weight: Font.Medium
                            }

                            PropertyKeyframeRow {
                                width: parent.width
                                propDef: root.propX
                                keyframeList: (clip.keyframes && clip.keyframes.x && clip.keyframes.x.points) || []
                                interpolationMode: (clip.keyframes && clip.keyframes.x && clip.keyframes.x.interpolation) || "linear"
                                useSlider: true
                                sliderFrom: -root.canvasW
                                sliderTo: root.canvasW * 2
                                unit: "px"
                            }
                            PropertyKeyframeRow {
                                width: parent.width
                                propDef: root.propY
                                keyframeList: (clip.keyframes && clip.keyframes.y && clip.keyframes.y.points) || []
                                interpolationMode: (clip.keyframes && clip.keyframes.y && clip.keyframes.y.interpolation) || "linear"
                                useSlider: true
                                sliderFrom: -root.canvasH
                                sliderTo: root.canvasH * 2
                                unit: "px"
                            }

                            Text {
                                text: qsTr("Size (px)")
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                font.weight: Font.Medium
                            }

                            PropertyKeyframeRow {
                                width: parent.width
                                propDef: root.propWidth
                                keyframeList: (clip.keyframes && clip.keyframes.width && clip.keyframes.width.points) || []
                                interpolationMode: (clip.keyframes && clip.keyframes.width && clip.keyframes.width.interpolation) || "linear"
                                useSlider: true
                                sliderFrom: 1
                                sliderTo: Math.max(root.canvasW * 2, 2)
                                unit: "px"
                            }
                            PropertyKeyframeRow {
                                width: parent.width
                                propDef: root.propHeight
                                keyframeList: (clip.keyframes && clip.keyframes.height && clip.keyframes.height.points) || []
                                interpolationMode: (clip.keyframes && clip.keyframes.height && clip.keyframes.height.interpolation) || "linear"
                                useSlider: true
                                sliderFrom: 1
                                sliderTo: Math.max(root.canvasH * 2, 2)
                                unit: "px"
                            }

                            Text {
                                text: qsTr("Opacity & rotation")
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                font.weight: Font.Medium
                            }

                            PropertyKeyframeRow {
                                width: parent.width
                                propDef: root.propOpacity
                                keyframeList: (clip.keyframes && clip.keyframes.opacity && clip.keyframes.opacity.points) || []
                                interpolationMode: (clip.keyframes && clip.keyframes.opacity && clip.keyframes.opacity.interpolation) || "linear"
                                useSlider: true
                                sliderFrom: 0
                                sliderTo: 1
                                percent: true
                            }

                            PropertyKeyframeRow {
                                width: parent.width
                                propDef: root.propRotation
                                keyframeList: (clip.keyframes && clip.keyframes.rotation && clip.keyframes.rotation.points) || []
                                interpolationMode: (clip.keyframes && clip.keyframes.rotation && clip.keyframes.rotation.interpolation) || "linear"
                                useSlider: true
                                sliderFrom: -180
                                sliderTo: 180
                                unit: "°"
                            }

                            Text {
                                text: qsTr("Rotate 90°")
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                font.weight: Font.Medium
                            }

                            Flow {
                                width: parent.width
                                spacing: 6
                                Repeater {
                                    // Plain ints avoid JS-object model role quirks (e.g. "value").
                                    model: [0, 90, 180, -90]
                                    delegate: ThemedChip {
                                        required property int modelData
                                        text: modelData + "°"
                                        selected: {
                                            void root.clipDataRevision
                                            void EditorState.playheadSeconds
                                            const cur = Number(clip.rotationAtPlayhead || 0)
                                            return Math.abs(cur - modelData) < 0.5
                                        }
                                        onClicked: EditorState.setClipRotationSnap(
                                                       EditorState.selectedTrack, EditorState.selectedClip,
                                                       modelData)
                                    }
                                }
                            }

                            Text {
                                text: qsTr("Flip")
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                font.weight: Font.Medium
                            }

                            Flow {
                                width: parent.width
                                spacing: 6
                                ThemedChip {
                                    text: qsTr("Flip H")
                                    selected: {
                                        void root.clipDataRevision
                                        return !!clip.flipH
                                    }
                                    onClicked: EditorState.setClipFlip(
                                                   EditorState.selectedTrack, EditorState.selectedClip,
                                                   !clip.flipH, !!clip.flipV)
                                }
                                ThemedChip {
                                    text: qsTr("Flip V")
                                    selected: {
                                        void root.clipDataRevision
                                        return !!clip.flipV
                                    }
                                    onClicked: EditorState.setClipFlip(
                                                   EditorState.selectedTrack, EditorState.selectedClip,
                                                   !!clip.flipH, !clip.flipV)
                                }
                            }

                            ThemedButton {
                                text: qsTr("Reset transform")
                                onClicked: EditorState.resetClipTransform(
                                               EditorState.selectedTrack, EditorState.selectedClip)
                            }
                        }
                    }

                    // ----- Audio -------------------------------------------------------------
                    Column {
                        width: tabColumn.width
                        spacing: Theme.spacingXl
                        visible: root.currentTabId === "audio"

                        EmptyState {
                            visible: root.clipKind !== "audio" && root.clipKind !== "video"
                            width: parent.width
                            compact: true
                            glyph: Theme.icons.volumeOff
                            title: qsTr("No audio")
                            hint: qsTr("This clip has no audio track.")
                        }

                        PropertyKeyframeRow {
                            width: tabColumn.width
                            visible: root.clipKind === "audio" || root.clipKind === "video"
                            propDef: root.propVolume
                            keyframeList: (clip.keyframes && clip.keyframes.volume && clip.keyframes.volume.points) || []
                            interpolationMode: (clip.keyframes && clip.keyframes.volume && clip.keyframes.volume.interpolation) || "linear"
                            useSlider: true
                            sliderFrom: 0
                            sliderTo: 2
                            percent: true
                        }

                        Rectangle {
                            visible: root.clipKind === "audio" || root.clipKind === "video"
                            width: parent.width
                            height: 1
                            color: Theme.panelBorder
                            opacity: 0.5
                        }

                        Text {
                            visible: root.clipKind === "audio" || root.clipKind === "video"
                            text: qsTr("Auto subtitles")
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                        }

                        // The transcriber is an addon; without it there are no languages to list
                        // and nothing to run, so offer the download in place of the controls.
                        property bool whisperReady: Addons.hasKind("whisper-model")

                        Connections {
                            target: Addons
                            function onKindChanged(kind) {
                                if (kind === "whisper-model")
                                    subtitleLanguageBox.parent.whisperReady = Addons.hasKind("whisper-model")
                            }
                        }

                        ThemedComboBox {
                            id: subtitleLanguageBox
                            visible: parent.whisperReady
                                     && (root.clipKind === "audio" || root.clipKind === "video")
                            width: parent.width
                            enabled: !EditorState.subtitleGenerating
                            textRole: "label"
                            valueRole: "code"
                            model: EditorState.whisperLanguages()
                            Component.onCompleted: currentIndex = 0
                        }

                        ThemedButton {
                            visible: parent.whisperReady
                                     && (root.clipKind === "audio" || root.clipKind === "video")
                            width: parent.width
                            text: EditorState.subtitleGenerating
                                  ? qsTr("Transcribing… %1%").arg(Math.round(EditorState.subtitleGenProgress * 100))
                                  : qsTr("Generate subtitles")
                            enabled: !EditorState.subtitleGenerating
                            onClicked: {
                                const lang = subtitleLanguageBox.currentValue !== undefined
                                             ? subtitleLanguageBox.currentValue
                                             : ""
                                EditorState.generateSubtitlesForClip(
                                    EditorState.selectedTrack, EditorState.selectedClip, lang)
                            }
                        }

                        ThemedButton {
                            visible: !parent.whisperReady
                                     && (root.clipKind === "audio" || root.clipKind === "video")
                            width: parent.width
                            text: qsTr("Install speech model (≈670 MB)")
                            variant: "primary"
                            onClicked: root.Window.window.openAddonManager("whisper-model")
                        }
                    }

                    // ----- Speed ---------------------------------------------------------------
                    Column {
                        width: tabColumn.width
                        spacing: Theme.spacingXl
                        visible: root.currentTabId === "speed"

                        EmptyState {
                            visible: root.clipKind !== "video" && root.clipKind !== "audio"
                            width: parent.width
                            compact: true
                            glyph: Theme.icons.gauge
                            title: qsTr("Not available")
                            hint: qsTr("Speed applies to video and audio clips.")
                        }

                        Text {
                            visible: root.clipKind === "video" || root.clipKind === "audio"
                            text: qsTr("Playback speed")
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                        }

                            Row {
                                width: parent.width
                                spacing: 6
                                visible: root.clipKind === "video" || root.clipKind === "audio"
                                Repeater {
                                    model: [
                                        { label: "0.25×", value: 0.25 },
                                        { label: "0.5×", value: 0.5 },
                                        { label: "1×", value: 1.0 },
                                        { label: "2×", value: 2.0 },
                                        { label: "4×", value: 4.0 }
                                    ]
                                    delegate: ThemedChip {
                                        required property var modelData
                                        text: modelData.label
                                        selected: Math.abs((clip.speed || 1) - modelData.value) < 0.01
                                        onClicked: EditorState.setClipSpeed(
                                                       EditorState.selectedTrack, EditorState.selectedClip,
                                                       modelData.value)
                                    }
                                }
                            }

                        ThemedSlider {
                            id: speedSlider
                            visible: root.clipKind === "video" || root.clipKind === "audio"
                            width: parent.width
                            from: 0.25
                            to: 4.0
                            stepSize: 0.05
                            value: clip.speed || 1.0
                            onMoved: EditorState.previewSetClipSpeed(
                                         EditorState.selectedTrack, EditorState.selectedClip, value)
                            onPressedChanged: {
                                if (pressed) {
                                    EditorState.beginPreviewDrag(qsTr("Speed changed"))
                                } else {
                                    EditorState.commitPreviewDrag()
                                    value = Qt.binding(() => clip.speed || 1.0)
                                }
                            }
                        }

                        Text {
                            visible: root.clipKind === "video" || root.clipKind === "audio"
                            text: (clip.speed || 1).toFixed(2) + "×"
                                    + (clip.reverse ? qsTr(" (reversed)") : "")
                            color: Theme.mutedForeground
                            font.family: Theme.monoFontFamily
                            font.pixelSize: Theme.fontSizeSm
                        }

                        ThemedChip {
                            visible: root.clipKind === "video" || root.clipKind === "audio"
                            text: qsTr("Reverse")
                            selected: {
                                void root.clipDataRevision
                                return !!clip.reverse
                            }
                            onClicked: EditorState.setClipReverse(
                                           EditorState.selectedTrack, EditorState.selectedClip,
                                           !clip.reverse)
                        }

                        Rectangle {
                            width: parent.width
                            height: 1
                            color: Theme.panelBorder
                            opacity: 0.5
                        }

                        Text {
                            text: root.clipKind === "audio" ? "Fade in / out (volume)" : "Fade in / out"
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                        }

                        property real fadeMax: Math.max(0.1, clip.duration || 1)

                        Row {
                            width: parent.width
                            spacing: 6

                            ThemedButton {
                                text: qsTr("Fade in")
                                variant: "secondary"
                                onClicked: EditorState.setClipFade(
                                               EditorState.selectedTrack, EditorState.selectedClip,
                                               0.5, clip.fadeOut || 0)
                            }
                            ThemedButton {
                                text: qsTr("Fade out")
                                variant: "secondary"
                                onClicked: EditorState.setClipFade(
                                               EditorState.selectedTrack, EditorState.selectedClip,
                                               clip.fadeIn || 0, 0.5)
                            }
                            ThemedButton {
                                text: qsTr("Clear")
                                variant: "ghost"
                                enabled: (clip.fadeIn || 0) > 0 || (clip.fadeOut || 0) > 0
                                onClicked: EditorState.setClipFade(
                                               EditorState.selectedTrack, EditorState.selectedClip, 0, 0)
                            }
                        }

                        Text {
                            text: "Fade in: " + (clip.fadeIn || 0).toFixed(2) + "s"
                            color: Theme.mutedForeground
                            font.family: Theme.monoFontFamily
                            font.pixelSize: Theme.fontSizeXs
                        }

                        ThemedSlider {
                            id: fadeInSlider
                            width: parent.width
                            from: 0
                            to: parent.fadeMax
                            stepSize: 0.05
                            value: clip.fadeIn || 0
                            onMoved: EditorState.previewSetClipFade(
                                         EditorState.selectedTrack, EditorState.selectedClip,
                                         value, clip.fadeOut || 0)
                            onPressedChanged: {
                                if (pressed) {
                                    EditorState.beginPreviewDrag(qsTr("Adjust fade"))
                                } else {
                                    EditorState.commitPreviewDrag()
                                    value = Qt.binding(() => clip.fadeIn || 0)
                                }
                            }
                        }

                        Text {
                            text: "Fade out: " + (clip.fadeOut || 0).toFixed(2) + "s"
                            color: Theme.mutedForeground
                            font.family: Theme.monoFontFamily
                            font.pixelSize: Theme.fontSizeXs
                        }

                        ThemedSlider {
                            id: fadeOutSlider
                            width: parent.width
                            from: 0
                            to: parent.fadeMax
                            stepSize: 0.05
                            value: clip.fadeOut || 0
                            onMoved: EditorState.previewSetClipFade(
                                         EditorState.selectedTrack, EditorState.selectedClip,
                                         clip.fadeIn || 0, value)
                            onPressedChanged: {
                                if (pressed) {
                                    EditorState.beginPreviewDrag(qsTr("Adjust fade"))
                                } else {
                                    EditorState.commitPreviewDrag()
                                    value = Qt.binding(() => clip.fadeOut || 0)
                                }
                            }
                        }

                        Text {
                            text: qsTr("Fade curve")
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                        }

                        ThemedComboBox {
                            id: fadeCurveCombo
                            width: parent.width
                            model: ["Linear", "Smooth", "Equal power"]
                            readonly property var curveIds: ["linear", "smooth", "equalPower"]
                            currentIndex: Math.max(0, curveIds.indexOf(clip.fadeCurve || "smooth"))
                            onActivated: (index) => EditorState.setClipFadeCurve(
                                             EditorState.selectedTrack, EditorState.selectedClip,
                                             curveIds[index])
                        }
                    }

                    // ----- Transition ----------------------------------------------------------
                    Column {
                        width: tabColumn.width
                        spacing: Theme.spacingXl
                        visible: root.currentTabId === "transition"

                        Text {
                            width: parent.width
                            wrapMode: Text.WordWrap
                            visible: !root.hasActiveTransition && !root.canAddOutgoingTransition
                            text: root.clipKind === "video" || root.clipKind === "shape"
                                  ? "Select a purple overlap between two clips, or drag a clip so it overlaps the next one."
                                  : "Transitions apply between two clips on a video or shape track."
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                        }

                        Column {
                            width: parent.width
                            spacing: 8
                            visible: !root.hasActiveTransition && root.canAddOutgoingTransition

                            Text {
                                width: parent.width
                                wrapMode: Text.WordWrap
                                text: qsTr("No transition after this clip. Add one at the cut to the next clip.")
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                            }

                            ThemedButton {
                                text: qsTr("Add crossfade (0.5 s)")
                                variant: "primary"
                                onClicked: EditorState.addTransition(
                                               EditorState.selectedTrack, EditorState.selectedClip,
                                               "crossfade", 0.5)
                            }
                        }

                        Column {
                            width: parent.width
                            spacing: 12
                            visible: root.hasActiveTransition

                            Text {
                                width: parent.width
                                wrapMode: Text.WordWrap
                                text: root.activeTransition.overlapping
                                      ? "Overlap transition. Drag another kind from the Transitions library to replace it."
                                      : "Outgoing transition to the next clip. Scrub the playhead across the cut to preview."
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }

                            Column {
                                width: parent.width
                                spacing: 4
                                Text {
                                    text: qsTr("Kind")
                                    color: Theme.mutedForeground
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                }
                                ThemedComboBox {
                                    id: transitionKindBox
                                    width: parent.width
                                    textRole: "label"
                                    valueRole: "kind"
                                    model: EditorState.transitionKinds()
                                    onActivated: transitionKindBox.commitTransitionKind()
                                    onCurrentIndexChanged: {
                                        if (root.suppressTransitionKindUpdate || !root.hasActiveTransition)
                                            return
                                        transitionKindBox.commitTransitionKind()
                                    }

                                    function commitTransitionKind() {
                                        const item = model[currentIndex]
                                        if (!item || !root.hasActiveTransition)
                                            return
                                        EditorState.setTransitionKind(
                                            root.transitionEditTrack,
                                            root.activeTransition.id, item.kind)
                                    }
                                }
                            }

                            Column {
                                width: parent.width
                                spacing: 4
                                Text {
                                    text: qsTr("Duration (s)")
                                    color: Theme.mutedForeground
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                }
                                ThemedNumberField {
                                    id: transitionDurationField
                                    to: 60
                                    unit: "s"
                                    width: parent.width
                                    decimals: 2
                                    step: 0.05
                                    from: 0.05
                                    onEdited: v => {
                                        if (!root.hasActiveTransition)
                                            return
                                        EditorState.setTransitionDuration(
                                            root.transitionEditTrack, root.activeTransition.id, v)
                                    }
                                }
                            }

                            // Shader parameters declared by the active transition package.
                            Repeater {
                                model: root.activeTransition.params || []
                                delegate: Column {
                                    id: trParamRow
                                    required property var modelData
                                    width: tabColumn.width
                                    spacing: 4

                                    Row {
                                        width: parent.width
                                        spacing: 8
                                        Text {
                                            width: parent.width - 48
                                            elide: Text.ElideRight
                                            text: trParamRow.modelData.label
                                            color: Theme.mutedForeground
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeXs
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        Text {
                                            width: 40
                                            horizontalAlignment: Text.AlignRight
                                            text: trParamRow.modelData.isBoolean
                                                  ? (trParamRow.modelData.value ? qsTr("On") : qsTr("Off"))
                                                  : Number(trParamSlider.value).toFixed(
                                                        Math.abs(trParamRow.modelData.max - trParamRow.modelData.min) >= 10 ? 1 : 2)
                                            color: Theme.panelForeground
                                            font.family: Theme.monoFontFamily
                                            font.pixelSize: Theme.fontSizeXs
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }

                                    ThemedSwitch {
                                        visible: !!trParamRow.modelData.isBoolean
                                        checked: !!trParamRow.modelData.value
                                        onToggled: EditorState.setTransitionParam(
                                                       root.transitionEditTrack, root.activeTransition.id,
                                                       trParamRow.modelData.key, checked ? 1 : 0)
                                    }

                                    ThemedSlider {
                                        id: trParamSlider
                                        visible: !trParamRow.modelData.isBoolean
                                        width: parent.width
                                        from: trParamRow.modelData.min
                                        to: trParamRow.modelData.max
                                        value: trParamRow.modelData.value
                                        onMoved: EditorState.previewSetTransitionParam(
                                                     root.transitionEditTrack, root.activeTransition.id,
                                                     trParamRow.modelData.key, value)
                                        onPressedChanged: {
                                            if (pressed) {
                                                EditorState.beginPreviewDrag(qsTr("Edit transition"))
                                            } else {
                                                EditorState.commitPreviewDrag()
                                                value = Qt.binding(() => trParamRow.modelData.value)
                                            }
                                        }
                                    }
                                }
                            }

                            ThemedButton {
                                text: qsTr("Remove transition")
                                variant: "destructive"
                                onClicked: EditorState.removeTransition(
                                               root.transitionEditTrack, root.activeTransition.id)
                            }
                        }
                    }

                    // ----- Blending ------------------------------------------------------------
                    Column {
                        width: tabColumn.width
                        spacing: Theme.spacingXl
                        visible: root.currentTabId === "blending"

                        EmptyState {
                            visible: root.clipKind === "audio"
                            width: parent.width
                            compact: true
                            glyph: Theme.icons.film
                            title: qsTr("Video only")
                            hint: qsTr("This tab does not apply to audio clips.")
                        }

                        Text {
                            visible: root.clipKind !== "audio"
                            text: qsTr("Blend mode")
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                        }

                        // The tab was a label plus one combo with no description.
                        Text {
                            visible: root.clipKind !== "audio"
                            width: parent.width
                            wrapMode: Text.WordWrap
                            text: qsTr("How this clip's colours combine with the tracks beneath it.")
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                            opacity: 0.8
                        }

                        ThemedComboBox {
                            id: blendModeBox
                            visible: root.clipKind !== "audio"
                            width: parent.width
                            model: ["normal", "multiply", "screen", "overlay", "add", "darken", "lighten"]
                            // Human labels — raw ids were shown to the user.
                            readonly property var labels: ({
                                "normal": qsTr("Normal"),
                                "multiply": qsTr("Multiply"),
                                "screen": qsTr("Screen"),
                                "overlay": qsTr("Overlay"),
                                "add": qsTr("Add"),
                                "darken": qsTr("Darken"),
                                "lighten": qsTr("Lighten")
                            })
                            displayText: labels[model[currentIndex]] || model[currentIndex]
                            tooltip: qsTr("How this clip blends with the layers below")
                            currentIndex: Math.max(0, model.indexOf(clip.blendMode || "normal"))
                            onActivated: EditorState.setClipBlendMode(
                                             EditorState.selectedTrack, EditorState.selectedClip, model[currentIndex])
                        }

                        ThemedButton {
                            visible: root.clipKind !== "audio" && (clip.blendMode || "normal") !== "normal"
                            text: qsTr("Reset to Normal")
                            variant: "ghost"
                            glyph: Theme.icons.reset
                            onClicked: EditorState.setClipBlendMode(
                                           EditorState.selectedTrack, EditorState.selectedClip, "normal")
                        }
                    }

                    // ----- Masks ---------------------------------------------------------------
                    Column {
                        width: tabColumn.width
                        spacing: Theme.spacingXl
                        visible: root.currentTabId === "masks"

                        EmptyState {
                            visible: root.clipKind === "audio" || root.clipKind === "text"
                                     || root.clipKind === "subtitle"
                            width: parent.width
                            compact: true
                            glyph: Theme.icons.mask
                            title: qsTr("Not available")
                            hint: qsTr("Masks apply to visual clips.")
                        }

                        // The tab used to open with a lone unlabelled combo box
                        // and no explanation of what a mask does.
                        Text {
                            visible: maskShapeBox.visible
                            width: parent.width
                            text: qsTr("Mask shape")
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                        }

                        Text {
                            visible: maskShapeBox.visible
                            width: parent.width
                            wrapMode: Text.WordWrap
                            text: qsTr("Hides everything outside the shape. Feather softens its edge.")
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                            opacity: 0.8
                        }

                        ThemedComboBox {
                            id: maskShapeBox
                            visible: root.clipKind !== "audio" && root.clipKind !== "text"
                                     && root.clipKind !== "subtitle"
                            width: parent.width
                            model: ["none", "rectangle", "ellipse", "star", "heart", "bars", "freeform"]
                            // Human labels — the raw ids were shown to the user.
                            readonly property var labels: ({
                                "none": qsTr("None"),
                                "rectangle": qsTr("Rectangle"),
                                "ellipse": qsTr("Ellipse"),
                                "star": qsTr("Star"),
                                "heart": qsTr("Heart"),
                                "bars": qsTr("Bars"),
                                "freeform": qsTr("Freeform")
                            })
                            displayText: labels[model[currentIndex]] || model[currentIndex]
                            tooltip: qsTr("Shape used to mask this clip")
                            currentIndex: Math.max(0, model.indexOf((clip.mask && clip.mask.shape) || "none"))
                            onActivated: {
                                const mask = Object.assign({}, clip.mask || {})
                                mask.shape = model[currentIndex]
                                EditorState.setClipMask(EditorState.selectedTrack, EditorState.selectedClip, mask)
                            }
                        }

                        // Clearing a mask previously required knowing to reselect
                        // "none" in the combo above.
                        ThemedButton {
                            visible: maskShapeBox.visible
                                     && ((clip.mask && clip.mask.shape) || "none") !== "none"
                            text: qsTr("Remove mask")
                            variant: "destructive"
                            glyph: Theme.icons.trash
                            onClicked: {
                                const mask = Object.assign({}, clip.mask || {})
                                mask.shape = "none"
                                EditorState.setClipMask(EditorState.selectedTrack, EditorState.selectedClip, mask)
                            }
                        }

                        Repeater {
                            model: [
                                { key: "x", label: "Center X", min: 0, max: 1 },
                                { key: "y", label: "Center Y", min: 0, max: 1 },
                                { key: "w", label: "Width", min: 0.05, max: 1 },
                                { key: "h", label: "Height", min: 0.05, max: 1 },
                                { key: "rotation", label: "Rotation", min: -180, max: 180 },
                                { key: "feather", label: "Feather", min: 0, max: 64 }
                            ]
                            delegate: Column {
                                required property var modelData
                                width: parent.width
                                spacing: 4
                                visible: root.clipKind !== "audio" && root.clipKind !== "text"
                                     && root.clipKind !== "subtitle"
                                         && clip.mask && clip.mask.shape !== "none"
                                         && (modelData.key !== "rotation" || clip.mask.shape !== "bars")

                                Text {
                                    text: modelData.label
                                    color: Theme.mutedForeground
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                }
                                ThemedSlider {
                                    width: parent.width
                                    from: modelData.min
                                    to: modelData.max
                                    stepSize: modelData.key === "feather" ? 1 : 0.01
                                    value: (clip.mask && clip.mask[modelData.key]) || 0
                                    onMoved: {
                                        const mask = Object.assign({}, clip.mask || {})
                                        mask[modelData.key] = value
                                        EditorState.previewSetClipMask(
                                            EditorState.selectedTrack, EditorState.selectedClip, mask)
                                    }
                                    onPressedChanged: {
                                        if (pressed) {
                                            EditorState.beginPreviewDrag(qsTr("Mask changed"))
                                        } else {
                                            EditorState.commitPreviewDrag()
                                            value = Qt.binding(() => (clip.mask && clip.mask[modelData.key]) || 0)
                                        }
                                    }
                                }
                            }
                        }

                        Row {
                            width: parent.width
                            spacing: 8
                            visible: root.clipKind !== "audio" && root.clipKind !== "text"
                                     && root.clipKind !== "subtitle"
                                     && clip.mask && clip.mask.shape !== "none"
                            Text {
                                text: qsTr("Invert")
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            ThemedSwitch {
                                checked: !!(clip.mask && clip.mask.invert)
                                onToggled: {
                                    const mask = Object.assign({}, clip.mask || {})
                                    mask.invert = checked
                                    EditorState.setClipMask(EditorState.selectedTrack, EditorState.selectedClip, mask)
                                }
                            }
                        }
                    }

                    // ----- Effects ---------------------------------------------------------------
                    Column {
                        width: tabColumn.width
                        spacing: Theme.spacingXl
                        visible: root.currentTabId === "effects"

                        // Has a CTA now: the copy told the user to go to the
                        // Effects library but gave them no way to get there.
                        EmptyState {
                            width: parent.width
                            visible: root.selectedEffects.length === 0
                            glyph: Theme.icons.wand
                            title: qsTr("No effects yet")
                            hint: qsTr("Drag a preset from the Effects library onto this clip, or click a preset card.")
                            actionText: qsTr("Browse effects")
                            onActionTriggered: root.browseEffectsRequested()
                        }

                        Repeater {
                            model: root.selectedEffects
                            delegate: Column {
                                id: effectCard
                                required property var modelData
                                required property int index
                                width: tabColumn.width
                                spacing: 6

                                Rectangle {
                                    width: parent.width
                                    height: effectHeader.implicitHeight + 8
                                    radius: Theme.radiusSm
                                    color: Theme.panelAccent

                                    Row {
                                        id: effectHeader
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 4
                                        Text {
                                            text: effectCard.modelData.label
                                            color: Theme.panelForeground
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeSm
                                            font.weight: Font.Medium
                                            width: parent.width - 28
                                            elide: Text.ElideRight
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        IconButton {
                                            glyph: Theme.icons.x
                                            variant: "ghost"
                                            buttonSize: 22
                                            iconSize: 12
                                            tooltip: qsTr("Remove effect")
                                            onClicked: EditorState.removeEffect(
                                                           EditorState.selectedTrack, EditorState.selectedClip,
                                                           effectCard.index)
                                        }
                                    }
                                }

                                Repeater {
                                    model: effectCard.modelData.params || []
                                    delegate: Column {
                                        id: paramRow
                                        required property var modelData
                                        width: tabColumn.width
                                        spacing: 4

                                        Row {
                                            width: parent.width
                                            spacing: 8
                                            Text {
                                                width: parent.width - 48
                                            elide: Text.ElideRight
                                                text: paramRow.modelData.label
                                                color: Theme.mutedForeground
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontSizeXs
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                            Text {
                                                width: 40
                                                horizontalAlignment: Text.AlignRight
                                                text: paramRow.modelData.isBoolean
                                                      ? (paramRow.modelData.value ? qsTr("On") : qsTr("Off"))
                                                      : Number(paramSlider.value).toFixed(
                                                            Math.abs(paramRow.modelData.max - paramRow.modelData.min) >= 10 ? 1 : 2)
                                                color: Theme.panelForeground
                                                font.family: Theme.monoFontFamily
                                                font.pixelSize: Theme.fontSizeXs
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                        }

                                        ThemedSwitch {
                                            visible: !!paramRow.modelData.isBoolean
                                            checked: !!paramRow.modelData.value
                                            onToggled: EditorState.setEffectParam(
                                                           EditorState.selectedTrack, EditorState.selectedClip,
                                                           effectCard.index, paramRow.modelData.key, checked ? 1 : 0)
                                        }

                                        ThemedSlider {
                                            id: paramSlider
                                            visible: !paramRow.modelData.isBoolean
                                            width: parent.width
                                            from: paramRow.modelData.min
                                            to: paramRow.modelData.max
                                            value: paramRow.modelData.value
                                            onMoved: EditorState.previewSetEffectParam(
                                                         EditorState.selectedTrack, EditorState.selectedClip,
                                                         effectCard.index, paramRow.modelData.key, value)
                                            onPressedChanged: {
                                                if (pressed) {
                                                    EditorState.beginPreviewDrag(qsTr("Edit effect"))
                                                } else {
                                                    EditorState.commitPreviewDrag()
                                                    value = Qt.binding(() => paramRow.modelData.value)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Full-height editor with its own internal cue list scrolling, so it
            // sits beside the tab Flickable rather than inside it.
            SubtitleEditor {
                width: parent.width - Theme.tabRailWidth - Theme.borderWidth
                height: Math.max(0, parent.height)
                visible: root.currentTabId === "subtitles"
                clip: root.hasSelection ? root.clip : null
                formatSeconds: root.formatSeconds
            }
        }
    }

    readonly property var propOpacity: { "key": "opacity", "label": "Opacity", "def": 1.0, "decimals": 2 }
    readonly property var propX: { "key": "x", "label": "X", "def": 0.0, "decimals": 0 }
    readonly property var propY: { "key": "y", "label": "Y", "def": 0.0, "decimals": 0 }
    readonly property var propWidth: { "key": "width", "label": "Width", "def": root.canvasW, "decimals": 0 }
    readonly property var propHeight: { "key": "height", "label": "Height", "def": root.canvasH, "decimals": 0 }
    readonly property var propRotation: { "key": "rotation", "label": "Angle", "def": 0.0, "decimals": 1 }
    readonly property var propVolume: { "key": "volume", "label": "Volume", "def": 1.0, "decimals": 2 }

    function applyTrim(inPoint, outPoint) {
        if (!root.hasSelection || isNaN(inPoint) || isNaN(outPoint))
            return
        EditorState.setClipTrim(EditorState.selectedTrack, EditorState.selectedClip, inPoint, outPoint)
    }

    ColorDialog {
        id: styleColorDialog
        title: qsTr("Select Color")
        property string targetStyleKey: ""

        function colorToHex(c) {
            var toHex = function(v) {
                var h = Math.round(v * 255).toString(16);
                return h.length === 1 ? "0" + h : h;
            }
            return "#" + toHex(c.a) + toHex(c.r) + toHex(c.g) + toHex(c.b);
        }

        onAccepted: {
            if (targetStyleKey !== "") {
                root.setTextStyleKey(targetStyleKey, colorToHex(selectedColor))
            }
        }
    }
}
