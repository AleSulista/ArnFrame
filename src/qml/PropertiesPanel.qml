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
    readonly property int transitionTabIndex: tabIndexOf("transition")
    readonly property var selectedEffects: EditorState.selectedClipEffects
    readonly property var selectedAudioEffects: EditorState.selectedClipAudioEffects
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
    readonly property var animKinds: ["none", "fade", "slideUp", "slideDown", "slideLeft", "slideRight", "pop", "blur", "typewriter", "rise", "bounce", "wave"]
    readonly property var animKindLabels: ["None", "Fade", "Slide up", "Slide down", "Slide left", "Slide right", "Pop", "Blur", "Typewriter", "Rise", "Bounce", "Wave"]
    readonly property var easeKinds: ["linear", "easeOut", "easeInOut", "back"]
    readonly property var easeLabels: ["Linear", "Ease out", "Ease in-out", "Back"]
    readonly property var animUnits: ["block", "character", "word", "line"]
    readonly property var animUnitLabels: ["Whole block", "Character", "Word", "Line"]
    readonly property var animOrders: ["forward", "backward", "centerOut", "random"]
    readonly property var animOrderLabels: ["Forward", "Backward", "Center out", "Random"]

    readonly property bool hasShapeStyle: hasSelection && clipKind === "shape" && !!clip.shapeStyle
    readonly property var shapeStyle: hasShapeStyle ? clip.shapeStyle : ({
                                                                       "kind": "rectangle",
                                                                       "fillKind": "solid",
                                                                       "fill": "#ff00b4ff",
                                                                       "fillSecondary": "#ff7a00ff",
                                                                       "gradientAngle": 90,
                                                                       "stroke": "#ffffffff",
                                                                       "strokeWidth": 4,
                                                                       "strokeStyle": "solid",
                                                                       "cornerRadius": 0,
                                                                       "points": 5,
                                                                       "innerRatio": 0.5,
                                                                       "headSize": 0.4,
                                                                       "thickness": 0.4,
                                                                       "tailX": 0.25,
                                                                       "tailSize": 0.2
                                                                   })
    readonly property var shapeCatalog: EditorState.builtinShapes()

    // Which geometry controls apply to the selected kind. The catalog id and the stored kind are
    // not always the same word ("circle" is an ellipse), so match on the stored kind.
    readonly property var shapeFamilies: ({
        "corner": ["rounded-rectangle", "speech-bubble-rect", "callout"],
        "star": ["star", "burst"],
        "arrow": ["arrow", "double-arrow", "block-arrow", "chevron", "banner"],
        "shaft": ["arrow", "double-arrow", "chevron", "cross", "curved-arrow"],
        "bubble": ["speech-bubble", "speech-bubble-rect", "thought-bubble", "callout"]
    })
    function shapeHas(family) {
        return root.shapeFamilies[family].indexOf(root.shapeStyle.kind) >= 0
    }

    function setTextStyleKey(key, value) {
        const patch = {}
        patch[key] = value
        EditorState.setTextStyle(EditorState.selectedTrack, EditorState.selectedClip, patch)
    }

    function setShapeKey(key, value) {
        const patch = {}
        patch[key] = value
        EditorState.setShapeStyle(EditorState.selectedTrack, EditorState.selectedClip, patch)
    }

    function setTextAnim(which, key, value) {
        const anim = {}
        anim[key] = value
        const patch = {}
        patch[which] = anim
        EditorState.setTextStyle(EditorState.selectedTrack, EditorState.selectedClip, patch)
    }

    // Reveal granularity / stagger / order are shared by the entrance and exit, so write both.
    function setTextReveal(key, value) {
        const anim = {}
        anim[key] = value
        const patch = { "animIn": anim, "animOut": anim }
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
        if (root.hasShapeStyle) {
            const sh = root.shapeStyle
            if (gradientAngleField && !gradientAngleField.activeFocus)
                gradientAngleField.value = sh.gradientAngle
            if (strokeWidthField && !strokeWidthField.activeFocus)
                strokeWidthField.value = sh.strokeWidth
            if (cornerRadiusField && !cornerRadiusField.activeFocus)
                cornerRadiusField.value = sh.cornerRadius
            if (shapePointsField && !shapePointsField.activeFocus)
                shapePointsField.value = sh.points
        }
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
            if (animStaggerField && !animStaggerField.activeFocus)
                animStaggerField.value = s.animIn.stagger
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
            root.syncShapeTab()
            root.syncTextTab()
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
        ListElement { tabId: "text"; icon: 10; label: "Text" }
        ListElement { tabId: "transform"; icon: 1; label: "Transform" }
        ListElement { tabId: "audio"; icon: 2; label: "Audio" }
        ListElement { tabId: "speed"; icon: 3; label: "Speed & Fade" }
        ListElement { tabId: "blending"; icon: 4; label: "Blending" }
        ListElement { tabId: "transition"; icon: 5; label: "Transition" }
        ListElement { tabId: "masks"; icon: 6; label: "Masks" }
        ListElement { tabId: "effects"; icon: 7; label: "Effects" }
        ListElement { tabId: "subtitles"; icon: 8; label: "Subtitles" }
        ListElement { tabId: "shape"; icon: 9; label: "Shape" }
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
        Theme.icons.messageSquare,
        Theme.icons.shapes,
        Theme.icons.type
    ]
    function tabIndexOf(id) {
        for (let i = 0; i < tabsModel.count; i++) {
            if (tabsModel.get(i).tabId === id)
                return i
        }
        return -1
    }
    readonly property int subtitlesTabIndex: tabIndexOf("subtitles")
    readonly property int shapeTabIndex: tabIndexOf("shape")
    readonly property int textTabIndex: tabIndexOf("text")

    function syncSubtitlesTab() {
        if (root.subtitlesTabIndex < 0)
            return
        if (root.clipKind === "subtitle")
            root.activeTab = root.subtitlesTabIndex
        else if (root.activeTab === root.subtitlesTabIndex)
            root.activeTab = 0
    }

    // The Shape tab is hidden for every other clip kind, so leaving it selected would show a blank
    // pane after the selection moves off a shape.
    function syncShapeTab() {
        if (root.shapeTabIndex >= 0 && root.activeTab === root.shapeTabIndex
                && root.clipKind !== "shape")
            root.activeTab = 0
    }

    // Same for the Text tab, which only exists for clips carrying a text style.
    function syncTextTab() {
        if (root.textTabIndex >= 0 && root.activeTab === root.textTabIndex && !root.hasTextStyle)
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
                        visible: (model.tabId !== "subtitles" || root.clipKind === "subtitle")
                                 && (model.tabId !== "shape" || root.clipKind === "shape")
                                 && (model.tabId !== "text" || root.hasTextStyle)
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

                    // ----- Text --------------------------------------------------------------
                    Column {
                        width: tabColumn.width
                        spacing: Theme.spacingXl
                        visible: root.currentTabId === "text"

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
                                id: textContentField
                                width: parent.width
                                height: 80
                                onTextChanged: {
                                    if (root.clipKind !== "text")
                                        return
                                    EditorState.previewSetClipTextContent(
                                        EditorState.selectedTrack, EditorState.selectedClip, text)
                                }
                                onEditingFinished: EditorState.commitTextEdit(
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

                            // Reveal granularity: stagger the entrance/exit across characters,
                            // words or lines instead of moving the whole block at once.
                            Row {
                                width: parent.width
                                spacing: 6

                                Text {
                                    text: qsTr("By")
                                    width: 20
                                    color: Theme.mutedForeground
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                ThemedComboBox {
                                    id: animUnitBox
                                    width: Math.max(40, (parent.width - 20 - parent.spacing * 3 - 56) / 2)
                                    model: root.animUnitLabels
                                    currentIndex: Math.max(0, root.animUnits.indexOf(root.textStyle.animIn.unit))
                                    onActivated: root.setTextReveal("unit", root.animUnits[currentIndex])
                                }
                                ThemedComboBox {
                                    id: animOrderBox
                                    width: Math.max(40, (parent.width - 20 - parent.spacing * 3 - 56) / 2)
                                    model: root.animOrderLabels
                                    enabled: root.textStyle.animIn.unit !== "block"
                                    currentIndex: Math.max(0, root.animOrders.indexOf(root.textStyle.animIn.order))
                                    onActivated: root.setTextReveal("order", root.animOrders[currentIndex])
                                }
                                ThemedNumberField {
                                    id: animStaggerField
                                    to: 2
                                    unit: "s"
                                    width: 56
                                    decimals: 2
                                    step: 0.02
                                    from: 0
                                    enabled: root.textStyle.animIn.unit !== "block"
                                    onEdited: v => root.setTextReveal("stagger", v)
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
                                useSlider: true
                                sliderFrom: -root.canvasW
                                sliderTo: root.canvasW * 2
                                unit: "px"
                            }
                            PropertyKeyframeRow {
                                width: parent.width
                                propDef: root.propY
                                keyframeList: (clip.keyframes && clip.keyframes.y && clip.keyframes.y.points) || []
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
                                useSlider: true
                                sliderFrom: 1
                                sliderTo: Math.max(root.canvasW * 2, 2)
                                unit: "px"
                            }
                            PropertyKeyframeRow {
                                width: parent.width
                                propDef: root.propHeight
                                keyframeList: (clip.keyframes && clip.keyframes.height && clip.keyframes.height.points) || []
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
                                useSlider: true
                                sliderFrom: 0
                                sliderTo: 1
                                percent: true
                            }

                            PropertyKeyframeRow {
                                width: parent.width
                                propDef: root.propRotation
                                keyframeList: (clip.keyframes && clip.keyframes.rotation && clip.keyframes.rotation.points) || []
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
                        id: audioTabColumn
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

                        // ----- Noise removal ---------------------------------------------
                        Column {
                            id: denoiseSection
                            width: parent.width
                            spacing: Theme.spacingSm
                            visible: root.clipKind === "audio" || root.clipKind === "video"

                            // Whether the model is on disk is a one-shot filesystem answer, not a
                            // binding, hence the reset below when an addon of this kind appears.
                            property bool denoiseReady: EditorState.denoiseAvailable()

                            Connections {
                                target: Addons
                                function onKindChanged(kind) {
                                    if (kind === "denoise-model")
                                        denoiseSection.denoiseReady = EditorState.denoiseAvailable()
                                }
                            }

                            Text {
                                width: parent.width
                                text: qsTr("Noise")
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }

                            ThemedButton {
                                visible: denoiseSection.denoiseReady
                                width: parent.width
                                text: qsTr("Remove noise…")
                                enabled: !EditorState.denoising
                                onClicked: {
                                    const data = EditorState.selectedClipData
                                    root.Window.window.openDenoise(
                                        EditorState.selectedTrack, EditorState.selectedClip,
                                        data.duration !== undefined ? data.duration : 0)
                                }
                            }

                            ThemedButton {
                                visible: !denoiseSection.denoiseReady
                                width: parent.width
                                text: qsTr("Install noise removal model (≈9 MB)")
                                variant: "primary"
                                onClicked: root.Window.window.openAddonManager("denoise-model")
                            }
                        }

                        Rectangle {
                            visible: root.clipKind === "audio" || root.clipKind === "video"
                            width: parent.width
                            height: 1
                            color: Theme.panelBorder
                            opacity: 0.5
                        }

                        // ----- Audio effects ---------------------------------------------
                        Text {
                            visible: root.clipKind === "audio" || root.clipKind === "video"
                            text: qsTr("Audio effects")
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                        }

                        // Empty state doubles as the discovery hint: the effects come from an addon,
                        // so when the catalog is empty point the user at the Addon Manager.
                        property var audioFxCatalog: (root.clipKind === "audio" || root.clipKind === "video")
                                                     ? EditorState.audioEffectCatalog() : []

                        Text {
                            visible: (root.clipKind === "audio" || root.clipKind === "video")
                                     && parent.audioFxCatalog.length === 0
                            width: parent.width
                            wrapMode: Text.WordWrap
                            text: qsTr("No audio effects installed. Get the Audio Effects pack from the Addon Manager.")
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                        }

                        ThemedButton {
                            visible: (root.clipKind === "audio" || root.clipKind === "video")
                                     && parent.audioFxCatalog.length === 0
                            width: parent.width
                            text: qsTr("Install audio effects")
                            variant: "primary"
                            onClicked: root.Window.window.openAddonManager("audio-effects")
                        }

                        Row {
                            visible: (root.clipKind === "audio" || root.clipKind === "video")
                                     && parent.audioFxCatalog.length > 0
                            width: parent.width
                            spacing: 8

                            ThemedComboBox {
                                id: audioFxPicker
                                width: parent.width - 96
                                textRole: "label"
                                valueRole: "id"
                                model: audioTabColumn.audioFxCatalog
                            }

                            ThemedButton {
                                width: 88
                                text: qsTr("Add")
                                enabled: audioFxPicker.currentValue !== undefined
                                onClicked: EditorState.addAudioEffect(
                                               EditorState.selectedTrack, EditorState.selectedClip,
                                               audioFxPicker.currentValue)
                            }
                        }

                        Repeater {
                            model: root.selectedAudioEffects
                            delegate: Column {
                                id: audioEffectCard
                                required property var modelData
                                required property int index
                                width: tabColumn.width
                                spacing: 6

                                Rectangle {
                                    width: parent.width
                                    height: audioEffectHeader.implicitHeight + 8
                                    radius: Theme.radiusSm
                                    color: Theme.panelAccent

                                    Row {
                                        id: audioEffectHeader
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 4
                                        Text {
                                            text: audioEffectCard.modelData.missing
                                                  ? qsTr("%1 (not installed)").arg(audioEffectCard.modelData.label)
                                                  : audioEffectCard.modelData.label
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
                                            tooltip: qsTr("Remove audio effect")
                                            onClicked: EditorState.removeAudioEffect(
                                                           EditorState.selectedTrack, EditorState.selectedClip,
                                                           audioEffectCard.index)
                                        }
                                    }
                                }

                                Repeater {
                                    model: audioEffectCard.modelData.params || []
                                    delegate: Column {
                                        id: audioParamRow
                                        required property var modelData
                                        width: tabColumn.width
                                        spacing: 4

                                        Row {
                                            width: parent.width
                                            spacing: 8
                                            Text {
                                                width: parent.width - 48
                                                elide: Text.ElideRight
                                                text: audioParamRow.modelData.label
                                                color: Theme.mutedForeground
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontSizeXs
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                            Text {
                                                width: 40
                                                horizontalAlignment: Text.AlignRight
                                                text: audioParamRow.modelData.isBoolean
                                                      ? (audioParamRow.modelData.value ? qsTr("On") : qsTr("Off"))
                                                      : Number(audioParamSlider.value).toFixed(
                                                            Math.abs(audioParamRow.modelData.max - audioParamRow.modelData.min) >= 10 ? 1 : 2)
                                                color: Theme.panelForeground
                                                font.family: Theme.monoFontFamily
                                                font.pixelSize: Theme.fontSizeXs
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                        }

                                        ThemedSwitch {
                                            visible: !!audioParamRow.modelData.isBoolean
                                            checked: !!audioParamRow.modelData.value
                                            onToggled: EditorState.previewSetAudioEffectParam(
                                                           EditorState.selectedTrack, EditorState.selectedClip,
                                                           audioEffectCard.index, audioParamRow.modelData.key,
                                                           checked ? 1 : 0)
                                        }

                                        ThemedSlider {
                                            id: audioParamSlider
                                            visible: !audioParamRow.modelData.isBoolean
                                            width: parent.width
                                            from: audioParamRow.modelData.min
                                            to: audioParamRow.modelData.max
                                            value: audioParamRow.modelData.value
                                            onMoved: EditorState.previewSetAudioEffectParam(
                                                         EditorState.selectedTrack, EditorState.selectedClip,
                                                         audioEffectCard.index, audioParamRow.modelData.key, value)
                                            onPressedChanged: {
                                                if (pressed) {
                                                    EditorState.beginPreviewDrag(qsTr("Edit audio effect"))
                                                } else {
                                                    EditorState.commitPreviewDrag()
                                                    value = Qt.binding(() => audioParamRow.modelData.value)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
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
                        id: speedColumn
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

                        // A ramp supersedes the constant rate outright, so the controls below
                        // are meaningless while one is attached.
                        property bool hasSpeedCurve: {
                            void root.clipDataRevision
                            return !!clip.hasSpeedCurve
                        }

                        Row {
                            width: parent.width
                            spacing: 6
                            visible: root.clipKind === "video" || root.clipKind === "audio"

                            ThemedButton {
                                text: qsTr("Speed curve…")
                                variant: "secondary"
                                onClicked: root.Window.window.openSpeedCurve(
                                               EditorState.selectedTrack, EditorState.selectedClip)
                            }

                            ThemedChip {
                                anchors.verticalCenter: parent.verticalCenter
                                visible: speedColumn.hasSpeedCurve
                                text: qsTr("Curve active — remove")
                                selected: true
                                onClicked: EditorState.clearClipSpeedCurve(
                                               EditorState.selectedTrack, EditorState.selectedClip)
                            }
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
                                        enabled: !speedColumn.hasSpeedCurve
                                        selected: !speedColumn.hasSpeedCurve
                                                  && Math.abs((clip.speed || 1) - modelData.value) < 0.01
                                        onClicked: EditorState.setClipSpeed(
                                                       EditorState.selectedTrack, EditorState.selectedClip,
                                                       modelData.value)
                                    }
                                }
                            }

                        ThemedSlider {
                            id: speedSlider
                            visible: root.clipKind === "video" || root.clipKind === "audio"
                            enabled: !speedColumn.hasSpeedCurve
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
                            text: (speedColumn.hasSpeedCurve
                                   ? qsTr("Variable (curve)")
                                   : (clip.speed || 1).toFixed(2) + "×")
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

                    // ----- Shape ---------------------------------------------------------------
                    Column {
                        id: shapeTabColumn
                        width: tabColumn.width
                        spacing: Theme.spacingXl
                        visible: root.currentTabId === "shape"

                        // One entry per ShapeKind: the catalog lists "circle" and "ellipse"
                        // separately so each gets its own default aspect, but they are one kind.
                        readonly property var kindOptions: {
                            const seen = ({})
                            const out = []
                            for (let i = 0; i < root.shapeCatalog.length; ++i) {
                                const entry = root.shapeCatalog[i]
                                if (seen[entry.kind])
                                    continue
                                seen[entry.kind] = true
                                out.push(entry)
                            }
                            return out
                        }

                        Column {
                            width: parent.width
                            spacing: Theme.spacingXs

                            ThemedLabel { text: qsTr("Shape") }

                            ThemedLabel {
                                width: parent.width
                                opacity: 0.8
                                text: qsTr("Swapping the shape keeps its position, size and effects.")
                            }

                            ThemedComboBox {
                                id: shapeKindBox
                                width: parent.width
                                model: shapeTabColumn.kindOptions
                                textRole: "label"
                                valueRole: "id"
                                tooltip: qsTr("Shape drawn by this clip")
                                currentIndex: {
                                    const options = shapeTabColumn.kindOptions
                                    for (let i = 0; i < options.length; ++i) {
                                        if (options[i].kind === root.shapeStyle.kind)
                                            return i
                                    }
                                    return 0
                                }
                                onActivated: root.setShapeKey("kind", currentValue)
                            }
                        }

                        // ----- Fill -------------------------------------------------------
                        Column {
                            width: parent.width
                            spacing: Theme.spacingSm

                            ThemedLabel { text: qsTr("Fill") }

                            ThemedComboBox {
                                id: fillKindBox
                                width: parent.width
                                model: ["none", "solid", "linear", "radial"]
                                readonly property var labels: ({
                                    "none": qsTr("None"),
                                    "solid": qsTr("Solid"),
                                    "linear": qsTr("Linear gradient"),
                                    "radial": qsTr("Radial gradient")
                                })
                                displayText: labels[model[currentIndex]] || model[currentIndex]
                                tooltip: qsTr("How the shape's interior is painted")
                                currentIndex: Math.max(0, model.indexOf(root.shapeStyle.fillKind))
                                onActivated: root.setShapeKey("fillKind", model[currentIndex])
                            }

                            ColorSwatchField {
                                visible: root.shapeStyle.fillKind !== "none"
                                hex: root.shapeStyle.fill
                                tooltip: root.shapeStyle.fillKind === "solid"
                                         ? qsTr("Choose fill colour")
                                         : qsTr("Choose the gradient's start colour")
                                onEdited: value => root.setShapeKey("fill", value)
                            }

                            ColorSwatchField {
                                visible: root.shapeStyle.fillKind === "linear"
                                         || root.shapeStyle.fillKind === "radial"
                                hex: root.shapeStyle.fillSecondary
                                tooltip: qsTr("Choose the gradient's end colour")
                                onEdited: value => root.setShapeKey("fillSecondary", value)
                            }

                            Column {
                                visible: root.shapeStyle.fillKind === "linear"
                                width: parent.width
                                spacing: Theme.spacingXs

                                ThemedLabel { text: qsTr("Gradient angle") }

                                ThemedNumberField {
                                    id: gradientAngleField
                                    width: parent.width
                                    unit: "°"
                                    decimals: 0
                                    step: 15
                                    from: -360
                                    to: 360
                                    onEdited: v => root.setShapeKey("gradientAngle", v)
                                }
                            }
                        }

                        // ----- Stroke -----------------------------------------------------
                        Column {
                            width: parent.width
                            spacing: Theme.spacingSm

                            ThemedLabel { text: qsTr("Stroke") }

                            ThemedComboBox {
                                id: strokeStyleBox
                                width: parent.width
                                model: ["none", "solid", "dash", "dot", "dashdot"]
                                readonly property var labels: ({
                                    "none": qsTr("None"),
                                    "solid": qsTr("Solid"),
                                    "dash": qsTr("Dashed"),
                                    "dot": qsTr("Dotted"),
                                    "dashdot": qsTr("Dash-dot")
                                })
                                displayText: labels[model[currentIndex]] || model[currentIndex]
                                tooltip: qsTr("Outline style")
                                currentIndex: Math.max(0, model.indexOf(root.shapeStyle.strokeStyle))
                                onActivated: root.setShapeKey("strokeStyle", model[currentIndex])
                            }

                            ColorSwatchField {
                                visible: root.shapeStyle.strokeStyle !== "none"
                                hex: root.shapeStyle.stroke
                                tooltip: qsTr("Choose stroke colour")
                                onEdited: value => root.setShapeKey("stroke", value)
                            }

                            Column {
                                visible: root.shapeStyle.strokeStyle !== "none"
                                width: parent.width
                                spacing: Theme.spacingXs

                                ThemedLabel { text: qsTr("Stroke width") }

                                ThemedNumberField {
                                    id: strokeWidthField
                                    width: parent.width
                                    unit: "px"
                                    decimals: 0
                                    step: 1
                                    from: 0
                                    to: 200
                                    onEdited: v => root.setShapeKey("strokeWidth", v)
                                }
                            }
                        }

                        // ----- Geometry ---------------------------------------------------
                        Column {
                            visible: root.shapeHas("corner")
                            width: parent.width
                            spacing: Theme.spacingXs

                            ThemedLabel { text: qsTr("Corner radius") }

                            ThemedNumberField {
                                id: cornerRadiusField
                                width: parent.width
                                unit: "px"
                                decimals: 0
                                step: 4
                                from: 0
                                to: 2000
                                onEdited: v => root.setShapeKey("cornerRadius", v)
                            }
                        }

                        Column {
                            visible: root.shapeHas("star")
                            width: parent.width
                            spacing: Theme.spacingSm

                            ThemedLabel { text: qsTr("Points") }

                            ThemedNumberField {
                                id: shapePointsField
                                width: parent.width
                                decimals: 0
                                step: 1
                                from: 3
                                to: 60
                                onEdited: v => root.setShapeKey("points", v)
                            }

                            ThemedLabel { text: qsTr("Inner radius") }

                            ThemedLabel {
                                width: parent.width
                                opacity: 0.8
                                text: qsTr("How deep the notches cut between the points.")
                            }

                            ThemedSlider {
                                width: parent.width
                                from: 0.05
                                to: 0.95
                                stepSize: 0.01
                                value: root.shapeStyle.innerRatio
                                onMoved: root.setShapeKey("innerRatio", value)
                                onPressedChanged: {
                                    if (pressed) {
                                        EditorState.beginPreviewDrag(qsTr("Shape style changed"))
                                    } else {
                                        EditorState.commitPreviewDrag()
                                        value = Qt.binding(() => root.shapeStyle.innerRatio)
                                    }
                                }
                            }
                        }

                        Column {
                            visible: root.shapeHas("arrow")
                            width: parent.width
                            spacing: Theme.spacingXs

                            ThemedLabel { text: qsTr("Head size") }

                            ThemedSlider {
                                width: parent.width
                                from: 0.05
                                to: 0.9
                                stepSize: 0.01
                                value: root.shapeStyle.headSize
                                onMoved: root.setShapeKey("headSize", value)
                                onPressedChanged: {
                                    if (pressed) {
                                        EditorState.beginPreviewDrag(qsTr("Shape style changed"))
                                    } else {
                                        EditorState.commitPreviewDrag()
                                        value = Qt.binding(() => root.shapeStyle.headSize)
                                    }
                                }
                            }
                        }

                        Column {
                            visible: root.shapeHas("shaft")
                            width: parent.width
                            spacing: Theme.spacingXs

                            ThemedLabel { text: qsTr("Thickness") }

                            ThemedSlider {
                                width: parent.width
                                from: 0.05
                                to: 1.0
                                stepSize: 0.01
                                value: root.shapeStyle.thickness
                                onMoved: root.setShapeKey("thickness", value)
                                onPressedChanged: {
                                    if (pressed) {
                                        EditorState.beginPreviewDrag(qsTr("Shape style changed"))
                                    } else {
                                        EditorState.commitPreviewDrag()
                                        value = Qt.binding(() => root.shapeStyle.thickness)
                                    }
                                }
                            }
                        }

                        Column {
                            visible: root.shapeHas("bubble")
                            width: parent.width
                            spacing: Theme.spacingSm

                            ThemedLabel { text: qsTr("Tail position") }

                            ThemedLabel {
                                width: parent.width
                                opacity: 0.8
                                text: qsTr("Where the tail meets the bottom of the bubble.")
                            }

                            ThemedSlider {
                                width: parent.width
                                from: 0.08
                                to: 0.92
                                stepSize: 0.01
                                value: root.shapeStyle.tailX
                                onMoved: root.setShapeKey("tailX", value)
                                onPressedChanged: {
                                    if (pressed) {
                                        EditorState.beginPreviewDrag(qsTr("Shape style changed"))
                                    } else {
                                        EditorState.commitPreviewDrag()
                                        value = Qt.binding(() => root.shapeStyle.tailX)
                                    }
                                }
                            }

                            ThemedLabel { text: qsTr("Tail size") }

                            ThemedSlider {
                                width: parent.width
                                from: 0.05
                                to: 0.5
                                stepSize: 0.01
                                value: root.shapeStyle.tailSize
                                onMoved: root.setShapeKey("tailSize", value)
                                onPressedChanged: {
                                    if (pressed) {
                                        EditorState.beginPreviewDrag(qsTr("Shape style changed"))
                                    } else {
                                        EditorState.commitPreviewDrag()
                                        value = Qt.binding(() => root.shapeStyle.tailSize)
                                    }
                                }
                            }
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

                        // Segmentation produces a matte — a per-frame mask — so it belongs beside
                        // the parametric shapes rather than in a tab of its own. It needs a
                        // prompting surface, so it opens a window instead of running from here.
                        Column {
                            id: segmentSection
                            visible: root.clipKind === "video"
                            width: parent.width
                            spacing: Theme.spacingSm

                            // The model is an addon, but it can equally come from a bundled
                            // models/sam2 or DRIFT_SAM2_MODEL_DIR, so ask the engine rather than
                            // the addon registry. That answer is not a binding, hence the reset
                            // below when an addon of this kind appears.
                            property bool segmentReady: EditorState.segmentationAvailable()

                            Connections {
                                target: Addons
                                function onKindChanged(kind) {
                                    if (kind === "sam2-model")
                                        segmentSection.segmentReady = EditorState.segmentationAvailable()
                                }
                            }

                            Text {
                                width: parent.width
                                text: qsTr("Subject")
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }

                            ThemedButton {
                                visible: segmentSection.segmentReady
                                width: parent.width
                                text: qsTr("Segment subject…")
                                enabled: !EditorState.segmenting
                                onClicked: {
                                    const data = EditorState.selectedClipData
                                    root.Window.window.openSegmentation(
                                        EditorState.selectedTrack, EditorState.selectedClip,
                                        data.start !== undefined ? data.start : 0,
                                        data.duration !== undefined ? data.duration : 0)
                                }
                            }

                            ThemedButton {
                                visible: !segmentSection.segmentReady
                                width: parent.width
                                text: qsTr("Install segmentation model (≈190 MB)")
                                variant: "primary"
                                onClicked: root.Window.window.openAddonManager("sam2-model")
                            }
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
                                         && !!clip.mask && clip.mask.shape !== "none"
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
                                     && !!clip.mask && clip.mask.shape !== "none"
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

                        // The face warp effects follow baked landmarks, so the clip has to be
                        // scanned before any of them do anything. This sits above the effect list
                        // because that ordering is the workflow: detect, then apply.
                        Column {
                            id: faceSection
                            visible: root.clipKind === "video"
                            width: parent.width
                            spacing: Theme.spacingSm

                            // The model is an addon, but it can equally come from a bundled
                            // models/face or DRIFT_FACE_MODEL_DIR, so ask the engine rather than
                            // the addon registry. That answer is not a binding, hence the reset
                            // below when an addon of this kind appears.
                            property bool faceReady: EditorState.faceDetectionAvailable()
                            property bool hasTrack: {
                                const data = EditorState.selectedClipData
                                return data && data.hasFaceTrack === true
                            }

                            Connections {
                                target: Addons
                                function onKindChanged(kind) {
                                    if (kind === "face-model")
                                        faceSection.faceReady = EditorState.faceDetectionAvailable()
                                }
                            }

                            Text {
                                width: parent.width
                                text: qsTr("Face tracking")
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }

                            Text {
                                width: parent.width
                                wrapMode: Text.WordWrap
                                visible: faceSection.faceReady && !faceSection.hasTrack
                                         && !EditorState.faceDetecting
                                text: qsTr("Scan this clip once, then the Funny Face effects will follow the face through it.")
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }

                            ThemedButton {
                                visible: faceSection.faceReady && !EditorState.faceDetecting
                                width: parent.width
                                text: faceSection.hasTrack ? qsTr("Re-detect faces") : qsTr("Detect faces…")
                                variant: faceSection.hasTrack ? "ghost" : "secondary"
                                onClicked: EditorState.detectFacesForClip(
                                               EditorState.selectedTrack, EditorState.selectedClip)
                            }

                            ThemedButton {
                                visible: faceSection.faceReady && faceSection.hasTrack
                                         && !EditorState.faceDetecting
                                width: parent.width
                                text: qsTr("Clear face track")
                                variant: "ghost"
                                onClicked: EditorState.clearFaceTrack(
                                               EditorState.selectedTrack, EditorState.selectedClip)
                            }

                            Text {
                                width: parent.width
                                wrapMode: Text.WordWrap
                                visible: EditorState.faceDetecting
                                text: EditorState.faceDetectStatus
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }

                            ThemedProgressBar {
                                visible: EditorState.faceDetecting
                                width: parent.width
                                value: EditorState.faceDetectProgress
                            }

                            ThemedButton {
                                visible: EditorState.faceDetecting
                                width: parent.width
                                text: qsTr("Cancel")
                                variant: "ghost"
                                onClicked: EditorState.cancelFaceDetection()
                            }

                            ThemedButton {
                                visible: !faceSection.faceReady
                                width: parent.width
                                text: qsTr("Install face model (≈5 MB)")
                                variant: "primary"
                                onClicked: root.Window.window.openAddonManager("face-model")
                            }
                        }

                        Column {
                            width: parent.width
                            spacing: 10
                            visible: root.selectedEffects.length > 0

                            Text {
                                width: parent.width
                                wrapMode: Text.WordWrap
                                text: qsTr("Scrub the playhead, set a value, then click the diamond to key it. With Auto-key on, dragging a slider also writes keys.")
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }

                            ThemedChip {
                                text: qsTr("Auto-key")
                                selected: EditorState.autoKeyEnabled
                                onClicked: EditorState.autoKeyEnabled = !EditorState.autoKeyEnabled
                            }
                        }

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

                                        // Booleans have nothing to interpolate, so they keep the
                                        // plain switch and stay off the keyframe strip.
                                        Row {
                                            visible: !!paramRow.modelData.isBoolean
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
                                                text: paramRow.modelData.value ? qsTr("On") : qsTr("Off")
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

                                        PropertyKeyframeRow {
                                            visible: !paramRow.modelData.isBoolean
                                            width: parent.width
                                            // `def` is the param's static value, which the row falls
                                            // back to whenever the track holds no keys.
                                            propDef: ({
                                                key: paramRow.modelData.prop,
                                                label: paramRow.modelData.label,
                                                def: paramRow.modelData.value,
                                                decimals: Math.abs(paramRow.modelData.max
                                                                   - paramRow.modelData.min) >= 10 ? 1 : 2
                                            })
                                            keyframeList: (paramRow.modelData.keyframes
                                                           && paramRow.modelData.keyframes.points) || []
                                            useSlider: true
                                            sliderFrom: paramRow.modelData.min
                                            sliderTo: paramRow.modelData.max
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
