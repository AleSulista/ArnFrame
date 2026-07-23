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
        return Theme.icons.video;
    }

    // Human label for a track type. Tracks previously showed no name at all.
    function trackTypeLabel(type) {
        if (type === "audio") return qsTr("Audio");
        if (type === "text") return qsTr("Text");
        if (type === "subtitle") return qsTr("Subtitle");
        if (type === "shape") return qsTr("Shape");
        return qsTr("Video");
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
        const maxY = Math.max(0, flick.contentHeight - flick.height)
        if (wheel.modifiers & Qt.ControlModifier) {
            const t = wheel.x / pxPerSecond
            const viewportX = wheel.x - flick.contentX
            const factor = wheel.angleDelta.y > 0 ? 1.15 : 1.0 / 1.15
            zoom = Math.max(minZoom, Math.min(maxZoom, zoom * factor))
            const newMaxX = Math.max(0, flick.contentWidth - flick.width)
            flick.contentX = Math.max(0, Math.min(newMaxX, t * pxPerSecond - viewportX))
            return
        }

        const dy = wheel.angleDelta.y
        const dx = wheel.angleDelta.x

        // Shift forces horizontal scrolling regardless of overflow.
        if (wheel.modifiers & Qt.ShiftModifier) {
            const delta = dy !== 0 ? dy : dx
            flick.contentX = Math.max(0, Math.min(maxX, flick.contentX - delta))
            return
        }

        // Trackpad horizontal component always scrolls horizontally.
        if (dx !== 0)
            flick.contentX = Math.max(0, Math.min(maxX, flick.contentX - dx))

        // Vertical wheel scrolls the tracks when they overflow the viewport;
        // otherwise it falls back to horizontal so short timelines keep the
        // previous wheel-to-pan behaviour.
        if (dy !== 0) {
            if (maxY > 0)
                flick.contentY = Math.max(0, Math.min(maxY, flick.contentY - dy))
            else
                flick.contentX = Math.max(0, Math.min(maxX, flick.contentX - dy))
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
        // Eased rather than teleporting: auto-scroll during playback used to jump
        // by up to a full viewport.
        if (playheadX < flick.contentX + margin)
            scrollToX(Math.max(0, playheadX - margin));
        else if (playheadX > flick.contentX + flick.width - margin)
            scrollToX(Math.min(Math.max(0, flick.contentWidth - flick.width),
                               playheadX - flick.width + margin));
    }

    // Smooth horizontal scroll helper. Skipped while the user is dragging the
    // view, so it never fights a flick in progress.
    function scrollToX(target) {
        if (flick.dragging || flick.flicking) {
            flick.contentX = target
            return
        }
        contentXAnimation.stop()
        contentXAnimation.from = flick.contentX
        contentXAnimation.to = target
        contentXAnimation.start()
    }

    NumberAnimation {
        id: contentXAnimation
        target: flick
        property: "contentX"
        duration: Theme.durationBase
        easing.type: Theme.easing
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
                id: leftControls
                anchors.left: parent.left
                anchors.leftMargin: Theme.spacingLg
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.spacingXs
                // Never runs under the right-hand controls; buttons past the
                // available width are clipped rather than overlapping.
                width: Math.max(0, rightControls.x - x - Theme.spacingLg)
                clip: true

                IconButton {
                    glyph: EditorState.playing ? Theme.icons.pause : Theme.icons.play
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
                    width: Theme.borderWidth
                    height: Theme.spacing3xl
                    color: Theme.panelBorder
                    anchors.verticalCenter: parent.verticalCenter
                }

                IconButton { glyph: Theme.icons.scissors; variant: "text"; tooltip: qsTr("Split at playhead"); onClicked: EditorState.splitAtPlayhead() }
                IconButton { glyph: Theme.icons.chevronsLeft; variant: "text"; tooltip: qsTr("Split left"); onClicked: EditorState.splitSelectedClipLeft() }
                IconButton { glyph: Theme.icons.chevronsRight; variant: "text"; tooltip: qsTr("Split right"); onClicked: EditorState.splitSelectedClipRight() }
                IconButton {
                    glyph: Theme.icons.unlink
                    variant: "text"
                    tooltip: qsTr("Unlink audio")
                    enabled: EditorState.unlinkAvailable
                    onClicked: EditorState.unlinkSelectedClips()
                }
                IconButton {
                    glyph: Theme.icons.linkTwo
                    variant: "text"
                    tooltip: qsTr("Merge adjacent clips")
                    enabled: EditorState.mergeAvailable
                    onClicked: EditorState.mergeSelectedClips()
                }
                IconButton { glyph: Theme.icons.copy; variant: "text"; tooltip: qsTr("Copy selection"); onClicked: EditorState.copySelection() }
                IconButton { glyph: Theme.icons.clipboardPaste; variant: "text"; tooltip: qsTr("Paste at playhead"); onClicked: EditorState.pasteAtPlayhead() }
                IconButton { glyph: Theme.icons.copyPlus; variant: "text"; tooltip: qsTr("Duplicate clip"); onClicked: EditorState.duplicateSelectedClip() }
                IconButton { glyph: Theme.icons.snowflake; variant: "text"; tooltip: qsTr("Freeze frame at playhead"); onClicked: EditorState.freezeFrameAtPlayhead() }
                IconButton { glyph: Theme.icons.trash; variant: "text"; tooltip: qsTr("Delete clip"); onClicked: EditorState.deleteSelectedClip() }

                Rectangle {
                    width: Theme.borderWidth
                    height: Theme.spacing3xl
                    color: Theme.panelBorder
                    anchors.verticalCenter: parent.verticalCenter
                }

                IconButton {
                    glyph: Theme.icons.bookmark
                    variant: "text"
                    tooltip: qsTr("Add bookmark at playhead")
                    onClicked: EditorState.addBookmark(EditorState.playheadSeconds, "Mark " + Math.round(EditorState.playheadSeconds))
                }
                IconButton {
                    glyph: Theme.icons.undo
                    variant: "text"
                    tooltip: qsTr("Undo")
                    onClicked: EditorState.undo()
                    enabled: EditorState.undoAvailable
                }
                IconButton {
                    glyph: Theme.icons.redo
                    variant: "text"
                    tooltip: qsTr("Redo")
                    onClicked: EditorState.redo()
                    enabled: EditorState.redoAvailable
                }

                Rectangle {
                    width: Theme.borderWidth
                    height: Theme.spacing3xl
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
                id: sceneBadge

                // Sits between the two button groups, which are anchored to the
                // toolbar edges and grow freely. Prefer dead centre, but slide
                // aside to stay clear of them, and drop out entirely once the
                // gap can no longer fit the badge.
                readonly property real gapStart: leftControls.x + leftControls.width + 12
                readonly property real gapEnd: rightControls.x - 12

                x: Math.max(gapStart, Math.min((toolbar.width - width) / 2, gapEnd - width))
                anchors.verticalCenter: parent.verticalCenter
                visible: gapEnd - gapStart >= width
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
                        text: qsTr("Scene 1")
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
                id: rightControls
                anchors.right: parent.right
                anchors.rightMargin: Theme.spacingLg
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.spacingSm

                IconButton {
                    id: magnetButton
                    glyph: Theme.icons.magnet
                    variant: "text"
                    tooltip: qsTr("Toggle snapping")
                    active: EditorState.snapEnabled
                    onClicked: EditorState.snapEnabled = !EditorState.snapEnabled
                }
                IconButton {
                    id: rippleButton
                    glyph: Theme.icons.foldHorizontal
                    variant: "text"
                    tooltip: qsTr("Toggle ripple editing")
                    active: EditorState.rippleEnabled
                    onClicked: EditorState.rippleEnabled = !EditorState.rippleEnabled
                }

                Rectangle {
                    width: Theme.borderWidth
                    height: Theme.spacing3xl
                    color: Theme.panelBorder
                    anchors.verticalCenter: parent.verticalCenter
                }

                IconButton {
                    glyph: Theme.icons.zoomOut
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
                    // There was no zoom readout anywhere, so the current level
                    // was simply unknowable.
                    valueFormatter: function () {
                        return qsTr("Zoom %1×").arg(root.zoom.toFixed(2))
                    }
                }

                // Numeric zoom level, and a click target to return to 1×.
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 44
                    text: root.zoom.toFixed(2) + "×"
                    color: Theme.mutedForeground
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.fontSizeTick
                    horizontalAlignment: Text.AlignHCenter

                    ThemedToolTip {
                        text: qsTr("Zoom level — click to reset to 1×. Ctrl+wheel over the timeline also zooms.")
                        visible: zoomLabelMouse.containsMouse
                    }

                    MouseArea {
                        id: zoomLabelMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.zoom = 1.0
                    }
                }
                IconButton {
                    glyph: Theme.icons.zoomIn
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
                    // Clamped: went negative at small panel heights, which spilled
                    // the absolutely-positioned label rows out of the column.
                    height: Math.max(0, parent.height - Theme.timelineRulerHeight
                                        - Theme.timelineBookmarkRowHeight)
                    clip: true

                    Repeater {
                        model: root.tracks.length
                        delegate: Item {
                            id: trackLabelRow
                            // Read through root.tracks (not the EditorState
                            // getters) so the toggles rebind on tracksChanged.
                            readonly property bool trackMuted: root.tracks[index].muted === true
                            readonly property bool trackHidden: root.tracks[index].hidden === true
                            readonly property bool trackWaveform: root.tracks[index].showWaveform === true
                            width: Theme.trackLabelsWidth
                            height: root.trackHeight(root.tracks[index].type)
                                    + (index < root.tracks.length - 1 ? Theme.trackGap : 0)
                            // Follows the timeline's vertical scroll so labels stay
                            // aligned with their rows.
                            y: root.trackRowTop(index) - flick.contentY
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

                                ThemedToolTip {
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
                                    glyph: trackLabelRow.trackMuted ? Theme.icons.volumeOff : Theme.icons.volumeHigh
                                    iconSize: 16
                                    iconColor: trackLabelRow.trackMuted ? Theme.destructive : Theme.mutedForeground
                                    anchors.verticalCenter: parent.verticalCenter

                                    ThemedToolTip {
                                        visible: muteMouse.containsMouse
                                        text: trackLabelRow.trackMuted ? qsTr("Unmute track") : qsTr("Mute track")
                                    }

                                    MouseArea {
                                        id: muteMouse
                                        anchors.fill: parent
                                        anchors.margins: -4
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: EditorState.setTrackMuted(index, !trackLabelRow.trackMuted)
                                    }
                                }

                                IconGlyph {
                                    visible: root.tracks[index].type === "video"
                                             || root.tracks[index].type === "text"
                                             || root.tracks[index].type === "subtitle"
                                             || root.tracks[index].type === "shape"
                                    glyph: trackLabelRow.trackHidden ? Theme.icons.eyeOff : Theme.icons.eye
                                    iconSize: 16
                                    iconColor: trackLabelRow.trackHidden ? Theme.destructive : Theme.mutedForeground
                                    anchors.verticalCenter: parent.verticalCenter

                                    ThemedToolTip {
                                        visible: hideMouse.containsMouse
                                        text: trackLabelRow.trackHidden ? qsTr("Show track") : qsTr("Hide track")
                                    }

                                    MouseArea {
                                        id: hideMouse
                                        anchors.fill: parent
                                        anchors.margins: -4
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: EditorState.setTrackHidden(index, !trackLabelRow.trackHidden)
                                    }
                                }

                                // Toggle the whole track between filmstrip previews and audio waveforms.
                                IconGlyph {
                                    visible: root.tracks[index].type === "video"
                                    glyph: trackLabelRow.trackWaveform ? Theme.icons.audioLines : Theme.icons.film
                                    iconSize: 16
                                    iconColor: trackLabelRow.trackWaveform ? Theme.primary : Theme.mutedForeground
                                    anchors.verticalCenter: parent.verticalCenter

                                    ThemedToolTip {
                                        visible: waveMouse.containsMouse
                                        text: trackLabelRow.trackWaveform ? qsTr("Show filmstrip")
                                                                          : qsTr("Show waveform")
                                    }

                                    MouseArea {
                                        id: waveMouse
                                        anchors.fill: parent
                                        anchors.margins: -4
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: EditorState.setTrackShowWaveform(index, !trackLabelRow.trackWaveform)
                                    }
                                }

                                IconGlyph {
                                    glyph: root.trackTypeIcon(root.tracks[index].type)
                                    iconSize: Theme.iconSizeBase
                                    iconColor: Theme.mutedForeground
                                    anchors.verticalCenter: parent.verticalCenter

                                    // Tracks were identifiable only by this 16px
                                    // glyph, with no name and no tooltip.
                                    ThemedToolTip {
                                        text: root.trackTypeLabel(root.tracks[index].type)
                                        visible: typeHover.hovered
                                    }

                                    HoverHandler { id: typeHover }
                                }
                            }

                            // Track name. Nothing in the header used to say which
                            // track this was beyond the type glyph.
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: Theme.spacing3xl + Theme.spacingSm
                                anchors.right: parent.right
                                anchors.rightMargin: Theme.spacingLg
                                anchors.top: parent.top
                                anchors.topMargin: Theme.spacingMd
                                text: root.trackTypeLabel(root.tracks[index].type)
                                      + " " + (index + 1)
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeTiny
                                elide: Text.ElideRight
                                visible: root.trackHeight(root.tracks[index].type) >= 40
                            }

                            // Track context menu.
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                onClicked: trackContextMenu.popup()

                                ThemedContextMenu {
                                    id: trackContextMenu

                                    MenuItem {
                                        text: trackLabelRow.trackMuted ? qsTr("Unmute track")
                                                                       : qsTr("Mute track")
                                        icon.name: trackLabelRow.trackMuted ? Theme.icons.volumeHigh
                                                                            : Theme.icons.volumeOff
                                        onTriggered: EditorState.setTrackMuted(index, !trackLabelRow.trackMuted)
                                    }
                                    MenuItem {
                                        text: trackLabelRow.trackHidden ? qsTr("Show track")
                                                                        : qsTr("Hide track")
                                        icon.name: trackLabelRow.trackHidden ? Theme.icons.eye
                                                                             : Theme.icons.eyeOff
                                        onTriggered: EditorState.setTrackHidden(index, !trackLabelRow.trackHidden)
                                    }
                                    MenuItem {
                                        visible: root.tracks[index].type === "video"
                                        height: visible ? implicitHeight : 0
                                        text: trackLabelRow.trackWaveform
                                              ? qsTr("Show filmstrip") : qsTr("Show waveform")
                                        icon.name: trackLabelRow.trackWaveform
                                                   ? Theme.icons.film : Theme.icons.audioLines
                                        onTriggered: EditorState.setTrackShowWaveform(
                                                         index, !trackLabelRow.trackWaveform)
                                    }
                                    MenuSeparator {}
                                    MenuItem {
                                        text: qsTr("Delete track")
                                        icon.name: Theme.icons.trash
                                        onTriggered: EditorState.removeTrack(index)
                                    }
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

                // Height of the pinned ruler + bookmark strip at the top.
                readonly property real headerHeight: Theme.timelineRulerHeight
                                                     + Theme.timelineBookmarkRowHeight

                contentWidth: Math.max(width, (EditorState.durationSeconds + 5) * root.pxPerSecond)
                // Was `height`, so once the tracks were taller than the panel the
                // lower ones were silently truncated and could not be reached at
                // all. totalTracksHeight() was already computed but never used.
                contentHeight: Math.max(height,
                                        headerHeight + root.totalTracksHeight() + Theme.trackGap)
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.horizontal: AppScrollBar { policy: ScrollBar.AlwaysOn }
                ScrollBar.vertical: AppScrollBar { }

                Item {
                    id: timelineContent
                    width: flick.contentWidth
                    height: flick.contentHeight

                    // Wheel handling: scoped to the ruler so it does not block timeline drops.
                    MouseArea {
                        id: rulerWheelArea
                        width: parent.width
                        y: flick.contentY
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

                    // Opaque backdrop behind the pinned ruler/bookmark strip.
                    Rectangle {
                        y: flick.contentY
                        width: parent.width
                        height: flick.headerHeight
                        color: Theme.panelBackground
                        z: 1
                    }

                    // ruler ------------------------------------------------------------
                    Item {
                        id: ruler
                        width: parent.width
                        // Pinned: stays at the top of the viewport as tracks scroll.
                        y: flick.contentY
                        z: 2
                        height: Theme.timelineRulerHeight

                        // Press-and-drag scrubs; it used to handle clicks only.
                        MouseArea {
                            id: rulerScrub
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            preventStealing: true

                            function scrubTo(x) {
                                EditorState.playheadSeconds =
                                    EditorState.snapTime(Math.max(0, x) / root.pxPerSecond)
                            }

                            onPressed: (mouse) => {
                                EditorState.clearSelection()
                                scrubTo(mouse.x)
                            }
                            onPositionChanged: (mouse) => {
                                if (pressed)
                                    scrubTo(mouse.x)
                            }

                            ThemedToolTip {
                                text: qsTr("Click or drag to move the playhead")
                                visible: rulerScrub.containsMouse && !rulerScrub.pressed
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
                        y: flick.contentY + Theme.timelineRulerHeight
                        z: 2
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

                                ThemedToolTip {
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

                    // A project with no tracks used to render as a completely
                    // blank timeline, with the only route to a first track being
                    // one button among sixteen in the toolbar.
                    EmptyState {
                        x: flick.contentX + (flick.width - width) / 2
                        y: flick.contentY + flick.headerHeight
                           + Math.max(0, (flick.height - flick.headerHeight - height) / 2)
                        width: Math.min(flick.width - Theme.spacing3xl, 320)
                        visible: root.tracks.length === 0
                        z: 4
                        glyph: Theme.icons.layers
                        title: qsTr("Your timeline is empty")
                        hint: qsTr("Drag media here from the library, or add an empty track to start.")
                        actionText: qsTr("New track")
                        actionVariant: "primary"
                        onActionTriggered: newTrackMenuFromEmpty.open()

                        NewTrackMenu {
                            id: newTrackMenuFromEmpty
                            y: parent.height
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
                                // Faint row tint on hover, and an empty track now
                                // reads as a track rather than as blank space.
                                color: trackHover.hovered
                                       ? Qt.rgba(Theme.panelAccent.r, Theme.panelAccent.g,
                                                 Theme.panelAccent.b, 0.5)
                                       : Qt.rgba(Theme.panelAccent.r, Theme.panelAccent.g,
                                                 Theme.panelAccent.b, 0.22)

                                Behavior on color {
                                    ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                                }

                                HoverHandler { id: trackHover }

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

                                        // Trim handles need room for themselves plus
                                        // a drag region between them; below that the
                                        // whole clip stays draggable instead.
                                        readonly property bool showTrimHandles:
                                            selected && width >= Theme.clipMinInteractiveWidth * 2

                                        // Name band height, derived once instead of
                                        // being hardcoded at three separate sites,
                                        // and clamped so it can never swallow a
                                        // short (25px) text or subtitle row.
                                        readonly property real headerBandHeight: {
                                            const wanted = clipEffects.length > 0
                                                ? Theme.clipHeaderBandHeight * 1.6
                                                : Theme.clipHeaderBandHeight
                                            return Math.min(wanted, Math.max(0, height * 0.5))
                                        }

                                        y: Theme.clipSelectionRingWidth
                                        // Clamped: went negative at minimum zoom for
                                        // short clips. The transition region below
                                        // already clamped; this did not.
                                        width: Math.max(1, clipData.duration * root.pxPerSecond
                                                           - 2 * Theme.clipSelectionRingWidth)
                                        height: Math.max(0, parent.height - 2 * Theme.clipSelectionRingWidth)

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
                                            // Lightens on hover — previously nothing
                                            // in the clip reacted to the pointer.
                                            color: {
                                                const base = root.clipColor(
                                                    clipItem.trackType === "shape" ? "graphic" : clipItem.trackType)
                                                return clipMouse.containsMouse ? Qt.lighter(base, 1.15) : base
                                            }
                                            border.width: clipItem.effectDropTarget
                                                          ? Theme.borderWidthFocus
                                                          : (clipItem.selected ? Theme.clipSelectionRingWidth : 0)
                                            border.color: clipItem.effectDropTarget ? Theme.clipEffect : Theme.primary
                                            clip: true

                                            Behavior on color {
                                                ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                                            }
                                            Behavior on border.width {
                                                NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                                            }

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
                                                                   ? clipItem.headerBandHeight
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
                                                height: clipItem.headerBandHeight
                                                color: Theme.scrimColor
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
                                                        color: Theme.onMedia
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
                                                        color: Theme.panelSecondaryForeground
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
                                                // Clamped so the label width cannot
                                                // go negative on very narrow clips.
                                                anchors.leftMargin: Math.min(Theme.spacingLg, parent.width / 4)
                                                anchors.rightMargin: Math.min(Theme.spacingLg, parent.width / 4)
                                                spacing: 1

                                                Text {
                                                    width: parent.width
                                                    text: clipItem.trackType === "subtitle"
                                                          ? (clipItem.clipData.name
                                                             || qsTr("Subtitles"))
                                                          : (clipItem.clipData.textContent
                                                             || clipItem.clipData.name)
                                                    color: Theme.onMedia
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
                                                    color: Theme.panelSecondaryForeground
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
                                                anchors.topMargin: clipItem.headerBandHeight
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
                                            hoverEnabled: true
                                            // Insets make room for the trim handles,
                                            // but only while the clip is wide enough
                                            // to keep a usable drag region.
                                            anchors.leftMargin: clipItem.showTrimHandles ? 14 : 0
                                            anchors.rightMargin: clipItem.showTrimHandles ? 14 : 0
                                            enabled: !leftTrimMouse.pressed && !rightTrimMouse.pressed
                                                         && !fadeInMouse.pressed && !fadeOutMouse.pressed
                                            // A clip is dragged, not clicked.
                                            cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                            // Right-click opens the clip menu; the app
                                            // previously had no context menus at all.
                                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                                            drag.target: clipItem
                                            drag.axis: Drag.XAndYAxis
                                            drag.threshold: 8
                                            drag.minimumX: Theme.clipSelectionRingWidth
                                            drag.minimumY: -trackRow.height
                                            drag.maximumY: trackRow.height * 2
                                            property int originTrack: trackRow.trackIndex

                                            onPressed: (mouse) => {
                                                originTrack = trackRow.trackIndex
                                                if (mouse.button === Qt.RightButton) {
                                                    // Right-click selects, then opens the menu.
                                                    if (!clipItem.selected)
                                                        EditorState.selectClip(trackRow.trackIndex, modelData)
                                                    clipContextMenu.popup()
                                                    return
                                                }
                                                if ((mouse.modifiers & Qt.ShiftModifier) !== 0)
                                                    EditorState.addToSelection(trackRow.trackIndex, modelData)
                                                else
                                                    EditorState.selectClip(trackRow.trackIndex, modelData)
                                            }
                                            onClicked: (mouse) => {
                                                if (mouse.button === Qt.RightButton)
                                                    return
                                                if ((mouse.modifiers & Qt.ShiftModifier) !== 0)
                                                    EditorState.addToSelection(trackRow.trackIndex, modelData)
                                                else
                                                    EditorState.selectClip(trackRow.trackIndex, modelData)
                                            }

                                            // Surfaces actions that were previously
                                            // reachable only by unlabelled shortcut,
                                            // plus cutSelection which had no UI at all.
                                            ThemedContextMenu {
                                                id: clipContextMenu

                                                MenuItem {
                                                    text: qsTr("Split at playhead")
                                                    icon.name: Theme.icons.scissors
                                                    onTriggered: EditorState.splitAtPlayhead()
                                                }
                                                MenuSeparator { }
                                                MenuItem {
                                                    text: qsTr("Cut")
                                                    icon.name: Theme.icons.scissors
                                                    onTriggered: EditorState.cutSelection()
                                                }
                                                MenuItem {
                                                    text: qsTr("Copy")
                                                    icon.name: Theme.icons.copy
                                                    onTriggered: EditorState.copySelection()
                                                }
                                                MenuItem {
                                                    text: qsTr("Duplicate")
                                                    icon.name: Theme.icons.copyPlus
                                                    onTriggered: EditorState.duplicateSelectedClip()
                                                }
                                                MenuSeparator { }
                                                MenuItem {
                                                    text: qsTr("Delete")
                                                    icon.name: Theme.icons.trash
                                                    onTriggered: EditorState.deleteSelectedClip()
                                                }
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
                                            border.color: Theme.onMedia
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

                                                ThemedToolTip {
                                                    visible: fadeInMouse.pressed || fadeInMouse.containsMouse
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
                                            border.color: Theme.onMedia
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

                                                ThemedToolTip {
                                                    visible: fadeOutMouse.pressed || fadeOutMouse.containsMouse
                                                    text: qsTr("Fade out %1s").arg((clipItem.clipData.fadeOut || 0).toFixed(2))
                                                }
                                            }
                                        }

                                        // Trim handles sit above fade dots so edge drags resize the clip.
                                        Rectangle {
                                            id: leftTrimHandle
                                            width: Theme.clipTrimHandleWidth
                                            anchors.left: clipBackground.left
                                            anchors.top: clipBackground.top
                                            anchors.bottom: clipBackground.bottom
                                            visible: clipItem.showTrimHandles
                                            color: Theme.primary
                                            opacity: leftTrimMouse.pressed ? 1.0
                                                     : (leftTrimMouse.containsMouse ? 0.95 : 0.75)

                                            Behavior on opacity {
                                                NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                                            }

                                            ThemedToolTip {
                                                text: qsTr("Drag to trim the start")
                                                visible: leftTrimMouse.containsMouse && !leftTrimMouse.pressed
                                            }
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
                                            width: Theme.clipTrimHandleWidth
                                            anchors.right: clipBackground.right
                                            anchors.top: clipBackground.top
                                            anchors.bottom: clipBackground.bottom
                                            visible: clipItem.showTrimHandles
                                            color: Theme.primary
                                            opacity: rightTrimMouse.pressed ? 1.0
                                                     : (rightTrimMouse.containsMouse ? 0.95 : 0.75)

                                            Behavior on opacity {
                                                NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                                            }

                                            ThemedToolTip {
                                                text: qsTr("Drag to trim the end")
                                                visible: rightTrimMouse.containsMouse && !rightTrimMouse.pressed
                                            }
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
                                                    ctx.strokeStyle = Theme.onMedia
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
                                                color: Theme.onMedia
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontSizeTiny
                                                font.weight: Font.Bold
                                            }

                                            ThemedToolTip {
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
                        visible: opacity > 0
                        opacity: root.snapGuideSeconds >= 0 ? 1 : 0
                        x: root.snapGuideSeconds * root.pxPerSecond
                        y: Theme.timelineRulerHeight + Theme.timelineBookmarkRowHeight
                        width: Theme.borderWidth
                        height: root.totalTracksHeight()
                        color: Theme.snapGuide
                        z: 6

                        Behavior on opacity {
                            NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                        }

                        // Says what the clip snapped to, instead of leaving a bare
                        // unexplained line on screen.
                        Rectangle {
                            visible: parent.opacity > 0
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            height: snapLabel.implicitHeight + Theme.spacingSm
                            width: snapLabel.implicitWidth + Theme.spacingLg
                            radius: Theme.radiusXs
                            color: Theme.snapGuide

                            Text {
                                id: snapLabel
                                anchors.centerIn: parent
                                text: root.formatTime(root.snapGuideSeconds)
                                color: Theme.overlayColor
                                font.family: Theme.monoFontFamily
                                font.pixelSize: Theme.fontSizeTiny
                            }
                        }
                    }

                    // playhead ---------------------------------------------------------------
                    Item {
                        id: playhead
                        y: 0
                        // Above the pinned ruler (z: 2), otherwise the ruler's
                        // scrub area covers the handle and swallows the press
                        // before the drag can start.
                        z: 3
                        width: Theme.playheadLineWidth
                        height: Theme.timelineRulerHeight + Theme.timelineBookmarkRowHeight + root.totalTracksHeight()

                        Binding {
                            target: playhead
                            property: "x"
                            value: EditorState.playheadSeconds * root.pxPerSecond
                            when: !playheadDragArea.drag.active
                        }

                        // Follow the drag live so the preview scrubs with it,
                        // matching what dragging along the ruler already does.
                        onXChanged: {
                            if (playheadDragArea.drag.active)
                                EditorState.playheadSeconds = playhead.x / root.pxPerSecond
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
                        }

                        // Grab strip running the whole height of the line, so the
                        // playhead can be dragged from anywhere down the timeline
                        // rather than only by its handle. Clips take selection on
                        // press, so this band is kept as narrow as it can be while
                        // staying grabbable — it is dead to clip clicks.
                        MouseArea {
                            id: playheadDragArea
                            width: 7
                            height: parent.height
                            x: -(width - Theme.playheadLineWidth) / 2
                            y: 0
                            cursorShape: Qt.SizeHorCursor
                            preventStealing: true
                            drag.target: playhead
                            drag.axis: Drag.XAxis
                            drag.threshold: 0
                            drag.minimumX: 0
                            drag.maximumX: flick.contentWidth - Theme.playheadLineWidth
                            onReleased: EditorState.playheadSeconds = EditorState.snapTime(playhead.x / root.pxPerSecond)
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
