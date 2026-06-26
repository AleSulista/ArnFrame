import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import Drift
import "components"

PanelFrame {
    id: root

    property real zoom: 1.0
    property string propertiesTab: ""
    readonly property real minZoom: 0.05
    readonly property real maxZoom: 40.0
    readonly property real pxPerSecond: Theme.pixelsPerSecondBase * zoom

    // Ruler tick interval (seconds): the smallest "nice" step whose labels still
    // have room to breathe at the current zoom, so timestamps never squash.
    readonly property real tickStepSeconds: {
        const minLabelPx = 66
        const needed = minLabelPx / pxPerSecond
        const steps = [0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30,
                       60, 120, 300, 600, 900, 1800, 3600]
        for (var i = 0; i < steps.length; i++)
            if (steps[i] >= needed)
                return steps[i]
        return steps[steps.length - 1]
    }

    // HH:MM:SS, plus .CC hundredths once zoomed into sub-second ticks.
    function formatTick(seconds) {
        const cc = Math.round(Math.max(0, seconds) * 100)
        const pad = (n) => (n < 10 ? "0" : "") + n
        let out = pad(Math.floor(cc / 360000)) + ":"
                + pad(Math.floor((cc % 360000) / 6000)) + ":"
                + pad(Math.floor((cc % 6000) / 100))
        if (tickStepSeconds < 1)
            out += "." + pad(cc % 100)
        return out
    }
    readonly property var tracks: EditorState.tracks
    readonly property real playheadSeconds: EditorState.playheadSeconds
    readonly property int selectedTrack: EditorState.selectedTrack
    readonly property int selectedClip: EditorState.selectedClip

    // Invisible extension above track rows while dragging (only when timeline already has clips).
    readonly property real assetDropTopSlop: EditorState.draggingAssetIndex >= 0 && timelineHasClips()
        ? Theme.newTrackHitSlop : 0

    // Live snap guide (seconds; < 0 when hidden) shown while dragging a clip.
    property real snapGuideSeconds: -1
    // Landing preview outline: where a dragged asset (from the library) or an
    // existing clip being moved would come to rest, snapped.
    property int dropTrackIndex: -1
    property real dropStartSeconds: 0
    property real dropDurationSeconds: 0
    // True while dragging a library asset outside any compatible track row.
    property bool dropCreatesNewTrack: false
    // Clip under an in-progress effect drag (for drop highlight).
    property int effectDropTrackIndex: -1
    property int effectDropClipIndex: -1
    // Track-header reorder: source index and live drop target while dragging.
    property int draggingTrackFrom: -1
    property int draggingTrackTo: -1

    // Shared by library drops and in-timeline clip moves so both snap and show
    // the same outline the same way.
    function showLandingPreview(trackIndex, desiredStart, duration) {
        const snapped = snapClipStart(desiredStart, duration)
        dropTrackIndex = trackIndex
        dropStartSeconds = snapped.start
        dropDurationSeconds = duration
        snapGuideSeconds = snapped.guide
    }

    function clearLandingPreview() {
        dropTrackIndex = -1
        snapGuideSeconds = -1
        dropCreatesNewTrack = false
    }

    function clearEffectDropHighlight() {
        effectDropTrackIndex = -1
        effectDropClipIndex = -1
    }

    function updateEffectDropHighlight(trackIndex, xPixels) {
        const clipIndex = clipIndexAtPosition(trackIndex, xPixels)
        effectDropTrackIndex = clipIndex >= 0 ? trackIndex : -1
        effectDropClipIndex = clipIndex
    }

    // Find the outgoing (earlier) clip index for a transition drop at timeline x.
    function transitionLeftClipAtPosition(trackIndex, xPixels) {
        if (trackIndex < 0 || trackIndex >= tracks.length)
            return -1
        const track = tracks[trackIndex]
        if (track.type !== "video" && track.type !== "shape")
            return -1
        const seconds = xPixels / pxPerSecond
        const clips = track.clips
        let best = -1
        let bestDist = 1e9
        for (let i = 0; i < clips.length; i++) {
            const left = clips[i]
            for (let j = 0; j < clips.length; j++) {
                if (i === j)
                    continue
                const right = clips[j]
                if (right.start < left.start)
                    continue
                const leftEnd = left.start + left.duration
                const gap = right.start - leftEnd
                if (gap > 0.001)
                    continue
                let regionStart
                let regionEnd
                if (right.start < leftEnd) {
                    regionStart = right.start
                    regionEnd = leftEnd
                } else {
                    regionStart = leftEnd - 0.25
                    regionEnd = leftEnd + 0.25
                }
                if (seconds >= regionStart && seconds <= regionEnd) {
                    const mid = (regionStart + regionEnd) / 2
                    const dist = Math.abs(seconds - mid)
                    if (dist < bestDist) {
                        bestDist = dist
                        best = i
                    }
                }
            }
        }
        return best
    }

    function applyTransitionDrop(trackIndex, xPixels, kind) {
        const leftClip = transitionLeftClipAtPosition(trackIndex, xPixels)
        if (leftClip < 0 || !kind || kind.length === 0)
            return
        EditorState.addTransition(trackIndex, leftClip, kind, 0.5)
    }

    function assetDurationSeconds(assetIndex) {
        const asset = AssetLibrary.assetAt(assetIndex)
        if (!asset)
            return 5.0
        if (asset.kind === "image" || !(asset.durationSeconds > 0))
            return 5.0
        return asset.durationSeconds
    }

    // Snap a clip's desired start against timeline targets, testing both edges.
    // Returns {start, guide}; guide < 0 means no snap occurred.
    function snapClipStart(desiredStart, duration) {
        const l = EditorState.snapTime(desiredStart)
        const rEdge = EditorState.snapTime(desiredStart + duration)
        const lSnapped = Math.abs(l - desiredStart) > 0.0005
        const rSnapped = Math.abs(rEdge - (desiredStart + duration)) > 0.0005
        if (lSnapped && (!rSnapped || Math.abs(l - desiredStart) <= Math.abs(rEdge - duration - desiredStart)))
            return { "start": l, "guide": l }
        if (rSnapped)
            return { "start": rEdge - duration, "guide": rEdge }
        return { "start": desiredStart, "guide": -1 }
    }

    function trackHeight(type) {
        if (type === "video") return Theme.trackHeightVideo;
        if (type === "audio") return Theme.trackHeightAudio;
        if (type === "shape") return Theme.trackHeightShape;
        if (type === "subtitle") return Theme.trackHeightSubtitle;
        return Theme.trackHeightText;
    }

    function trackTypeIcon(type) {
        if (type === "audio") return Theme.icons.music;
        if (type === "text") return Theme.icons.type;
        if (type === "subtitle") return Theme.icons.messageSquare;
        if (type === "shape") return Theme.icons.shapes;
        return Theme.icons.film;
    }

    function clipColor(type) {
        if (type === "text") return Theme.clipText;
        if (type === "subtitle") return Theme.clipSubtitle;
        if (type === "audio") return Theme.clipAudio;
        if (type === "graphic") return Theme.clipGraphic;
        if (type === "effect") return Theme.clipEffect;
        return Theme.clipVideoPlaceholder; // video: no flat fill, thumbnails would go here
    }

    function totalTracksHeight() {
        var h = 0;
        for (var i = 0; i < tracks.length; i++) {
            h += trackHeight(tracks[i].type);
            if (i > 0) h += Theme.trackGap;
        }
        return h;
    }

    // Finds the clip (if any) under a given x position (px) on a track, for
    // dropping an effect card directly onto a clip.
    function clipIndexAtPosition(trackIndex, xPixels) {
        if (trackIndex < 0 || trackIndex >= tracks.length)
            return -1
        const seconds = xPixels / pxPerSecond
        const clips = tracks[trackIndex].clips
        for (var i = 0; i < clips.length; i++) {
            if (seconds >= clips[i].start && seconds < clips[i].start + clips[i].duration)
                return i
        }
        return -1
    }

    function timelineHasClips() {
        for (var i = 0; i < tracks.length; i++) {
            if (tracks[i].clips.length > 0)
                return true
        }
        return false
    }

    function firstCompatibleTrackIndex(assetIndex) {
        for (var i = 0; i < tracks.length; i++) {
            if (EditorState.trackAcceptsAsset(i, assetIndex))
                return i
        }
        return -1
    }

    function trackIndexAtY(y) {
        var cursor = 0;
        for (var i = 0; i < tracks.length; i++) {
            const th = trackHeight(tracks[i].type);
            if (y >= cursor && y < cursor + th)
                return i;
            cursor += th + Theme.trackGap;
        }
        return -1;
    }

    // Target index for QList::move while dragging a track header.
    function trackMoveTargetAtY(y) {
        if (tracks.length === 0)
            return -1
        var cursor = 0
        for (var i = 0; i < tracks.length; i++) {
            const th = trackHeight(tracks[i].type)
            if (y < cursor + th / 2)
                return i
            cursor += th + Theme.trackGap
        }
        return tracks.length - 1
    }

    function trackRowTop(index) {
        var cursor = 0
        for (var i = 0; i < index && i < tracks.length; i++)
            cursor += trackHeight(tracks[i].type) + Theme.trackGap
        return cursor
    }

    function clearTrackDrag() {
        draggingTrackFrom = -1
        draggingTrackTo = -1
    }

    function updateAssetDropPreview(assetIndex, dropX, dropY) {
        const duration = assetDurationSeconds(assetIndex)
        const desired = Math.max(0, dropX / pxPerSecond)

        if (!timelineHasClips()) {
            const trackIdx = firstCompatibleTrackIndex(assetIndex)
            if (trackIdx >= 0) {
                dropCreatesNewTrack = false
                showLandingPreview(trackIdx, desired, duration)
            } else {
                clearLandingPreview()
            }
            return
        }

        if (assetDropTopSlop > 0 && dropY < assetDropTopSlop) {
            const snapped = snapClipStart(desired, duration)
            dropCreatesNewTrack = true
            dropTrackIndex = -1
            dropStartSeconds = snapped.start
            dropDurationSeconds = duration
            snapGuideSeconds = snapped.guide
            return
        }

        const trackIdx = trackIndexAtY(dropY - assetDropTopSlop)
        if (trackIdx >= 0 && EditorState.trackAcceptsAsset(trackIdx, assetIndex)) {
            dropCreatesNewTrack = false
            showLandingPreview(trackIdx, desired, duration)
        } else {
            const snapped = snapClipStart(desired, duration)
            dropCreatesNewTrack = true
            dropTrackIndex = -1
            dropStartSeconds = snapped.start
            dropDurationSeconds = duration
            snapGuideSeconds = snapped.guide
        }
    }

    function performAssetDrop(assetIndex, dropX, dropY) {
        const atSeconds = Math.max(0, dropX / pxPerSecond)
        if (assetIndex < 0)
            return

        function runAdd() {
            if (!timelineHasClips()) {
                const trackIdx = firstCompatibleTrackIndex(assetIndex)
                if (trackIdx >= 0)
                    EditorState.addClipFromAssetAt(assetIndex, trackIdx, atSeconds)
                return
            }

            if (assetDropTopSlop > 0 && dropY < assetDropTopSlop) {
                EditorState.addClipFromAssetOnNewTrack(assetIndex, atSeconds)
                return
            }

            const trackIdx = trackIndexAtY(dropY - assetDropTopSlop)
            if (trackIdx >= 0 && EditorState.trackAcceptsAsset(trackIdx, assetIndex))
                EditorState.addClipFromAssetAt(assetIndex, trackIdx, atSeconds)
            else
                EditorState.addClipFromAssetOnNewTrack(assetIndex, atSeconds)
        }

        if (typeof Window !== "undefined" && Window.window && Window.window.configureAndAddAsset)
            Window.window.configureAndAddAsset(assetIndex, runAdd)
        else
            runAdd()
    }

    function handleTimelineWheel(wheel) {
        const maxX = Math.max(0, flick.contentWidth - flick.width)
        if (wheel.modifiers & Qt.ControlModifier) {
            const t = wheel.x / pxPerSecond
            const viewportX = wheel.x - flick.contentX
            const factor = wheel.angleDelta.y > 0 ? 1.15 : 1.0 / 1.15
            zoom = Math.max(minZoom, Math.min(maxZoom, zoom * factor))
            const newMaxX = Math.max(0, flick.contentWidth - flick.width)
            flick.contentX = Math.max(0, Math.min(newMaxX, t * pxPerSecond - viewportX))
        } else {
            const delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
            flick.contentX = Math.max(0, Math.min(maxX, flick.contentX - delta))
        }
    }

    function formatTime(seconds) {
        const total = Math.max(0, Math.round(seconds));
        const m = Math.floor(total / 60);
        const s = total % 60;
        return m.toString().padStart(2, "0") + ":" + s.toString().padStart(2, "0");
    }

    function ensurePlayheadVisible() {
        const playheadX = EditorState.playheadSeconds * pxPerSecond;
        const margin = 64;
        if (playheadX < flick.contentX + margin)
            flick.contentX = Math.max(0, playheadX - margin);
        else if (playheadX > flick.contentX + flick.width - margin)
            flick.contentX = Math.min(Math.max(0, flick.contentWidth - flick.width),
                                      playheadX - flick.width + margin);
    }

    Column {
        anchors.fill: parent

        // === toolbar =================================================================
        Item {
            id: toolbar
            width: parent.width
            height: Theme.timelineToolbarHeight

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.panelBorder
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                IconButton {
                    icon: EditorState.playing ? Theme.icons.pause : Theme.icons.play
                    variant: "text"
                    tooltip: EditorState.playing ? qsTr("Pause") : qsTr("Play")
                    onClicked: EditorState.togglePlayback()
                }

                Text {
                    text: root.formatTime(EditorState.playheadSeconds) + " / " + root.formatTime(EditorState.durationSeconds)
                    color: Theme.mutedForeground
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.fontSizeXs
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    width: 1
                    height: 24
                    color: Theme.panelBorder
                    anchors.verticalCenter: parent.verticalCenter
                }

                IconButton { icon: Theme.icons.scissors; variant: "text"; tooltip: qsTr("Split at playhead"); onClicked: EditorState.splitAtPlayhead() }
                IconButton { icon: Theme.icons.chevronsLeft; variant: "text"; tooltip: qsTr("Split left"); onClicked: EditorState.splitSelectedClipLeft() }
                IconButton { icon: Theme.icons.chevronsRight; variant: "text"; tooltip: qsTr("Split right"); onClicked: EditorState.splitSelectedClipRight() }
                IconButton {
                    icon: Theme.icons.unlink
                    variant: "text"
                    tooltip: qsTr("Unlink audio")
                    buttonEnabled: EditorState.unlinkAvailable
                    onClicked: EditorState.unlinkSelectedClips()
                }
                IconButton {
                    icon: Theme.icons.linkTwo
                    variant: "text"
                    tooltip: qsTr("Merge adjacent clips")
                    buttonEnabled: EditorState.mergeAvailable
                    onClicked: EditorState.mergeSelectedClips()
                }
                IconButton { icon: Theme.icons.copy; variant: "text"; tooltip: qsTr("Copy selection"); onClicked: EditorState.copySelection() }
                IconButton { icon: Theme.icons.clipboardPaste; variant: "text"; tooltip: qsTr("Paste at playhead"); onClicked: EditorState.pasteAtPlayhead() }
                IconButton { icon: Theme.icons.copyPlus; variant: "text"; tooltip: qsTr("Duplicate clip"); onClicked: EditorState.duplicateSelectedClip() }
                IconButton { icon: Theme.icons.snowflake; variant: "text"; tooltip: qsTr("Freeze frame at playhead"); onClicked: EditorState.freezeFrameAtPlayhead() }
                IconButton { icon: Theme.icons.trash; variant: "text"; tooltip: qsTr("Delete clip"); onClicked: EditorState.deleteSelectedClip() }

                Rectangle {
                    width: 1
                    height: 24
                    color: Theme.panelBorder
                    anchors.verticalCenter: parent.verticalCenter
                }

                IconButton {
                    icon: Theme.icons.bookmark
                    variant: "text"
                    tooltip: qsTr("Add bookmark at playhead")
                    onClicked: EditorState.addBookmark(EditorState.playheadSeconds, "Mark " + Math.round(EditorState.playheadSeconds))
                }
                IconButton {
                    icon: Theme.icons.undo
                    variant: "text"
                    tooltip: qsTr("Undo")
                    onClicked: EditorState.undo()
                    buttonEnabled: EditorState.undoAvailable
                }
                IconButton {
                    icon: Theme.icons.redo
                    variant: "text"
                    tooltip: qsTr("Redo")
                    onClicked: EditorState.redo()
                    buttonEnabled: EditorState.redoAvailable
                }

                Rectangle {
                    width: 1
                    height: 24
                    color: Theme.panelBorder
                    anchors.verticalCenter: parent.verticalCenter
                }

                ThemedButton {
                    id: newTrackButton
                    text: qsTr("New Track")
                    variant: "ghost"
                    glyph: Theme.icons.plus
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: newTrackMenu.open()

                    NewTrackMenu {
                        id: newTrackMenu
                        x: 0
                        y: newTrackButton.height + 4
                    }
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: sceneRow.implicitWidth + 20
                height: 26
                radius: Theme.radiusSm
                color: "transparent"
                border.width: 1
                border.color: Qt.rgba(Theme.panelForeground.r, Theme.panelForeground.g, Theme.panelForeground.b, 0.1)

                Row {
                    id: sceneRow
                    anchors.centerIn: parent
                    spacing: 6

                    Text {
                        text: "Scene 1"
                        color: Theme.panelForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    IconGlyph {
                        glyph: Theme.icons.layers
                        iconSize: 14
                        iconColor: Theme.mutedForeground
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                IconButton {
                    id: magnetButton
                    icon: Theme.icons.magnet
                    variant: "text"
                    tooltip: qsTr("Toggle snapping")
                    active: EditorState.snapEnabled
                    onClicked: EditorState.snapEnabled = !EditorState.snapEnabled
                }
                IconButton {
                    id: rippleButton
                    icon: Theme.icons.foldHorizontal
                    variant: "text"
                    tooltip: qsTr("Toggle ripple editing")
                    active: EditorState.rippleEnabled
                    onClicked: EditorState.rippleEnabled = !EditorState.rippleEnabled
                }

                Rectangle {
                    width: 1
                    height: 24
                    color: Theme.panelBorder
                    anchors.verticalCenter: parent.verticalCenter
                }

                IconButton {
                    icon: Theme.icons.zoomOut
                    variant: "text"
                    tooltip: qsTr("Zoom out")
                    onClicked: root.zoom = Math.max(root.minZoom, root.zoom / 1.5)
                }
                ThemedSlider {
                    id: zoomSlider
                    width: 112
                    anchors.verticalCenter: parent.verticalCenter
                    // Logarithmic mapping so the wide zoom range stays controllable.
                    from: 0
                    to: 1
                    value: Math.log(root.zoom / root.minZoom) / Math.log(root.maxZoom / root.minZoom)
                    onMoved: root.zoom = root.minZoom * Math.pow(root.maxZoom / root.minZoom, value)
                }
                IconButton {
                    icon: Theme.icons.zoomIn
                    variant: "text"
                    tooltip: qsTr("Zoom in")
                    onClicked: root.zoom = Math.min(root.maxZoom, root.zoom * 1.5)
                }
            }
        }

        // === ruler + track labels + tracks ================================================
        Column {
            width: parent.width
            height: parent.height - toolbar.height

            KeyframeGraph {
                id: keyframesBar
                width: parent.width
                pxPerSecond: root.pxPerSecond
                labelsWidth: Theme.trackLabelsWidth
                propertiesTab: root.propertiesTab
                // Keep keys/playhead lined up with the track scroll view below.
                contentX: flick.contentX
                contentWidth: flick.contentWidth
            }

            SubtitleCueLane {
                id: subtitleLane
                width: parent.width
                pxPerSecond: root.pxPerSecond
                labelsWidth: Theme.trackLabelsWidth
                contentX: flick.contentX
                contentWidth: flick.contentWidth
            }

            Row {
                width: parent.width
                height: parent.height - keyframesBar.height - subtitleLane.height

            // --- fixed left label column --------------------------------------------
            Column {
                width: Theme.trackLabelsWidth
                height: parent.height

                Item { width: parent.width; height: Theme.timelineRulerHeight + Theme.timelineBookmarkRowHeight }

                Item {
                    id: trackLabelsArea
                    width: parent.width
                    height: parent.height - Theme.timelineRulerHeight - Theme.timelineBookmarkRowHeight

                    Repeater {
                        model: root.tracks.length
                        delegate: Item {
                            id: trackLabelRow
                            width: Theme.trackLabelsWidth
                            height: root.trackHeight(root.tracks[index].type)
                                    + (index < root.tracks.length - 1 ? Theme.trackGap : 0)
                            y: root.trackRowTop(index)
                            opacity: root.draggingTrackFrom === index ? 0.45 : 1.0

                            Rectangle {
                                anchors.right: parent.right
                                width: 1
                                height: root.trackHeight(root.tracks[index].type)
                                color: Theme.panelBorder
                            }

                            // Drag handle — left-aligned reorder grip.
                            IconGlyph {
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.verticalCenterOffset: index < root.tracks.length - 1 ? -Theme.trackGap / 2 : 0
                                glyph: Theme.icons.gripVertical
                                iconSize: 14
                                iconColor: trackDragMouse.containsMouse || root.draggingTrackFrom === index
                                           ? Theme.panelForeground : Theme.mutedForeground

                                ToolTip {
                                    visible: trackDragMouse.containsMouse && root.draggingTrackFrom < 0
                                    text: qsTr("Drag to reorder track")
                                }

                                MouseArea {
                                    id: trackDragMouse
                                    anchors.fill: parent
                                    anchors.margins: -4
                                    hoverEnabled: true
                                    cursorShape: Qt.SizeAllCursor
                                    preventStealing: true

                                    onPressed: {
                                        root.draggingTrackFrom = index
                                        root.draggingTrackTo = index
                                    }
                                    onPositionChanged: (mouse) => {
                                        if (root.draggingTrackFrom < 0)
                                            return
                                        const local = mapToItem(trackLabelsArea, mouse.x, mouse.y)
                                        root.draggingTrackTo = root.trackMoveTargetAtY(local.y)
                                    }
                                    onReleased: {
                                        if (root.draggingTrackFrom >= 0
                                                && root.draggingTrackTo >= 0
                                                && root.draggingTrackFrom !== root.draggingTrackTo)
                                            EditorState.moveTrack(root.draggingTrackFrom,
                                                                  root.draggingTrackTo)
                                        root.clearTrackDrag()
                                    }
                                    onCanceled: root.clearTrackDrag()
                                }
                            }

                            Row {
                                anchors.right: parent.right
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.verticalCenterOffset: index < root.tracks.length - 1 ? -Theme.trackGap / 2 : 0
                                spacing: 8

                                IconGlyph {
                                    visible: root.tracks[index].type === "video"
                                             || root.tracks[index].type === "audio"
                                    glyph: EditorState.trackMuted(index) ? Theme.icons.volumeOff : Theme.icons.volumeHigh
                                    iconSize: 16
                                    iconColor: EditorState.trackMuted(index) ? Theme.destructive : Theme.mutedForeground
                                    anchors.verticalCenter: parent.verticalCenter

                                    ToolTip {
                                        visible: muteMouse.containsMouse
                                        text: EditorState.trackMuted(index) ? qsTr("Unmute track") : qsTr("Mute track")
                                    }

                                    MouseArea {
                                        id: muteMouse
                                        anchors.fill: parent
                                        anchors.margins: -4
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: EditorState.setTrackMuted(index, !EditorState.trackMuted(index))
                                    }
                                }

                                IconGlyph {
                                    visible: root.tracks[index].type === "video"
                                             || root.tracks[index].type === "text"
                                             || root.tracks[index].type === "subtitle"
                                             || root.tracks[index].type === "shape"
                                    glyph: EditorState.trackHidden(index) ? Theme.icons.eyeOff : Theme.icons.eye
                                    iconSize: 16
                                    iconColor: EditorState.trackHidden(index) ? Theme.destructive : Theme.mutedForeground
                                    anchors.verticalCenter: parent.verticalCenter

                                    ToolTip {
                                        visible: hideMouse.containsMouse
                                        text: EditorState.trackHidden(index) ? qsTr("Show track") : qsTr("Hide track")
                                    }

                                    MouseArea {
                                        id: hideMouse
                                        anchors.fill: parent
                                        anchors.margins: -4
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: EditorState.setTrackHidden(index, !EditorState.trackHidden(index))
                                    }
                                }

                                // Toggle the whole track between filmstrip previews and audio waveforms.
                                IconGlyph {
                                    visible: root.tracks[index].type === "video"
                                    glyph: EditorState.trackShowWaveform(index) ? Theme.icons.music : Theme.icons.film
                                    iconSize: 16
                                    iconColor: EditorState.trackShowWaveform(index) ? Theme.primary : Theme.mutedForeground
                                    anchors.verticalCenter: parent.verticalCenter

                                    ToolTip {
                                        visible: waveMouse.containsMouse
                                        text: EditorState.trackShowWaveform(index) ? qsTr("Show filmstrip")
                                                                                   : qsTr("Show waveform")
                                    }

                                    MouseArea {
                                        id: waveMouse
                                        anchors.fill: parent
                                        anchors.margins: -4
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: EditorState.setTrackShowWaveform(index, !EditorState.trackShowWaveform(index))
                                    }
                                }

                                IconGlyph {
                                    glyph: root.trackTypeIcon(root.tracks[index].type)
                                    iconSize: 16
                                    iconColor: Theme.mutedForeground
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }
                    }

                    // Insertion line while reordering tracks.
                    Rectangle {
                        visible: root.draggingTrackFrom >= 0 && root.draggingTrackTo >= 0
                                 && root.draggingTrackFrom !== root.draggingTrackTo
                        width: parent.width - 8
                        height: 2
                        radius: 1
                        x: 4
                        color: Theme.primary
                        z: 10
                        y: {
                            if (root.draggingTrackTo < 0)
                                return 0
                            const from = root.draggingTrackFrom
                            const to = root.draggingTrackTo
                            if (from < to)
                                return root.trackRowTop(to) + root.trackHeight(root.tracks[to].type) - 1
                            return root.trackRowTop(to)
                        }
                    }
                }
            }

            // --- scrollable ruler + tracks --------------------------------------------
            Flickable {
                id: flick
                width: parent.width - Theme.trackLabelsWidth
                height: parent.height
                contentWidth: Math.max(width, (EditorState.durationSeconds + 5) * root.pxPerSecond)
                contentHeight: height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.horizontal: AppScrollBar { policy: ScrollBar.AlwaysOn }

                Item {
                    id: timelineContent
                    width: flick.contentWidth
                    height: flick.height

                    // Wheel handling: scoped to the ruler so it does not block timeline drops.
                    MouseArea {
                        id: rulerWheelArea
                        width: parent.width
                        height: Theme.timelineRulerHeight + Theme.timelineBookmarkRowHeight
                        z: 1
                        acceptedButtons: Qt.NoButton
                        onWheel: (wheel) => root.handleTimelineWheel(wheel)
                    }

                    // Library asset drops
                    DropArea {
                        id: timelineAssetDrop
                        enabled: EditorState.draggingAssetIndex >= 0
                        opacity: 0
                        x: 0
                        y: Theme.timelineRulerHeight + Theme.timelineBookmarkRowHeight - root.assetDropTopSlop
                        width: parent.width
                        height: Math.max(root.totalTracksHeight() + root.assetDropTopSlop,
                                         Theme.trackHeightVideo)
                        z: 250
                        keys: ["text/plain"]

                        function assetIndexFromDrop(drop) {
                            if (EditorState.draggingAssetIndex >= 0)
                                return EditorState.draggingAssetIndex
                            const text = drop.hasText ? drop.text : drop.getDataAsString("text/plain")
                            const idx = parseInt(text)
                            return isNaN(idx) ? -1 : idx
                        }

                        function updateAssetDrag(drop) {
                            const assetIndex = assetIndexFromDrop(drop)
                            if (assetIndex < 0) {
                                root.clearLandingPreview()
                                return
                            }
                            root.updateAssetDropPreview(assetIndex, drop.x, drop.y)
                        }

                        onEntered: (drop) => updateAssetDrag(drop)
                        onPositionChanged: (drop) => updateAssetDrag(drop)
                        onExited: root.clearLandingPreview()
                        onDropped: (drop) => {
                            drop.accept(Qt.CopyAction)
                            const assetIndex = assetIndexFromDrop(drop)
                            root.clearLandingPreview()
                            root.performAssetDrop(assetIndex, drop.x, drop.y)
                        }
                    }

                    // Horizontal scroll / zoom wheel over track rows (below the drop overlay).
                    MouseArea {
                        x: 0
                        y: Theme.timelineRulerHeight + Theme.timelineBookmarkRowHeight - root.assetDropTopSlop
                        width: parent.width
                        height: Math.max(root.totalTracksHeight() + root.assetDropTopSlop,
                                         Theme.trackHeightVideo)
                        z: 150
                        acceptedButtons: Qt.NoButton
                        onWheel: (wheel) => root.handleTimelineWheel(wheel)
                    }

                    // ruler ------------------------------------------------------------
                    Item {
                        id: ruler
                        width: parent.width
                        height: Theme.timelineRulerHeight

                        MouseArea {
                            anchors.fill: parent
                            onClicked: (mouse) => {
                                EditorState.clearSelection()
                                EditorState.playheadSeconds = EditorState.snapTime(mouse.x / root.pxPerSecond)
                            }
                        }

                        Repeater {
                            model: Math.ceil(flick.contentWidth / (root.tickStepSeconds * root.pxPerSecond)) + 1
                            delegate: Item {
                                readonly property real tickSeconds: index * root.tickStepSeconds
                                x: tickSeconds * root.pxPerSecond
                                width: root.tickStepSeconds * root.pxPerSecond
                                height: ruler.height

                                Rectangle {
                                    x: 0
                                    y: 6
                                    width: 1
                                    height: 6
                                    color: Qt.rgba(Theme.mutedForeground.r, Theme.mutedForeground.g, Theme.mutedForeground.b, 0.25)
                                }

                                Text {
                                    x: 4
                                    y: 4
                                    text: root.formatTick(parent.tickSeconds)
                                    color: Theme.mutedForeground
                                    font.pixelSize: Theme.fontSizeTick
                                    font.family: Theme.fontFamily
                                }
                            }
                        }
                    }

                    Item {
                        id: bookmarkRow
                        y: Theme.timelineRulerHeight
                        width: parent.width
                        height: Theme.timelineBookmarkRowHeight

                        Repeater {
                            model: EditorState.bookmarks
                            delegate: Rectangle {
                                x: modelData.seconds * root.pxPerSecond - width / 2
                                anchors.verticalCenter: parent.verticalCenter
                                width: 8
                                height: 8
                                radius: 4
                                color: Theme.primary

                                ToolTip {
                                    visible: bookmarkMouse.containsMouse
                                    text: modelData.label + " @ " + root.formatTime(modelData.seconds)
                                }

                                MouseArea {
                                    id: bookmarkMouse
                                    anchors.fill: parent
                                    anchors.margins: -6
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: EditorState.goToBookmark(index)
                                }
                            }
                        }
                    }

                    // track rows ---------------------------------------------------------
                    Column {
                        id: trackColumn
                        y: Theme.timelineRulerHeight + Theme.timelineBookmarkRowHeight
                        width: parent.width
                        spacing: Theme.trackGap

                        // Landing preview when creating a new track above existing rows.
                        Rectangle {
                            visible: root.dropCreatesNewTrack
                            x: root.dropStartSeconds * root.pxPerSecond
                            width: root.dropDurationSeconds * root.pxPerSecond
                            height: root.trackHeight(EditorState.trackTypeForAsset(EditorState.draggingAssetIndex))
                            radius: Theme.radiusSm
                            color: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.12)
                            border.width: 2
                            border.color: Theme.primary
                            z: 5
                        }

                        Repeater {
                            model: root.tracks.length
                            delegate: Rectangle {
                                id: trackRow
                                property int trackIndex: index
                                width: flick.contentWidth
                                height: root.trackHeight(root.tracks[trackIndex].type)
                                color: "transparent"

                                DropArea {
                                    anchors.fill: parent
                                    keys: ["text/plain", "application/x-drift-effect", "application/x-drift-shape", "application/x-drift-transition"]

                                    function isEffectDrag(drop) {
                                        return drop.keys.indexOf("application/x-drift-effect") !== -1
                                    }

                                    function isShapeDrag(drop) {
                                        return drop.keys.indexOf("application/x-drift-shape") !== -1
                                    }

                                    function isTransitionDrag(drop) {
                                        return drop.keys.indexOf("application/x-drift-transition") !== -1
                                    }

                                    function assetIndexFromDrop(drop) {
                                        if (EditorState.draggingAssetIndex >= 0)
                                            return EditorState.draggingAssetIndex
                                        const text = drop.hasText ? drop.text : drop.getDataAsString("text/plain")
                                        const idx = parseInt(text)
                                        return isNaN(idx) ? -1 : idx
                                    }

                                    function updateAssetPreview(drop) {
                                        if (isTransitionDrag(drop)) {
                                            root.clearLandingPreview()
                                            root.clearEffectDropHighlight()
                                            return
                                        }
                                        if (isEffectDrag(drop)) {
                                            root.updateEffectDropHighlight(trackRow.trackIndex, drop.x)
                                            return
                                        }
                                        root.clearEffectDropHighlight()
                                        if (isShapeDrag(drop)) {
                                            if (root.tracks[trackRow.trackIndex].type === "shape") {
                                                const desired = Math.max(0, drop.x / root.pxPerSecond)
                                                root.showLandingPreview(trackRow.trackIndex, desired, 5.0)
                                            } else {
                                                root.clearLandingPreview()
                                            }
                                            return
                                        }
                                        const assetIndex = assetIndexFromDrop(drop)
                                        if (assetIndex < 0)
                                            return
                                        const columnPos = trackRow.mapFromItem(timelineAssetDrop, drop.x, drop.y)
                                        if (!EditorState.trackAcceptsAsset(trackRow.trackIndex, assetIndex))
                                            return
                                        if (!root.timelineHasClips())
                                            return
                                        if (root.assetDropTopSlop > 0 && columnPos.y < root.assetDropTopSlop)
                                            return
                                        if (root.trackIndexAtY(columnPos.y - root.assetDropTopSlop) !== trackRow.trackIndex)
                                            return
                                        const duration = root.assetDurationSeconds(assetIndex)
                                        const desired = Math.max(0, drop.x / root.pxPerSecond)
                                        root.showLandingPreview(trackRow.trackIndex, desired, duration)
                                    }

                                    onEntered: (drop) => updateAssetPreview(drop)
                                    onPositionChanged: (drop) => updateAssetPreview(drop)
                                    onExited: {
                                        root.clearLandingPreview()
                                        root.clearEffectDropHighlight()
                                    }
                                    onDropped: (drop) => {
                                        drop.accept(Qt.CopyAction)
                                        if (isTransitionDrag(drop)) {
                                            const kind = drop.getDataAsString("application/x-drift-transition")
                                            root.applyTransitionDrop(trackRow.trackIndex, drop.x, kind)
                                            return
                                        }
                                        if (isEffectDrag(drop)) {
                                            const effectId = drop.getDataAsString("application/x-drift-effect")
                                            const clipIndex = root.clipIndexAtPosition(trackRow.trackIndex, drop.x)
                                            root.clearEffectDropHighlight()
                                            if (clipIndex >= 0 && effectId.length > 0) {
                                                EditorState.addEffect(trackRow.trackIndex, clipIndex, effectId)
                                                EditorState.selectClip(trackRow.trackIndex, clipIndex)
                                            }
                                            return
                                        }
                                        if (isShapeDrag(drop)) {
                                            const shapeId = drop.getDataAsString("application/x-drift-shape")
                                            const atSeconds = Math.max(0, drop.x / root.pxPerSecond)
                                            root.clearLandingPreview()
                                            EditorState.addShapeClipAt(shapeId, trackRow.trackIndex, atSeconds)
                                            return
                                        }
                                        const assetIndex = assetIndexFromDrop(drop)
                                        if (assetIndex < 0)
                                            return
                                        const columnPos = trackRow.mapFromItem(timelineAssetDrop, drop.x, drop.y)
                                        root.clearLandingPreview()
                                        root.performAssetDrop(assetIndex, drop.x, columnPos.y)
                                    }
                                }

                                Rectangle {
                                    visible: root.dropCreatesNewTrack && trackRow.trackIndex === 0
                                    anchors.top: parent.top
                                    width: parent.width
                                    height: 3
                                    color: Theme.primary
                                    z: 6
                                }

                                // Landing preview outline (library drop or clip move).
                                Rectangle {
                                    visible: root.dropTrackIndex === trackRow.trackIndex
                                    x: root.dropStartSeconds * root.pxPerSecond
                                    width: root.dropDurationSeconds * root.pxPerSecond
                                    height: parent.height
                                    radius: Theme.radiusSm
                                    color: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.12)
                                    border.width: 2
                                    border.color: Theme.primary
                                    z: 5
                                }

                                Repeater {
                                    model: root.tracks[trackRow.trackIndex].clips.length
                                    delegate: Item {
                                        id: clipItem
                                        property var clipData: root.tracks[trackRow.trackIndex].clips[modelData]
                                        property bool selected: (EditorState.selection,
                                                                 EditorState.selectionContains(trackRow.trackIndex, modelData))
                                        property string trackType: root.tracks[trackRow.trackIndex].type
                                        property bool showWaveform: root.tracks[trackRow.trackIndex].showWaveform === true
                                        property var clipEffects: clipData.effects || []
                                        property bool effectDropTarget: root.effectDropTrackIndex === trackRow.trackIndex
                                                                        && root.effectDropClipIndex === modelData
                                        readonly property bool timelineFadeHandles: trackType !== "text"
                                                                                    && trackType !== "subtitle"

                                        y: Theme.clipSelectionRingWidth
                                        width: clipData.duration * root.pxPerSecond - 2 * Theme.clipSelectionRingWidth
                                        height: parent.height - 2 * Theme.clipSelectionRingWidth

                                        // While dragging, show the same snapped landing outline the
                                        // library drop uses, on whichever track the clip is over.
                                        function updateMovePreview() {
                                            if (!clipMouse.drag.active)
                                                return
                                            const desired = Math.max(0, (x - Theme.clipSelectionRingWidth) / root.pxPerSecond)
                                            const pos = mapToItem(trackColumn, width / 2, height / 2)
                                            const targetTrack = root.trackIndexAtY(pos.y)
                                            root.showLandingPreview(targetTrack >= 0 ? targetTrack : trackRow.trackIndex,
                                                                    desired, clipData.duration)
                                        }
                                        onXChanged: updateMovePreview()
                                        onYChanged: updateMovePreview()

                                        Binding {
                                            target: clipItem
                                            property: "x"
                                            when: !clipMouse.drag.active
                                            value: clipData.start * root.pxPerSecond + Theme.clipSelectionRingWidth
                                        }

                                        Binding {
                                            target: clipItem
                                            property: "y"
                                            when: !clipMouse.drag.active
                                            value: Theme.clipSelectionRingWidth
                                        }

                                        Rectangle {
                                            id: clipBackground
                                            anchors.fill: parent
                                            radius: Theme.radiusSm
                                            color: root.clipColor(clipItem.trackType === "shape" ? "graphic" : clipItem.trackType)
                                            border.width: clipItem.effectDropTarget
                                                          ? 2
                                                          : (clipItem.selected ? Theme.clipSelectionRingWidth : 0)
                                            border.color: clipItem.effectDropTarget ? Theme.clipEffect : Theme.primary
                                            clip: true

                                            Rectangle {
                                                anchors.fill: parent
                                                visible: clipItem.effectDropTarget
                                                color: Qt.rgba(Theme.clipEffect.r, Theme.clipEffect.g, Theme.clipEffect.b, 0.28)
                                                z: 4
                                            }

                                            // Fade ramp overlay (always visible so fades read at a glance).
                                            Canvas {
                                                id: fadeCanvas
                                                anchors.fill: parent
                                                z: 2
                                                property real fadeInPx: Math.min(width, (clipItem.clipData.fadeIn || 0) * root.pxPerSecond)
                                                property real fadeOutPx: Math.min(width, (clipItem.clipData.fadeOut || 0) * root.pxPerSecond)
                                                onFadeInPxChanged: requestPaint()
                                                onFadeOutPxChanged: requestPaint()
                                                onWidthChanged: requestPaint()
                                                onHeightChanged: requestPaint()
                                                onPaint: {
                                                    var ctx = getContext("2d")
                                                    ctx.reset()
                                                    ctx.fillStyle = "rgba(0,0,0,0.38)"
                                                    ctx.strokeStyle = "rgba(255,255,255,0.9)"
                                                    ctx.lineWidth = 1.5
                                                    if (fadeInPx > 0.5) {
                                                        ctx.beginPath()
                                                        ctx.moveTo(0, 0)
                                                        ctx.lineTo(fadeInPx, 0)
                                                        ctx.lineTo(0, height)
                                                        ctx.closePath()
                                                        ctx.fill()
                                                        ctx.beginPath()
                                                        ctx.moveTo(0, height)
                                                        ctx.lineTo(fadeInPx, 0)
                                                        ctx.stroke()
                                                    }
                                                    if (fadeOutPx > 0.5) {
                                                        ctx.beginPath()
                                                        ctx.moveTo(width, 0)
                                                        ctx.lineTo(width - fadeOutPx, 0)
                                                        ctx.lineTo(width, height)
                                                        ctx.closePath()
                                                        ctx.fill()
                                                        ctx.beginPath()
                                                        ctx.moveTo(width, height)
                                                        ctx.lineTo(width - fadeOutPx, 0)
                                                        ctx.stroke()
                                                    }
                                                }
                                            }

                                            ClipFilmstrip {
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.top: parent.top
                                                anchors.bottom: parent.bottom
                                                anchors.topMargin: (clipItem.trackType === "video"
                                                                    || clipItem.trackType === "audio"
                                                                    || clipItem.trackType === "shape")
                                                                   ? (clipItem.clipEffects.length > 0 ? 32 : 20)
                                                                   : 0
                                                visible: clipItem.clipData.filmstripPath
                                                         && clipItem.clipData.filmstripPath.length > 0
                                                         && !clipItem.showWaveform
                                                         && (clipItem.trackType === "video"
                                                             || clipItem.trackType === "shape"
                                                             || clipItem.clipData.kind === "image")
                                                filmstripPath: clipItem.clipData.filmstripPath
                                                z: 0
                                            }

                                            Rectangle {
                                                visible: clipItem.trackType === "video"
                                                         || clipItem.trackType === "audio"
                                                         || clipItem.trackType === "shape"
                                                width: parent.width
                                                height: clipItem.clipEffects.length > 0 ? 32 : 20
                                                color: "#00000066"
                                                z: 1

                                                Column {
                                                    anchors.left: parent.left
                                                    anchors.right: parent.right
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    anchors.leftMargin: 6
                                                    anchors.rightMargin: 6
                                                    spacing: 1

                                                    Text {
                                                        width: parent.width
                                                        text: clipItem.clipData.name
                                                        color: "#ffffffbf"
                                                        font.pixelSize: Theme.fontSizeTiny
                                                        font.family: Theme.fontFamily
                                                        elide: Text.ElideRight
                                                    }

                                                    Text {
                                                        width: parent.width
                                                        visible: clipItem.clipEffects.length > 0
                                                        text: {
                                                            const names = []
                                                            for (var i = 0; i < clipItem.clipEffects.length; i++)
                                                                names.push(clipItem.clipEffects[i].label || qsTr("Effect"))
                                                            return names.join(" · ")
                                                        }
                                                        color: "#a8d8ff"
                                                        font.pixelSize: Theme.fontSizeTiny
                                                        font.family: Theme.fontFamily
                                                        elide: Text.ElideRight
                                                    }
                                                }
                                            }

                                            Column {
                                                visible: clipItem.trackType === "text"
                                                         || clipItem.trackType === "subtitle"
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.verticalCenter: parent.verticalCenter
                                                anchors.leftMargin: 8
                                                anchors.rightMargin: 8
                                                spacing: 1

                                                Text {
                                                    width: parent.width
                                                    text: clipItem.trackType === "subtitle"
                                                          ? (clipItem.clipData.name
                                                             || qsTr("Subtitles"))
                                                          : (clipItem.clipData.textContent
                                                             || clipItem.clipData.name)
                                                    color: "white"
                                                    font.pixelSize: Theme.fontSizeXs
                                                    font.family: Theme.fontFamily
                                                    elide: Text.ElideRight
                                                }

                                                Text {
                                                    width: parent.width
                                                    visible: clipItem.clipEffects.length > 0
                                                    text: {
                                                        const names = []
                                                        for (var i = 0; i < clipItem.clipEffects.length; i++)
                                                            names.push(clipItem.clipEffects[i].label || qsTr("Effect"))
                                                        return names.join(" · ")
                                                    }
                                                    color: "#a8d8ff"
                                                    font.pixelSize: Theme.fontSizeTiny
                                                    font.family: Theme.fontFamily
                                                    elide: Text.ElideRight
                                                }
                                            }

                                            Canvas {
                                                id: waveformCanvas
                                                visible: clipItem.trackType === "audio"
                                                         || (clipItem.trackType === "video"
                                                             && clipItem.showWaveform)
                                                property var peaks: clipItem.clipData.path
                                                              ? EditorState.waveformPeaks(clipItem.clipData.path)
                                                              : []
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.top: parent.top
                                                anchors.topMargin: clipItem.clipEffects.length > 0 ? 32 : 20
                                                anchors.bottom: parent.bottom
                                                onPeaksChanged: requestPaint()
                                                onWidthChanged: requestPaint()
                                                onHeightChanged: requestPaint()

                                                Connections {
                                                    target: EditorState
                                                    function onWaveformReady(path) {
                                                        if (path === clipItem.clipData.path)
                                                            waveformCanvas.peaks = EditorState.waveformPeaks(path)
                                                    }
                                                }

                                                onPaint: {
                                                    var ctx = getContext("2d");
                                                    ctx.clearRect(0, 0, width, height);
                                                    if (!peaks || peaks.length === 0)
                                                        return;
                                                    // One filled column per screen pixel: zoomed-out columns
                                                    // take the max of covered peaks; zoomed-in columns
                                                    // reuse peaks so there are never gaps between bars.
                                                    ctx.fillStyle = Theme.waveformColor;
                                                    var mid = height / 2;
                                                    var w = Math.max(1, Math.floor(width));
                                                    var n = peaks.length;
                                                    for (var x = 0; x < w; x++) {
                                                        var i0 = Math.floor(x * n / w);
                                                        var i1 = Math.floor((x + 1) * n / w);
                                                        if (i1 <= i0)
                                                            i1 = Math.min(n, i0 + 1);
                                                        var peak = 0;
                                                        for (var i = i0; i < i1; i++) {
                                                            if (peaks[i] > peak)
                                                                peak = peaks[i];
                                                        }
                                                        var amp = peak * mid * 0.9;
                                                        if (amp > 0.5)
                                                            ctx.fillRect(x, mid - amp, 1, amp * 2);
                                                    }
                                                }
                                            }
                                        }

                                        MouseArea {
                                            id: clipMouse
                                            z: 2
                                            anchors.fill: parent
                                            anchors.leftMargin: clipItem.selected ? 14 : 0
                                            anchors.rightMargin: clipItem.selected ? 14 : 0
                                            enabled: !leftTrimMouse.pressed && !rightTrimMouse.pressed
                                                         && !fadeInMouse.pressed && !fadeOutMouse.pressed
                                            cursorShape: Qt.PointingHandCursor
                                            drag.target: clipItem
                                            drag.axis: Drag.XAndYAxis
                                            drag.threshold: 8
                                            drag.minimumX: Theme.clipSelectionRingWidth
                                            drag.minimumY: -trackRow.height
                                            drag.maximumY: trackRow.height * 2
                                            property int originTrack: trackRow.trackIndex

                                            onPressed: {
                                                originTrack = trackRow.trackIndex
                                                if ((mouse.modifiers & Qt.ShiftModifier) !== 0)
                                                    EditorState.addToSelection(trackRow.trackIndex, modelData)
                                                else
                                                    EditorState.selectClip(trackRow.trackIndex, modelData)
                                            }
                                            onClicked: (mouse) => {
                                                if ((mouse.modifiers & Qt.ShiftModifier) !== 0)
                                                    EditorState.addToSelection(trackRow.trackIndex, modelData)
                                                else
                                                    EditorState.selectClip(trackRow.trackIndex, modelData)
                                            }
                                            onReleased: {
                                                root.clearLandingPreview()
                                                if (!drag.active)
                                                    return
                                                const newStart = (clipItem.x - Theme.clipSelectionRingWidth) / root.pxPerSecond
                                                const pos = clipItem.mapToItem(trackColumn, clipItem.width / 2, clipItem.height / 2)
                                                const targetTrack = root.trackIndexAtY(pos.y)
                                                clipItem.y = Theme.clipSelectionRingWidth
                                                if (targetTrack >= 0 && targetTrack !== originTrack)
                                                    EditorState.moveClipToTrack(originTrack, modelData, targetTrack, newStart)
                                                else
                                                    EditorState.moveClip(trackRow.trackIndex, modelData, newStart)
                                            }
                                        }

                                        // Fade handles live on the (non-clipping) clip item so they
                                        // render fully at the corners and sit above the move/trim areas.
                                        Rectangle {
                                            id: fadeInHandle
                                            width: 13
                                            height: 13
                                            radius: 6.5
                                            y: 2
                                            z: 20
                                            visible: clipItem.timelineFadeHandles && clipItem.selected && clipItem.width > 26
                                            color: Theme.primary
                                            border.color: "white"
                                            border.width: 2

                                            Binding {
                                                target: fadeInHandle
                                                property: "x"
                                                when: !fadeInMouse.pressed
                                                value: Math.max(0, Math.min(clipItem.width - fadeInHandle.width,
                                                                            (clipItem.clipData.fadeIn || 0) * root.pxPerSecond - fadeInHandle.width / 2))
                                            }

                                            MouseArea {
                                                id: fadeInMouse
                                                anchors.fill: parent
                                                anchors.margins: -6
                                                preventStealing: true
                                                hoverEnabled: true
                                                cursorShape: Qt.SizeHorCursor
                                                onPositionChanged: (mouse) => {
                                                    if (!pressed)
                                                        return
                                                    const px = Math.max(0, Math.min(clipItem.width,
                                                                                    mapToItem(clipItem, mouse.x, mouse.y).x))
                                                    fadeInHandle.x = Math.min(clipItem.width - fadeInHandle.width, px - fadeInHandle.width / 2)
                                                    EditorState.previewSetClipFade(trackRow.trackIndex, modelData,
                                                                                   px / root.pxPerSecond,
                                                                                   clipItem.clipData.fadeOut || 0)
                                                }
                                                onReleased: EditorState.commitPreviewDrag()

                                                ToolTip {
                                                    visible: fadeInMouse.pressed
                                                    text: qsTr("Fade in %1s").arg((clipItem.clipData.fadeIn || 0).toFixed(2))
                                                }
                                            }
                                        }

                                        Rectangle {
                                            id: fadeOutHandle
                                            width: 13
                                            height: 13
                                            radius: 6.5
                                            y: 2
                                            z: 20
                                            visible: clipItem.timelineFadeHandles && clipItem.selected && clipItem.width > 26
                                            color: Theme.primary
                                            border.color: "white"
                                            border.width: 2

                                            Binding {
                                                target: fadeOutHandle
                                                property: "x"
                                                when: !fadeOutMouse.pressed
                                                value: Math.max(0, Math.min(clipItem.width - fadeOutHandle.width,
                                                                            clipItem.width - (clipItem.clipData.fadeOut || 0) * root.pxPerSecond - fadeOutHandle.width / 2))
                                            }

                                            MouseArea {
                                                id: fadeOutMouse
                                                anchors.fill: parent
                                                anchors.margins: -6
                                                preventStealing: true
                                                hoverEnabled: true
                                                cursorShape: Qt.SizeHorCursor
                                                onPositionChanged: (mouse) => {
                                                    if (!pressed)
                                                        return
                                                    const px = Math.max(0, Math.min(clipItem.width,
                                                                                    mapToItem(clipItem, mouse.x, mouse.y).x))
                                                    fadeOutHandle.x = Math.max(0, px - fadeOutHandle.width / 2)
                                                    EditorState.previewSetClipFade(trackRow.trackIndex, modelData,
                                                                                   clipItem.clipData.fadeIn || 0,
                                                                                   Math.max(0, (clipItem.width - px) / root.pxPerSecond))
                                                }
                                                onReleased: EditorState.commitPreviewDrag()

                                                ToolTip {
                                                    visible: fadeOutMouse.pressed
                                                    text: qsTr("Fade out %1s").arg((clipItem.clipData.fadeOut || 0).toFixed(2))
                                                }
                                            }
                                        }

                                        // Trim handles sit above fade dots so edge drags resize the clip.
                                        Rectangle {
                                            id: leftTrimHandle
                                            width: 12
                                            anchors.left: clipBackground.left
                                            anchors.top: clipBackground.top
                                            anchors.bottom: clipBackground.bottom
                                            visible: clipItem.selected
                                            color: Theme.primary
                                            opacity: leftTrimMouse.pressed ? 1.0 : 0.75
                                            z: 30

                                            MouseArea {
                                                id: leftTrimMouse
                                                anchors.fill: parent
                                                anchors.leftMargin: -10
                                                anchors.rightMargin: -4
                                                anchors.topMargin: -6
                                                anchors.bottomMargin: -6
                                                preventStealing: true
                                                hoverEnabled: true
                                                cursorShape: Qt.SizeHorCursor
                                                onPositionChanged: (mouse) => {
                                                    if (!pressed)
                                                        return
                                                    const timelineX = mapToItem(trackRow, mouse.x, mouse.y).x
                                                    EditorState.trimClipLeft(trackRow.trackIndex, modelData,
                                                                           timelineX / root.pxPerSecond)
                                                }
                                            }
                                        }

                                        Rectangle {
                                            id: rightTrimHandle
                                            width: 12
                                            anchors.right: clipBackground.right
                                            anchors.top: clipBackground.top
                                            anchors.bottom: clipBackground.bottom
                                            visible: clipItem.selected
                                            color: Theme.primary
                                            opacity: rightTrimMouse.pressed ? 1.0 : 0.75
                                            z: 30

                                            MouseArea {
                                                id: rightTrimMouse
                                                anchors.fill: parent
                                                anchors.leftMargin: -4
                                                anchors.rightMargin: -10
                                                anchors.topMargin: -6
                                                anchors.bottomMargin: -6
                                                preventStealing: true
                                                hoverEnabled: true
                                                cursorShape: Qt.SizeHorCursor
                                                onPositionChanged: (mouse) => {
                                                    if (!pressed)
                                                        return
                                                    const timelineX = mapToItem(trackRow, mouse.x, mouse.y).x
                                                    EditorState.trimClipRight(trackRow.trackIndex, modelData,
                                                                            timelineX / root.pxPerSecond)
                                                }
                                            }
                                        }
                                    }
                                }

                                // Transition overlap regions (purple) — above clips for hit-testing.
                                Repeater {
                                    model: Math.max(0, root.tracks[trackRow.trackIndex].clips.length)
                                    delegate: Item {
                                        id: transitionRegion
                                        property int leftClipIndex: modelData
                                        property var leftClip: root.tracks[trackRow.trackIndex].clips[leftClipIndex]
                                        property string trackType: root.tracks[trackRow.trackIndex].type
                                        property var transitionData: EditorState.transitionBetweenClips(
                                                                         trackRow.trackIndex, leftClipIndex)
                                        property bool hasTransition: transitionData
                                                                       && Object.keys(transitionData).length > 0
                                        property bool transitionSelected: EditorState.selectedTransitionTrack === trackRow.trackIndex
                                                                          && EditorState.selectedTransitionLeftClip === leftClipIndex

                                        // Find partner clip that abuts/overlaps this one.
                                        property int partnerIndex: {
                                            const clips = root.tracks[trackRow.trackIndex].clips
                                            const left = leftClip
                                            if (!left)
                                                return -1
                                            let best = -1
                                            let bestStart = 1e12
                                            for (let i = 0; i < clips.length; i++) {
                                                if (i === leftClipIndex)
                                                    continue
                                                const right = clips[i]
                                                if (right.start < left.start)
                                                    continue
                                                const gap = right.start - (left.start + left.duration)
                                                if (gap > 0.001)
                                                    continue
                                                if (right.start < bestStart) {
                                                    bestStart = right.start
                                                    best = i
                                                }
                                            }
                                            return best
                                        }
                                        property var rightClip: partnerIndex >= 0
                                            ? root.tracks[trackRow.trackIndex].clips[partnerIndex]
                                            : null
                                        property bool clipsLinked: partnerIndex >= 0
                                        property real regionStart: {
                                            if (!hasTransition && !clipsLinked)
                                                return 0
                                            if (hasTransition && transitionData.start !== undefined)
                                                return transitionData.start
                                            if (!rightClip)
                                                return 0
                                            const leftEnd = leftClip.start + leftClip.duration
                                            if (rightClip.start < leftEnd)
                                                return rightClip.start
                                            return leftEnd - 0.25
                                        }
                                        property real regionEnd: {
                                            if (!hasTransition && !clipsLinked)
                                                return 0
                                            if (hasTransition && transitionData.end !== undefined)
                                                return transitionData.end
                                            if (!rightClip)
                                                return 0
                                            const leftEnd = leftClip.start + leftClip.duration
                                            if (rightClip.start < leftEnd)
                                                return leftEnd
                                            return leftEnd + 0.25
                                        }
                                        property bool showRegion: (trackType === "video" || trackType === "shape")
                                                                  && clipsLinked
                                                                  && regionEnd > regionStart

                                        z: 10
                                        visible: showRegion
                                        x: regionStart * root.pxPerSecond
                                        width: Math.max(8, (regionEnd - regionStart) * root.pxPerSecond)
                                        height: parent.height - Theme.clipSelectionRingWidth * 2
                                        y: Theme.clipSelectionRingWidth

                                        Rectangle {
                                            anchors.fill: parent
                                            radius: Theme.radiusSm
                                            color: transitionRegion.transitionSelected
                                                   ? Qt.rgba(Theme.transitionOverlap.r, Theme.transitionOverlap.g,
                                                             Theme.transitionOverlap.b, 0.85)
                                                   : (transitionRegion.hasTransition
                                                      ? Qt.rgba(Theme.transitionOverlap.r, Theme.transitionOverlap.g,
                                                                Theme.transitionOverlap.b, 0.65)
                                                      : Qt.rgba(Theme.transitionOverlap.r, Theme.transitionOverlap.g,
                                                                Theme.transitionOverlap.b, 0.35))
                                            border.width: transitionRegion.transitionSelected ? 2 : 1
                                            border.color: Theme.transitionOverlap

                                            // Diagonal hatch so overlaps read as a blend zone.
                                            Canvas {
                                                anchors.fill: parent
                                                anchors.margins: 1
                                                opacity: 0.35
                                                onPaint: {
                                                    const ctx = getContext("2d")
                                                    ctx.clearRect(0, 0, width, height)
                                                    ctx.strokeStyle = "#ffffff"
                                                    ctx.lineWidth = 1
                                                    const step = 6
                                                    for (let x = -height; x < width; x += step) {
                                                        ctx.beginPath()
                                                        ctx.moveTo(x, height)
                                                        ctx.lineTo(x + height, 0)
                                                        ctx.stroke()
                                                    }
                                                }
                                                onWidthChanged: requestPaint()
                                                onHeightChanged: requestPaint()
                                            }

                                            Text {
                                                anchors.centerIn: parent
                                                visible: parent.width >= 28
                                                text: transitionRegion.hasTransition
                                                      ? (transitionRegion.transitionData.label
                                                         ? transitionRegion.transitionData.label.charAt(0)
                                                         : "≫")
                                                      : "≫"
                                                color: "#ffffff"
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontSizeTiny
                                                font.weight: Font.Bold
                                            }

                                            ToolTip {
                                                visible: transitionMouse.containsMouse
                                                         && transitionRegion.hasTransition
                                                delay: 400
                                                text: transitionRegion.transitionData.label
                                                      || transitionRegion.transitionData.kind
                                                      || ""
                                            }
                                        }

                                        DropArea {
                                            anchors.fill: parent
                                            keys: ["application/x-drift-transition"]
                                            onDropped: (drop) => {
                                                drop.accept(Qt.CopyAction)
                                                const kind = drop.getDataAsString("application/x-drift-transition")
                                                if (kind.length > 0)
                                                    EditorState.addTransition(trackRow.trackIndex,
                                                                              transitionRegion.leftClipIndex,
                                                                              kind, 0.5)
                                            }
                                        }

                                        MouseArea {
                                            id: transitionMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (transitionRegion.hasTransition) {
                                                    EditorState.selectTransition(trackRow.trackIndex,
                                                                                 transitionRegion.leftClipIndex)
                                                } else {
                                                    EditorState.addTransition(trackRow.trackIndex,
                                                                              transitionRegion.leftClipIndex,
                                                                              "crossfade", 0.5)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // snap guide -------------------------------------------------------------
                    Rectangle {
                        visible: root.snapGuideSeconds >= 0
                        x: root.snapGuideSeconds * root.pxPerSecond
                        y: Theme.timelineRulerHeight + Theme.timelineBookmarkRowHeight
                        width: 1
                        height: root.totalTracksHeight()
                        color: "#f5c542"
                        z: 6
                    }

                    // playhead ---------------------------------------------------------------
                    Item {
                        id: playhead
                        y: 0
                        width: Theme.playheadLineWidth
                        height: Theme.timelineRulerHeight + Theme.timelineBookmarkRowHeight + root.totalTracksHeight()

                        Binding {
                            target: playhead
                            property: "x"
                            value: EditorState.playheadSeconds * root.pxPerSecond
                            when: !playheadDragArea.drag.active
                        }

                        Rectangle {
                            anchors.left: parent.left
                            width: Theme.playheadLineWidth
                            height: parent.height
                            color: Theme.primary
                        }

                        Rectangle {
                            id: playheadHandle
                            width: Theme.playheadHandleSize
                            height: Theme.playheadHandleSize
                            radius: Theme.playheadHandleSize / 2
                            x: -width / 2 + Theme.playheadLineWidth / 2
                            y: 4
                            color: Theme.primary
                            border.width: 2
                            border.color: Theme.primary

                            MouseArea {
                                id: playheadDragArea
                                anchors.fill: parent
                                anchors.margins: -4
                                cursorShape: Qt.SizeHorCursor
                                drag.target: playhead
                                drag.axis: Drag.XAxis
                                drag.minimumX: 0
                                drag.maximumX: flick.contentWidth - Theme.playheadLineWidth
                                onReleased: EditorState.playheadSeconds = EditorState.snapTime(playhead.x / root.pxPerSecond)
                            }
                        }
                    }
                }
            }
        }
        } // Column (keyframes + tracks)
    }

    Connections {
        target: EditorState
        function onPlayheadSecondsChanged() {
            if (EditorState.playing)
                root.ensurePlayheadVisible()
        }
    }
}
