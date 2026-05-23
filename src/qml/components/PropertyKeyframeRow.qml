import QtQuick
import QtQuick.Controls.Basic
import Drift

// One animatable-property control for the clip inspector: keyframe diamond +
// label on top, value field / slider below. Tracks playhead and project edits
// (including WYSIWYG preview drags) so the readout stays live.
Column {
    id: root

    required property var propDef       // {key, label, def, decimals}
    required property var keyframeList  // [{seconds, value}, ...] for this property
    property string interpolationMode: "linear"

    // When true, the value is edited through a slider + readout instead of a
    // numeric text field. Drags are coalesced into a single undo entry via
    // begin/commit preview drag.
    property bool useSlider: false
    property real sliderFrom: 0
    property real sliderTo: 1
    property bool percent: false
    property string unit: "" // e.g. "°" — appended to the numeric readout

    spacing: 4

    // Bumped whenever external state that affects the displayed value changes.
    // TextField/Slider bindings are fragile once the user interacts; this forces
    // a refresh from WYSIWYG preview drags and playhead moves.
    property int valueRevision: 0
    property bool editing: false
    property real liveValue: 0

    readonly property var activeKey: {
        void valueRevision
        return keyframeAtPlayhead()
    }
    readonly property real currentValue: {
        void valueRevision
        void keyframeList
        void EditorState.playheadSeconds
        return valueAtPlayhead()
    }
    readonly property real displayedValue: editing ? liveValue : currentValue

    function bumpValue() {
        valueRevision++
        syncEditors()
    }

    function syncEditors() {
        if (editing)
            return
        if (useSlider) {
            if (!valueSlider.pressed)
                valueSlider.value = currentValue
        } else if (!valueField.activeFocus) {
            valueField.text = formatValue(currentValue)
        }
    }

    function formatValue(v) {
        if (percent)
            return Math.round(v * 100).toString()
        return Number(v).toFixed(propDef.decimals)
    }

    function parseInput(text) {
        const v = parseFloat(text)
        if (isNaN(v))
            return NaN
        return percent ? v / 100 : v
    }

    function displayReadout(v) {
        if (percent)
            return Math.round(v * 100) + "%"
        const body = Number(v).toFixed(propDef.decimals)
        return unit.length > 0 ? body + unit : body
    }

    // Linear interpolation over keyframeList, mirroring
    // drift::KeyframeTrack<double>::evaluateAt's default (Linear) mode.
    // Hold mode is approximated here by snapping to the previous key.
    function valueAtPlayhead() {
        if (!keyframeList || keyframeList.length === 0)
            return propDef.def

        const t = EditorState.playheadSeconds
        const hold = interpolationMode === "hold"
        let prev = null
        let next = null
        for (let i = 0; i < keyframeList.length; ++i) {
            const kf = keyframeList[i]
            if (kf.seconds <= t && (!prev || kf.seconds > prev.seconds))
                prev = kf
            if (kf.seconds >= t && (!next || kf.seconds < next.seconds))
                next = kf
        }
        if (prev && next) {
            if (next.seconds === prev.seconds || hold)
                return prev.value
            const frac = (t - prev.seconds) / (next.seconds - prev.seconds)
            return prev.value + (next.value - prev.value) * frac
        }
        if (prev)
            return prev.value
        if (next)
            return next.value
        return propDef.def
    }

    function keyframeAtPlayhead() {
        if (!keyframeList)
            return null

        const t = EditorState.playheadSeconds
        const tolerance = 1 / 30
        let best = null
        for (let i = 0; i < keyframeList.length; ++i) {
            const kf = keyframeList[i]
            const delta = Math.abs(kf.seconds - t)
            if (delta <= tolerance && (!best || delta < Math.abs(best.seconds - t)))
                best = kf
        }
        return best
    }

    function commitValue(v) {
        if (isNaN(v))
            return
        EditorState.setClipKeyframe(
            EditorState.selectedTrack, EditorState.selectedClip, root.propDef.key,
            EditorState.playheadSeconds, v)
    }

    function setInterpolation(mode) {
        EditorState.setKeyframeInterpolation(
            EditorState.selectedTrack, EditorState.selectedClip, root.propDef.key, mode)
    }

    Connections {
        target: EditorState
        function onSelectedClipDataChanged() { root.bumpValue() }
        function onPlayheadSecondsChanged() { root.bumpValue() }
        function onTracksChanged() { root.bumpValue() }
    }

    Component.onCompleted: syncEditors()
    onKeyframeListChanged: bumpValue()
    onInterpolationModeChanged: bumpValue()

    Row {
        width: parent.width
        spacing: 6

        KeyframeDiamond {
            anchors.verticalCenter: parent.verticalCenter
            hasKey: root.activeKey !== null
            onToggled: {
                if (root.activeKey) {
                    EditorState.removeClipKeyframe(
                        EditorState.selectedTrack, EditorState.selectedClip, root.propDef.key,
                        root.activeKey.seconds)
                } else {
                    commitValue(root.currentValue)
                }
            }
        }

        Text {
            text: root.propDef.label
            color: Theme.mutedForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            width: Math.max(40, parent.width - 90)
        }

        Rectangle {
            width: 30
            height: 18
            radius: Theme.radiusSm
            color: root.interpolationMode === "linear" ? Theme.panelSecondaryBg : "transparent"
            border.width: 1
            border.color: Theme.panelBorder
            anchors.verticalCenter: parent.verticalCenter
            Text {
                anchors.centerIn: parent
                text: "Lin"
                color: Theme.panelForeground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.setInterpolation("linear")
            }
        }

        Rectangle {
            width: 34
            height: 18
            radius: Theme.radiusSm
            color: root.interpolationMode === "hold" ? Theme.panelSecondaryBg : "transparent"
            border.width: 1
            border.color: Theme.panelBorder
            anchors.verticalCenter: parent.verticalCenter
            Text {
                anchors.centerIn: parent
                text: "Hold"
                color: Theme.panelForeground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.setInterpolation("hold")
            }
        }
    }

    // --- Numeric field (default) ---------------------------------------------
    Rectangle {
        visible: !root.useSlider
        width: root.width
        height: 30
        radius: Theme.radiusSm
        color: Theme.panelAccent

        TextField {
            id: valueField
            anchors.fill: parent
            anchors.margins: 1
            verticalAlignment: TextInput.AlignVCenter
            text: root.formatValue(root.currentValue)
            color: Theme.panelForeground
            font.family: Theme.monoFontFamily
            font.pixelSize: Theme.fontSizeSm
            leftPadding: 8
            rightPadding: root.unit.length > 0 || root.percent ? 22 : 8
            background: Item {}
            onEditingFinished: {
                const v = root.parseInput(text)
                root.editing = false
                if (!isNaN(v))
                    root.commitValue(v)
                text = root.formatValue(root.currentValue)
            }
            onActiveFocusChanged: {
                if (activeFocus) {
                    root.editing = true
                    root.liveValue = root.currentValue
                } else {
                    root.editing = false
                    text = root.formatValue(root.currentValue)
                }
            }
            onTextEdited: {
                const v = root.parseInput(text)
                if (!isNaN(v))
                    root.liveValue = v
            }
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            visible: root.unit.length > 0 || root.percent
            text: root.percent ? "%" : root.unit
            color: Theme.mutedForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
        }
    }

    // --- Slider (opt-in) -----------------------------------------------------
    Row {
        visible: root.useSlider
        width: root.width
        spacing: 8

        ThemedSlider {
            id: valueSlider
            width: root.width - readout.width - parent.spacing
            anchors.verticalCenter: parent.verticalCenter
            from: root.sliderFrom
            to: root.sliderTo
            value: root.currentValue
            onMoved: {
                root.liveValue = value
                EditorState.previewSetClipKeyframe(
                    EditorState.selectedTrack, EditorState.selectedClip, root.propDef.key,
                    EditorState.playheadSeconds, value)
            }
            onPressedChanged: {
                if (pressed) {
                    root.editing = true
                    root.liveValue = value
                    EditorState.beginPreviewDrag(qsTr("Edit %1").arg(root.propDef.label))
                } else {
                    EditorState.commitPreviewDrag()
                    root.editing = false
                    value = root.currentValue
                }
            }
        }

        Text {
            id: readout
            width: Math.max(44, implicitWidth)
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignRight
            text: root.displayReadout(root.displayedValue)
            color: Theme.panelForeground
            font.family: Theme.monoFontFamily
            font.pixelSize: Theme.fontSizeSm
        }
    }
}
