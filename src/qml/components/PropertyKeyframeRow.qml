import QtQuick
import QtQuick.Controls.Basic
import Drift

// One animatable-property control for the clip inspector: keyframe diamond +
// label on top, a rounded value field below. Self-contained so it can be
// reused from any tab without depending on the caller's ids.
Column {
    id: root

    required property var propDef       // {key, label, def, decimals}
    required property var keyframeList  // [{seconds, value}, ...] for this property
    property string interpolationMode: "linear"
    spacing: 4

    readonly property var activeKey: keyframeAtPlayhead()
    readonly property real currentValue: valueAtPlayhead()

    // Linear interpolation over keyframeList, mirroring
    // drift::KeyframeTrack<double>::evaluateAt's default (Linear) mode.
    function valueAtPlayhead() {
        if (!keyframeList || keyframeList.length === 0)
            return propDef.def

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
        return propDef.def
    }

    // Finds a keyframe within one frame (1/30s) of the playhead, or null.
    function keyframeAtPlayhead() {
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

    Row {
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
                    EditorState.setClipKeyframe(
                        EditorState.selectedTrack, EditorState.selectedClip, root.propDef.key,
                        EditorState.playheadSeconds, root.currentValue)
                }
            }
        }

        Text {
            text: root.propDef.label
            color: Theme.mutedForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            anchors.verticalCenter: parent.verticalCenter
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
                onClicked: {
                    root.interpolationMode = "linear"
                    EditorState.setKeyframeInterpolation(
                        EditorState.selectedTrack, EditorState.selectedClip, root.propDef.key, "linear")
                }
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
                onClicked: {
                    root.interpolationMode = "hold"
                    EditorState.setKeyframeInterpolation(
                        EditorState.selectedTrack, EditorState.selectedClip, root.propDef.key, "hold")
                }
            }
        }
    }

    Rectangle {
        width: root.width
        height: 30
        radius: Theme.radiusSm
        color: Theme.panelAccent

        TextField {
            anchors.fill: parent
            anchors.margins: 1
            verticalAlignment: TextInput.AlignVCenter
            text: root.currentValue.toFixed(root.propDef.decimals)
            color: Theme.panelForeground
            font.family: Theme.monoFontFamily
            font.pixelSize: Theme.fontSizeSm
            background: Item {}
            onEditingFinished: {
                const v = parseFloat(text)
                if (!isNaN(v))
                    EditorState.setClipKeyframe(
                        EditorState.selectedTrack, EditorState.selectedClip, root.propDef.key,
                        EditorState.playheadSeconds, v)
            }
        }
    }
}
