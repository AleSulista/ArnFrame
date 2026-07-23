import QtQuick
import QtQuick.Controls.Basic
import Drift

ThemedDialog {
    id: root

    title: qsTr("Export video")
    acceptText: qsTr("Export")
    preferredWidth: Theme.dialogWidthMd

    property string presetId: "source"

    function openDialog() {
        presetId = "source"
        open()
    }

    onAccepted: {
        var url = FileDialogs.saveFile(qsTr("Export Video"), [qsTr("MP4 video (*.mp4)")], "mp4")
        if (url != "") {
            EditorState.exportWithPreset(url, root.presetId)
            Toasts.info(qsTr("Export started…"))
        } else {
            // Cancelling the save picker used to be a completely silent no-op:
            // the export dialog had already closed, so nothing happened and
            // nothing said so.
            Toasts.info(qsTr("Export cancelled."))
        }
    }

    contentItem: Column {
        spacing: Theme.spacingXl
        width: parent ? parent.width : Theme.dialogWidthMd

        ThemedLabel {
            width: parent.width
            size: "sm"
            text: qsTr("Saves what you see in the preview. Pick a quality — the picture shape stays the same.")
        }

        ThemedLabel {
            text: qsTr("Quality")
        }

        Flow {
            width: parent.width
            spacing: Theme.spacingMd

            Repeater {
                model: EditorState.exportPresets()

                delegate: ThemedChip {
                    required property var modelData
                    text: modelData.label
                    selected: root.presetId === modelData.id
                    tooltip: qsTr("Export as %1").arg(modelData.label)
                    onClicked: root.presetId = modelData.id
                }
            }
        }
    }
}
