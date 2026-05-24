import QtQuick
import QtQuick.Controls.Basic
import Drift

// Offered at startup when a recovery file from a previous (crashed) session is
// found. Restore loads the unsaved work; Discard deletes the recovery file.
ThemedDialog {
    id: root

    title: qsTr("Recover unsaved work?")
    acceptText: qsTr("Restore")
    rejectText: qsTr("Discard")
    rejectVariant: "destructive"
    width: 440
    closePolicy: Popup.NoAutoClose

    readonly property var info: EditorState.recoveryInfo

    onAccepted: EditorState.restoreAutosave()
    onRejected: EditorState.discardAutosave()

    contentItem: Column {
        spacing: 12
        width: parent ? parent.width : 400

        ThemedLabel {
            width: parent.width
            size: "sm"
            wrapMode: Text.WordWrap
            text: qsTr("Drift didn't shut down cleanly last time. Unsaved changes were auto-saved and can be restored.")
        }

        Rectangle {
            width: parent.width
            radius: Theme.radiusSm
            color: Theme.appBackground
            border.width: 1
            border.color: Theme.panelBorder
            height: infoColumn.implicitHeight + 20

            Column {
                id: infoColumn
                x: 12
                y: 10
                width: parent.width - 24
                spacing: 4

                ThemedLabel {
                    width: parent.width
                    tone: "default"
                    size: "base"
                    text: (root.info && root.info.projectName && root.info.projectName.length > 0)
                          ? root.info.projectName
                          : qsTr("Untitled project")
                }

                ThemedLabel {
                    width: parent.width
                    size: "sm"
                    tone: "muted"
                    visible: root.info && root.info.savedAt && root.info.savedAt.length > 0
                    text: qsTr("Auto-saved: %1").arg(root.info ? root.info.savedAt : "")
                }

                ThemedLabel {
                    width: parent.width
                    size: "sm"
                    tone: "muted"
                    wrapMode: Text.WordWrap
                    visible: root.info && root.info.originalPath && root.info.originalPath.length > 0
                    text: root.info ? root.info.originalPath : ""
                }
            }
        }
    }
}
