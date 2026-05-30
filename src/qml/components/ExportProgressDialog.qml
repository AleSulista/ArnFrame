import QtQuick
import QtQuick.Controls.Basic
import Drift

// Shows live progress for a background export. Closable while the export keeps
// running; EditorHeader reopens it via the circular-progress badge next to the
// Export button when it's been dismissed mid-export.
ThemedDialog {
    id: root

    title: EditorState.exportInProgress ? qsTr("Exporting video") : qsTr("Export")
    width: 340
    showAccept: false
    rejectText: qsTr("Close")
    rejectVariant: "secondary"

    function openDialog() {
        open()
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
                value: EditorState.exportProgress
            }

            ThemedLabel {
                anchors.centerIn: parent
                tone: "default"
                size: "base"
                text: Math.round(EditorState.exportProgress * 100) + "%"
            }
        }

        ThemedLabel {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            size: "sm"
            text: EditorState.exportInProgress
                  ? qsTr("Rendering your video. You can close this dialog and keep editing.")
                  : qsTr("Export finished.")
        }
    }
}
