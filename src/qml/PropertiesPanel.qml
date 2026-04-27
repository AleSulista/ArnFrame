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

            Column {
                width: propsColumn.width
                spacing: 4
                Text {
                    text: "Kind"
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }
                Text {
                    text: clip.kind || "—"
                    color: Theme.panelForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                }
            }

            Row {
                width: parent.width
                spacing: 8

                Column {
                    width: (parent.width - parent.spacing) / 2
                    spacing: 4
                    Text {
                        text: "Start (s)"
                        color: Theme.mutedForeground
                        font.pixelSize: Theme.fontSizeXs
                        font.family: Theme.fontFamily
                    }
                    TextField {
                        width: parent.width
                        text: (clip.start || 0).toFixed(2)
                        color: Theme.panelForeground
                        font.family: Theme.monoFontFamily
                        font.pixelSize: Theme.fontSizeSm
                        onEditingFinished: {
                            const v = parseFloat(text)
                            if (!isNaN(v))
                                EditorState.setClipStart(EditorState.selectedTrack, EditorState.selectedClip, v)
                        }
                    }
                }

                Column {
                    width: (parent.width - parent.spacing) / 2
                    spacing: 4
                    Text {
                        text: "Duration (s)"
                        color: Theme.mutedForeground
                        font.pixelSize: Theme.fontSizeXs
                        font.family: Theme.fontFamily
                    }
                    TextField {
                        width: parent.width
                        text: (clip.duration || 0).toFixed(2)
                        color: Theme.panelForeground
                        font.family: Theme.monoFontFamily
                        font.pixelSize: Theme.fontSizeSm
                        onEditingFinished: {
                            const v = parseFloat(text)
                            if (!isNaN(v))
                                EditorState.setClipDuration(EditorState.selectedTrack, EditorState.selectedClip, v)
                        }
                    }
                }
            }

            Column {
                width: propsColumn.width
                spacing: 4
                visible: clip.kind === "text"
                Text {
                    text: "Text content"
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }
                TextArea {
                    width: parent.width
                    height: 80
                    text: clip.textContent || ""
                    color: Theme.panelForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    wrapMode: TextArea.Wrap
                    onEditingFinished: EditorState.setClipTextContent(
                                           EditorState.selectedTrack, EditorState.selectedClip, text)
                }
            }

            Column {
                width: propsColumn.width
                spacing: 8
                visible: clip.kind !== "text"

                Text {
                    text: "Trim"
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }

                Row {
                    width: parent.width
                    spacing: 8

                    Column {
                        width: (parent.width - parent.spacing) / 2
                        spacing: 4
                        Text {
                            text: "In point (s)"
                            color: Theme.mutedForeground
                            font.pixelSize: Theme.fontSizeXs
                            font.family: Theme.fontFamily
                        }
                        TextField {
                            width: parent.width
                            text: (clip.inPoint || 0).toFixed(2)
                            color: Theme.panelForeground
                            font.family: Theme.monoFontFamily
                            font.pixelSize: Theme.fontSizeSm
                            onEditingFinished: applyTrim(parseFloat(text), clip.outPoint)
                        }
                    }

                    Column {
                        width: (parent.width - parent.spacing) / 2
                        spacing: 4
                        Text {
                            text: "Out point (s)"
                            color: Theme.mutedForeground
                            font.pixelSize: Theme.fontSizeXs
                            font.family: Theme.fontFamily
                        }
                        TextField {
                            width: parent.width
                            text: (clip.outPoint || 0).toFixed(2)
                            color: Theme.panelForeground
                            font.family: Theme.monoFontFamily
                            font.pixelSize: Theme.fontSizeSm
                            onEditingFinished: applyTrim(clip.inPoint, parseFloat(text))
                        }
                    }
                }
            }

            Column {
                width: propsColumn.width
                spacing: 4
                visible: clip.path && clip.path.length > 0

                Text {
                    text: "Path"
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }

                Text {
                    text: clip.path || "—"
                    color: Theme.panelForeground
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.fontSizeSm
                    width: parent.width
                    wrapMode: Text.WrapAnywhere
                }
            }
        }
    }

    function applyTrim(inPoint, outPoint) {
        if (!root.hasSelection || isNaN(inPoint) || isNaN(outPoint))
            return
        EditorState.setClipTrim(EditorState.selectedTrack, EditorState.selectedClip, inPoint, outPoint)
    }
}
