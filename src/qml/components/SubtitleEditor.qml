import QtQuick
import QtQuick.Controls.Basic
import Drift
import "."

// Time-organized subtitle editor for a single subtitle clip. The playhead picks the
// active moment; typing at the playhead creates or updates the cue for that time.
Column {
    id: root

    required property var clip
    required property var formatSeconds
    property int trackIndex: EditorState.selectedTrack
    property int clipIndex: EditorState.selectedClip

    spacing: 8
    width: parent ? parent.width : 0

    readonly property double localPlayhead: {
        void EditorState.playheadSeconds
        void root.clip
        return EditorState.subtitleLocalPlayheadSeconds(trackIndex, clipIndex)
    }
    readonly property var cues: clip.subtitleCues || []
    readonly property int activeCueIndex: {
        const t = localPlayhead
        if (t < 0)
            return -1
        for (let i = 0; i < cues.length; i++) {
            if (t >= cues[i].start && t < cues[i].end)
                return i
        }
        return -1
    }

    function formatCueTime(seconds) {
        const clamped = Math.max(0, seconds)
        const m = Math.floor(clamped / 60)
        const s = clamped - m * 60
        const whole = Math.floor(s)
        const frac = Math.floor((s - whole) * 1000)
        return String(m).padStart(2, "0") + ":" + String(whole).padStart(2, "0") + "."
               + String(frac).padStart(3, "0")
    }

    function replaceCues(newCues) {
        EditorState.setSubtitleCues(trackIndex, clipIndex, newCues)
    }

    function updateCue(index, patch) {
        const next = []
        for (let i = 0; i < cues.length; i++) {
            const cue = {
                start: cues[i].start,
                end: cues[i].end,
                text: cues[i].text
            }
            if (i === index)
                Object.assign(cue, patch)
            next.push(cue)
        }
        replaceCues(next)
    }

    function removeCue(index) {
        const next = []
        for (let i = 0; i < cues.length; i++) {
            if (i !== index)
                next.push(cues[i])
        }
        replaceCues(next)
    }

    function refreshPlayheadInput() {
        if (playheadInput.activeFocus)
            return
        if (activeCueIndex >= 0)
            playheadInput.text = cues[activeCueIndex].text
        else
            playheadInput.text = ""
    }

    onLocalPlayheadChanged: refreshPlayheadInput()
    onActiveCueIndexChanged: refreshPlayheadInput()
    Component.onCompleted: refreshPlayheadInput()

    Text {
        width: parent.width
        wrapMode: Text.WordWrap
        text: qsTr("Move the playhead to a moment inside this clip, then type the subtitle that should appear there. Each cue is tied to a time range, not a line number.")
        color: Theme.mutedForeground
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeXs
    }

    Column {
        width: parent.width
        spacing: 4

        Text {
            text: localPlayhead >= 0
                  ? qsTr("At %1").arg(formatCueTime(localPlayhead))
                  : qsTr("Playhead is outside this clip")
            color: Theme.mutedForeground
            font.family: Theme.monoFontFamily
            font.pixelSize: Theme.fontSizeXs
        }

        ThemedTextArea {
            id: playheadInput
            width: parent.width
            height: 72
            enabled: localPlayhead >= 0
            placeholderText: qsTr("Type subtitle for this moment…")
            font.family: Theme.fontFamily
            onEditingFinished: {
                const trimmed = text.trim()
                if (trimmed.length === 0) {
                    if (activeCueIndex >= 0)
                        removeCue(activeCueIndex)
                    return
                }
                if (activeCueIndex >= 0)
                    updateCue(activeCueIndex, { text: trimmed })
                else
                    EditorState.upsertSubtitleCueAtPlayhead(trackIndex, clipIndex, trimmed)
            }
        }
    }

    ThemedButton {
        width: parent.width
        text: qsTr("Add cue at playhead")
        variant: "secondary"
        visible: localPlayhead >= 0 && activeCueIndex < 0
        onClicked: EditorState.upsertSubtitleCueAtPlayhead(trackIndex, clipIndex, qsTr("Subtitle"))
    }

    Text {
        visible: cues.length > 0
        text: qsTr("All cues")
        color: Theme.mutedForeground
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeXs
    }

    Repeater {
        model: cues

        delegate: Rectangle {
            id: cueCard
            required property int index
            required property var modelData

            width: root.width
            radius: Theme.radiusSm
            color: index === root.activeCueIndex ? Theme.panelAccent : Theme.appBackground
            border.color: index === root.activeCueIndex ? Theme.primary : Theme.panelBorder
            border.width: 1

            Column {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                Row {
                    width: parent.width
                    spacing: 6

                    Text {
                        id: timeLabel
                        width: parent.width - goButton.implicitWidth - parent.spacing
                        text: root.formatCueTime(modelData.start) + " → " + root.formatCueTime(modelData.end)
                        color: Theme.panelForeground
                        font.family: Theme.monoFontFamily
                        font.pixelSize: Theme.fontSizeXs
                        elide: Text.ElideRight
                        anchors.verticalCenter: parent.verticalCenter

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: EditorState.seekToSubtitleCue(root.trackIndex, root.clipIndex, index)
                        }
                    }

                    ThemedButton {
                        id: goButton
                        text: qsTr("Go")
                        variant: "ghost"
                        onClicked: EditorState.seekToSubtitleCue(root.trackIndex, root.clipIndex, index)
                    }
                }

                Row {
                    width: parent.width
                    spacing: 8

                    Column {
                        id: startCol
                        width: (parent.width - parent.spacing) / 2
                        spacing: 4
                        Text {
                            text: qsTr("Start (s)")
                            color: Theme.mutedForeground
                            font.pixelSize: Theme.fontSizeXs
                            font.family: Theme.fontFamily
                        }
                        ThemedTextField {
                            id: startField
                            width: parent.width
                            text: root.formatSeconds(modelData.start)
                            font.family: Theme.monoFontFamily
                            font.pixelSize: Theme.fontSizeSm
                            onEditingFinished: {
                                const v = parseFloat(text)
                                if (!isNaN(v))
                                    root.updateCue(index, { start: v })
                            }
                        }
                    }

                    Column {
                        width: (parent.width - parent.spacing) / 2
                        spacing: 4
                        Text {
                            text: qsTr("End (s)")
                            color: Theme.mutedForeground
                            font.pixelSize: Theme.fontSizeXs
                            font.family: Theme.fontFamily
                        }
                        ThemedTextField {
                            id: endField
                            width: parent.width
                            text: root.formatSeconds(modelData.end)
                            font.family: Theme.monoFontFamily
                            font.pixelSize: Theme.fontSizeSm
                            onEditingFinished: {
                                const v = parseFloat(text)
                                if (!isNaN(v))
                                    root.updateCue(index, { end: v })
                            }
                        }
                    }
                }

                ThemedTextArea {
                    width: parent.width
                    height: 56
                    text: modelData.text
                    font.family: Theme.fontFamily
                    onEditingFinished: {
                        const trimmed = text.trim()
                        if (trimmed.length === 0)
                            root.removeCue(index)
                        else
                            root.updateCue(index, { text: trimmed })
                    }
                }
            }
        }
    }
}
