import QtQuick
import QtQuick.Window
import QtMultimedia
import Drift 1.0
import "components"

// Preview and light edit for one bin row: watch it, crop the picture, pick the in/out
// range, then save. Saving rewrites that row in the media bin; the timeline is untouched
// until the user drags the result onto it.
Window {
    id: root

    property int assetIndex: -1
    property string assetId: ""
    property string kind: ""
    property string sourcePath: ""
    property string assetName: ""
    property string filmstripPath: ""
    property real durationSeconds: 0
    property int sourceWidth: 0
    property int sourceHeight: 0
    property int rotationDegrees: 0

    property real inSeconds: 0
    property real outSeconds: 0
    property real cropX: 0
    property real cropY: 0
    property real cropW: 1
    property real cropH: 1

    readonly property bool isImage: kind === "image"
    readonly property bool isAudio: kind === "audio"
    readonly property bool isVideo: kind === "video"
    readonly property bool canCrop: !isAudio
    readonly property bool canTrim: !isImage && durationSeconds > 0.05

    readonly property int displayW: {
        const rot = Math.abs(root.rotationDegrees)
        return (rot === 90 || rot === 270) ? root.sourceHeight : root.sourceWidth
    }
    readonly property int displayH: {
        const rot = Math.abs(root.rotationDegrees)
        return (rot === 90 || rot === 270) ? root.sourceWidth : root.sourceHeight
    }

    readonly property bool cropDirty: cropX > 0.001 || cropY > 0.001
                                      || cropW < 0.999 || cropH < 0.999
    readonly property bool trimDirty: canTrim
                                      && (inSeconds > 0.02
                                          || outSeconds < durationSeconds - 0.02)
    readonly property bool dirty: cropDirty || trimDirty
    readonly property bool saving: EditorState.editingAsset

    width: 920
    height: 680
    minimumWidth: 640
    minimumHeight: 480
    title: assetName.length > 0 ? qsTr("Preview — %1").arg(assetName) : qsTr("Preview")
    color: Theme.appBackground

    function openFor(index) {
        const asset = AssetLibrary.assetAt(index)
        if (!asset || Object.keys(asset).length === 0)
            return
        player.stop()
        root.assetIndex = index
        root.assetId = asset.id || ""
        root.kind = asset.kind || ""
        root.sourcePath = asset.path || ""
        root.assetName = asset.name || ""
        root.filmstripPath = asset.filmstripPath || ""
        root.durationSeconds = asset.durationSeconds || 0
        root.sourceWidth = asset.width || 0
        root.sourceHeight = asset.height || 0
        root.rotationDegrees = asset.rotationDegrees || 0
        resetEdits()
        root.show()
        root.raise()
        root.requestActivate()
        if (!root.isImage)
            player.source = EditorState.fileUrl(root.sourcePath)
    }

    function resetEdits() {
        root.inSeconds = 0
        root.outSeconds = Math.max(0, root.durationSeconds)
        root.cropX = 0
        root.cropY = 0
        root.cropW = 1
        root.cropH = 1
    }

    function formatTime(seconds) {
        const s = Math.max(0, seconds)
        const total = Math.floor(s)
        const m = Math.floor(total / 60)
        const sec = total % 60
        const cs = Math.floor((s - total) * 100)
        function pad(n) { return n.toString().padStart(2, "0") }
        return pad(m) + ":" + pad(sec) + "." + pad(cs)
    }

    function clampRange() {
        const minSpan = root.isVideo ? 0.05 : 0.02
        const dur = Math.max(minSpan, root.durationSeconds)
        root.inSeconds = Math.max(0, Math.min(root.inSeconds, dur - minSpan))
        root.outSeconds = Math.max(root.inSeconds + minSpan, Math.min(root.outSeconds, dur))
    }

    function seekTo(seconds) {
        if (root.isImage)
            return
        player.position = Math.round(Math.max(0, seconds) * 1000)
    }

    function togglePlay() {
        if (root.isImage)
            return
        if (player.playbackState === MediaPlayer.PlayingState)
            player.pause()
        else {
            const at = player.position / 1000
            if (at < root.inSeconds - 0.02 || at >= root.outSeconds - 0.02)
                seekTo(root.inSeconds)
            player.play()
        }
    }

    onClosing: {
        player.stop()
        if (root.saving)
            EditorState.cancelAssetEdit()
    }

    Connections {
        target: EditorState
        function onAssetEditFinished(ok, message) {
            if (!ok)
                return
            player.stop()
            root.close()
        }
        function onProjectReset() {
            player.stop()
            root.close()
        }
    }

    MediaPlayer {
        id: player
        audioOutput: AudioOutput {}
        videoOutput: videoOut
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.LoadedMedia
                    || mediaStatus === MediaPlayer.BufferedMedia)
                root.seekTo(root.inSeconds)
        }
        onPositionChanged: {
            if (root.isImage || player.playbackState !== MediaPlayer.PlayingState)
                return
            const at = position / 1000
            if (at >= root.outSeconds - 0.01) {
                seekTo(root.inSeconds)
                if (player.playbackState !== MediaPlayer.PlayingState)
                    player.play()
            }
        }
    }

    Shortcut {
        sequence: "Space"
        onActivated: root.togglePlay()
    }
    Shortcut {
        sequence: "I"
        enabled: root.canTrim && !root.saving
        onActivated: {
            root.inSeconds = Math.max(0, player.position / 1000)
            root.clampRange()
        }
    }
    Shortcut {
        sequence: "O"
        enabled: root.canTrim && !root.saving
        onActivated: {
            root.outSeconds = Math.max(root.inSeconds, player.position / 1000)
            root.clampRange()
        }
    }
    Shortcut {
        sequence: "Esc"
        enabled: !root.saving
        onActivated: root.close()
    }

    Column {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: footer.top
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingLg

        ThemedLabel {
            id: hintLabel
            width: parent.width
            wrapMode: Text.WordWrap
            text: root.isAudio
                  ? qsTr("Play the clip and drag the ends to keep only the part you want. Save replaces this item in the media bin.")
                  : root.isImage
                    ? qsTr("Drag the frame to crop. Save replaces this item in the media bin — then drag it onto the timeline.")
                    : qsTr("Play, crop, and drag the ends to keep a range. Save replaces this item in the media bin — then drag it onto the timeline.")
        }

        Rectangle {
            id: stage
            width: parent.width
            height: {
                let h = parent.height - hintLabel.height - Theme.spacingLg
                h -= transport.height + Theme.spacingLg
                if (root.canTrim)
                    h -= strip.height + Theme.spacingLg
                return Math.max(80, h)
            }
            radius: Theme.radiusMd
            color: Theme.overlayColor
            clip: true

            readonly property var fit: {
                const srcW = Math.max(1, root.displayW)
                const srcH = Math.max(1, root.displayH)
                const scale = Math.min(width / srcW, height / srcH)
                const w = srcW * scale
                const h = srcH * scale
                return { x: (width - w) / 2, y: (height - h) / 2, w: w, h: h }
            }

            Image {
                id: still
                visible: root.isImage
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                source: root.isImage && root.sourcePath.length > 0
                        ? EditorState.imageUrl(root.sourcePath) : ""
            }

            VideoOutput {
                id: videoOut
                visible: root.isVideo
                anchors.fill: parent
                fillMode: VideoOutput.PreserveAspectFit
            }

            Column {
                visible: root.isAudio
                anchors.centerIn: parent
                spacing: Theme.spacingLg
                IconGlyph {
                    anchors.horizontalCenter: parent.horizontalCenter
                    glyph: Theme.icons.music
                    iconSize: Theme.iconSizeXl * 2
                    iconColor: Theme.mutedForeground
                }
                ThemedLabel {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.assetName
                    tone: "default"
                    size: "sm"
                }
            }

            // Crop frame, mapped onto the fitted picture so letterboxing is not part of the crop.
            Item {
                id: cropHost
                visible: root.canCrop
                x: root.isImage
                   ? (stage.width - still.paintedWidth) / 2
                   : stage.fit.x
                y: root.isImage
                   ? (stage.height - still.paintedHeight) / 2
                   : stage.fit.y
                width: root.isImage ? still.paintedWidth : stage.fit.w
                height: root.isImage ? still.paintedHeight : stage.fit.h

                readonly property real frameX: root.cropX * width
                readonly property real frameY: root.cropY * height
                readonly property real frameW: root.cropW * width
                readonly property real frameH: root.cropH * height
                readonly property real minFrac: 0.08

                function setCrop(nx, ny, nw, nh) {
                    const min = cropHost.minFrac
                    nw = Math.max(min, Math.min(1, nw))
                    nh = Math.max(min, Math.min(1, nh))
                    nx = Math.max(0, Math.min(1 - nw, nx))
                    ny = Math.max(0, Math.min(1 - nh, ny))
                    root.cropX = nx
                    root.cropY = ny
                    root.cropW = nw
                    root.cropH = nh
                }

                // Dim outside the keep-rect.
                Rectangle { width: cropHost.frameX; height: parent.height; color: Theme.scrimStrong }
                Rectangle {
                    x: cropHost.frameX + cropHost.frameW
                    width: Math.max(0, parent.width - x)
                    height: parent.height
                    color: Theme.scrimStrong
                }
                Rectangle {
                    x: cropHost.frameX
                    width: cropHost.frameW
                    height: cropHost.frameY
                    color: Theme.scrimStrong
                }
                Rectangle {
                    x: cropHost.frameX
                    y: cropHost.frameY + cropHost.frameH
                    width: cropHost.frameW
                    height: Math.max(0, parent.height - y)
                    color: Theme.scrimStrong
                }

                Rectangle {
                    x: cropHost.frameX
                    y: cropHost.frameY
                    width: cropHost.frameW
                    height: cropHost.frameH
                    color: "transparent"
                    border.width: Theme.borderWidthFocus
                    border.color: Theme.primary
                }

                MouseArea {
                    x: cropHost.frameX
                    y: cropHost.frameY
                    width: cropHost.frameW
                    height: cropHost.frameH
                    enabled: !root.saving
                    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    property real grabX: 0
                    property real grabY: 0
                    onPressed: (mouse) => {
                        grabX = mouse.x
                        grabY = mouse.y
                    }
                    onPositionChanged: (mouse) => {
                        if (!pressed || cropHost.width <= 0)
                            return
                        const dx = (mouse.x - grabX) / cropHost.width
                        const dy = (mouse.y - grabY) / cropHost.height
                        cropHost.setCrop(root.cropX + dx, root.cropY + dy, root.cropW, root.cropH)
                    }
                }

                Repeater {
                    model: [
                        { x: 0, y: 0, dx: -1, dy: -1 },
                        { x: 0.5, y: 0, dx: 0, dy: -1 },
                        { x: 1, y: 0, dx: 1, dy: -1 },
                        { x: 0, y: 0.5, dx: -1, dy: 0 },
                        { x: 1, y: 0.5, dx: 1, dy: 0 },
                        { x: 0, y: 1, dx: -1, dy: 1 },
                        { x: 0.5, y: 1, dx: 0, dy: 1 },
                        { x: 1, y: 1, dx: 1, dy: 1 }
                    ]
                    Rectangle {
                        required property var modelData
                        readonly property int grip: Theme.touchUi ? 16 : 10
                        width: grip
                        height: grip
                        radius: 1
                        color: Theme.primary
                        x: cropHost.frameX + modelData.x * cropHost.frameW - width / 2
                        y: cropHost.frameY + modelData.y * cropHost.frameH - height / 2
                        z: 2

                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: Theme.touchUi ? -10 : -6
                            enabled: !root.saving
                            cursorShape: {
                                if (modelData.dx !== 0 && modelData.dy !== 0)
                                    return (modelData.dx === modelData.dy)
                                           ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
                                return modelData.dx !== 0 ? Qt.SizeHorCursor : Qt.SizeVerCursor
                            }
                            property real startX: 0
                            property real startY: 0
                            property real startW: 1
                            property real startH: 1
                            property real origX: 0
                            property real origY: 0
                            onPressed: {
                                startX = root.cropX
                                startY = root.cropY
                                startW = root.cropW
                                startH = root.cropH
                                origX = mouseX
                                origY = mouseY
                            }
                            onPositionChanged: {
                                if (!pressed || cropHost.width <= 0)
                                    return
                                const dx = (mouseX - origX) / cropHost.width
                                const dy = (mouseY - origY) / cropHost.height
                                let nx = startX
                                let ny = startY
                                let nw = startW
                                let nh = startH
                                if (modelData.dx < 0) {
                                    nx = startX + dx
                                    nw = startW - dx
                                } else if (modelData.dx > 0) {
                                    nw = startW + dx
                                }
                                if (modelData.dy < 0) {
                                    ny = startY + dy
                                    nh = startH - dy
                                } else if (modelData.dy > 0) {
                                    nh = startH + dy
                                }
                                cropHost.setCrop(nx, ny, nw, nh)
                            }
                        }
                    }
                }
            }

            Rectangle {
                visible: root.canCrop
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: Theme.spacingMd
                color: Theme.scrimStrong
                radius: Theme.radiusSm
                width: cropSizeLabel.implicitWidth + Theme.spacingLg
                height: cropSizeLabel.implicitHeight + Theme.spacingSm
                Text {
                    id: cropSizeLabel
                    anchors.centerIn: parent
                    color: Theme.onMedia
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.fontSizeXs
                    text: {
                        const w = Math.max(1, Math.round(root.displayW * root.cropW))
                        const h = Math.max(1, Math.round(root.displayH * root.cropH))
                        return w + "×" + h
                    }
                }
            }
        }

        Row {
            id: transport
            width: parent.width
            spacing: Theme.spacingMd

            IconButton {
                visible: !root.isImage
                width: visible ? implicitWidth : 0
                anchors.verticalCenter: parent.verticalCenter
                glyph: player.playbackState === MediaPlayer.PlayingState
                       ? Theme.icons.pause : Theme.icons.play
                tooltip: player.playbackState === MediaPlayer.PlayingState ? qsTr("Pause") : qsTr("Play")
                enabled: !root.saving
                onClicked: root.togglePlay()
            }

            ThemedLabel {
                visible: !root.isImage
                width: visible ? implicitWidth : 0
                anchors.verticalCenter: parent.verticalCenter
                text: root.formatTime(player.position / 1000) + "  /  "
                      + root.formatTime(root.outSeconds - root.inSeconds)
                size: "sm"
                tone: "default"
            }

            ThemedButton {
                visible: root.canTrim
                width: visible ? implicitWidth : 0
                variant: "ghost"
                text: qsTr("Set In")
                glyph: Theme.icons.setStart
                enabled: !root.saving
                onClicked: {
                    root.inSeconds = player.position / 1000
                    root.clampRange()
                }
            }
            ThemedButton {
                visible: root.canTrim
                width: visible ? implicitWidth : 0
                variant: "ghost"
                text: qsTr("Set Out")
                glyph: Theme.icons.setEnd
                enabled: !root.saving
                onClicked: {
                    root.outSeconds = player.position / 1000
                    root.clampRange()
                }
            }

            ThemedButton {
                variant: "ghost"
                text: qsTr("Reset")
                enabled: root.dirty && !root.saving
                onClicked: root.resetEdits()
            }
        }

        Rectangle {
            id: strip
            visible: root.canTrim
            width: parent.width
            height: visible ? 64 : 0
            radius: Theme.radiusSm
            color: Theme.panelBackground
            border.width: Theme.borderWidth
            border.color: Theme.panelBorder
            clip: true

            ClipFilmstrip {
                anchors.fill: parent
                anchors.margins: Theme.borderWidth
                visible: root.filmstripPath.length > 0
                filmstripPath: root.filmstripPath
                frameWidth: Math.max(1, width / frameCount)
                sourcePath: root.sourcePath
                inPoint: 0
                outPoint: root.durationSeconds
                sourceDuration: root.durationSeconds
            }

            readonly property real dur: Math.max(0.001, root.durationSeconds)
            readonly property real inX: (root.inSeconds / dur) * width
            readonly property real outX: (root.outSeconds / dur) * width
            readonly property real playX: ((player.position / 1000) / dur) * width

            Rectangle {
                width: strip.inX
                height: parent.height
                color: Theme.scrimStrong
            }
            Rectangle {
                x: strip.outX
                width: Math.max(0, parent.width - x)
                height: parent.height
                color: Theme.scrimStrong
            }
            Rectangle {
                x: strip.inX
                width: Math.max(2, strip.outX - strip.inX)
                height: parent.height
                color: "transparent"
                border.width: Theme.borderWidth
                border.color: Theme.primary
            }
            Rectangle {
                x: strip.playX - 1
                width: 2
                height: parent.height
                color: Theme.onMedia
            }

            MouseArea {
                anchors.fill: parent
                enabled: !root.saving
                cursorShape: Qt.PointingHandCursor
                onPressed: (mouse) => {
                    const t = (mouse.x / Math.max(1, width)) * root.durationSeconds
                    root.seekTo(t)
                    if (player.playbackState !== MediaPlayer.PlayingState)
                        player.pause()
                }
                onPositionChanged: (mouse) => {
                    if (!pressed)
                        return
                    const t = (mouse.x / Math.max(1, width)) * root.durationSeconds
                    root.seekTo(Math.max(0, Math.min(root.durationSeconds, t)))
                }
            }

            Repeater {
                model: [
                    { edge: "in" },
                    { edge: "out" }
                ]
                Rectangle {
                    required property var modelData
                    width: Theme.touchUi ? 14 : 8
                    height: parent.height
                    x: (modelData.edge === "in" ? strip.inX : strip.outX) - width / 2
                    color: Theme.primary
                    z: 3

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: Theme.touchUi ? -8 : -4
                        enabled: !root.saving
                        cursorShape: Qt.SizeHorCursor
                        preventStealing: true
                        onPositionChanged: (mouse) => {
                            if (!pressed)
                                return
                            const t = ((parent.x + width / 2 + mouse.x)
                                       / Math.max(1, strip.width)) * root.durationSeconds
                            if (modelData.edge === "in")
                                root.inSeconds = t
                            else
                                root.outSeconds = t
                            root.clampRange()
                            root.seekTo(modelData.edge === "in" ? root.inSeconds : root.outSeconds)
                        }
                    }
                }
            }
        }
    }

    Row {
        id: footer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        ThemedLabel {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - cancelBtn.width - saveBtn.width - parent.spacing * 2
            elide: Text.ElideRight
            tone: "muted"
            text: root.saving
                  ? (EditorState.assetEditStatus.length > 0
                     ? EditorState.assetEditStatus
                     : qsTr("Saving…"))
                  : root.dirty
                    ? qsTr("Save writes a new file over this item in the bin.")
                    : qsTr("Nothing to save — drag this item onto the timeline when you are ready.")
        }

        ThemedButton {
            id: cancelBtn
            variant: "ghost"
            text: root.saving ? qsTr("Cancel") : qsTr("Close")
            onClicked: {
                if (root.saving)
                    EditorState.cancelAssetEdit()
                else
                    root.close()
            }
        }

        ThemedButton {
            id: saveBtn
            variant: "primary"
            text: qsTr("Save")
            enabled: root.dirty && !root.saving && root.assetIndex >= 0
            onClicked: {
                player.pause()
                EditorState.saveAssetEdit(root.assetIndex, root.inSeconds,
                                          root.canTrim ? root.outSeconds : -1,
                                          root.cropX, root.cropY, root.cropW, root.cropH)
            }
        }
    }

    Rectangle {
        visible: root.saving
        anchors.fill: parent
        color: Theme.scrimColor
        z: 10

        Column {
            anchors.centerIn: parent
            spacing: Theme.spacingLg
            width: Math.min(parent.width - Theme.spacing3xl * 2, 320)

            ThemedLabel {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: EditorState.assetEditStatus.length > 0
                      ? EditorState.assetEditStatus : qsTr("Saving…")
                tone: "default"
                size: "sm"
            }
            ThemedProgressBar {
                width: parent.width
                value: EditorState.assetEditProgress
            }
        }
    }
}
