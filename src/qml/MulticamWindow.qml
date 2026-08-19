import QtQuick
import QtQuick.Window
import Drift 1.0
import "components"

// Multicam switching surface: every camera at the current time, side by side, with one click to
// cut the program over to whichever one is worth watching.
//
// This window holds no state of its own. Every value below is a binding onto the EditorState
// singleton the main window uses — the same object, not a copy — so the playhead, the transport
// and the timeline stay in step in both directions without anything having to synchronise them.
// The one edit it makes, switchMulticamAngle, goes through the shared undo stack like any other.
Window {
    id: root

    width: 1180
    height: 760
    minimumWidth: 720
    minimumHeight: 520
    title: qsTr("Multicam")
    color: Theme.appBackground

    readonly property var angles: EditorState.multicamAngles
    readonly property real currentSeconds: EditorState.playheadSeconds
    readonly property real durationSeconds: EditorState.durationSeconds

    readonly property int projectFps: {
        void EditorState.tracks
        const fps = EditorState.projectFps()
        return fps > 0 ? fps : 30
    }

    // Same shape as the preview panel's readout, so the two windows never disagree about what
    // time it is.
    function formatTimecode(seconds) {
        const fps = root.projectFps
        const totalFrames = Math.round(seconds * fps)
        const h = Math.floor(totalFrames / (fps * 3600))
        const m = Math.floor(totalFrames / (fps * 60)) % 60
        const s = Math.floor(totalFrames / fps) % 60
        const f = totalFrames % fps
        function pad(n) { return n.toString().padStart(2, "0") }
        return pad(h) + ":" + pad(m) + ":" + pad(s) + ":" + pad(f)
    }

    function openSession() {
        EditorState.beginMulticamSession()
        root.show()
        root.raise()
        root.requestActivate()
    }

    // Tile decoding costs a decode per angle per refresh, so it stops with the window rather
    // than running on behind it.
    onClosing: EditorState.endMulticamSession()

    // Number keys switch angles, the way a vision mixer's bus does. WindowShortcut context
    // (the default) keeps them out of the main window, where digits mean nothing yet.
    Repeater {
        model: 9
        Item {
            id: keyHost
            required property int index
            width: 0
            height: 0
            Shortcut {
                sequence: String(keyHost.index + 1)
                enabled: keyHost.index < root.angles.length
                onActivated: EditorState.switchMulticamAngle(keyHost.index)
            }
        }
    }

    Column {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingLg

        // ----- Header: which track is the program, which are the angles ---------------------
        Item {
            id: header
            width: parent.width
            height: Theme.controlHeight

            Row {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.spacingLg

                ThemedLabel {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Program")
                    size: "sm"
                }

                ThemedComboBox {
                    id: programBox
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(140, widestContentWidth)
                    textRole: "label"
                    valueRole: "trackIndex"
                    // Rebuilt whenever the track list changes, so a track that has been removed
                    // cannot linger here as a choice that no longer exists.
                    model: {
                        void EditorState.tracks
                        return EditorState.multicamCandidateTracks()
                    }
                    onActivated: EditorState.setMulticamProgramTrack(currentValue)

                    // Assigning currentIndex during a pick would fight the selection, so the
                    // controller's value is only reasserted while the popup is closed — the same
                    // arrangement the scrub slider below uses against the playhead.
                    Binding on currentIndex {
                        when: !programBox.popup.visible
                        value: {
                            void EditorState.multicamProgramTrack
                            for (let i = 0; i < programBox.count; ++i) {
                                if (programBox.valueAt(i) === EditorState.multicamProgramTrack)
                                    return i
                            }
                            return -1
                        }
                    }
                }
            }

            ThemedSwitch {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Switch audio too")
                tooltip: qsTr("Off: cuts change the picture only, so a separate audio bed plays through them.")
                checked: EditorState.multicamSwitchAudio
                onToggled: EditorState.multicamSwitchAudio = checked
            }
        }

        // ----- Angle grid + program monitor -------------------------------------------------
        Row {
            id: body
            width: parent.width
            height: parent.height - header.height - transport.height - Theme.spacingLg * 2
            spacing: Theme.spacingLg

            Item {
                id: gridPane
                width: (parent.width - Theme.spacingLg) * 0.62
                height: parent.height

                // Two different situations, and they need different answers: an empty timeline
                // can be scaffolded from the media bin in one click, but once there are clips the
                // arrangement is the user's and all this can do is say what it is looking for.
                EmptyState {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - Theme.spacing3xl, 340)
                    visible: root.angles.length === 0
                    glyph: Theme.icons.grid
                    title: EditorState.multicamCanSetUp
                           ? qsTr("Ready to set up")
                           : qsTr("No angles to switch between")
                    hint: EditorState.multicamCanSetUp
                          ? qsTr("Your imported videos will go on a track each, with an empty program track above them to cut into.")
                          : qsTr("Put each camera on its own video track. Every video track except the program becomes an angle.")
                    actionText: EditorState.multicamCanSetUp ? qsTr("Set up from my media") : ""
                    actionVariant: "primary"
                    onActionTriggered: EditorState.setUpMulticamFromAssets()
                }

                Grid {
                    id: angleGrid
                    anchors.fill: parent
                    visible: root.angles.length > 0
                    spacing: Theme.spacingMd
                    // Keeps tiles roughly square-ish however many cameras there are: 1 wide for
                    // one angle, 2 wide up to four, 3 wide beyond that.
                    columns: root.angles.length <= 1 ? 1 : (root.angles.length <= 4 ? 2 : 3)

                    readonly property int rowsCount: Math.max(1, Math.ceil(root.angles.length / columns))
                    readonly property real cellWidth:
                        Math.max(1, (width - spacing * (columns - 1)) / columns)
                    readonly property real cellHeight:
                        Math.max(1, (height - spacing * (rowsCount - 1)) / rowsCount)

                    Repeater {
                        model: root.angles

                        Rectangle {
                            id: tile
                            required property int index
                            required property var modelData

                            width: angleGrid.cellWidth
                            height: angleGrid.cellHeight
                            color: Theme.panelBackground
                            radius: Theme.radiusMd
                            border.width: modelData.active ? 2 : Theme.borderWidth
                            border.color: modelData.active ? Theme.primary
                                                           : (tileMouse.containsMouse ? Theme.panelForeground
                                                                                      : Theme.panelBorder)
                            clip: true

                            Behavior on border.color {
                                ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                            }

                            Image {
                                anchors.fill: parent
                                anchors.margins: Theme.spacingXs
                                fillMode: Image.PreserveAspectFit
                                // The pixels behind this URL change on every refresh, so the
                                // revision is what makes QML fetch them again.
                                cache: false
                                asynchronous: true
                                source: modelData.hasClip
                                        ? "image://multicam/" + tile.index + "?rev=" + EditorState.multicamRevision
                                        : ""
                            }

                            ThemedLabel {
                                anchors.centerIn: parent
                                visible: !tile.modelData.hasClip
                                horizontalAlignment: Text.AlignHCenter
                                text: qsTr("Nothing here")
                            }

                            // Number badge — the key that switches to this angle.
                            Rectangle {
                                x: Theme.spacingSm
                                y: Theme.spacingSm
                                width: badge.width + Theme.spacingMd
                                height: badge.height + Theme.spacingXs
                                radius: Theme.radiusSm
                                color: tile.modelData.active ? Theme.primary : Theme.scrimStrong

                                Text {
                                    id: badge
                                    anchors.centerIn: parent
                                    text: tile.index + 1
                                    color: tile.modelData.active ? Theme.primaryForeground : "#ffffff"
                                    font.family: Theme.monoFontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    font.weight: Font.Medium
                                }
                            }

                            Text {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: Theme.spacingSm
                                text: tile.modelData.clipName.length > 0 ? tile.modelData.clipName
                                                                         : tile.modelData.label
                                elide: Text.ElideMiddle
                                color: "#ffffff"
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                style: Text.Outline
                                styleColor: Theme.overlayColor
                            }

                            MouseArea {
                                id: tileMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: EditorState.switchMulticamAngle(tile.index)
                            }

                            ThemedToolTip {
                                visible: tileMouse.containsMouse
                                text: qsTr("Switch the program to %1 (key %2)")
                                      .arg(tile.modelData.label).arg(tile.index + 1)
                            }
                        }
                    }
                }
            }

            // Program monitor — the very texture the main preview is showing, wrapped again
            // rather than composited a second time.
            Rectangle {
                id: programPane
                width: body.width - gridPane.width - Theme.spacingLg
                height: parent.height
                color: Theme.overlayColor
                radius: Theme.radiusMd
                border.width: Theme.borderWidth
                border.color: Theme.border
                clip: true

                PreviewItem {
                    id: programPreview
                    anchors.fill: parent
                    anchors.margins: Theme.spacingXs
                    // Deliberately does not call setPreviewRenderSize: the compositor renders one
                    // frame for the whole app, and its size belongs to the main preview panel.
                }

                Connections {
                    target: EditorState.playback
                    function onCurrentFrameChanged() {
                        programPreview.textureSize = EditorState.playback.previewTextureSize
                        programPreview.textureId = EditorState.playback.previewTextureId
                    }
                }

                Component.onCompleted: {
                    programPreview.textureSize = EditorState.playback.previewTextureSize
                    programPreview.textureId = EditorState.playback.previewTextureId
                }

                ThemedLabel {
                    anchors.centerIn: parent
                    visible: !EditorState.playback.hasFrame
                    text: qsTr("No clip at the current time")
                }

                Text {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: Theme.spacingSm
                    text: qsTr("Program")
                    color: "#ffffff"
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    style: Text.Outline
                    styleColor: Theme.overlayColor
                }
            }
        }

        // ----- Transport + program lane -----------------------------------------------------
        Column {
            id: transport
            width: parent.width
            height: transportRow.height + laneStrip.height + Theme.spacingSm
            spacing: Theme.spacingSm

            Row {
                id: transportRow
                width: parent.width
                height: Theme.controlHeight
                spacing: Theme.spacingSm

                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    glyph: Theme.icons.stepBack
                    tooltip: qsTr("Previous frame")
                    onClicked: EditorState.stepFrames(-1)
                }

                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    glyph: EditorState.playing ? Theme.icons.pause : Theme.icons.play
                    tooltip: EditorState.playing ? qsTr("Pause") : qsTr("Play")
                    onClicked: EditorState.togglePlayback()
                }

                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    glyph: Theme.icons.stepForward
                    tooltip: qsTr("Next frame")
                    onClicked: EditorState.stepFrames(1)
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.formatTimecode(root.currentSeconds) + "  /  "
                          + root.formatTimecode(root.durationSeconds)
                    color: Theme.panelForeground
                    font.family: Theme.monoFontFamily
                    font.pixelSize: Theme.fontSizeSm
                }

                ThemedSlider {
                    id: scrubSlider
                    label: qsTr("Seek")
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(80, transportRow.width - Theme.iconButtonSize * 3
                                    - 200 - Theme.spacingSm * 5)
                    from: 0
                    // Never a zero-width range: an empty project would make the handle jump.
                    to: Math.max(0.001, root.durationSeconds)
                    valueFormatter: function (v) { return root.formatTimecode(v) }

                    onMoved: EditorState.playheadSeconds = value

                    // Dragging assigns `value` directly, which would clobber a plain binding.
                    // Reasserting only while released lets playback drive the handle without
                    // fighting the drag — the same arrangement the preview panel's scrub uses.
                    Binding on value {
                        when: !scrubSlider.pressed
                        value: root.currentSeconds
                    }
                }
            }

            // Read-only view of the program track, so the cuts a switch just made are visible
            // without going back to the main window. Derived from EditorState.tracks on every
            // change — there is no copy of the timeline here.
            Item {
                id: laneStrip
                width: parent.width
                height: 22

                readonly property var programClips: {
                    void EditorState.tracks
                    const index = EditorState.multicamProgramTrack
                    if (index < 0 || index >= EditorState.tracks.length)
                        return []
                    return EditorState.tracks[index].clips || []
                }
                readonly property real span: Math.max(0.001, root.durationSeconds)

                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radiusSm
                    color: Theme.panelMuted
                }

                Repeater {
                    model: laneStrip.programClips

                    Rectangle {
                        required property var modelData

                        x: laneStrip.width * (modelData.start / laneStrip.span)
                        width: Math.max(1, laneStrip.width * (modelData.duration / laneStrip.span))
                        height: laneStrip.height
                        radius: Theme.radiusSm
                        color: Theme.panelSecondaryBg
                        border.width: Theme.borderWidth
                        border.color: Theme.panelBorder

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.spacingXs
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - Theme.spacingSm
                            text: modelData.name
                            elide: Text.ElideRight
                            color: Theme.panelForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                        }
                    }
                }

                // Playhead.
                Rectangle {
                    width: 2
                    height: parent.height
                    x: Math.round(laneStrip.width * (root.currentSeconds / laneStrip.span)) - 1
                    color: Theme.primary
                }
            }
        }
    }
}
