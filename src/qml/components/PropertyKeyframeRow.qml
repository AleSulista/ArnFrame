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
        const ease = interpolationMode === "ease"
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
            let frac = (t - prev.seconds) / (next.seconds - prev.seconds)
            if (ease)
                frac = frac * frac * (3 - 2 * frac)
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
        EditorState.keyframeGraphProperty = root.propDef.key
        EditorState.setClipKeyframe(
            EditorState.selectedTrack, EditorState.selectedClip, root.propDef.key,
            EditorState.playheadSeconds, v)
    }

    function setInterpolation(mode) {
        EditorState.keyframeGraphProperty = root.propDef.key
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
            id: diamond
            anchors.verticalCenter: parent.verticalCenter
            hasKey: root.activeKey !== null
            onToggled: {
                EditorState.keyframeGraphProperty = root.propDef.key
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
            id: labelText
            text: root.propDef.label
            color: Theme.mutedForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            width: Math.min(implicitWidth + 2,
                            Math.max(24, parent.width - diamond.width - linChip.width - easeChip.width
                                           - holdChip.width - parent.spacing * 5))
        }

        Item {
            width: Math.max(0, parent.width - diamond.width - labelText.width
                                   - linChip.width - easeChip.width - holdChip.width
                                   - parent.spacing * 5)
            height: 1
        }

        ThemedChip {
            id: linChip
            text: qsTr("Lin")
            tooltip: qsTr("Linear — constant rate between keyframes")
            selected: root.interpolationMode === "linear"
            chipHeight: 18
            horizontalPadding: Theme.spacingSm
            width: 28
            anchors.verticalCenter: parent.verticalCenter
            onClicked: root.setInterpolation("linear")
        }

        ThemedChip {
            id: easeChip
            text: qsTr("Ease")
            tooltip: qsTr("Ease — accelerates out and decelerates in")
            selected: root.interpolationMode === "ease"
            chipHeight: 18
            horizontalPadding: Theme.spacingSm
            width: 36
            anchors.verticalCenter: parent.verticalCenter
            onClicked: root.setInterpolation("ease")
        }

        ThemedChip {
            id: holdChip
            text: qsTr("Hold")
            tooltip: qsTr("Hold — jumps to the next value with no interpolation")
            selected: root.interpolationMode === "hold"
            chipHeight: 18
            horizontalPadding: Theme.spacingSm
            width: 34
            anchors.verticalCenter: parent.verticalCenter
            onClicked: root.setInterpolation("hold")
        }
    }

    // --- Numeric field (default) ---------------------------------------------
    Item {
        visible: !root.useSlider
        width: root.width
        height: 30

        ThemedTextField {
            id: valueField
            anchors.fill: parent
            verticalAlignment: TextInput.AlignVCenter
            text: root.formatValue(root.currentValue)
            rightPadding: root.unit.length > 0 || root.percent ? 22 : 8
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
            z: 1
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
                EditorState.keyframeGraphProperty = root.propDef.key
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
