import QtQuick
import QtQuick.Controls.Basic
import Drift

// Shows live progress for a background export. Closable while the export keeps
// running; EditorHeader reopens it via the circular-progress badge next to the
// Export button when it's been dismissed mid-export.
ThemedDialog {
    id: root

    title: EditorState.exportInProgress ? qsTr("Exporting video") : qsTr("Export")
    preferredWidth: Theme.dialogWidthSm
    showAccept: false
    rejectText: qsTr("Close")
    rejectVariant: "secondary"

    function openDialog() {
        open()
    }

    contentItem: Column {
        spacing: Theme.spacing2xl
        width: parent ? parent.width : 308

        LabelledProgressRing {
            width: parent.width
            value: EditorState.exportProgress
            indeterminate: EditorState.exportInProgress && EditorState.exportProgress <= 0
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
