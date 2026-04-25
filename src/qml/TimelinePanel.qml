import QtQuick
import QtQuick.Controls.Basic
import Drift
import "components"

PanelFrame {
    id: root

    property real zoom: 1.0
    readonly property real pxPerSecond: Theme.pixelsPerSecondBase * zoom
    property real playheadSeconds: 3.2
    property var selectedClip: ({ track: 0, clip: 0 })

    property var tracks: [
        {
            type: "video",
            clips: [
                { name: "beach-sunset.mp4", start: 0, duration: 8 },
                { name: "b-roll-city.mp4", start: 8.2, duration: 4 }
            ]
        },
        {
            type: "text",
            clips: [
                { name: "Title", start: 2, duration: 3 }
            ]
        },
        {
            type: "audio",
            clips: [
                { name: "background-music.mp3", start: 0, duration: 10 }
            ]
        }
    ]

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

                IconButton { icon: Theme.icons.scissors; variant: "text" }
                IconButton { icon: Theme.icons.alignLeft; variant: "text" }
                IconButton { icon: Theme.icons.alignRight; variant: "text" }
                IconButton { icon: Theme.icons.linkTwo; variant: "text" }
                IconButton { icon: Theme.icons.copy; variant: "text" }
                IconButton { icon: Theme.icons.snowflake; variant: "text"; buttonEnabled: false }
                IconButton { icon: Theme.icons.trash; variant: "text" }

                Rectangle {
                    width: 1
                    height: 24
                    color: Theme.panelBorder
                    anchors.verticalCenter: parent.verticalCenter
                }

                IconButton { icon: Theme.icons.bookmark; variant: "text" }
                IconButton { icon: Theme.icons.layers; variant: "text" }
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
                    active: true
                }
                IconButton {
                    id: rippleButton
                    icon: Theme.icons.linkTwo
                    variant: "text"
                    active: false
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
                                glyph: muted ? Theme.icons.volumeOff : Theme.icons.volumeHigh
                                iconSize: 16
                                iconColor: muted ? Theme.destructive : Theme.mutedForeground
                                property bool muted: false
                                anchors.verticalCenter: parent.verticalCenter

                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -4
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: parent.muted = !parent.muted
                                }
                            }

                            IconGlyph {
                                glyph: hidden ? Theme.icons.eyeOff : Theme.icons.eye
                                iconSize: 16
                                iconColor: hidden ? Theme.destructive : Theme.mutedForeground
                                property bool hidden: false
                                anchors.verticalCenter: parent.verticalCenter

                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -4
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: parent.hidden = !parent.hidden
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

                                Repeater {
                                    model: root.tracks[trackRow.trackIndex].clips.length
                                    delegate: Item {
                                        id: clipItem
                                        property var clipData: root.tracks[trackRow.trackIndex].clips[modelData]
                                        property bool selected: root.selectedClip.track === trackRow.trackIndex && root.selectedClip.clip === modelData
                                        property string trackType: root.tracks[trackRow.trackIndex].type

                                        x: clipData.start * root.pxPerSecond + Theme.clipSelectionRingWidth
                                        y: Theme.clipSelectionRingWidth
                                        width: clipData.duration * root.pxPerSecond - 2 * Theme.clipSelectionRingWidth
                                        height: parent.height - 2 * Theme.clipSelectionRingWidth

                                        Rectangle {
                                            anchors.fill: parent
                                            radius: Theme.radiusSm
                                            color: root.clipColor(clipItem.trackType)
                                            border.width: clipItem.selected ? Theme.clipSelectionRingWidth : 0
                                            border.color: Theme.primary
                                            clip: true

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
                                                text: clipItem.clipData.name
                                                color: "white"
                                                font.pixelSize: Theme.fontSizeXs
                                                font.family: Theme.fontFamily
                                            }

                                            Canvas {
                                                visible: clipItem.trackType === "audio"
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.top: parent.top
                                                anchors.topMargin: 20
                                                anchors.bottom: parent.bottom
                                                onPaint: {
                                                    var ctx = getContext("2d");
                                                    ctx.clearRect(0, 0, width, height);
                                                    ctx.strokeStyle = Theme.waveformColor;
                                                    ctx.lineWidth = 1;
                                                    ctx.beginPath();
                                                    var mid = height / 2;
                                                    for (var px = 0; px < width; px += 3) {
                                                        var amp = (Math.sin(px * 0.3) * 0.5 + Math.sin(px * 0.13) * 0.3) * mid * 0.8;
                                                        ctx.moveTo(px, mid - amp);
                                                        ctx.lineTo(px, mid + amp);
                                                    }
                                                    ctx.stroke();
                                                }
                                            }
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.selectedClip = { track: trackRow.trackIndex, clip: modelData }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // playhead ---------------------------------------------------------------
                    Item {
                        id: playhead
                        x: root.playheadSeconds * root.pxPerSecond
                        y: 0
                        width: Theme.playheadLineWidth
                        height: Theme.timelineRulerHeight + Theme.timelineBookmarkRowHeight + root.totalTracksHeight()

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
                                anchors.fill: parent
                                anchors.margins: -4
                                cursorShape: Qt.SizeHorCursor
                                drag.target: playhead
                                drag.axis: Drag.XAxis
                                drag.minimumX: 0
                                drag.maximumX: flick.contentWidth - Theme.playheadLineWidth
                                onReleased: root.playheadSeconds = playhead.x / root.pxPerSecond
                            }
                        }
                    }
                }
            }
        }
    }
}
