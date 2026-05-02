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
                        id: transformOverlay
                        anchors.fill: parent
                        visible: !root.playing && EditorState.projectWidth() > 0

                        property var overlayClips: []
                        // True while a handle is being dragged. Rebuilding the model
                        // mid-drag would destroy the delegate that owns the active
                        // grab, so refreshes are suppressed until the drag ends.
                        property bool interacting: false

                        function refreshOverlay() {
                            if (interacting)
                                return
                            overlayClips = EditorState.previewClipsAtPlayhead()
                        }

                        function endInteraction() {
                            EditorState.commitPreviewDrag()
                            interacting = false
                            Qt.callLater(refreshOverlay)
                        }

                        Component.onCompleted: refreshOverlay()

                        Connections {
                            target: EditorState
                            function onTracksChanged() { transformOverlay.refreshOverlay() }
                            function onSelectionChanged() { transformOverlay.refreshOverlay() }
                            function onPlayheadSecondsChanged() { transformOverlay.refreshOverlay() }
                        }

                        Repeater {
                            model: transformOverlay.overlayClips

                            delegate: Item {
                                id: handle
                                required property var modelData

                                readonly property bool selected: EditorState.selectedTrack === modelData.track
                                                                        && EditorState.selectedClip === modelData.clip

                                // Live overrides applied during a drag so the box tracks
                                // the cursor without rebuilding the (stale) model.
                                property real liveDX: 0
                                property real liveDY: 0
                                property real liveScale: -1
                                property real liveRotation: 1e9

                                readonly property real baseHalfW: modelData.halfW / Math.max(0.0001, modelData.scale)
                                readonly property real baseHalfH: modelData.halfH / Math.max(0.0001, modelData.scale)
                                readonly property real effScale: liveScale >= 0 ? liveScale : modelData.scale
                                readonly property real centerX: modelData.posX * parent.width + liveDX
                                readonly property real centerY: modelData.posY * parent.height + liveDY
                                readonly property real boxW: Math.max(24, baseHalfW * effScale * 2 * parent.width)
                                readonly property real boxH: Math.max(24, baseHalfH * effScale * 2 * parent.height)

                                x: centerX - boxW / 2
                                y: centerY - boxH / 2
                                width: boxW
                                height: boxH
                                transformOrigin: Item.Center
                                rotation: liveRotation < 1e8 ? liveRotation : modelData.rotation

                                property real dragStartX: 0
                                property real dragStartY: 0
                                property real dragStartScale: 1
                                property real dragStartDist: 1

                                Rectangle {
                                    anchors.fill: parent
                                    color: "transparent"
                                    border.width: handle.selected ? 2 : 1
                                    border.color: handle.selected ? Theme.primary : "#99ffffff"
                                    radius: 2
                                }

                                TapHandler {
                                    onTapped: EditorState.selectClip(handle.modelData.track, handle.modelData.clip)
                                }

                                DragHandler {
                                    target: null
                                    cursorShape: Qt.SizeAllCursor
                                    onActiveChanged: {
                                        if (active) {
                                            transformOverlay.interacting = true
                                            handle.dragStartX = handle.modelData.posX
                                            handle.dragStartY = handle.modelData.posY
                                            EditorState.selectClip(handle.modelData.track, handle.modelData.clip)
                                            EditorState.beginPreviewDrag()
                                        } else {
                                            handle.liveDX = 0
                                            handle.liveDY = 0
                                            transformOverlay.endInteraction()
                                        }
                                    }
                                    onTranslationChanged: {
                                        // translation is in the rotated box frame; rotate it back to canvas axes
                                        const a = handle.rotation * Math.PI / 180
                                        const dx = translation.x * Math.cos(a) - translation.y * Math.sin(a)
                                        const dy = translation.x * Math.sin(a) + translation.y * Math.cos(a)
                                        handle.liveDX = dx
                                        handle.liveDY = dy
                                        EditorState.previewSetClipPosition(
                                            handle.modelData.track,
                                            handle.modelData.clip,
                                            handle.dragStartX + dx / handle.parent.width,
                                            handle.dragStartY + dy / handle.parent.height)
                                    }
                                }

                                // Corner resize handles (uniform scale)
                                Repeater {
                                    model: handle.selected ? 4 : 0

                                    delegate: Rectangle {
                                        id: corner
                                        required property int index
                                        readonly property real sx: (index === 0 || index === 2) ? -1 : 1
                                        readonly property real sy: (index < 2) ? -1 : 1

                                        width: 12
                                        height: 12
                                        radius: 2
                                        color: Theme.primary
                                        border.width: 1
                                        border.color: "#ffffff"
                                        x: (sx < 0 ? 0 : handle.width) - width / 2
                                        y: (sy < 0 ? 0 : handle.height) - height / 2

                                        DragHandler {
                                            id: cornerDrag
                                            target: null
                                            cursorShape: (corner.sx * corner.sy < 0) ? Qt.SizeBDiagCursor : Qt.SizeFDiagCursor
                                            onActiveChanged: {
                                                if (active) {
                                                    const p0 = transformOverlay.mapFromItem(null, cornerDrag.centroid.scenePosition.x,
                                                                                             cornerDrag.centroid.scenePosition.y)
                                                    handle.dragStartScale = handle.modelData.scale
                                                    handle.dragStartDist = Math.max(1, Math.hypot(p0.x - handle.centerX,
                                                                                                   p0.y - handle.centerY))
                                                    transformOverlay.interacting = true
                                                    handle.liveScale = handle.dragStartScale
                                                    EditorState.selectClip(handle.modelData.track, handle.modelData.clip)
                                                    EditorState.beginPreviewDrag()
                                                } else {
                                                    handle.liveScale = -1
                                                    transformOverlay.endInteraction()
                                                }
                                            }
                                            onCentroidChanged: {
                                                if (!active)
                                                    return
                                                const p = transformOverlay.mapFromItem(null, cornerDrag.centroid.scenePosition.x,
                                                                                        cornerDrag.centroid.scenePosition.y)
                                                const dist = Math.hypot(p.x - handle.centerX, p.y - handle.centerY)
                                                const newScale = Math.max(0.05, handle.dragStartScale * dist / handle.dragStartDist)
                                                handle.liveScale = newScale
                                                EditorState.previewSetClipScale(
                                                    handle.modelData.track,
                                                    handle.modelData.clip,
                                                    newScale)
                                            }
                                        }
                                    }
                                }

                                // Rotation handle above the box
                                Item {
                                    visible: handle.selected
                                    width: 14
                                    height: 14
                                    x: handle.width / 2 - width / 2
                                    y: -28

                                    Rectangle {
                                        anchors.top: parent.bottom
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        width: 1
                                        height: 16
                                        color: Theme.primary
                                    }

                                    Rectangle {
                                        anchors.fill: parent
                                        radius: width / 2
                                        color: Theme.primary
                                        border.width: 1
                                        border.color: "#ffffff"
                                    }

                                    DragHandler {
                                        id: rotateDrag
                                        target: null
                                        cursorShape: Qt.CrossCursor
                                        onActiveChanged: {
                                            if (active) {
                                                transformOverlay.interacting = true
                                                handle.liveRotation = handle.modelData.rotation
                                                EditorState.selectClip(handle.modelData.track, handle.modelData.clip)
                                                EditorState.beginPreviewDrag()
                                            } else {
                                                handle.liveRotation = 1e9
                                                transformOverlay.endInteraction()
                                            }
                                        }
                                        onCentroidChanged: {
                                            if (!active)
                                                return
                                            const p = transformOverlay.mapFromItem(null, rotateDrag.centroid.scenePosition.x,
                                                                                     rotateDrag.centroid.scenePosition.y)
                                            const ang = Math.atan2(p.y - handle.centerY, p.x - handle.centerX)
                                            const deg = ang * 180 / Math.PI + 90
                                            handle.liveRotation = deg
                                            EditorState.previewSetClipRotation(
                                                handle.modelData.track,
                                                handle.modelData.clip,
                                                deg)
                                        }
                                    }
                                }
                            }
                        }
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
