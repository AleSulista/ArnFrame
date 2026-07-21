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
    // Easing is a property of the key at the playhead, not of the whole track. Hand-dragged
    // tangents match no preset, which is what leaves every chip unlit.
    readonly property string interpolationMode:
        (activeKey && !activeKey.custom) ? (activeKey.easing || "linear") : ""

    // When true, the value is edited through a slider + readout instead of a
    // numeric text field. Drags are coalesced into a single undo entry via
    // begin/commit preview drag.
    property bool useSlider: false
    property real sliderFrom: 0
    property real sliderTo: 1
    property bool percent: false
    property string unit: "" // e.g. "°" — appended to the numeric readout

    // Whether this property is currently drawn on the timeline keyframe strip.
    // Clicking the row's label adds it to the strip alongside whatever is
    // already there; clicking again takes it back off.
    readonly property bool graphSelected:
        EditorState.keyframeGraphProperties.indexOf(propDef.key) >= 0
    readonly property color graphColor: Theme.keyframeCurveColor(propDef.key)

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

    // Asks the engine rather than reimplementing evaluateAt here. This used to be a JS mirror
    // of the interpolation math that had to be kept in sync with Keyframe.h by hand — which
    // stopped being tractable once keys grew individual bezier tangents.
    function valueAtPlayhead() {
        if (!keyframeList || keyframeList.length === 0)
            return propDef.def
        return EditorState.propertyValueAt(EditorState.selectedTrack, EditorState.selectedClip,
                                           propDef.key, EditorState.playheadSeconds, propDef.def)
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
        EditorState.ensureKeyframeGraphProperty(root.propDef.key)
        EditorState.setClipKeyframe(
            EditorState.selectedTrack, EditorState.selectedClip, root.propDef.key,
            EditorState.playheadSeconds, v)
    }

    function setInterpolation(mode) {
        EditorState.ensureKeyframeGraphProperty(root.propDef.key)
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

    Item {
        width: root.width
        height: headerRow.height

        // Selection highlight tinted to this property's curve color, so the row
        // and its chip/curve on the keyframe strip read as the same series.
        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: -4
            anchors.rightMargin: -4
            radius: Theme.radiusSm
            visible: root.graphSelected
            color: Qt.rgba(root.graphColor.r, root.graphColor.g, root.graphColor.b, 0.12)
            border.width: Theme.borderWidth
            border.color: Qt.rgba(root.graphColor.r, root.graphColor.g, root.graphColor.b, 0.45)
        }

        Row {
            id: headerRow
            width: parent.width
            spacing: 6

            KeyframeDiamond {
                id: diamond
                anchors.verticalCenter: parent.verticalCenter
                hasKey: root.activeKey !== null
                onToggled: {
                    EditorState.ensureKeyframeGraphProperty(root.propDef.key)
                    if (root.activeKey) {
                        EditorState.removeClipKeyframe(
                            EditorState.selectedTrack, EditorState.selectedClip, root.propDef.key,
                            root.activeKey.seconds)
                    } else {
                        commitValue(root.currentValue)
                    }
                }
            }

            // Label + the flexible gap after it, together forming the click target
            // that drives the keyframe strip's selection.
            Item {
                id: labelZone
                anchors.verticalCenter: parent.verticalCenter
                height: labelText.implicitHeight
                width: Math.max(24, parent.width - diamond.width - linChip.width - easeChip.width
                                      - holdChip.width - parent.spacing * 4)

                Text {
                    id: labelText
                    text: root.propDef.label
                    color: root.graphSelected ? Theme.panelForeground : Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    font.weight: root.graphSelected ? Font.Medium : Font.Normal
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                    width: Math.min(implicitWidth + 2, parent.width)
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: EditorState.toggleKeyframeGraphProperty(root.propDef.key)

                    ThemedToolTip {
                        text: root.graphSelected
                              ? qsTr("Hide %1 from the keyframe strip").arg(root.propDef.label)
                              : qsTr("Add %1 to the keyframe strip").arg(root.propDef.label)
                        visible: parent.containsMouse
                    }
                }
            }

            ThemedChip {
                id: linChip
                text: qsTr("Lin")
                tooltip: qsTr("Linear — constant rate between keyframes")
                selected: root.interpolationMode === "linear"
                enabled: root.activeKey !== null
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
                enabled: root.activeKey !== null
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
                enabled: root.activeKey !== null
                chipHeight: 18
                horizontalPadding: Theme.spacingSm
                width: 34
                anchors.verticalCenter: parent.verticalCenter
                onClicked: root.setInterpolation("hold")
            }
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
                EditorState.ensureKeyframeGraphProperty(root.propDef.key)
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
