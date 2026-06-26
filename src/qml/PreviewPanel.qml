import QtQuick
import QtQuick.Controls
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

            Connections {
                target: EditorState
                function onTransformBlocked(reason) { blockToast.show(reason) }
            }

            Rectangle {
                id: blockToast
                z: 10
                property string message: ""
                function show(msg) { message = msg; opacity = 1; hideTimer.restart() }
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 16
                visible: opacity > 0
                opacity: 0
                radius: Theme.radiusMd
                color: Theme.panelBackground
                border.width: 1
                border.color: Theme.panelBorder
                implicitWidth: toastLabel.implicitWidth + 24
                implicitHeight: toastLabel.implicitHeight + 14
                Behavior on opacity { NumberAnimation { duration: 200 } }
                Text {
                    id: toastLabel
                    anchors.centerIn: parent
                    text: blockToast.message
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }
                Timer { id: hideTimer; interval: 2000; onTriggered: blockToast.opacity = 0 }
            }

            Item {
                id: viewport
                anchors.fill: parent
                anchors.margins: 8
                anchors.bottomMargin: 0

                property real aspect: {
                    void EditorState.tracks
                    const w = EditorState.projectWidth()
                    const h = EditorState.projectHeight()
                    return (w > 0 && h > 0) ? (w / h) : (16 / 9)
                }
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

                        function updateRenderSize() {
                            EditorState.playback.setPreviewRenderSize(Math.round(width), Math.round(height))
                        }

                        Component.onCompleted: updateRenderSize()
                        onWidthChanged: updateRenderSize()
                        onHeightChanged: updateRenderSize()
                    }

                    Item {
                        id: transformOverlay
                        anchors.fill: parent
                        visible: !root.playing && EditorState.projectWidth() > 0

                        property var overlayClips: []
                        // True while a handle is being dragged. Rebuilding the model
                        // mid-drag would destroy the delegate that owns the active
                        // grab, so refreshes are suppressed until the drag ends.
                        // (Also held true while a text clip is edited in place, so
                        // the inline editor delegate is not destroyed mid-edit.)
                        property bool interacting: false

                        // "track:clip" of the text clip currently edited in place,
                        // or "" when no inline edit is active.
                        property string editingKey: ""

                        // "track:clip" of a freshly added placeholder text clip that
                        // should open its editor as soon as its box exists.
                        property string pendingEditKey: ""

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
                            function onInlineTextEditRequested(trackIndex, clipIndex) {
                                transformOverlay.pendingEditKey = trackIndex + ":" + clipIndex
                                transformOverlay.refreshOverlay()
                            }
                        }

                        Repeater {
                            model: transformOverlay.overlayClips

                            delegate: Item {
                                id: handle
                                required property var modelData

                                readonly property bool selected: EditorState.selectedTrack === modelData.track
                                                                        && EditorState.selectedClip === modelData.clip
                                readonly property bool isText: modelData.kind === "text"
                                readonly property bool editing: transformOverlay.editingKey
                                                                        === (modelData.track + ":" + modelData.clip)
                                // Snapshot of the clip's TextStyle taken when editing
                                // begins; drives the inline editor's font/color/align.
                                property var editStyle: null

                                function enterEdit() {
                                    const info = EditorState.clipAt(modelData.track, modelData.clip)
                                    handle.editStyle = info.textStyle
                                    editor.text = info.textContent || ""
                                    transformOverlay.editingKey = modelData.track + ":" + modelData.clip
                                    transformOverlay.interacting = true
                                    EditorState.selectClip(modelData.track, modelData.clip)
                                    EditorState.beginTextEdit(modelData.track, modelData.clip)
                                    editor.forceActiveFocus()
                                    editor.selectAll()
                                }

                                function commitEdit() {
                                    if (!handle.editing)
                                        return
                                    EditorState.setClipTextContent(modelData.track, modelData.clip, editor.text)
                                    handle.finishEdit()
                                }

                                function cancelEdit() {
                                    if (!handle.editing)
                                        return
                                    handle.finishEdit()
                                }

                                function finishEdit() {
                                    transformOverlay.editingKey = ""
                                    EditorState.endTextEdit()
                                    transformOverlay.interacting = false
                                    Qt.callLater(transformOverlay.refreshOverlay)
                                }

                                // A just-added placeholder clip opens its own editor.
                                // Checked both on creation and when the key arrives,
                                // since either can happen first.
                                function claimPendingEdit() {
                                    if (!handle.isText || handle.editing)
                                        return
                                    if (transformOverlay.pendingEditKey
                                            !== (modelData.track + ":" + modelData.clip))
                                        return
                                    transformOverlay.pendingEditKey = ""
                                    Qt.callLater(handle.enterEdit)
                                }

                                Component.onCompleted: handle.claimPendingEdit()

                                Connections {
                                    target: transformOverlay
                                    function onPendingEditKeyChanged() { handle.claimPendingEdit() }
                                }

                                readonly property real canvasW: Math.max(1, modelData.canvasWidth)
                                readonly property real canvasH: Math.max(1, modelData.canvasHeight)
                                readonly property real sx: parent.width / canvasW
                                readonly property real sy: parent.height / canvasH

                                // Live overrides applied during a drag so the box tracks
                                // the cursor without rebuilding the (stale) model.
                                property real liveX: -1e12
                                property real liveY: -1e12
                                property real liveW: -1
                                property real liveH: -1
                                property real liveRotation: 1e9

                                readonly property real layoutX: liveX > -1e11 ? liveX : modelData.x
                                readonly property real layoutY: liveY > -1e11 ? liveY : modelData.y
                                readonly property real layoutW: liveW >= 0 ? liveW : modelData.width
                                readonly property real layoutH: liveH >= 0 ? liveH : modelData.height
                                readonly property real centerX: (layoutX + layoutW * 0.5) * sx
                                readonly property real centerY: (layoutY + layoutH * 0.5) * sy

                                x: layoutX * sx
                                y: layoutY * sy
                                width: Math.max(24, layoutW * sx)
                                height: Math.max(24, layoutH * sy)
                                // Front-most track (lowest index) sits on top so it
                                // wins click hit-testing over boxes behind it. The clip
                                // being edited jumps above everything so its editor and
                                // the click-away catcher order correctly.
                                z: handle.editing ? 1000 : -modelData.track
                                transformOrigin: Item.Center
                                rotation: liveRotation < 1e8 ? liveRotation : modelData.rotation

                                property real dragStartX: 0
                                property real dragStartY: 0
                                property real dragStartW: 1
                                property real dragStartH: 1
                                property int dragStartPixelSize: 64

                                Rectangle {
                                    anchors.fill: parent
                                    color: "transparent"
                                    border.width: (handle.selected || handle.editing) ? 2 : 1
                                    border.color: handle.selected ? Theme.primary : "#99ffffff"
                                    radius: 2
                                }

                                // In-place text editor (Canva-style). Shown over the
                                // box while editing; the baked raster is hidden by the
                                // compositor via beginTextEdit. Plain TextArea (not a
                                // Themed* control): this is canvas content that must
                                // match the rendered text, not app chrome.
                                TextArea {
                                    id: editor
                                    anchors.fill: parent
                                    visible: handle.editing
                                    enabled: handle.editing
                                    background: null
                                    padding: 0
                                    selectByMouse: true
                                    color: handle.editStyle ? handle.editStyle.color : "white"
                                    font.family: handle.editStyle ? handle.editStyle.fontFamily : Theme.fontFamily
                                    font.pixelSize: handle.editStyle
                                                    ? Math.max(1, Math.round(handle.editStyle.pixelSize * handle.sy))
                                                    : 16
                                    font.weight: handle.editStyle ? handle.editStyle.fontWeight : Font.Normal
                                    font.italic: handle.editStyle ? handle.editStyle.italic : false
                                    font.letterSpacing: handle.editStyle ? handle.editStyle.letterSpacing * handle.sy : 0
                                    wrapMode: (handle.editStyle && handle.editStyle.wordWrap === false)
                                              ? TextEdit.NoWrap : TextEdit.WordWrap
                                    horizontalAlignment: !handle.editStyle ? TextEdit.AlignHCenter
                                                         : handle.editStyle.align === "left" ? TextEdit.AlignLeft
                                                         : handle.editStyle.align === "right" ? TextEdit.AlignRight
                                                         : TextEdit.AlignHCenter
                                    verticalAlignment: !handle.editStyle ? TextEdit.AlignVCenter
                                                       : handle.editStyle.valign === "top" ? TextEdit.AlignTop
                                                       : handle.editStyle.valign === "bottom" ? TextEdit.AlignBottom
                                                       : TextEdit.AlignVCenter
                                    Keys.onEscapePressed: handle.cancelEdit()
                                    onActiveFocusChanged: if (!activeFocus && handle.editing) handle.commitEdit()
                                }

                                TapHandler {
                                    enabled: !handle.editing
                                    onTapped: EditorState.selectClip(handle.modelData.track, handle.modelData.clip)
                                    onDoubleTapped: if (handle.isText) handle.enterEdit()
                                }

                                DragHandler {
                                    target: null
                                    enabled: !handle.editing
                                    cursorShape: Qt.SizeAllCursor
                                    onActiveChanged: {
                                        if (active) {
                                            transformOverlay.interacting = true
                                            handle.dragStartX = handle.modelData.x
                                            handle.dragStartY = handle.modelData.y
                                            handle.liveX = handle.dragStartX
                                            handle.liveY = handle.dragStartY
                                            EditorState.selectClip(handle.modelData.track, handle.modelData.clip)
                                            EditorState.beginPreviewDrag()
                                        } else {
                                            handle.liveX = -1e12
                                            handle.liveY = -1e12
                                            transformOverlay.endInteraction()
                                        }
                                    }
                                    onTranslationChanged: {
                                        // translation is in the rotated box frame; rotate it back to canvas axes
                                        const a = handle.rotation * Math.PI / 180
                                        const dx = translation.x * Math.cos(a) - translation.y * Math.sin(a)
                                        const dy = translation.x * Math.sin(a) + translation.y * Math.cos(a)
                                        const xPx = handle.dragStartX + dx / handle.sx
                                        const yPx = handle.dragStartY + dy / handle.sy
                                        handle.liveX = xPx
                                        handle.liveY = yPx
                                        EditorState.previewSetClipPosition(
                                            handle.modelData.track,
                                            handle.modelData.clip,
                                            xPx,
                                            yPx)
                                    }
                                }

                                // Corner resize handles (opposite corner stays fixed)
                                Repeater {
                                    model: (handle.selected && !handle.editing) ? 4 : 0

                                    delegate: Rectangle {
                                        id: corner
                                        required property int index
                                        // 0 TL, 1 TR, 2 BL, 3 BR
                                        readonly property real sxSign: (index === 0 || index === 2) ? -1 : 1
                                        readonly property real sySign: (index < 2) ? -1 : 1

                                        width: 12
                                        height: 12
                                        radius: 2
                                        color: Theme.primary
                                        border.width: 1
                                        border.color: "#ffffff"
                                        x: (sxSign < 0 ? 0 : handle.width) - width / 2
                                        y: (sySign < 0 ? 0 : handle.height) - height / 2

                                        DragHandler {
                                            id: cornerDrag
                                            target: null
                                            cursorShape: (corner.sxSign * corner.sySign < 0) ? Qt.SizeBDiagCursor : Qt.SizeFDiagCursor
                                            onActiveChanged: {
                                                if (active) {
                                                    handle.dragStartX = handle.layoutX
                                                    handle.dragStartY = handle.layoutY
                                                    handle.dragStartW = handle.layoutW
                                                    handle.dragStartH = handle.layoutH
                                                    handle.dragStartPixelSize = handle.modelData.pixelSize || 64
                                                    handle.liveX = handle.dragStartX
                                                    handle.liveY = handle.dragStartY
                                                    handle.liveW = handle.dragStartW
                                                    handle.liveH = handle.dragStartH
                                                    transformOverlay.interacting = true
                                                    EditorState.selectClip(handle.modelData.track, handle.modelData.clip)
                                                    EditorState.beginPreviewDrag()
                                                } else {
                                                    handle.liveX = -1e12
                                                    handle.liveY = -1e12
                                                    handle.liveW = -1
                                                    handle.liveH = -1
                                                    transformOverlay.endInteraction()
                                                }
                                            }
                                            onCentroidChanged: {
                                                if (!active)
                                                    return
                                                const p = transformOverlay.mapFromItem(null, cornerDrag.centroid.scenePosition.x,
                                                                                        cornerDrag.centroid.scenePosition.y)
                                                const px = p.x / handle.sx
                                                const py = p.y / handle.sy
                                                const right = handle.dragStartX + handle.dragStartW
                                                const bottom = handle.dragStartY + handle.dragStartH
                                                let x = handle.dragStartX
                                                let y = handle.dragStartY
                                                let w = handle.dragStartW
                                                let h = handle.dragStartH
                                                if (corner.index === 0) { // TL
                                                    x = Math.min(px, right - 1)
                                                    y = Math.min(py, bottom - 1)
                                                    w = right - x
                                                    h = bottom - y
                                                } else if (corner.index === 1) { // TR
                                                    y = Math.min(py, bottom - 1)
                                                    w = Math.max(1, px - handle.dragStartX)
                                                    h = bottom - y
                                                } else if (corner.index === 2) { // BL
                                                    x = Math.min(px, right - 1)
                                                    w = right - x
                                                    h = Math.max(1, py - handle.dragStartY)
                                                } else { // BR
                                                    w = Math.max(1, px - handle.dragStartX)
                                                    h = Math.max(1, py - handle.dragStartY)
                                                }
                                                handle.liveX = x
                                                handle.liveY = y
                                                handle.liveW = w
                                                handle.liveH = h
                                                if (handle.modelData.kind === "text") {
                                                    // Height drives the glyph scale; a width-only drag
                                                    // just re-wraps, since the box is the wrap width.
                                                    const px = Math.round(handle.dragStartPixelSize
                                                                          * h / Math.max(1, handle.dragStartH))
                                                    EditorState.previewSetTextRect(
                                                        handle.modelData.track,
                                                        handle.modelData.clip,
                                                        x, y, w, h, px)
                                                } else {
                                                    EditorState.previewSetClipRect(
                                                        handle.modelData.track,
                                                        handle.modelData.clip,
                                                        x, y, w, h)
                                                }
                                            }
                                        }
                                    }
                                }

                                // Rotation handle above the box
                                Item {
                                    visible: handle.selected && !handle.editing
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

                        // Click-away catcher: while a text clip is edited in place,
                        // a press outside the (raised) editor box drops focus, which
                        // commits the edit via the editor's onActiveFocusChanged.
                        MouseArea {
                            anchors.fill: parent
                            z: 500
                            visible: transformOverlay.editingKey !== ""
                            enabled: visible
                            onPressed: (mouse) => {
                                transformOverlay.forceActiveFocus()
                                mouse.accepted = true
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
                            preview.textureSize = EditorState.playback.previewTextureSize
                            preview.textureId = EditorState.playback.previewTextureId
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
                onClicked: EditorState.togglePlayback()
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
