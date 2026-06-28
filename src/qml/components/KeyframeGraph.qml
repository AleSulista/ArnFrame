import QtQuick
import QtQuick.Controls.Basic
import Drift

// Keyframe strip aligned with the timeline: left gutter matches track labels,
// graph scroll + playhead share the timeline's contentX / pxPerSecond.
Item {
    id: root

    property real pxPerSecond: 50
    property real contentX: 0
    property real contentWidth: 800
    property real labelsWidth: Theme.trackLabelsWidth
    property string propertiesTab: ""

    readonly property bool hasClip: EditorState.selectedTrack >= 0 && EditorState.selectedClip >= 0
    readonly property var clip: {
        void EditorState.selectedClipData
        return EditorState.selectedClipData
    }
    readonly property string prop: EditorState.keyframeGraphProperty

    // Spelled-out names for the one-to-three character property chips.
    function propertyLabel(id) {
        switch (id) {
        case "x": return qsTr("X position")
        case "y": return qsTr("Y position")
        case "width": return qsTr("Width")
        case "height": return qsTr("Height")
        case "rotation": return qsTr("Rotation")
        case "opacity": return qsTr("Opacity")
        case "volume": return qsTr("Volume")
        }
        return id
    }
    // A preview move emits tracksChanged, which would re-evaluate `points` and
    // make the Repeater destroy the delegate owning the active DragHandler —
    // killing the grab on the first mouse move. Freeze the model while dragging
    // and track the in-flight key separately.
    property bool draggingKey: false
    property var frozenPoints: []
    property int dragIndex: -1
    property real dragSeconds: 0
    property real dragValue: 0

    readonly property var points: {
        void EditorState.selectedClipData
        void EditorState.tracks
        if (draggingKey)
            return frozenPoints
        if (!hasClip)
            return []
        return EditorState.clipKeyframes(EditorState.selectedTrack, EditorState.selectedClip, prop) || []
    }
    readonly property real clipStart: clip.start || 0
    readonly property real clipDuration: Math.max(0.1, clip.duration || 1)
    readonly property real valueMin: {
        void points
        if (prop === "opacity" || prop === "volume")
            return 0
        if (prop === "rotation")
            return -180
        let lo = Infinity
        for (let i = 0; i < points.length; ++i)
            lo = Math.min(lo, Number(points[i].value))
        if (!isFinite(lo))
            return 0
        return lo - Math.max(1, Math.abs(lo) * 0.1)
    }
    readonly property real valueMax: {
        void points
        if (prop === "opacity")
            return 1
        if (prop === "volume")
            return 2
        if (prop === "rotation")
            return 180
        let hi = -Infinity
        for (let i = 0; i < points.length; ++i)
            hi = Math.max(hi, Number(points[i].value))
        if (!isFinite(hi))
            return 100
        return hi + Math.max(1, Math.abs(hi) * 0.1)
    }

    height: visible ? 88 : 0
    visible: {
        if (propertiesTab !== "transform")
            return false
        if (!hasClip || !clip)
            return false
        if (prop === "volume")
            return clip.kind === "audio" || clip.kind === "video"
        return clip.kind !== "audio"
    }

    // Absolute timeline X — same mapping as clips on the track.
    function xForSeconds(seconds) {
        return seconds * pxPerSecond
    }
    function secondsForX(x) {
        return x / pxPerSecond
    }
    function yForValue(v) {
        const span = Math.max(1e-6, valueMax - valueMin)
        return graph.height - ((v - valueMin) / span) * graph.height
    }
    function valueForY(y) {
        const span = Math.max(1e-6, valueMax - valueMin)
        const t = (graph.height - y) / Math.max(1, graph.height)
        return valueMin + t * span
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.panelBackground
        border.width: 1
        border.color: Theme.panelBorder
    }

    Row {
        anchors.fill: parent

        // Left gutter — same width as track labels so the graph lines up.
        Item {
            width: root.labelsWidth
            height: parent.height

            Column {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 4

                Text {
                    text: qsTr("Keys")
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    font.weight: Font.Medium
                }

                Flow {
                    width: parent.width
                    spacing: 3
                    Repeater {
                        model: [
                            { id: "x", label: "X" },
                            { id: "y", label: "Y" },
                            { id: "width", label: "W" },
                            { id: "height", label: "H" },
                            { id: "rotation", label: "°" },
                            { id: "opacity", label: "Op" },
                            { id: "volume", label: "Vol" }
                        ]
                        delegate: ThemedChip {
                            required property var modelData
                            text: modelData.label
                            chipHeight: 18
                            horizontalPadding: 3
                            tooltip: root.propertyLabel(modelData.id)
                            selected: root.prop === modelData.id
                            onClicked: EditorState.keyframeGraphProperty = modelData.id
                        }
                    }
                }

                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: points.length === 0 ? qsTr("No keys") : (points.length + qsTr(" key(s)"))
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }
            }

            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: Theme.panelBorder
            }
        }

        // Timeline-aligned viewport (scrolls with the tracks below).
        Item {
            id: viewport
            width: parent.width - root.labelsWidth
            height: parent.height
            clip: true

            Item {
                id: content
                x: -root.contentX
                width: Math.max(viewport.width + root.contentX, root.contentWidth)
                height: parent.height

                Item {
                    id: graph
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.topMargin: 6
                    anchors.bottomMargin: 6

                    // Selected clip's time span — sits under the same region as the clip.
                    Rectangle {
                        x: root.xForSeconds(root.clipStart)
                        width: Math.max(2, root.clipDuration * root.pxPerSecond)
                        height: parent.height
                        color: Theme.panelAccent
                        opacity: 0.55
                        radius: 2
                    }

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        x: root.xForSeconds(root.clipStart)
                        width: Math.max(2, root.clipDuration * root.pxPerSecond)
                        height: 1
                        color: Theme.panelBorder
                    }

                    // Playhead — same absolute X as the timeline playhead.
                    Rectangle {
                        x: root.xForSeconds(EditorState.playheadSeconds) - Theme.playheadLineWidth / 2
                        width: Theme.playheadLineWidth
                        height: parent.height
                        color: Theme.primary
                        z: 3
                    }

                    Canvas {
                        id: curveCanvas
                        anchors.fill: parent
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            if (!points || points.length < 2)
                                return
                            const live = points.map(function (p, i) {
                                return i === root.dragIndex
                                    ? { seconds: root.dragSeconds, value: root.dragValue }
                                    : p
                            })
                            const sorted = live.sort((a, b) => a.seconds - b.seconds)
                            ctx.strokeStyle = String(Theme.primary)
                            ctx.lineWidth = 1.5
                            ctx.beginPath()
                            for (let i = 0; i < sorted.length; ++i) {
                                const px = root.xForSeconds(sorted[i].seconds)
                                const py = root.yForValue(sorted[i].value)
                                if (i === 0)
                                    ctx.moveTo(px, py)
                                else
                                    ctx.lineTo(px, py)
                            }
                            ctx.stroke()
                        }
                        Connections {
                            target: root
                            function onPointsChanged() { curveCanvas.requestPaint() }
                            function onValueMinChanged() { curveCanvas.requestPaint() }
                            function onValueMaxChanged() { curveCanvas.requestPaint() }
                            function onPxPerSecondChanged() { curveCanvas.requestPaint() }
                            function onContentXChanged() { curveCanvas.requestPaint() }
                            function onDragSecondsChanged() { curveCanvas.requestPaint() }
                            function onDragValueChanged() { curveCanvas.requestPaint() }
                        }
                    }

                    Repeater {
                        model: root.points
                        delegate: Rectangle {
                            id: keyDot
                            required property var modelData
                            required property int index
                            width: 10
                            height: 10
                            rotation: 45
                            radius: 1
                            color: Theme.primary
                            border.width: 1
                            border.color: "#ffffff"
                            x: root.xForSeconds(modelData.seconds) - width / 2 + dragDx
                            y: root.yForValue(modelData.value) - height / 2 + dragDy
                            z: 2

                            // Offsets rather than direct x/y writes, which would
                            // clobber the bindings above for good.
                            property real dragDx: 0
                            property real dragDy: 0
                            property real editSeconds: modelData.seconds

                            DragHandler {
                                target: null
                                onActiveChanged: {
                                    if (active) {
                                        keyDot.editSeconds = keyDot.modelData.seconds
                                        root.frozenPoints = root.points
                                        root.dragIndex = keyDot.index
                                        root.dragSeconds = keyDot.modelData.seconds
                                        root.dragValue = keyDot.modelData.value
                                        root.draggingKey = true
                                        EditorState.beginPreviewDrag(qsTr("Move keyframe"))
                                    } else {
                                        EditorState.commitPreviewDrag()
                                        root.draggingKey = false
                                        root.dragIndex = -1
                                        keyDot.dragDx = 0
                                        keyDot.dragDy = 0
                                    }
                                }
                                onTranslationChanged: {
                                    if (!active)
                                        return
                                    const baseSec = keyDot.modelData.seconds
                                    const baseVal = keyDot.modelData.value
                                    const newSec = Math.max(
                                        root.clipStart,
                                        Math.min(root.clipStart + root.clipDuration,
                                                 baseSec + translation.x / root.pxPerSecond))
                                    const newVal = Math.max(
                                        root.valueMin,
                                        Math.min(root.valueMax,
                                                 root.valueForY(root.yForValue(baseVal) + translation.y)))
                                    EditorState.previewMoveClipKeyframe(
                                        EditorState.selectedTrack, EditorState.selectedClip,
                                        root.prop, keyDot.editSeconds, newSec, newVal)
                                    keyDot.editSeconds = newSec
                                    keyDot.dragDx = root.xForSeconds(newSec) - root.xForSeconds(baseSec)
                                    keyDot.dragDy = root.yForValue(newVal) - root.yForValue(baseVal)
                                    root.dragSeconds = newSec
                                    root.dragValue = newVal
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
