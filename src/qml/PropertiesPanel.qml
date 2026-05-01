import QtQuick
import QtQuick.Controls.Basic
import Drift
import "components"

PanelFrame {
    id: root

    readonly property var clip: EditorState.selectedClipData
    readonly property bool hasSelection: Object.keys(clip).length > 0

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
            text: "Properties"
            color: Theme.mutedForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
        }
    }

    Text {
        anchors.centerIn: parent
        visible: !root.hasSelection
        text: "No clip selected"
        color: Theme.mutedForeground
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSm
    }

    Flickable {
        anchors.fill: parent
        anchors.topMargin: Theme.panelHeaderHeight
        contentHeight: propsColumn.height + 24
        clip: true
        visible: root.hasSelection
        ScrollBar.vertical: AppScrollBar { }

        Column {
            id: propsColumn
            x: 12
            y: 12
            width: parent.width - 24
            spacing: 12

            Text {
                text: clip.name || ""
                color: Theme.panelForeground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBase
                font.weight: Font.Medium
                width: parent.width
                elide: Text.ElideRight
            }

            Column {
                width: propsColumn.width
                spacing: 4
                Text {
                    text: "Kind"
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }
                Text {
                    text: clip.kind || "—"
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
                        width: parent.width
                        text: (clip.start || 0).toFixed(2)
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
                        width: parent.width
                        text: (clip.duration || 0).toFixed(2)
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
                width: propsColumn.width
                spacing: 4
                visible: clip.kind === "text"
                Text {
                    text: "Text content"
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }
                TextArea {
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
                width: propsColumn.width
                spacing: 8
                visible: clip.kind === "text" && clip.textStyle !== undefined

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
                    width: parent.width
                    model: Qt.fontFamilies()
                    currentIndex: Math.max(0, model.indexOf(clip.textStyle.fontFamily))
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
                            width: parent.width
                            text: clip.textStyle.pixelSize.toString()
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
                                color: clip.textStyle.color
                                border.width: 1
                                border.color: Theme.panelBorder
                            }
                            TextField {
                                width: 92
                                text: clip.textStyle.color
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
                        color: clip.textStyle.bold ? Theme.panelSecondaryBg : "transparent"
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
                                           { bold: !clip.textStyle.bold })
                        }
                    }

                    Rectangle {
                        width: 44
                        height: 26
                        radius: Theme.radiusSm
                        color: clip.textStyle.italic ? Theme.panelSecondaryBg : "transparent"
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
                                           { italic: !clip.textStyle.italic })
                        }
                    }

                    Repeater {
                        model: ["left", "center", "right"]
                        delegate: Rectangle {
                            required property string modelData
                            width: 44
                            height: 26
                            radius: Theme.radiusSm
                            color: clip.textStyle.align === modelData ? Theme.panelSecondaryBg : "transparent"
                            border.width: 1
                            border.color: Theme.panelBorder
                            Text {
                                anchors.centerIn: parent
                                text: modelData.charAt(0).toUpperCase()
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                color: Theme.panelForeground
                            }
                            MouseArea {
                                anchors.fill: parent
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
                            width: parent.width
                            text: clip.textStyle.outlineWidth.toFixed(1)
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
                                color: clip.textStyle.outlineColor
                                border.width: 1
                                border.color: Theme.panelBorder
                            }
                            TextField {
                                width: 92
                                text: clip.textStyle.outlineColor
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
                        color: clip.textStyle.boxEnabled ? Theme.panelSecondaryBg : "transparent"
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
                                           { boxEnabled: !clip.textStyle.boxEnabled })
                        }
                    }

                    Rectangle {
                        visible: clip.textStyle.boxEnabled
                        width: 24
                        height: 24
                        radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: clip.textStyle.boxColor
                        border.width: 1
                        border.color: Theme.panelBorder
                    }
                    TextField {
                        visible: clip.textStyle.boxEnabled
                        width: 92
                        text: clip.textStyle.boxColor
                        color: Theme.panelForeground
                        font.family: Theme.monoFontFamily
                        font.pixelSize: Theme.fontSizeSm
                        onEditingFinished: EditorState.setTextStyle(
                                               EditorState.selectedTrack, EditorState.selectedClip,
                                               { boxColor: text })
                    }
                }
            }

            Column {
                width: propsColumn.width
                spacing: 4
                visible: clip.kind !== undefined && clip.kind !== "audio"

                Text {
                    text: "Blend mode"
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }
                ComboBox {
                    width: parent.width
                    model: ["normal", "multiply", "screen", "overlay", "add", "darken", "lighten"]
                    currentIndex: Math.max(0, model.indexOf(clip.blendMode))
                    onActivated: EditorState.setClipBlendMode(
                                     EditorState.selectedTrack, EditorState.selectedClip, model[currentIndex])
                }
            }

            Column {
                width: propsColumn.width
                spacing: 8
                visible: clip.keyframes !== undefined

                Text {
                    text: "Transform"
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }

                Repeater {
                    model: root.animatablePropsFor(clip.kind)
                    delegate: Row {
                        id: propRow
                        required property var modelData
                        width: propsColumn.width
                        spacing: 8

                        readonly property var keyframeList: (clip.keyframes && clip.keyframes[modelData.key]) || []
                        readonly property var activeKey: root.keyframeAtPlayhead(keyframeList)
                        readonly property real currentValue: root.valueAtPlayhead(keyframeList, modelData.def)

                        Text {
                            width: 80
                            text: modelData.label
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        KeyframeDiamond {
                            anchors.verticalCenter: parent.verticalCenter
                            hasKey: propRow.activeKey !== null
                            onToggled: {
                                if (propRow.activeKey) {
                                    EditorState.removeClipKeyframe(
                                        EditorState.selectedTrack, EditorState.selectedClip, modelData.key,
                                        propRow.activeKey.seconds)
                                } else {
                                    EditorState.setClipKeyframe(
                                        EditorState.selectedTrack, EditorState.selectedClip, modelData.key,
                                        EditorState.playheadSeconds, propRow.currentValue)
                                }
                            }
                        }

                        TextField {
                            width: propsColumn.width - 80 - 20 - parent.spacing * 2
                            text: propRow.currentValue.toFixed(modelData.decimals)
                            color: Theme.panelForeground
                            font.family: Theme.monoFontFamily
                            font.pixelSize: Theme.fontSizeSm
                            onEditingFinished: {
                                const v = parseFloat(text)
                                if (!isNaN(v))
                                    EditorState.setClipKeyframe(
                                        EditorState.selectedTrack, EditorState.selectedClip, modelData.key,
                                        EditorState.playheadSeconds, v)
                            }
                        }
                    }
                }
            }

            Column {
                width: propsColumn.width
                spacing: 8
                visible: clip.effects !== undefined && clip.kind !== "text"

                Row {
                    width: parent.width
                    Text {
                        text: "Effects"
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Repeater {
                    model: clip.effects || []
                    delegate: Column {
                        id: effectCard
                        required property var modelData
                        required property int index
                        width: propsColumn.width
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
                                onClicked: EditorState.removeEffect(
                                               EditorState.selectedTrack, EditorState.selectedClip,
                                               effectCard.index)
                            }
                        }

                        Repeater {
                            model: effectCard.modelData.params || []
                            delegate: Row {
                                required property var modelData
                                width: propsColumn.width
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
                                    width: propsColumn.width - 84 - parent.spacing
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

            Column {
                width: propsColumn.width
                spacing: 8
                visible: clip.kind !== "text"

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
                            width: parent.width
                            text: (clip.inPoint || 0).toFixed(2)
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
                            width: parent.width
                            text: (clip.outPoint || 0).toFixed(2)
                            color: Theme.panelForeground
                            font.family: Theme.monoFontFamily
                            font.pixelSize: Theme.fontSizeSm
                            onEditingFinished: applyTrim(clip.inPoint, parseFloat(text))
                        }
                    }
                }
            }

            Column {
                width: propsColumn.width
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
    }

    function applyTrim(inPoint, outPoint) {
        if (!root.hasSelection || isNaN(inPoint) || isNaN(outPoint))
            return
        EditorState.setClipTrim(EditorState.selectedTrack, EditorState.selectedClip, inPoint, outPoint)
    }

    function animatablePropsFor(kind) {
        if (kind === "audio")
            return [{ key: "volume", label: "Volume", def: 1.0, decimals: 2 }]

        const props = [
            { key: "opacity", label: "Opacity", def: 1.0, decimals: 2 },
            { key: "posX", label: "Position X", def: 0.5, decimals: 2 },
            { key: "posY", label: "Position Y", def: 0.5, decimals: 2 },
            { key: "scale", label: "Scale", def: 1.0, decimals: 2 },
            { key: "rotation", label: "Rotation", def: 0.0, decimals: 1 },
        ]
        if (kind === "video")
            props.push({ key: "volume", label: "Volume", def: 1.0, decimals: 2 })
        return props
    }

    // Linear interpolation over a {seconds, value} keyframe list, mirroring
    // drift::KeyframeTrack<double>::evaluateAt's default (Linear) mode.
    function valueAtPlayhead(keyframeList, defaultValue) {
        if (!keyframeList || keyframeList.length === 0)
            return defaultValue

        const t = EditorState.playheadSeconds
        let prev = null
        let next = null
        for (const kf of keyframeList) {
            if (kf.seconds <= t && (!prev || kf.seconds > prev.seconds))
                prev = kf
            if (kf.seconds >= t && (!next || kf.seconds < next.seconds))
                next = kf
        }
        if (prev && next) {
            if (next.seconds === prev.seconds)
                return prev.value
            const frac = (t - prev.seconds) / (next.seconds - prev.seconds)
            return prev.value + (next.value - prev.value) * frac
        }
        if (prev)
            return prev.value
        if (next)
            return next.value
        return defaultValue
    }

    // Finds a keyframe within one frame (1/30s) of the playhead, or null.
    function keyframeAtPlayhead(keyframeList) {
        if (!keyframeList)
            return null

        const t = EditorState.playheadSeconds
        const tolerance = 1 / 30
        let best = null
        for (const kf of keyframeList) {
            const delta = Math.abs(kf.seconds - t)
            if (delta <= tolerance && (!best || delta < Math.abs(best.seconds - t)))
                best = kf
        }
        return best
    }
}
