import QtQuick
import QtMultimedia
import Drift
import "components"

PanelFrame {
    id: root

    readonly property real currentSeconds: EditorState.playheadSeconds
    readonly property real durationSeconds: EditorState.durationSeconds
    readonly property bool playing: EditorState.playing

    property string previewKind: "none"
    property string currentSource: ""
    property real pendingSeekMs: -1

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

    function applySeek() {
        if (pendingSeekMs < 0)
            return
        mediaPlayer.position = pendingSeekMs
        pendingSeekMs = -1
    }

    function syncPreview() {
        const clip = EditorState.activeVideoClipAtPlayhead()
        if (!clip || !clip.path) {
            previewKind = "none"
            currentSource = ""
            pendingSeekMs = -1
            mediaPlayer.stop()
            stillImage.source = ""
            return
        }

        previewKind = clip.kind
        if (clip.kind === "image") {
            stillImage.source = EditorState.fileUrl(clip.path)
            mediaPlayer.stop()
            return
        }

        const url = EditorState.fileUrl(clip.path)
        const sourceTime = Math.max(0, EditorState.sourceTimeAtPlayhead() * 1000)
        const urlString = url.toString()

        if (currentSource !== urlString) {
            currentSource = urlString
            mediaPlayer.source = url
            pendingSeekMs = sourceTime
        } else {
            const drift = Math.abs(mediaPlayer.position - sourceTime)
            if (drift > 120)
                mediaPlayer.position = sourceTime
        }

        if (EditorState.playing) {
            if (mediaPlayer.playbackState !== MediaPlayer.PlayingState)
                mediaPlayer.play()
        } else {
            pendingSeekMs = sourceTime
            if (mediaPlayer.mediaStatus === MediaPlayer.LoadedMedia
                    || mediaPlayer.mediaStatus === MediaPlayer.BufferedMedia) {
                applySeek()
            }
            mediaPlayer.pause()
        }
    }

    MediaPlayer {
        id: mediaPlayer
        videoOutput: videoOutput
        audioOutput: AudioOutput {
            volume: 0.8
        }

        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.LoadedMedia
                    || mediaStatus === MediaPlayer.BufferedMedia) {
                root.applySeek()
                if (EditorState.playing)
                    play()
            }
        }
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
                property real fitWidth: Math.min(width, height * aspect)
                property real fitHeight: fitWidth / aspect

                Rectangle {
                    width: viewport.fitWidth
                    height: viewport.fitHeight
                    anchors.centerIn: parent
                    color: "#000000"
                    border.width: 1
                    border.color: Theme.border
                    clip: true

                    VideoOutput {
                        id: videoOutput
                        anchors.fill: parent
                        visible: root.previewKind === "video"
                    }

                    Image {
                        id: stillImage
                        anchors.fill: parent
                        visible: root.previewKind === "image"
                        fillMode: Image.PreserveAspectFit
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: root.previewKind === "none"
                        text: "No clip at playhead"
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
                onClicked: EditorState.playing = !EditorState.playing
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10

                Text {
                    text: "Fit"
                    color: Theme.panelForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    width: 1
                    height: 16
                    color: Theme.panelBorder
                    anchors.verticalCenter: parent.verticalCenter
                }

                IconButton {
                    icon: Theme.icons.maximize
                    variant: "text"
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    Connections {
        target: EditorState
        function onPlayheadSecondsChanged() { root.syncPreview() }
        function onTracksChanged() { root.syncPreview() }
        function onPlayingChanged() { root.syncPreview() }
    }

    Component.onCompleted: syncPreview()
}
