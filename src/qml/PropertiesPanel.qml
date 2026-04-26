import QtQuick
import QtQuick.Controls.Basic
import Drift
import "components"

PanelFrame {
    id: root

    readonly property var clip: EditorState.selectedClipData
    readonly property bool hasSelection: Object.keys(clip).length > 0

    Rectangle {
        width: parent.width
        height: Theme.panelHeaderHeight
        color: Theme.appBackground

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.panelBorder
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: "Properties"
            color: Theme.mutedForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
        }
    }

    Text {
        anchors.centerIn: parent
        visible: !root.hasSelection
        text: "No clip selected"
        color: Theme.mutedForeground
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSm
    }

    Flickable {
        anchors.fill: parent
        anchors.topMargin: Theme.panelHeaderHeight
        contentHeight: propsColumn.height + 24
        clip: true
        visible: root.hasSelection
        ScrollBar.vertical: AppScrollBar { }

        Column {
            id: propsColumn
            x: 12
            y: 12
            width: parent.width - 24
            spacing: 12

            Text {
                text: clip.name || ""
                color: Theme.panelForeground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBase
                font.weight: Font.Medium
                width: parent.width
                elide: Text.ElideRight
            }

            Repeater {
                model: [
                    { label: "Kind", value: clip.kind || "—" },
                    { label: "Start", value: formatSeconds(clip.start) },
                    { label: "Duration", value: formatSeconds(clip.duration) },
                    { label: "In point", value: formatSeconds(clip.inPoint) },
                    { label: "Out point", value: formatSeconds(clip.outPoint) },
                    { label: "Path", value: clip.path || "—" }
                ]

                Column {
                    width: propsColumn.width
                    spacing: 4

                    Text {
                        text: modelData.label
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                    }

                    Text {
                        text: modelData.value
                        color: Theme.panelForeground
                        font.family: modelData.label === "Path" ? Theme.monoFontFamily : Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        width: parent.width
                        wrapMode: modelData.label === "Path" ? Text.WrapAnywhere : Text.NoWrap
                        elide: modelData.label === "Path" ? Text.ElideNone : Text.ElideRight
                    }
                }
            }
        }
    }

    function formatSeconds(seconds) {
        if (seconds === undefined || seconds === null)
            return "—"
        const total = Math.max(0, Math.round(seconds))
        const m = Math.floor(total / 60)
        const s = total % 60
        return m.toString().padStart(2, "0") + ":" + s.toString().padStart(2, "0")
    }
}
