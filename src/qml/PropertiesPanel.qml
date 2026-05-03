import QtQuick
import QtQuick.Controls.Basic
import Drift
import "components"

PanelFrame {
    id: root

    // selectedClipData is a QVariantMap; key the binding on an explicit revision
    // so nested fields such as effects refresh after project edits.
    property int clipDataRevision: 0
    readonly property var clip: {
        void clipDataRevision
        return EditorState.selectedClipData
    }
    readonly property bool hasSelection: !!clip && Object.keys(clip).length > 0
    readonly property var selectedEffects: EditorState.selectedClipEffects
    readonly property string clipKind: hasSelection ? (clip.kind || "") : ""
    readonly property bool hasTextStyle: hasSelection && clipKind === "text" && !!clip.textStyle
    readonly property var textStyle: hasTextStyle ? clip.textStyle : ({
                                                                       "fontFamily": "Sans Serif",
                                                                       "pixelSize": 64,
                                                                       "color": "#ffffffff",
                                                                       "bold": true,
                                                                       "italic": false,
                                                                       "align": "center",
                                                                       "outlineWidth": 0,
                                                                       "outlineColor": "#ff000000",
                                                                       "boxEnabled": false,
                                                                       "boxColor": "#80000000",
                                                                       "boxPadding": 8
                                                                   })
    property int activeTab: 0
    readonly property string currentTabId: tabsModel.get(activeTab).tabId

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
            if (fontFamilyBox)
                fontFamilyBox.currentIndex = Math.max(0, fontFamilyBox.model.indexOf(root.textStyle.fontFamily))
            if (pixelSizeField && !pixelSizeField.activeFocus)
                pixelSizeField.text = Number(root.textStyle.pixelSize).toString()
            if (textColorField && !textColorField.activeFocus)
                textColorField.text = root.textStyle.color
            if (outlineWidthField && !outlineWidthField.activeFocus)
                outlineWidthField.text = Number(root.textStyle.outlineWidth).toFixed(1)
            if (outlineColorField && !outlineColorField.activeFocus)
                outlineColorField.text = root.textStyle.outlineColor
            if (boxColorField && !boxColorField.activeFocus)
                boxColorField.text = root.textStyle.boxColor
            if (boxPaddingField && !boxPaddingField.activeFocus)
                boxPaddingField.text = Number(root.textStyle.boxPadding).toFixed(1)
        }
    }

    Connections {
        target: EditorState
        function onSelectionChanged() {
            root.clipDataRevision++
            root.refreshInspectorFields()
        }
        function onSelectedClipDataChanged() {
            root.clipDataRevision++
            root.refreshInspectorFields()
        }
        function onTracksChanged() {
            root.clipDataRevision++
            root.refreshInspectorFields()
        }
    }

    Component.onCompleted: root.refreshInspectorFields()

    ListModel {
        id: tabsModel
        ListElement { tabId: "general"; icon: 0; label: "General" }
        ListElement { tabId: "transform"; icon: 1; label: "Transform" }
        ListElement { tabId: "audio"; icon: 2; label: "Audio" }
        ListElement { tabId: "speed"; icon: 3; label: "Speed" }
        ListElement { tabId: "blending"; icon: 4; label: "Blending" }
        ListElement { tabId: "masks"; icon: 5; label: "Masks" }
        ListElement { tabId: "effects"; icon: 6; label: "Effects" }
    }
    property var tabIcons: [
        Theme.icons.folder,
        Theme.icons.maximize,
        Theme.icons.headphones,
        Theme.icons.zoomIn,
        Theme.icons.layers,
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
            text: "Click an element on the timeline to edit its properties"
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
                                TextField {
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
                                TextField {
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
                            TextArea {
                                id: textContentField
                                width: parent.width
                                height: 80
                                text: clip.textContent || ""
                                color: Theme.panelForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                                wrapMode: TextArea.Wrap
                                onEditingFinished: EditorState.setClipTextContent(
                                                       EditorState.selectedTrack, EditorState.selectedClip, text)
                            }
                        }

                        Column {
                            width: tabColumn.width
                            spacing: 8
                            visible: root.hasTextStyle

                            Text {
                                text: "Text style"
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }

                            Row {
                                width: parent.width
                                spacing: 6
                                Repeater {
                                    model: ["Title", "Lower third", "Subtitle"]
                                    delegate: Rectangle {
                                        required property string modelData
                                        width: presetLabel.implicitWidth + 16
                                        height: 26
                                        radius: Theme.radiusSm
                                        color: "transparent"
                                        border.width: 1
                                        border.color: Theme.panelBorder
                                        Text {
                                            id: presetLabel
                                            anchors.centerIn: parent
                                            text: modelData
                                            color: Theme.panelForeground
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeXs
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: EditorState.applyTextPreset(
                                                           EditorState.selectedTrack, EditorState.selectedClip,
                                                           modelData.toLowerCase())
                                        }
                                    }
                                }
                            }

                            ComboBox {
                                id: fontFamilyBox
                                width: parent.width
                                model: Qt.fontFamilies()
                                currentIndex: Math.max(0, model.indexOf(root.textStyle.fontFamily))
                                onActivated: EditorState.setTextStyle(
                                                 EditorState.selectedTrack, EditorState.selectedClip,
                                                 { fontFamily: currentText })
                            }

                            Row {
                                width: parent.width
                                spacing: 8

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: "Size"
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    TextField {
                                        id: pixelSizeField
                                        width: parent.width
                                        text: Number(root.textStyle.pixelSize).toString()
                                        color: Theme.panelForeground
                                        font.family: Theme.monoFontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        onEditingFinished: {
                                            const v = parseInt(text)
                                            if (!isNaN(v))
                                                EditorState.setTextStyle(EditorState.selectedTrack, EditorState.selectedClip,
                                                                         { pixelSize: v })
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
                                            color: root.textStyle.color
                                            border.width: 1
                                            border.color: Theme.panelBorder
                                        }
                                        TextField {
                                            id: textColorField
                                            width: 92
                                            text: root.textStyle.color
                                            color: Theme.panelForeground
                                            font.family: Theme.monoFontFamily
                                            font.pixelSize: Theme.fontSizeSm
                                            onEditingFinished: EditorState.setTextStyle(
                                                                   EditorState.selectedTrack, EditorState.selectedClip,
                                                                   { color: text })
                                        }
                                    }
                                }
                            }

                            Row {
                                width: parent.width
                                spacing: 6

                                Rectangle {
                                    width: 44
                                    height: 26
                                    radius: Theme.radiusSm
                                    color: root.textStyle.bold ? Theme.panelSecondaryBg : "transparent"
                                    border.width: 1
                                    border.color: Theme.panelBorder
                                    Text {
                                        anchors.centerIn: parent
                                        text: "Bold"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeXs
                                        color: Theme.panelForeground
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: EditorState.setTextStyle(
                                                       EditorState.selectedTrack, EditorState.selectedClip,
                                                       { bold: !root.textStyle.bold })
                                    }
                                }

                                Rectangle {
                                    width: 44
                                    height: 26
                                    radius: Theme.radiusSm
                                    color: root.textStyle.italic ? Theme.panelSecondaryBg : "transparent"
                                    border.width: 1
                                    border.color: Theme.panelBorder
                                    Text {
                                        anchors.centerIn: parent
                                        text: "Italic"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeXs
                                        color: Theme.panelForeground
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: EditorState.setTextStyle(
                                                       EditorState.selectedTrack, EditorState.selectedClip,
                                                       { italic: !root.textStyle.italic })
                                    }
                                }

                                Repeater {
                                    model: ["left", "center", "right"]
                                    delegate: Rectangle {
                                        required property string modelData
                                        width: 44
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
                                            onClicked: EditorState.setTextStyle(
                                                           EditorState.selectedTrack, EditorState.selectedClip,
                                                           { align: modelData })
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
                                        text: "Outline width"
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                    TextField {
                                        id: outlineWidthField
                                        width: parent.width
                                        text: Number(root.textStyle.outlineWidth).toFixed(1)
                                        color: Theme.panelForeground
                                        font.family: Theme.monoFontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        onEditingFinished: {
                                            const v = parseFloat(text)
                                            if (!isNaN(v))
                                                EditorState.setTextStyle(EditorState.selectedTrack, EditorState.selectedClip,
                                                                         { outlineWidth: v })
                                        }
                                    }
                                }

                                Column {
                                    width: (parent.width - parent.spacing) / 2
                                    spacing: 4
                                    Text {
                                        text: "Outline color"
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
                                        TextField {
                                            id: outlineColorField
                                            width: 92
                                            text: root.textStyle.outlineColor
                                            color: Theme.panelForeground
                                            font.family: Theme.monoFontFamily
                                            font.pixelSize: Theme.fontSizeSm
                                            onEditingFinished: EditorState.setTextStyle(
                                                                   EditorState.selectedTrack, EditorState.selectedClip,
                                                                   { outlineColor: text })
                                        }
                                    }
                                }
                            }

                            Row {
                                width: parent.width
                                spacing: 6

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
                                        onClicked: EditorState.setTextStyle(
                                                       EditorState.selectedTrack, EditorState.selectedClip,
                                                       { boxEnabled: !root.textStyle.boxEnabled })
                                    }
                                }

                                Rectangle {
                                    visible: root.textStyle.boxEnabled
                                    width: 24
                                    height: 24
                                    radius: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: root.textStyle.boxColor
                                    border.width: 1
                                    border.color: Theme.panelBorder
                                }
                                TextField {
                                    id: boxColorField
                                    visible: root.textStyle.boxEnabled
                                    width: 92
                                    text: root.textStyle.boxColor
                                    color: Theme.panelForeground
                                    font.family: Theme.monoFontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    onEditingFinished: EditorState.setTextStyle(
                                                           EditorState.selectedTrack, EditorState.selectedClip,
                                                           { boxColor: text })
                                }
                            }

                            Row {
                                width: parent.width
                                spacing: 8
                                visible: root.textStyle.boxEnabled

                                Text {
                                    text: "Box padding"
                                    color: Theme.mutedForeground
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                TextField {
                                    id: boxPaddingField
                                    width: 72
                                    text: Number(root.textStyle.boxPadding).toFixed(1)
                                    color: Theme.panelForeground
                                    font.family: Theme.monoFontFamily
                                    font.pixelSize: Theme.fontSizeSm
                                    onEditingFinished: {
                                        const v = parseFloat(text)
                                        if (!isNaN(v))
                                            EditorState.setTextStyle(
                                                EditorState.selectedTrack, EditorState.selectedClip, { boxPadding: v })
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
                                    TextField {
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
                                    TextField {
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
                        spacing: 8
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
                            spacing: 8
                            visible: root.clipKind !== "audio"

                            Row {
                                width: parent.width
                                spacing: 8
                                PropertyKeyframeRow {
                                    width: (parent.width - parent.spacing) / 2
                                    propDef: root.propOpacity
                                    keyframeList: (clip.keyframes && clip.keyframes.opacity && clip.keyframes.opacity.points) || []
                                    interpolationMode: (clip.keyframes && clip.keyframes.opacity && clip.keyframes.opacity.interpolation) || "linear"
                                }
                                PropertyKeyframeRow {
                                    width: (parent.width - parent.spacing) / 2
                                    propDef: root.propScale
                                    keyframeList: (clip.keyframes && clip.keyframes.scale && clip.keyframes.scale.points) || []
                                    interpolationMode: (clip.keyframes && clip.keyframes.scale && clip.keyframes.scale.interpolation) || "linear"
                                }
                            }
                            Row {
                                width: parent.width
                                spacing: 8
                                PropertyKeyframeRow {
                                    width: (parent.width - parent.spacing) / 2
                                    propDef: root.propPosX
                                    keyframeList: (clip.keyframes && clip.keyframes.posX && clip.keyframes.posX.points) || []
                                    interpolationMode: (clip.keyframes && clip.keyframes.posX && clip.keyframes.posX.interpolation) || "linear"
                                }
                                PropertyKeyframeRow {
                                    width: (parent.width - parent.spacing) / 2
                                    propDef: root.propPosY
                                    keyframeList: (clip.keyframes && clip.keyframes.posY && clip.keyframes.posY.points) || []
                                    interpolationMode: (clip.keyframes && clip.keyframes.posY && clip.keyframes.posY.interpolation) || "linear"
                                }
                            }
                            PropertyKeyframeRow {
                                width: parent.width
                                propDef: root.propRotation
                                keyframeList: (clip.keyframes && clip.keyframes.rotation && clip.keyframes.rotation.points) || []
                                interpolationMode: (clip.keyframes && clip.keyframes.rotation && clip.keyframes.rotation.interpolation) || "linear"
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

                    // ----- Speed (not implemented yet) ---------------------------------------
                    Text {
                        width: tabColumn.width
                        visible: root.currentTabId === "speed"
                        text: "Speed — coming soon"
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
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
                        ComboBox {
                            id: blendModeBox
                            visible: root.clipKind !== "audio"
                            width: parent.width
                            model: ["normal", "multiply", "screen", "overlay", "add", "darken", "lighten"]
                            currentIndex: Math.max(0, model.indexOf(clip.blendMode || "normal"))
                            onActivated: EditorState.setClipBlendMode(
                                             EditorState.selectedTrack, EditorState.selectedClip, model[currentIndex])
                        }
                    }

                    // ----- Masks (not implemented yet) -----------------------------------------
                    Text {
                        width: tabColumn.width
                        visible: root.currentTabId === "masks"
                        text: "Masks — coming soon"
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                    }

                    // ----- Effects ---------------------------------------------------------------
                    Column {
                        width: tabColumn.width
                        spacing: 10
                        visible: root.currentTabId === "effects"

                        Text {
                            width: parent.width
                            visible: root.selectedEffects.length === 0
                            text: "No effects yet. Use the + on an effect card, or drag one onto this clip."
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

                                Row {
                                    width: parent.width
                                    Text {
                                        text: effectCard.modelData.label
                                        color: Theme.panelForeground
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        width: parent.width - 24
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

                                Repeater {
                                    model: effectCard.modelData.params || []
                                    delegate: Row {
                                        required property var modelData
                                        width: tabColumn.width
                                        spacing: 8

                                        Text {
                                            width: 84
                                            text: modelData.label
                                            color: Theme.mutedForeground
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSizeXs
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        Slider {
                                            width: tabColumn.width - 84 - parent.spacing
                                            anchors.verticalCenter: parent.verticalCenter
                                            from: modelData.min
                                            to: modelData.max
                                            value: modelData.value
                                            onMoved: EditorState.setEffectParam(
                                                         EditorState.selectedTrack, EditorState.selectedClip,
                                                         effectCard.index, modelData.key, value)
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
    readonly property var propScale: { "key": "scale", "label": "Scale", "def": 1.0, "decimals": 2 }
    readonly property var propPosX: { "key": "posX", "label": "Position X", "def": 0.5, "decimals": 2 }
    readonly property var propPosY: { "key": "posY", "label": "Position Y", "def": 0.5, "decimals": 2 }
    readonly property var propRotation: { "key": "rotation", "label": "Rotation", "def": 0.0, "decimals": 1 }
    readonly property var propVolume: { "key": "volume", "label": "Volume", "def": 1.0, "decimals": 2 }

    function applyTrim(inPoint, outPoint) {
        if (!root.hasSelection || isNaN(inPoint) || isNaN(outPoint))
            return
        EditorState.setClipTrim(EditorState.selectedTrack, EditorState.selectedClip, inPoint, outPoint)
    }
}
