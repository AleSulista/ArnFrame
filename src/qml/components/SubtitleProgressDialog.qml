import QtQuick
import QtQuick.Controls.Basic
import Drift

// Live progress for background Whisper subtitle generation. Opens when generation
// starts and closes when it ends; the reject button cancels the run.
ThemedDialog {
    id: root

    title: qsTr("Generating subtitles")
    width: 340
    showAccept: false
    rejectText: qsTr("Cancel")
    rejectVariant: "secondary"
    closePolicy: Popup.NoAutoClose

    onRejected: EditorState.cancelSubtitleGeneration()

    Connections {
        target: EditorState
        function onSubtitleGeneratingChanged() {
            if (EditorState.subtitleGenerating)
                root.open()
            else
                root.close()
        }
    }

    contentItem: Column {
        spacing: 16
        width: parent ? parent.width : 308

        Item {
            width: parent.width
            height: 100

            CircularProgress {
                anchors.centerIn: parent
                size: 100
                strokeWidth: 7
                value: EditorState.subtitleGenProgress
            }

            ThemedLabel {
                anchors.centerIn: parent
                tone: "default"
                size: "base"
                text: Math.round(EditorState.subtitleGenProgress * 100) + "%"
            }
        }

        ThemedLabel {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            size: "sm"
            text: qsTr("Transcribing the clip's audio with Whisper. This runs on the CPU and may take a while.")
        }
    }
}
