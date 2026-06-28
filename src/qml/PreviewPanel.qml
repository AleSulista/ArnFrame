import QtQuick
// .Basic, matching every other file. Plain QtQuick.Controls pulled in the
// platform style, so the inline text editor below was styled differently from
// the rest of the app.
import QtQuick.Controls.Basic
// Window was used (fullscreen toggle) without being imported.
import QtQuick.Window
import QtQuick.Layouts
import Drift
import "components"

PanelFrame {
    id: root

    readonly property real currentSeconds: EditorState.playheadSeconds
    readonly property real durationSeconds: EditorState.durationSeconds
    readonly property bool playing: EditorState.playing

    // Driven by Main, which owns the window and the panels that hide around it.
    property bool previewFullscreen: false
    signal fullscreenRequested()

    // Frame rate of the project, not a fixed 30 — the timecode readout showed
    // wrong frame numbers for every project that was not 30fps.
    readonly property int projectFps: {
        void EditorState.tracks
        const fps = EditorState.projectFps()
        return fps > 0 ? fps : 30
    }

    function formatTimecode(seconds) {
        const fps = root.projectFps;
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
            height: parent.height - toolbar.height - scrubBar.height
            clip: true

            // `transformBlocked` is now handled centrally in Main.qml and shown
            // through the app-wide toast host, so the same block raised by a
            // timeline drag is reported too. This panel-local toast is gone.

            Item {
                id: viewport
                anchors.fill: parent
                anchors.margins: Theme.spacingLg
                anchors.bottomMargin: 0

                property real aspect: {
                    void EditorState.tracks
                    const w = EditorState.projectWidth()
                    const h = EditorState.projectHeight()
                    return (w > 0 && h > 0) ? (w / h) : (16 / 9)
                }
                property bool fitMode: true
                // Crop mode pulls the canvas in so there is room around it to drag
                // an edge outward and grow the frame.
                property real cropZoom: EditorState.canvasCropMode ? 0.72 : 1.0
                // Crop-mode navigation: wheel zoom about the cursor, middle-drag pan.
                // Both reset when crop mode ends, so normal preview is never left
                // scrolled off-centre.
                property real userZoom: 1.0
                property real panX: 0
                property real panY: 0

                readonly property real baseWidth: fitMode ? Math.min(width, height * aspect) : width
                readonly property real baseHeight: fitMode ? baseWidth / aspect : height
                property real fitWidth: baseWidth * cropZoom * userZoom
                property real fitHeight: baseHeight * cropZoom * userZoom

                function resetView() {
                    userZoom = 1.0
                    panX = 0
                    panY = 0
                }

                // Scales about (mx, my) in viewport coords: the point under the
                // cursor keeps its position, so zooming into a crop corner keeps
                // that corner in place instead of drifting off screen.
                function zoomAt(mx, my, factor) {
                    const next = Math.max(0.25, Math.min(12.0, userZoom * factor))
                    if (next === userZoom)
                        return
                    const w = fitWidth
                    const h = fitHeight
                    const fx = w > 0 ? (mx - ((width - w) / 2 + panX)) / w : 0.5
                    const fy = h > 0 ? (my - ((height - h) / 2 + panY)) / h : 0.5
                    const nw = baseWidth * cropZoom * next
                    const nh = baseHeight * cropZoom * next
                    userZoom = next
                    panX = (mx - fx * nw) - (width - nw) / 2
                    panY = (my - fy * nh) - (height - nh) / 2
                }

                Behavior on cropZoom {
                    NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easing }
                }

                Rectangle {
                    id: canvasRect
                    width: viewport.fitWidth
                    height: viewport.fitHeight
                    x: (viewport.width - width) / 2 + viewport.panX
                    y: (viewport.height - height) / 2 + viewport.panY
                    color: Theme.overlayColor
                    border.width: Theme.borderWidth
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
                                 && !EditorState.canvasCropMode

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

                                // Arrow keys move the selected clip; Shift makes
                                // the step coarse. Transform used to be
                                // drag-only, with no keyboard path at all.
                                focus: handle.selected && !handle.editing
                                Keys.onPressed: function(event) {
                                    if (!handle.selected || handle.editing)
                                        return
                                    const step = (event.modifiers & Qt.ShiftModifier) ? 10 : 1
                                    let dx = 0
                                    let dy = 0
                                    switch (event.key) {
                                    case Qt.Key_Left:  dx = -step; break
                                    case Qt.Key_Right: dx = step; break
                                    case Qt.Key_Up:    dy = -step; break
                                    case Qt.Key_Down:  dy = step; break
                                    default: return
                                    }
                                    EditorState.beginPreviewDrag()
                                    EditorState.previewSetClipPosition(
                                        handle.modelData.track,
                                        handle.modelData.clip,
                                        handle.modelData.x + dx,
                                        handle.modelData.y + dy)
                                    EditorState.commitPreviewDrag()
                                    event.accepted = true
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    color: "transparent"
                                    border.width: (handle.selected || handle.editing)
                                                  ? Theme.borderWidthFocus : Theme.borderWidth
                                    border.color: handle.selected ? Theme.primary : Theme.guideStrong
                                    radius: Theme.radiusXs

                                    Behavior on border.width {
                                        NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                                    }
                                    Behavior on border.color {
                                        ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                                    }
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
                                        border.color: Theme.onMedia
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
                                        border.color: Theme.onMedia
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
                                color: Theme.guideMedium
                            }
                        }
                        Repeater {
                            model: EditorState.guideType === "thirds" ? 2 : 0
                            Rectangle {
                                height: 1
                                width: parent.width
                                y: parent.height * (index + 1) / 3
                                color: Theme.guideMedium
                            }
                        }

                        Rectangle {
                            visible: EditorState.guideType === "crosshair"
                            width: 1
                            height: parent.height
                            x: parent.width / 2
                            color: Theme.guideMedium
                        }
                        Rectangle {
                            visible: EditorState.guideType === "crosshair"
                            height: 1
                            width: parent.width
                            y: parent.height / 2
                            color: Theme.guideMedium
                        }

                        Rectangle {
                            visible: EditorState.guideType === "safe"
                            x: parent.width * 0.05
                            y: parent.height * 0.05
                            width: parent.width * 0.90
                            height: parent.height * 0.90
                            color: "transparent"
                            border.width: 1
                            border.color: Theme.guideMedium
                        }
                        Rectangle {
                            visible: EditorState.guideType === "safe"
                            x: parent.width * 0.025
                            y: parent.height * 0.025
                            width: parent.width * 0.95
                            height: parent.height * 0.95
                            color: "transparent"
                            border.width: 1
                            border.color: Theme.guideWeak
                        }
                    }

                    Connections {
                        target: EditorState.playback
                        function onCurrentFrameChanged() {
                            preview.textureSize = EditorState.playback.previewTextureSize
                            preview.textureId = EditorState.playback.previewTextureId
                        }
                    }

                    // Fades rather than popping, so scrubbing across a gap no
                    // longer flickers this text on and off.
                    Text {
                        anchors.centerIn: parent
                        visible: opacity > 0
                        opacity: EditorState.playback.hasFrame ? 0 : 1
                        text: EditorState.activeAudioClipAtPlayhead().path
                              ? qsTr("Audio only") : qsTr("No clip at playhead")
                        // Drawn on the letterbox scrim, not a panel surface, so it
                        // follows the on-media tokens in both themes.
                        color: Theme.guideMedium
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm

                        Behavior on opacity {
                            NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easing }
                        }
                    }
                }

                // Canvas crop tool. Lives outside the (clipped) canvas rect so the
                // crop frame can be dragged past the current edges to grow the
                // output. Values are kept in project pixels; committing hands the
                // rect to AppController, which rebases clip layout so nothing
                // moves or rescales — content outside the new frame is simply lost.
                Item {
                    id: cropOverlay
                    anchors.fill: parent
                    visible: EditorState.canvasCropMode
                    enabled: visible
                    z: 200

                    readonly property int projW: { void EditorState.tracks; return Math.max(1, EditorState.projectWidth()) }
                    readonly property int projH: { void EditorState.tracks; return Math.max(1, EditorState.projectHeight()) }
                    // Project px → viewport px.
                    readonly property real pxScale: canvasRect.width / projW

                    property real cropX: 0
                    property real cropY: 0
                    property real cropW: projW
                    property real cropH: projH

                    readonly property int outW: Math.round(cropW)
                    readonly property int outH: Math.round(cropH)

                    // Hint bookkeeping. Set as each input is first used; once all
                    // three are known the hint retires and does not come back, so
                    // reopening the tool is not nagging.
                    property bool didResize: false
                    property bool didZoom: false
                    property bool didPan: false
                    property bool hintDismissed: false
                    onDidResizeChanged: maybeDismissHint()
                    onDidZoomChanged: maybeDismissHint()
                    onDidPanChanged: maybeDismissHint()

                    function maybeDismissHint() {
                        if (didResize && didZoom && didPan)
                            hintDismissed = true
                    }
                    readonly property bool changed: outW !== projW || outH !== projH
                                                    || Math.round(cropX) !== 0 || Math.round(cropY) !== 0

                    function reset() {
                        cropX = 0
                        cropY = 0
                        cropW = projW
                        cropH = projH
                    }

                    function apply() {
                        if (changed)
                            EditorState.applyCanvasCrop(cropX, cropY, cropW, cropH)
                        EditorState.canvasCropMode = false
                    }

                    // Screen-space crop frame, relative to the viewport.
                    readonly property real frameX: canvasRect.x + cropX * pxScale
                    readonly property real frameY: canvasRect.y + cropY * pxScale
                    readonly property real frameW: cropW * pxScale
                    readonly property real frameH: cropH * pxScale

                    onVisibleChanged: {
                        viewport.resetView()
                        if (visible)
                            reset()
                    }

                    // Navigation sits below the handles in stacking order, and takes
                    // only the middle button, so left-drags still reach the grips.
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.MiddleButton
                        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.ArrowCursor

                        property real lastX: 0
                        property real lastY: 0

                        onPressed: (mouse) => {
                            lastX = mouse.x
                            lastY = mouse.y
                        }
                        onPositionChanged: (mouse) => {
                            if (!pressed)
                                return
                            viewport.panX += mouse.x - lastX
                            viewport.panY += mouse.y - lastY
                            lastX = mouse.x
                            lastY = mouse.y
                            cropOverlay.didPan = true
                        }

                        // Zoom lives here rather than in a sibling WheelHandler: a
                        // MouseArea accepts wheel events even with no onWheel bound,
                        // so a separate handler behind it never got them. Ctrl-less
                        // scrolls are explicitly rejected so they keep propagating.
                        onWheel: (wheel) => {
                            if (!(wheel.modifiers & Qt.ControlModifier)
                                    || wheel.angleDelta.y === 0) {
                                wheel.accepted = false
                                return
                            }
                            viewport.zoomAt(wheel.x, wheel.y,
                                            wheel.angleDelta.y > 0 ? 1.15 : 1 / 1.15)
                            cropOverlay.didZoom = true
                            wheel.accepted = true
                        }
                    }

                    // Everything outside the crop frame is discarded, so dim it.
                    // Four bands rather than a mask: no shader, no clipping cost.
                    Rectangle {
                        color: Theme.overlayColor
                        opacity: 0.65
                        x: 0
                        y: 0
                        width: Math.max(0, cropOverlay.frameX)
                        height: cropOverlay.height
                    }
                    Rectangle {
                        color: Theme.overlayColor
                        opacity: 0.65
                        x: cropOverlay.frameX + cropOverlay.frameW
                        y: 0
                        width: Math.max(0, cropOverlay.width - x)
                        height: cropOverlay.height
                    }
                    Rectangle {
                        color: Theme.overlayColor
                        opacity: 0.65
                        x: cropOverlay.frameX
                        y: 0
                        width: Math.max(0, cropOverlay.frameW)
                        height: Math.max(0, cropOverlay.frameY)
                    }
                    Rectangle {
                        color: Theme.overlayColor
                        opacity: 0.65
                        x: cropOverlay.frameX
                        y: cropOverlay.frameY + cropOverlay.frameH
                        width: Math.max(0, cropOverlay.frameW)
                        height: Math.max(0, cropOverlay.height - y)
                    }

                    Rectangle {
                        x: cropOverlay.frameX
                        y: cropOverlay.frameY
                        width: cropOverlay.frameW
                        height: cropOverlay.frameH
                        color: "transparent"
                        border.width: Theme.borderWidthFocus
                        border.color: Theme.primary

                        // Rule-of-thirds inside the crop frame, the usual cue for
                        // judging a reframe.
                        Repeater {
                            model: 2
                            Rectangle {
                                width: 1
                                height: parent.height
                                x: parent.width * (index + 1) / 3
                                color: Theme.guideWeak
                            }
                        }
                        Repeater {
                            model: 2
                            Rectangle {
                                height: 1
                                width: parent.width
                                y: parent.height * (index + 1) / 3
                                color: Theme.guideWeak
                            }
                        }
                    }

                    // Eight drag handles: 4 edges then 4 corners. `dx`/`dy` say
                    // which edges each handle moves (-1 = left/top, +1 = right/bottom).
                    Repeater {
                        model: [
                            { dx: -1, dy:  0, cursor: Qt.SizeHorCursor },
                            { dx:  1, dy:  0, cursor: Qt.SizeHorCursor },
                            { dx:  0, dy: -1, cursor: Qt.SizeVerCursor },
                            { dx:  0, dy:  1, cursor: Qt.SizeVerCursor },
                            { dx: -1, dy: -1, cursor: Qt.SizeFDiagCursor },
                            { dx:  1, dy:  1, cursor: Qt.SizeFDiagCursor },
                            { dx:  1, dy: -1, cursor: Qt.SizeBDiagCursor },
                            { dx: -1, dy:  1, cursor: Qt.SizeBDiagCursor }
                        ]

                        delegate: Rectangle {
                            id: grip
                            required property var modelData

                            readonly property real hs: Theme.spacingLg
                            width: hs
                            height: hs
                            radius: Theme.radiusXs
                            color: gripArea.containsMouse || gripArea.pressed
                                   ? Theme.primaryForeground : Theme.primary
                            border.width: Theme.borderWidth
                            border.color: gripArea.containsMouse || gripArea.pressed
                                          ? Theme.primary : Theme.primaryForeground

                            Behavior on color {
                                ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                            }

                            x: cropOverlay.frameX + (modelData.dx === 0 ? cropOverlay.frameW / 2
                                                     : (modelData.dx < 0 ? 0 : cropOverlay.frameW)) - hs / 2
                            y: cropOverlay.frameY + (modelData.dy === 0 ? cropOverlay.frameH / 2
                                                     : (modelData.dy < 0 ? 0 : cropOverlay.frameH)) - hs / 2

                            property real startX: 0
                            property real startY: 0
                            property real startCropX: 0
                            property real startCropY: 0
                            property real startCropW: 0
                            property real startCropH: 0

                            MouseArea {
                                id: gripArea
                                anchors.fill: parent
                                // Generous invisible margin: the visible dot stays
                                // small enough not to hide the frame it sits on.
                                anchors.margins: -Theme.spacingMd
                                hoverEnabled: true
                                cursorShape: grip.modelData.cursor

                                // Zooming onto a vertex means the cursor is over a
                                // grip; without this the grip would eat the wheel
                                // event exactly where zoom is most wanted.
                                onWheel: (wheel) => { wheel.accepted = false }

                                onPressed: (mouse) => {
                                    const p = mapToItem(cropOverlay, mouse.x, mouse.y)
                                    grip.startX = p.x
                                    grip.startY = p.y
                                    grip.startCropX = cropOverlay.cropX
                                    grip.startCropY = cropOverlay.cropY
                                    grip.startCropW = cropOverlay.cropW
                                    grip.startCropH = cropOverlay.cropH
                                }

                                onPositionChanged: (mouse) => {
                                    if (!pressed)
                                        return
                                    const p = mapToItem(cropOverlay, mouse.x, mouse.y)
                                    const ddx = (p.x - grip.startX) / cropOverlay.pxScale
                                    const ddy = (p.y - grip.startY) / cropOverlay.pxScale
                                    cropOverlay.didResize = true

                                    if (grip.modelData.dx < 0) {
                                        // Dragging the left edge moves the origin and
                                        // shrinks the width by the same amount.
                                        const nx = Math.min(grip.startCropX + ddx,
                                                            grip.startCropX + grip.startCropW - 16)
                                        cropOverlay.cropX = nx
                                        cropOverlay.cropW = grip.startCropX + grip.startCropW - nx
                                    } else if (grip.modelData.dx > 0) {
                                        cropOverlay.cropW = Math.max(16, grip.startCropW + ddx)
                                    }

                                    if (grip.modelData.dy < 0) {
                                        const ny = Math.min(grip.startCropY + ddy,
                                                            grip.startCropY + grip.startCropH - 16)
                                        cropOverlay.cropY = ny
                                        cropOverlay.cropH = grip.startCropY + grip.startCropH - ny
                                    } else if (grip.modelData.dy > 0) {
                                        cropOverlay.cropH = Math.max(16, grip.startCropH + ddy)
                                    }
                                }
                            }
                        }
                    }

                    // First-run hint. The crop tool has three non-obvious inputs and
                    // no menu surface to discover them from, so they are spelled out
                    // while the tool is open. Fades once the user has driven all
                    // three, and stays gone for the rest of the session.
                    Rectangle {
                        id: cropHint
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: Theme.spacingLg
                        width: hintRow.width + Theme.spacing2xl
                        height: hintRow.height + Theme.spacingLg
                        radius: Theme.radiusSm
                        // Sits on the preview scrim, not a panel surface, so it uses
                        // the on-media tokens — panelForeground would go dark-on-dark
                        // in light mode.
                        color: Theme.scrimStrong
                        border.width: Theme.borderWidth
                        border.color: Theme.guideWeak

                        opacity: cropOverlay.hintDismissed ? 0 : 1
                        visible: opacity > 0

                        Behavior on opacity {
                            NumberAnimation { duration: Theme.durationSlow; easing.type: Theme.easing }
                        }

                        Row {
                            id: hintRow
                            anchors.centerIn: parent
                            spacing: Theme.spacingLg

                            Text {
                                text: qsTr("Drag the edges to reframe")
                                color: cropOverlay.didResize ? Theme.guideMedium : Theme.onMedia
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }
                            Text {
                                text: "·"
                                color: Theme.guideWeak
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }
                            Text {
                                text: qsTr("Ctrl + scroll to zoom")
                                color: cropOverlay.didZoom ? Theme.guideMedium : Theme.onMedia
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }
                            Text {
                                text: "·"
                                color: Theme.guideWeak
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }
                            Text {
                                text: qsTr("Middle-drag to pan")
                                color: cropOverlay.didPan ? Theme.guideMedium : Theme.onMedia
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }
                        }
                    }

                    // Readout + commit, pinned under the crop frame but kept inside
                    // the viewport so it stays reachable at any crop size.
                    Row {
                        spacing: Theme.spacingMd
                        x: Math.max(0, Math.min(cropOverlay.width - width,
                                                cropOverlay.frameX + (cropOverlay.frameW - width) / 2))
                        y: Math.max(0, Math.min(cropOverlay.height - height,
                                                cropOverlay.frameY + cropOverlay.frameH + Theme.spacingMd))

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: sizeLabel.width + Theme.spacingLg
                            height: sizeLabel.height + Theme.spacingSm
                            radius: Theme.radiusSm
                            // On-media surface, same as the hint: fixed dark scrim
                            // with an on-media foreground rather than panel tokens.
                            color: Theme.scrimStrong
                            border.width: Theme.borderWidth
                            border.color: Theme.guideWeak

                            Text {
                                id: sizeLabel
                                anchors.centerIn: parent
                                text: cropOverlay.outW + "×" + cropOverlay.outH
                                      + "   " + Math.round(viewport.userZoom * 100) + "%"
                                color: Theme.onMedia
                                font.family: Theme.monoFontFamily
                                font.pixelSize: Theme.fontSizeXs
                            }
                        }

                        ThemedButton {
                            variant: "secondary"
                            text: qsTr("Fit view")
                            tooltip: qsTr("Recentre and reset zoom")
                            enabled: viewport.userZoom !== 1.0 || viewport.panX !== 0 || viewport.panY !== 0
                            onClicked: viewport.resetView()
                        }

                        ThemedButton {
                            variant: "secondary"
                            text: qsTr("Reset")
                            tooltip: qsTr("Restore the crop frame to the full canvas")
                            enabled: cropOverlay.changed
                            onClicked: cropOverlay.reset()
                        }

                        ThemedButton {
                            variant: "primary"
                            text: qsTr("Apply")
                            onClicked: cropOverlay.apply()
                        }
                    }
                }
            }
        }

        // Scrub bar. Only in fullscreen: the timeline panel is the seek surface
        // everywhere else, and it is hidden in this mode.
        Item {
            id: scrubBar
            width: parent.width
            visible: root.previewFullscreen
            height: visible ? Theme.controlHeight : 0

            ThemedSlider {
                id: scrubSlider
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Theme.spacing2xl + Theme.spacingSm
                anchors.rightMargin: Theme.spacing2xl + Theme.spacingSm

                from: 0
                // Never collapse to a zero-width range: an empty project would
                // otherwise make the handle jump erratically.
                to: Math.max(0.001, root.durationSeconds)
                valueFormatter: function (v) { return root.formatTimecode(v) }

                onMoved: EditorState.playheadSeconds = value

                // Dragging assigns `value` directly, which would clobber a plain
                // binding to the playhead. Reasserting it only while released lets
                // playback drive the handle without fighting the drag.
                Binding on value {
                    when: !scrubSlider.pressed
                    value: root.currentSeconds
                }
            }
        }

        // The three groups were independently anchored and overlapped at narrow
        // preview widths. A RowLayout keeps Play centred while the side groups
        // compress instead of colliding.
        Item {
            id: toolbar
            width: parent.width
            height: Theme.previewToolbarPaddingTop + Theme.previewToolbarPaddingBottom
                    + Theme.iconButtonSize

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacing2xl + Theme.spacingSm
                anchors.rightMargin: Theme.spacing2xl + Theme.spacingSm
                spacing: Theme.spacingLg

            Row {
                Layout.alignment: Qt.AlignVCenter
                spacing: 0

                HoverHandler { id: timecodeHover }

                ThemedToolTip {
                    text: qsTr("Playhead / total duration — hh:mm:ss:ff at %1 fps").arg(root.projectFps)
                    visible: timecodeHover.hovered
                }

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

            // Spacers on both sides keep Play optically centred while still
            // letting the whole row shrink instead of overlap.
            Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }

            IconButton {
                Layout.alignment: Qt.AlignVCenter
                glyph: root.playing ? Theme.icons.pause : Theme.icons.play
                variant: "text"
                tooltip: root.playing ? qsTr("Pause") : qsTr("Play")
                onClicked: EditorState.togglePlayback()
            }

            Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }

            Row {
                Layout.alignment: Qt.AlignVCenter
                spacing: Theme.spacingLg + Theme.spacingXs

                // Was plain clickable text that read as a label, with no hover,
                // no pressed state and no explanation of the two modes.
                ThemedToggleButton {
                    anchors.verticalCenter: parent.verticalCenter
                    text: viewport.fitMode ? qsTr("Fit") : qsTr("Fill")
                    checked: !viewport.fitMode
                    tooltip: viewport.fitMode
                             ? qsTr("Fit: the whole frame is visible. Click for Fill.")
                             : qsTr("Fill: the frame covers the viewport. Click for Fit.")
                    onClicked: viewport.fitMode = !viewport.fitMode
                }

                Rectangle {
                    width: Theme.borderWidth
                    height: Theme.iconSizeBase
                    color: Theme.panelBorder
                    anchors.verticalCenter: parent.verticalCenter
                }

                IconButton {
                    glyph: Theme.icons.grid
                    variant: "text"
                    tooltip: qsTr("Toggle guides")
                    anchors.verticalCenter: parent.verticalCenter
                    active: EditorState.guidesEnabled
                    onClicked: EditorState.guidesEnabled = !EditorState.guidesEnabled
                }

                IconButton {
                    glyph: root.previewFullscreen ? Theme.icons.minimize : Theme.icons.maximize
                    variant: "text"
                    tooltip: root.previewFullscreen
                             ? qsTr("Exit fullscreen preview (Esc)")
                             : qsTr("Fullscreen preview")
                    anchors.verticalCenter: parent.verticalCenter
                    active: root.previewFullscreen
                    // The window and the surrounding panels belong to Main, so the
                    // toggle is requested rather than performed here.
                    onClicked: root.fullscreenRequested()
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
