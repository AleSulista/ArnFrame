import QtQuick
import QtQuick.Controls.Basic
import Drift

ThemedDialog {
    id: root

    title: qsTr("Export video")
    acceptText: qsTr("Export")
    width: 420

    property string presetId: "source"

    function openDialog() {
        presetId = "source"
        open()
    }

    onAccepted: {
        var url = FileDialogs.saveFile(qsTr("Export Video"), [qsTr("MP4 video (*.mp4)")], "mp4")
        if (url != "")
            EditorState.exportWithPreset(url, root.presetId)
    }

    contentItem: Column {
        spacing: 12
        width: parent ? parent.width : 400

        ThemedLabel {
            width: parent.width
            size: "sm"
            text: qsTr("Encoded straight from the preview (H.264 + AAC), so the file matches what you see. Presets scale by height and keep your project's aspect ratio.")
        }

        ThemedLabel {
            text: qsTr("Quality")
        }

        Flow {
            width: parent.width
            spacing: 6

            Repeater {
                model: EditorState.exportPresets()

                delegate: ThemedChip {
                    required property var modelData
                    text: modelData.label
                    selected: root.presetId === modelData.id
                    chipHeight: 28
                    onClicked: root.presetId = modelData.id
                }
            }
        }
    }
}
