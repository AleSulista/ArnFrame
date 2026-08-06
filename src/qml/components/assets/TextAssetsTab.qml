import QtQuick
import QtQuick.Controls.Basic
import Drift
import ".."

// Text tab: add on-canvas text clips. Timed captions live under Subtitles.
Item {
    id: root

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: textColumn.height + Theme.spacing3xl
        clip: true
        ScrollBar.vertical: AppScrollBar { }

        Column {
            id: textColumn
            x: Theme.pagePadding
            width: parent.width - Theme.pagePadding * 2
            spacing: Theme.spacingLg
            topPadding: Theme.pagePadding

            readonly property real contentWidth: width

            ThemedTextField {
                id: textClipInput
                width: textColumn.contentWidth
                placeholderText: qsTr("Enter text (optional)")
                font.family: Theme.fontFamily
            }

            ThemedButton {
                text: qsTr("Add to timeline")
                variant: "primary"
                glyph: Theme.icons.type
                onClicked: {
                    EditorState.addTextClip(textClipInput.text, -1)
                    textClipInput.clear()
                }
            }

            Text {
                width: textColumn.contentWidth
                wrapMode: Text.WordWrap
                text: qsTr("Leave it empty and just add — the clip lands on the preview ready to type. You can always double-click text on the preview to edit it.")
                color: Theme.mutedForeground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
            }
        }
    }
}
