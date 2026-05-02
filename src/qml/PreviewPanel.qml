import QtQuick
import Drift
import "components"

PanelFrame {
    id: root

    readonly property real currentSeconds: EditorState.playheadSeconds
    readonly property real durationSeconds: EditorState.durationSeconds
    readonly property bool playing: EditorState.playing

    function formatTimecode(seconds) {
        const fps = 30;
        const totalFrames = Math.round(seconds * fps);
        const h = Math.floor(totalFrames / (fps * 3600));
        const m = Math.floor(totalFrames / (fps * 60)) % 60;
        const s = Math.floor(totalFrames / fps) % 60;
        const f = totalFrames % fps;
        function pad(n) { return n.toString().padStart(2, "0"); }
        return pad(h) + ":" + pad(m) + ":" + pad(s) + ":" + pad(f);
    }

    Column {
        anchors.fill: parent

        Item {
            id: viewportOuter
            width: parent.width
            height: parent.height - toolbar.height
            clip: true

            Item {
                id: viewport
                anchors.fill: parent
                anchors.margins: 8
                anchors.bottomMargin: 0

                property real aspect: 16 / 9
                property bool fitMode: true
                property real fitWidth: fitMode ? Math.min(width, height * aspect) : width
                property real fitHeight: fitMode ? fitWidth / aspect : height

                Rectangle {
                    width: viewport.fitWidth
                    height: viewport.fitHeight
                    anchors.centerIn: parent
                    color: "#000000"
                    border.width: 1
                    border.color: Theme.border
                    clip: true

                    PreviewItem {
                        id: preview
                        anchors.fill: parent
                    }

                    Item {
                        anchors.fill: parent
                        visible: EditorState.guidesEnabled

                        Repeater {
                            model: EditorState.guideType === "thirds" ? 2 : 0
                            Rectangle {
                                width: 1
                                height: parent.height
                                x: parent.width * (index + 1) / 3
                                color: "#80ffffff"
                            }
                        }
                        Repeater {
                            model: EditorState.guideType === "thirds" ? 2 : 0
                            Rectangle {
                                height: 1
                                width: parent.width
                                y: parent.height * (index + 1) / 3
                                color: "#80ffffff"
                            }
                        }

                        Rectangle {
                            visible: EditorState.guideType === "crosshair"
                            width: 1
                            height: parent.height
                            x: parent.width / 2
                            color: "#80ffffff"
                        }
                        Rectangle {
                            visible: EditorState.guideType === "crosshair"
                            height: 1
                            width: parent.width
                            y: parent.height / 2
                            color: "#80ffffff"
                        }

                        Rectangle {
                            visible: EditorState.guideType === "safe"
                            x: parent.width * 0.05
                            y: parent.height * 0.05
                            width: parent.width * 0.90
                            height: parent.height * 0.90
                            color: "transparent"
                            border.width: 1
                            border.color: "#80ffffff"
                        }
                        Rectangle {
                            visible: EditorState.guideType === "safe"
                            x: parent.width * 0.025
                            y: parent.height * 0.025
                            width: parent.width * 0.95
                            height: parent.height * 0.95
                            color: "transparent"
                            border.width: 1
                            border.color: "#66ffffff"
                        }
                    }

                    Connections {
                        target: EditorState.playback
                        function onCurrentFrameChanged() {
                            preview.frame = EditorState.playback.currentFrame
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: !EditorState.playback.hasFrame
                        text: EditorState.activeAudioClipAtPlayhead().path ? "Audio only" : "No clip at playhead"
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                    }
                }
            }
        }

        Item {
            id: toolbar
            width: parent.width
            height: 20 + 12 + 28

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                spacing: 0

                Text {
                    text: root.formatTimecode(root.currentSeconds)
                    color: Theme.primary
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.fontSizeXs
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: " / "
                    color: Theme.mutedForeground
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.fontSizeXs
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: root.formatTimecode(root.durationSeconds)
                    color: Theme.mutedForeground
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.fontSizeXs
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            IconButton {
                anchors.centerIn: parent
                icon: root.playing ? Theme.icons.pause : Theme.icons.play
                variant: "text"
                tooltip: root.playing ? qsTr("Pause") : qsTr("Play")
                onClicked: EditorState.playing = !EditorState.playing
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10

                Text {
                    text: viewport.fitMode ? "Fit" : "Fill"
                    color: Theme.panelForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    anchors.verticalCenter: parent.verticalCenter

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: viewport.fitMode = !viewport.fitMode
                    }
                }

                Rectangle {
                    width: 1
                    height: 16
                    color: Theme.panelBorder
                    anchors.verticalCenter: parent.verticalCenter
                }

                IconButton {
                    icon: Theme.icons.grid
                    variant: "text"
                    tooltip: qsTr("Toggle guides")
                    anchors.verticalCenter: parent.verticalCenter
                    active: EditorState.guidesEnabled
                    onClicked: EditorState.guidesEnabled = !EditorState.guidesEnabled
                }

                IconButton {
                    icon: Theme.icons.maximize
                    variant: "text"
                    tooltip: qsTr("Toggle fullscreen")
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: {
                        const win = root.Window.window
                        if (win)
                            win.visibility = win.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen
                    }
                }
            }
        }
    }

    Connections {
        target: EditorState
        function onPlayheadSecondsChanged() {
            if (!EditorState.playing)
                EditorState.playback.refreshFrame()
        }
        function onTracksChanged() {
            EditorState.playback.refreshFrame()
        }
        function onPlayingChanged() {
            if (!EditorState.playing)
                EditorState.playback.refreshFrame()
        }
    }

    Component.onCompleted: EditorState.playback.refreshFrame()
}
