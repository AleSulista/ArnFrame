import QtQuick
import QtQuick.Controls.Basic
import Drift
import "components"

PanelFrame {
    id: root

    property real zoom: 1.0
    readonly property real pxPerSecond: Theme.pixelsPerSecondBase * zoom
    readonly property var tracks: EditorState.tracks
    readonly property real playheadSeconds: EditorState.playheadSeconds
    readonly property int selectedTrack: EditorState.selectedTrack
    readonly property int selectedClip: EditorState.selectedClip

    function trackHeight(type) {
        if (type === "video") return Theme.trackHeightVideo;
        if (type === "audio") return Theme.trackHeightAudio;
        return Theme.trackHeightText;
    }

    function clipColor(type) {
        if (type === "text") return Theme.clipText;
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
                    onClicked: EditorState.playing = !EditorState.playing
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

                IconButton { icon: Theme.icons.scissors; variant: "text"; onClicked: EditorState.splitAtPlayhead() }
                IconButton { icon: Theme.icons.alignLeft; variant: "text"; onClicked: EditorState.alignSelectedClipLeft() }
                IconButton { icon: Theme.icons.alignRight; variant: "text"; onClicked: EditorState.alignSelectedClipRight() }
                IconButton { icon: Theme.icons.linkTwo; variant: "text"; onClicked: EditorState.rippleEnabled = !EditorState.rippleEnabled }
                IconButton { icon: Theme.icons.copy; variant: "text"; onClicked: EditorState.duplicateSelectedClip() }
                IconButton { icon: Theme.icons.snowflake; variant: "text"; onClicked: EditorState.freezeFrameAtPlayhead() }
                IconButton { icon: Theme.icons.trash; variant: "text"; onClicked: EditorState.deleteSelectedClip() }

                Rectangle {
                    width: 1
                    height: 24
                    color: Theme.panelBorder
                    anchors.verticalCenter: parent.verticalCenter
                }

                IconButton {
                    icon: Theme.icons.bookmark
                    variant: "text"
                    onClicked: EditorState.addBookmark(EditorState.playheadSeconds, "Mark " + Math.round(EditorState.playheadSeconds))
                }
                IconButton {
                    icon: Theme.icons.layers
                    variant: "text"
                    onClicked: EditorState.undo()
                    buttonEnabled: EditorState.undoAvailable
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
                    active: EditorState.snapEnabled
                    onClicked: EditorState.snapEnabled = !EditorState.snapEnabled
                }
                IconButton {
                    id: rippleButton
                    icon: Theme.icons.linkTwo
                    variant: "text"
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
                    onClicked: root.zoom = Math.max(0.25, root.zoom - 0.25)
                }
                Slider {
                    id: zoomSlider
                    width: 112
                    anchors.verticalCenter: parent.verticalCenter
                    from: 0.25
                    to: 3.0
                    value: root.zoom
                    onMoved: root.zoom = value

                    background: Rectangle {
                        x: zoomSlider.leftPadding
                        y: zoomSlider.topPadding + zoomSlider.availableHeight / 2 - height / 2
                        width: zoomSlider.availableWidth
                        height: 4
                        radius: 2
                        color: Theme.panelMuted

                        Rectangle {
                            width: zoomSlider.visualPosition * parent.width
                            height: parent.height
                            radius: 2
                            color: Theme.primary
                        }
                    }

                    handle: Rectangle {
                        x: zoomSlider.leftPadding + zoomSlider.visualPosition * (zoomSlider.availableWidth - width)
                        y: zoomSlider.topPadding + zoomSlider.availableHeight / 2 - height / 2
                        width: 12
                        height: 12
                        radius: 6
                        color: Theme.primary
                        border.width: 2
                        border.color: Theme.primaryForeground
                    }
                }
                IconButton {
                    icon: Theme.icons.zoomIn
                    variant: "text"
                    onClicked: root.zoom = Math.min(3.0, root.zoom + 0.25)
                }
            }
        }

        // === ruler + track labels + tracks ================================================
        Row {
            width: parent.width
            height: parent.height - toolbar.height

            // --- fixed left label column --------------------------------------------
            Column {
                width: Theme.trackLabelsWidth
                height: parent.height

                Item { width: parent.width; height: Theme.timelineRulerHeight + Theme.timelineBookmarkRowHeight }

                Repeater {
                    model: root.tracks.length
                    delegate: Item {
                        width: Theme.trackLabelsWidth
                        height: root.trackHeight(root.tracks[index].type)

                        Rectangle {
                            anchors.right: parent.right
                            width: 1
                            height: parent.height
                            color: Theme.panelBorder
                        }

                        Row {
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 8

                            IconGlyph {
                                visible: root.tracks[index].type !== "text"
                                glyph: EditorState.trackMuted(index) ? Theme.icons.volumeOff : Theme.icons.volumeHigh
                                iconSize: 16
                                iconColor: EditorState.trackMuted(index) ? Theme.destructive : Theme.mutedForeground
                                anchors.verticalCenter: parent.verticalCenter

                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -4
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: EditorState.setTrackMuted(index, !EditorState.trackMuted(index))
                                }
                            }

                            IconGlyph {
                                glyph: EditorState.trackHidden(index) ? Theme.icons.eyeOff : Theme.icons.eye
                                iconSize: 16
                                iconColor: EditorState.trackHidden(index) ? Theme.destructive : Theme.mutedForeground
                                anchors.verticalCenter: parent.verticalCenter

                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -4
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: EditorState.setTrackHidden(index, !EditorState.trackHidden(index))
                                }
                            }

                            IconGlyph {
                                glyph: root.tracks[index].type === "video" ? Theme.icons.film
                                     : root.tracks[index].type === "audio" ? Theme.icons.music
                                     : Theme.icons.type
                                iconSize: 16
                                iconColor: Theme.mutedForeground
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }

            // --- scrollable ruler + tracks --------------------------------------------
            Flickable {
                id: flick
                width: parent.width - Theme.trackLabelsWidth
                height: parent.height
                contentWidth: Math.max(width, 16 * root.pxPerSecond)
                contentHeight: height
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.horizontal: AppScrollBar { }

                Item {
                    width: flick.contentWidth
                    height: flick.height

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
                            model: Math.ceil(flick.contentWidth / root.pxPerSecond) + 1
                            delegate: Item {
                                x: index * root.pxPerSecond
                                width: root.pxPerSecond
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
                                    text: {
                                        var m = Math.floor(index / 60);
                                        var s = index % 60;
                                        return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s;
                                    }
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
                                    keys: ["text/plain"]
                                    onDropped: (drop) => {
                                        const assetIndex = parseInt(drop.text)
                                        if (isNaN(assetIndex))
                                            return
                                        const atSeconds = drop.x / root.pxPerSecond
                                        EditorState.addClipFromAssetAt(assetIndex, trackRow.trackIndex, atSeconds)
                                    }
                                }

                                Repeater {
                                    model: root.tracks[trackRow.trackIndex].clips.length
                                    delegate: Item {
                                        id: clipItem
                                        property var clipData: root.tracks[trackRow.trackIndex].clips[modelData]
                                        property bool selected: root.selectedTrack === trackRow.trackIndex
                                                                  && root.selectedClip === modelData
                                        property string trackType: root.tracks[trackRow.trackIndex].type

                                        y: Theme.clipSelectionRingWidth
                                        width: clipData.duration * root.pxPerSecond - 2 * Theme.clipSelectionRingWidth
                                        height: parent.height - 2 * Theme.clipSelectionRingWidth

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
                                            anchors.fill: parent
                                            radius: Theme.radiusSm
                                            color: root.clipColor(clipItem.trackType)
                                            border.width: clipItem.selected ? Theme.clipSelectionRingWidth : 0
                                            border.color: Theme.primary
                                            clip: true

                                            Rectangle {
                                                id: leftTrimHandle
                                                width: 12
                                                anchors.left: parent.left
                                                anchors.top: parent.top
                                                anchors.bottom: parent.bottom
                                                visible: clipItem.selected
                                                color: Theme.primary
                                                opacity: leftTrimMouse.pressed ? 1.0 : 0.75
                                                z: 3

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
                                                anchors.right: parent.right
                                                anchors.top: parent.top
                                                anchors.bottom: parent.bottom
                                                visible: clipItem.selected
                                                color: Theme.primary
                                                opacity: rightTrimMouse.pressed ? 1.0 : 0.75
                                                z: 3

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

                                            ClipFilmstrip {
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.top: parent.top
                                                anchors.bottom: parent.bottom
                                                anchors.bottomMargin: (clipItem.trackType === "video"
                                                                       || clipItem.trackType === "audio") ? 20 : 0
                                                visible: clipItem.clipData.filmstripPath
                                                         && clipItem.clipData.filmstripPath.length > 0
                                                         && (clipItem.trackType === "video"
                                                             || clipItem.clipData.kind === "image")
                                                filmstripPath: clipItem.clipData.filmstripPath
                                                z: 0
                                            }

                                            Rectangle {
                                                visible: clipItem.trackType === "video" || clipItem.trackType === "audio"
                                                width: parent.width
                                                height: 20
                                                color: "#00000066"

                                                Text {
                                                    anchors.left: parent.left
                                                    anchors.leftMargin: 6
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    text: clipItem.clipData.name
                                                    color: "#ffffffbf"
                                                    font.pixelSize: Theme.fontSizeTiny
                                                    font.family: Theme.fontFamily
                                                    elide: Text.ElideRight
                                                    width: parent.width - 12
                                                }
                                            }

                                            Text {
                                                visible: clipItem.trackType === "text"
                                                anchors.left: parent.left
                                                anchors.leftMargin: 8
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: clipItem.clipData.textContent || clipItem.clipData.name
                                                color: "white"
                                                font.pixelSize: Theme.fontSizeXs
                                                font.family: Theme.fontFamily
                                                width: parent.width - 16
                                                elide: Text.ElideRight
                                            }

                                            Canvas {
                                                id: waveformCanvas
                                                visible: clipItem.trackType === "audio"
                                                property var peaks: clipItem.clipData.path
                                                              ? EditorState.waveformPeaks(clipItem.clipData.path)
                                                              : []
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.top: parent.top
                                                anchors.topMargin: 20
                                                anchors.bottom: parent.bottom
                                                onPeaksChanged: requestPaint()
                                                onPaint: {
                                                    var ctx = getContext("2d");
                                                    ctx.clearRect(0, 0, width, height);
                                                    if (!peaks || peaks.length === 0)
                                                        return;
                                                    ctx.strokeStyle = Theme.waveformColor;
                                                    ctx.lineWidth = 1;
                                                    ctx.beginPath();
                                                    var mid = height / 2;
                                                    var step = width / peaks.length;
                                                    for (var i = 0; i < peaks.length; i++) {
                                                        var amp = peaks[i] * mid * 0.9;
                                                        var px = i * step;
                                                        ctx.moveTo(px, mid - amp);
                                                        ctx.lineTo(px, mid + amp);
                                                    }
                                                    ctx.stroke();
                                                }
                                            }
                                        }

                                        MouseArea {
                                            id: clipMouse
                                            anchors.fill: parent
                                            anchors.leftMargin: clipItem.selected ? 14 : 0
                                            anchors.rightMargin: clipItem.selected ? 14 : 0
                                            enabled: !leftTrimMouse.pressed && !rightTrimMouse.pressed
                                            cursorShape: Qt.PointingHandCursor
                                            drag.target: clipItem
                                            drag.axis: Drag.XAndYAxis
                                            drag.minimumX: Theme.clipSelectionRingWidth
                                            drag.minimumY: -trackRow.height
                                            drag.maximumY: trackRow.height * 2
                                            property int originTrack: trackRow.trackIndex

                                            onPressed: {
                                                originTrack = trackRow.trackIndex
                                                EditorState.selectClip(trackRow.trackIndex, modelData)
                                            }
                                            onReleased: {
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
                                    }
                                }
                            }
                        }
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
    }

    Connections {
        target: EditorState
        function onPlayheadSecondsChanged() {
            if (EditorState.playing)
                root.ensurePlayheadVisible()
        }
    }
}
