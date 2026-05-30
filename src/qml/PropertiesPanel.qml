import QtQuick
import QtQuick.Controls.Basic
import Drift
import "components"

PanelFrame {
    id: root

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
    readonly property bool hasTextStyle: hasSelection && clipKind === "text" && !!clip.textStyle
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
            startField.text = root.formatSeconds(root.clip.start)
        if (durationField && !durationField.activeFocus)
            durationField.text = root.formatSeconds(root.clip.duration)
        if (inPointField && !inPointField.activeFocus)
            inPointField.text = root.formatSeconds(root.clip.inPoint)
        if (outPointField && !outPointField.activeFocus)
            outPointField.text = root.formatSeconds(root.clip.outPoint)
        if (textContentField && !textContentField.activeFocus)
            textContentField.text = root.clip.textContent || ""
        if (blendModeBox)
            blendModeBox.currentIndex = Math.max(0, blendModeBox.model.indexOf(root.clip.blendMode || "normal"))
        if (root.hasTextStyle) {
            const s = root.textStyle
            if (pixelSizeField && !pixelSizeField.activeFocus)
                pixelSizeField.text = Number(s.pixelSize).toString()
            if (textColorField && !textColorField.activeFocus)
                textColorField.text = s.color
            if (lineHeightField && !lineHeightField.activeFocus)
                lineHeightField.text = Number(s.lineHeight).toFixed(2)
            if (letterSpacingField && !letterSpacingField.activeFocus)
                letterSpacingField.text = Number(s.letterSpacing).toFixed(1)
            if (outlineWidthField && !outlineWidthField.activeFocus)
                outlineWidthField.text = Number(s.outlineWidth).toFixed(1)
            if (outlineColorField && !outlineColorField.activeFocus)
                outlineColorField.text = s.outlineColor
            if (shadowOffsetXField && !shadowOffsetXField.activeFocus)
                shadowOffsetXField.text = Number(s.shadowOffsetX).toFixed(1)
            if (shadowOffsetYField && !shadowOffsetYField.activeFocus)
                shadowOffsetYField.text = Number(s.shadowOffsetY).toFixed(1)
            if (shadowBlurField && !shadowBlurField.activeFocus)
                shadowBlurField.text = Number(s.shadowBlur).toFixed(1)
            if (shadowOpacityField && !shadowOpacityField.activeFocus)
                shadowOpacityField.text = Number(s.shadowOpacity).toFixed(2)
            if (shadowColorField && !shadowColorField.activeFocus)
                shadowColorField.text = s.shadowColor
            if (boxColorField && !boxColorField.activeFocus)
                boxColorField.text = s.boxColor
            if (boxPaddingField && !boxPaddingField.activeFocus)
                boxPaddingField.text = Number(s.boxPadding).toFixed(1)
            if (boxRadiusField && !boxRadiusField.activeFocus)
                boxRadiusField.text = Number(s.boxRadius).toFixed(1)
            if (animInDurationField && !animInDurationField.activeFocus)
                animInDurationField.text = Number(s.animIn.duration).toFixed(2)
            if (animOutDurationField && !animOutDurationField.activeFocus)
                animOutDurationField.text = Number(s.animOut.duration).toFixed(2)
        }
    }

    function refreshTransitionFields() {
        if (!root.hasActiveTransition)
            return
        if (transitionDurationField && !transitionDurationField.activeFocus)
            transitionDurationField.text = Number(root.activeTransition.duration || 0.5).toFixed(2)
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
            const v = parseFloat(transitionDurationField.text)
            if (!isNaN(v)) {
                const current = Number(root.activeTransition.duration || 0.5)
                if (Math.abs(v - current) > 0.0001)
                    EditorState.setTransitionDuration(
                        root.transitionEditTrack, transitionId, v)
            }
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
        }
        function onSelectedClipDataChanged() {
            root.clipDataRevision++
            root.refreshInspectorFields()
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
    }
    property var tabIcons: [
        Theme.icons.folder,
        Theme.icons.maximize,
        Theme.icons.headphones,
        Theme.icons.zoomIn,
        Theme.icons.layers,
        Theme.icons.linkTwo,
        Theme.icons.grid,
        Theme.icons.wand
    ]

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
            text: "It's empty here"
            color: Theme.panelForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBase
            font.weight: Font.Medium
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: "Click a clip on the timeline to edit its properties"
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

            Flickable {
                id: tabFlick
                width: parent.width - Theme.tabRailWidth - 1
                height: parent.height
                contentWidth: width
                contentHeight: tabColumn.height + 24
                clip: true
                ScrollBar.vertical: AppScrollBar { }

                Column {
                    id: tabColumn
                    x: 12
                    y: 12
                    width: parent.width - 24
                    spacing: 12

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
                        spacing: 12
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
                                text: "Kind"
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }
                            Text {
                                text: root.clipKind.length > 0 ? root.clipKind : "—"
                                color: Theme.panelForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                            }
                        }

                        Row {
                            width: parent.width
                            spacing: 8

                            Column {
                                width: (parent.width - parent.spacing) / 2
                                spacing: 4
                                Text {
                                    text: "Start (s)"
                                    color: Theme.mutedForeground
                                    font.pixelSize: Theme.fontSizeXs
                                    font.family: Theme.fontFamily
                                }
                                ThemedTextField {
                                    id: startField
                                    width: parent.width
                                    text: root.formatSeconds(clip.start)
                                    color: Theme.panelForeground
                                    font.family: Theme.monoFontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    onEditingFinished: {
                                        const v = parseFloat(text)
                                        if (!isNaN(v))
                                            EditorState.setClipStart(EditorState.selectedTrack, EditorState.selectedClip, v)
                                    }
                                }
                            }

                            Column {
                                width: (parent.width - parent.spacing) / 2
                                spacing: 4
                                Text {
                                    text: "Duration (s)"
                                    color: Theme.mutedForeground
                                    font.pixelSize: Theme.fontSizeXs
                                    font.family: Theme.fontFamily
                                }
                                ThemedTextField {
                                    id: durationField
                                    width: parent.width
                                    text: root.formatSeconds(clip.duration)
                                    color: Theme.panelForeground
                                    font.family: Theme.monoFontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    onEditingFinished: {
                                        const v = parseFloat(text)
                                        if (!isNaN(v))
                                            EditorState.setClipDuration(EditorState.selectedTrack, EditorState.selectedClip, v)
                                    }
                                }
                            }
                        }

                        Column {
                            width: tabColumn.width
                            spacing: 4
                            visible: root.clipKind === "text"
                            Text {
                                text: "Text content"
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
                                text: "Presets"
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
                                text: "Font"
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
                                        text: "Weight"
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
                                        text: "Size"
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedTextField {
                                        id: pixelSizeField
                                        width: parent.width
                                        text: Number(root.textStyle.pixelSize).toString()
                                        color: Theme.panelForeground
                                        font.family: Theme.monoFontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        onEditingFinished: {
                                            const v = parseInt(text)
                                            if (!isNaN(v))
                                                root.setTextStyleKey("pixelSize", v)
                                        }
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
                                        text: "Color"
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    Row {
                                        spacing: 6
                                        Rectangle {
                                            width: 24
                                            height: 24
                                            radius: 4
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: root.textStyle.color
                                            border.width: 1
                                            border.color: Theme.panelBorder
                                        }
                                        ThemedTextField {
                                            id: textColorField
                                            width: 92
                                            text: root.textStyle.color
                                            color: Theme.panelForeground
                                            font.family: Theme.monoFontFamily
                                            font.pixelSize: Theme.fontSizeSm
                                            onEditingFinished: root.setTextStyleKey("color", text)
                                        }
                                    }
                                }

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: "Style"
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    Rectangle {
                                        width: 60
                                        height: 26
                                        radius: Theme.radiusSm
                                        // Greyed out for the single-face display fonts, which have no italic.
                                        opacity: root.familyHasItalic ? 1.0 : 0.4
                                        color: root.textStyle.italic ? Theme.panelSecondaryBg : "transparent"
                                        border.width: 1
                                        border.color: Theme.panelBorder
                                        Text {
                                            anchors.centerIn: parent
                                            text: "Italic"
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeXs
                                            font.italic: true
                                            color: Theme.panelForeground
                                        }
                                        ToolTip {
                                            visible: italicMouse.containsMouse && !root.familyHasItalic
                                            text: qsTr("%1 has no italic face").arg(root.textStyle.fontFamily)
                                        }
                                        MouseArea {
                                            id: italicMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            enabled: root.familyHasItalic
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.setTextStyleKey("italic", !root.textStyle.italic)
                                        }
                                    }
                                }
                            }

                            Row {
                                width: parent.width
                                spacing: 6

                                Repeater {
                                    model: ["left", "center", "right"]
                                    delegate: Rectangle {
                                        required property string modelData
                                        width: 34
                                        height: 26
                                        radius: Theme.radiusSm
                                        color: root.textStyle.align === modelData ? Theme.panelSecondaryBg : "transparent"
                                        border.width: 1
                                        border.color: Theme.panelBorder
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.charAt(0).toUpperCase()
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeXs
                                            color: Theme.panelForeground
                                        }
                                        ToolTip {
                                            visible: alignMouse.containsMouse
                                            text: qsTr("Align %1").arg(modelData)
                                        }
                                        MouseArea {
                                            id: alignMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.setTextStyleKey("align", modelData)
                                        }
                                    }
                                }

                                Rectangle {
                                    width: 1
                                    height: 26
                                    color: Theme.panelBorder
                                }

                                Repeater {
                                    model: ["top", "middle", "bottom"]
                                    delegate: Rectangle {
                                        required property string modelData
                                        width: 34
                                        height: 26
                                        radius: Theme.radiusSm
                                        color: root.textStyle.valign === modelData ? Theme.panelSecondaryBg : "transparent"
                                        border.width: 1
                                        border.color: Theme.panelBorder
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.charAt(0).toUpperCase()
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeXs
                                            color: Theme.panelForeground
                                        }
                                        ToolTip {
                                            visible: valignMouse.containsMouse
                                            text: qsTr("Vertical align %1").arg(modelData)
                                        }
                                        MouseArea {
                                            id: valignMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.setTextStyleKey("valign", modelData)
                                        }
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
                                        text: "Line height"
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedTextField {
                                        id: lineHeightField
                                        width: parent.width
                                        text: Number(root.textStyle.lineHeight).toFixed(2)
                                        color: Theme.panelForeground
                                        font.family: Theme.monoFontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        onEditingFinished: {
                                            const v = parseFloat(text)
                                            if (!isNaN(v))
                                                root.setTextStyleKey("lineHeight", v)
                                        }
                                    }
                                }

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: "Letter spacing"
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedTextField {
                                        id: letterSpacingField
                                        width: parent.width
                                        text: Number(root.textStyle.letterSpacing).toFixed(1)
                                        color: Theme.panelForeground
                                        font.family: Theme.monoFontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        onEditingFinished: {
                                            const v = parseFloat(text)
                                            if (!isNaN(v))
                                                root.setTextStyleKey("letterSpacing", v)
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                width: 96
                                height: 26
                                radius: Theme.radiusSm
                                color: root.textStyle.wordWrap ? Theme.panelSecondaryBg : "transparent"
                                border.width: 1
                                border.color: Theme.panelBorder
                                Text {
                                    anchors.centerIn: parent
                                    text: "Word wrap"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    color: Theme.panelForeground
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.setTextStyleKey("wordWrap", !root.textStyle.wordWrap)
                                }
                            }

                            Text {
                                text: "Outline"
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
                                        text: "Width"
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedTextField {
                                        id: outlineWidthField
                                        width: parent.width
                                        text: Number(root.textStyle.outlineWidth).toFixed(1)
                                        color: Theme.panelForeground
                                        font.family: Theme.monoFontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        onEditingFinished: {
                                            const v = parseFloat(text)
                                            if (!isNaN(v))
                                                root.setTextStyleKey("outlineWidth", v)
                                        }
                                    }
                                }

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: "Color"
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    Row {
                                        spacing: 6
                                        Rectangle {
                                            width: 24
                                            height: 24
                                            radius: 4
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: root.textStyle.outlineColor
                                            border.width: 1
                                            border.color: Theme.panelBorder
                                        }
                                        ThemedTextField {
                                            id: outlineColorField
                                            width: 92
                                            text: root.textStyle.outlineColor
                                            color: Theme.panelForeground
                                            font.family: Theme.monoFontFamily
                                            font.pixelSize: Theme.fontSizeSm
                                            onEditingFinished: root.setTextStyleKey("outlineColor", text)
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                width: 96
                                height: 26
                                radius: Theme.radiusSm
                                color: root.textStyle.shadowEnabled ? Theme.panelSecondaryBg : "transparent"
                                border.width: 1
                                border.color: Theme.panelBorder
                                Text {
                                    anchors.centerIn: parent
                                    text: "Shadow"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    color: Theme.panelForeground
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.setTextStyleKey("shadowEnabled", !root.textStyle.shadowEnabled)
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
                                        text: "Offset X"
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedTextField {
                                        id: shadowOffsetXField
                                        width: parent.width
                                        text: Number(root.textStyle.shadowOffsetX).toFixed(1)
                                        color: Theme.panelForeground
                                        font.family: Theme.monoFontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        onEditingFinished: {
                                            const v = parseFloat(text)
                                            if (!isNaN(v))
                                                root.setTextStyleKey("shadowOffsetX", v)
                                        }
                                    }
                                }

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: "Offset Y"
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedTextField {
                                        id: shadowOffsetYField
                                        width: parent.width
                                        text: Number(root.textStyle.shadowOffsetY).toFixed(1)
                                        color: Theme.panelForeground
                                        font.family: Theme.monoFontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        onEditingFinished: {
                                            const v = parseFloat(text)
                                            if (!isNaN(v))
                                                root.setTextStyleKey("shadowOffsetY", v)
                                        }
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
                                        text: "Blur"
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedTextField {
                                        id: shadowBlurField
                                        width: parent.width
                                        text: Number(root.textStyle.shadowBlur).toFixed(1)
                                        color: Theme.panelForeground
                                        font.family: Theme.monoFontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        onEditingFinished: {
                                            const v = parseFloat(text)
                                            if (!isNaN(v))
                                                root.setTextStyleKey("shadowBlur", v)
                                        }
                                    }
                                }

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: "Opacity"
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedTextField {
                                        id: shadowOpacityField
                                        width: parent.width
                                        text: Number(root.textStyle.shadowOpacity).toFixed(2)
                                        color: Theme.panelForeground
                                        font.family: Theme.monoFontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        onEditingFinished: {
                                            const v = parseFloat(text)
                                            if (!isNaN(v))
                                                root.setTextStyleKey("shadowOpacity", v)
                                        }
                                    }
                                }
                            }

                            Row {
                                width: parent.width
                                spacing: 6
                                visible: root.textStyle.shadowEnabled

                                Text {
                                    text: "Shadow color"
                                    color: Theme.mutedForeground
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Rectangle {
                                    width: 24
                                    height: 24
                                    radius: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: root.textStyle.shadowColor
                                    border.width: 1
                                    border.color: Theme.panelBorder
                                }
                                ThemedTextField {
                                    id: shadowColorField
                                    width: 92
                                    text: root.textStyle.shadowColor
                                    color: Theme.panelForeground
                                    font.family: Theme.monoFontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    onEditingFinished: root.setTextStyleKey("shadowColor", text)
                                }
                            }

                            Rectangle {
                                width: 96
                                height: 26
                                radius: Theme.radiusSm
                                color: root.textStyle.boxEnabled ? Theme.panelSecondaryBg : "transparent"
                                border.width: 1
                                border.color: Theme.panelBorder
                                Text {
                                    anchors.centerIn: parent
                                    text: "Background"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    color: Theme.panelForeground
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.setTextStyleKey("boxEnabled", !root.textStyle.boxEnabled)
                                }
                            }

                            Row {
                                width: parent.width
                                spacing: 6
                                visible: root.textStyle.boxEnabled

                                Rectangle {
                                    width: 24
                                    height: 24
                                    radius: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: root.textStyle.boxColor
                                    border.width: 1
                                    border.color: Theme.panelBorder
                                }
                                ThemedTextField {
                                    id: boxColorField
                                    width: 92
                                    text: root.textStyle.boxColor
                                    color: Theme.panelForeground
                                    font.family: Theme.monoFontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    onEditingFinished: root.setTextStyleKey("boxColor", text)
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
                                        text: "Padding"
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedTextField {
                                        id: boxPaddingField
                                        width: parent.width
                                        text: Number(root.textStyle.boxPadding).toFixed(1)
                                        color: Theme.panelForeground
                                        font.family: Theme.monoFontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        onEditingFinished: {
                                            const v = parseFloat(text)
                                            if (!isNaN(v))
                                                root.setTextStyleKey("boxPadding", v)
                                        }
                                    }
                                }

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: "Corner radius"
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedTextField {
                                        id: boxRadiusField
                                        width: parent.width
                                        text: Number(root.textStyle.boxRadius).toFixed(1)
                                        color: Theme.panelForeground
                                        font.family: Theme.monoFontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        onEditingFinished: {
                                            const v = parseFloat(text)
                                            if (!isNaN(v))
                                                root.setTextStyleKey("boxRadius", v)
                                        }
                                    }
                                }
                            }

                            Text {
                                text: "Animation"
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }

                            Row {
                                width: parent.width
                                spacing: 6

                                Text {
                                    text: "In"
                                    width: 20
                                    color: Theme.mutedForeground
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                ThemedComboBox {
                                    id: animInKindBox
                                    width: (parent.width - 20 - parent.spacing * 3 - 56) / 2
                                    model: root.animKindLabels
                                    currentIndex: Math.max(0, root.animKinds.indexOf(root.textStyle.animIn.kind))
                                    onActivated: root.setTextAnim("animIn", "kind", root.animKinds[currentIndex])
                                }
                                ThemedComboBox {
                                    id: animInEaseBox
                                    width: (parent.width - 20 - parent.spacing * 3 - 56) / 2
                                    model: root.easeLabels
                                    currentIndex: Math.max(0, root.easeKinds.indexOf(root.textStyle.animIn.ease))
                                    onActivated: root.setTextAnim("animIn", "ease", root.easeKinds[currentIndex])
                                }
                                ThemedTextField {
                                    id: animInDurationField
                                    width: 56
                                    text: Number(root.textStyle.animIn.duration).toFixed(2)
                                    color: Theme.panelForeground
                                    font.family: Theme.monoFontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    onEditingFinished: {
                                        const v = parseFloat(text)
                                        if (!isNaN(v))
                                            root.setTextAnim("animIn", "duration", v)
                                    }
                                }
                            }

                            Row {
                                width: parent.width
                                spacing: 6

                                Text {
                                    text: "Out"
                                    width: 20
                                    color: Theme.mutedForeground
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                ThemedComboBox {
                                    id: animOutKindBox
                                    width: (parent.width - 20 - parent.spacing * 3 - 56) / 2
                                    model: root.animKindLabels
                                    currentIndex: Math.max(0, root.animKinds.indexOf(root.textStyle.animOut.kind))
                                    onActivated: root.setTextAnim("animOut", "kind", root.animKinds[currentIndex])
                                }
                                ThemedComboBox {
                                    id: animOutEaseBox
                                    width: (parent.width - 20 - parent.spacing * 3 - 56) / 2
                                    model: root.easeLabels
                                    currentIndex: Math.max(0, root.easeKinds.indexOf(root.textStyle.animOut.ease))
                                    onActivated: root.setTextAnim("animOut", "ease", root.easeKinds[currentIndex])
                                }
                                ThemedTextField {
                                    id: animOutDurationField
                                    width: 56
                                    text: Number(root.textStyle.animOut.duration).toFixed(2)
                                    color: Theme.panelForeground
                                    font.family: Theme.monoFontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    onEditingFinished: {
                                        const v = parseFloat(text)
                                        if (!isNaN(v))
                                            root.setTextAnim("animOut", "duration", v)
                                    }
                                }
                            }
                        }

                        Column {
                            width: tabColumn.width
                            spacing: 8
                            visible: root.clipKind !== "text"

                            Text {
                                text: "Trim"
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
                                        text: "In point (s)"
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedTextField {
                                        id: inPointField
                                        width: parent.width
                                        text: root.formatSeconds(clip.inPoint)
                                        color: Theme.panelForeground
                                        font.family: Theme.monoFontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        onEditingFinished: applyTrim(parseFloat(text), clip.outPoint)
                                    }
                                }

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: "Out point (s)"
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    ThemedTextField {
                                        id: outPointField
                                        width: parent.width
                                        text: root.formatSeconds(clip.outPoint)
                                        color: Theme.panelForeground
                                        font.family: Theme.monoFontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        onEditingFinished: applyTrim(clip.inPoint, parseFloat(text))
                                    }
                                }
                            }
                        }

                        Column {
                            width: tabColumn.width
                            spacing: 4
                            visible: clip.path !== undefined && clip.path.length > 0

                            Text {
                                text: "Path"
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }

                            Text {
                                text: clip.path || "—"
                                color: Theme.panelForeground
                                font.family: Theme.monoFontFamily
                                font.pixelSize: Theme.fontSizeSm
                                width: parent.width
                                wrapMode: Text.WrapAnywhere
                            }
                        }
                    }

                    // ----- Transform -------------------------------------------------------
                    Column {
                        width: tabColumn.width
                        spacing: 10
                        visible: root.currentTabId === "transform"

                        Text {
                            visible: root.clipKind === "audio"
                            text: "Not applicable to audio clips"
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
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
                        spacing: 8
                        visible: root.currentTabId === "audio"

                        Text {
                            visible: root.clipKind !== "audio" && root.clipKind !== "video"
                            text: "No audio on this clip"
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
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
                    }

                    // ----- Speed ---------------------------------------------------------------
                    Column {
                        width: tabColumn.width
                        spacing: 8
                        visible: root.currentTabId === "speed"

                        Text {
                            visible: root.clipKind !== "video" && root.clipKind !== "audio"
                            text: "Speed applies to video and audio clips"
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                        }

                        Text {
                            visible: root.clipKind === "video" || root.clipKind === "audio"
                            text: "Playback speed"
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
                                text: "Fade in"
                                variant: "secondary"
                                onClicked: EditorState.setClipFade(
                                               EditorState.selectedTrack, EditorState.selectedClip,
                                               0.5, clip.fadeOut || 0)
                            }
                            ThemedButton {
                                text: "Fade out"
                                variant: "secondary"
                                onClicked: EditorState.setClipFade(
                                               EditorState.selectedTrack, EditorState.selectedClip,
                                               clip.fadeIn || 0, 0.5)
                            }
                            ThemedButton {
                                text: "Clear"
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
                            text: "Fade curve"
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
                        spacing: 12
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
                                text: "No transition after this clip. Add one at the cut to the next clip."
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                            }

                            ThemedButton {
                                text: "Add crossfade (0.5 s)"
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
                                    text: "Kind"
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
                                    text: "Duration (s)"
                                    color: Theme.mutedForeground
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                }
                                ThemedTextField {
                                    id: transitionDurationField
                                    width: parent.width
                                    text: "0.50"
                                    color: Theme.panelForeground
                                    font.family: Theme.monoFontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    onEditingFinished: root.commitTransitionDuration()

                                    function commitTransitionDuration() {
                                        if (!root.hasActiveTransition)
                                            return
                                        const v = parseFloat(text)
                                        if (!isNaN(v))
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

                                    Switch {
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
                                text: "Remove transition"
                                variant: "destructive"
                                onClicked: EditorState.removeTransition(
                                               root.transitionEditTrack, root.activeTransition.id)
                            }
                        }
                    }

                    // ----- Blending ------------------------------------------------------------
                    Column {
                        width: tabColumn.width
                        spacing: 4
                        visible: root.currentTabId === "blending"

                        Text {
                            visible: root.clipKind === "audio"
                            text: "Not applicable to audio clips"
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                        }

                        Text {
                            visible: root.clipKind !== "audio"
                            text: "Blend mode"
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                        }
                        ThemedComboBox {
                            id: blendModeBox
                            visible: root.clipKind !== "audio"
                            width: parent.width
                            model: ["normal", "multiply", "screen", "overlay", "add", "darken", "lighten"]
                            currentIndex: Math.max(0, model.indexOf(clip.blendMode || "normal"))
                            onActivated: EditorState.setClipBlendMode(
                                             EditorState.selectedTrack, EditorState.selectedClip, model[currentIndex])
                        }
                    }

                    // ----- Masks ---------------------------------------------------------------
                    Column {
                        width: tabColumn.width
                        spacing: 8
                        visible: root.currentTabId === "masks"

                        Text {
                            visible: root.clipKind === "audio" || root.clipKind === "text"
                            text: "Masks apply to visual clips"
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                        }

                        ThemedComboBox {
                            id: maskShapeBox
                            visible: root.clipKind !== "audio" && root.clipKind !== "text"
                            width: parent.width
                            model: ["none", "rectangle", "ellipse", "star", "heart", "bars", "freeform"]
                            currentIndex: Math.max(0, model.indexOf((clip.mask && clip.mask.shape) || "none"))
                            onActivated: {
                                const mask = Object.assign({}, clip.mask || {})
                                mask.shape = model[currentIndex]
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
                                     && clip.mask && clip.mask.shape !== "none"
                            Text {
                                text: "Invert"
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Switch {
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
                        spacing: 10
                        visible: root.currentTabId === "effects"

                        Text {
                            width: parent.width
                            visible: root.selectedEffects.length === 0
                            text: "No effects yet. Drag a preset from the Effects library onto this clip, or click a preset card."
                            wrapMode: Text.WordWrap
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
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
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        IconButton {
                                            icon: Theme.icons.x
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

                                        Switch {
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
}
