import QtQuick
import Drift
import "components"

PanelFrame {
    id: root

    property bool playing: false
    property real currentSeconds: 4.5
    property real durationSeconds: 42.0

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

        // --- viewport ------------------------------------------------------------
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

                    Text {
                        anchors.centerIn: parent
                        text: "Preview"
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                    }
                }
            }
        }

        // --- transport toolbar -------------------------------------------------------
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
                onClicked: root.playing = !root.playing
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
}
