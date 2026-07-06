import QtQuick
import QtQuick.Controls.Basic
import Drift

// Keyframe strip aligned with the timeline: left gutter matches track labels,
// graph scroll + playhead share the timeline's contentX / pxPerSecond.
//
// The strip is a *mirror* of the inspector's property selection, not its own
// picker: it shows one color-coded series per property selected over in the
// Transform or Effects panel. The selection accumulates — clicking or editing a
// second property adds its curve rather than replacing the first — so clicking a
// chip here is the way back to a single series. Deselecting everything collapses
// the strip to nothing.
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

    // Effect parameters are addressed as "fx.<effectIndex>.<paramKey>"; the label, range and
    // color all come from the effect's own param spec on the selected clip.
    function effectParam(id) {
        if (!clip || !clip.effects || id.substring(0, 3) !== "fx.")
            return null
        const dot = id.indexOf(".", 3)
        if (dot < 0)
            return null
        const effectIndex = parseInt(id.substring(3, dot))
        const key = id.substring(dot + 1)
        if (isNaN(effectIndex) || effectIndex < 0 || effectIndex >= clip.effects.length)
            return null
        const effect = clip.effects[effectIndex]
        const params = effect.params || []
        for (let i = 0; i < params.length; ++i) {
            if (params[i].key === key)
                return { effect: effect, param: params[i] }
        }
        return null
    }

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
        const fx = effectParam(id)
        if (fx)
            return fx.effect.label + " · " + fx.param.label
        return id
    }
    function chipLabel(id) {
        switch (id) {
        case "x": return "X"
        case "y": return "Y"
        case "width": return "W"
        case "height": return "H"
        case "rotation": return "°"
        case "opacity": return "Op"
        case "volume": return "Vol"
        }
        const fx = effectParam(id)
        if (fx)
            return fx.param.label.substring(0, 3)
        return id
    }
    // Volume only exists on clips that carry audio; the rest are visual.
    function supportsProperty(id) {
        if (!clip)
            return false
        if (id === "volume")
            return clip.kind === "audio" || clip.kind === "video"
        if (id.substring(0, 3) === "fx.")
            return clip.kind !== "audio" && effectParam(id) !== null
        return clip.kind !== "audio"
    }

    // Each series is normalized against its own value range, so curves in wildly
    // different units (px vs. 0–1 opacity) stay comparable when overlaid.
    function valueRangeFor(id, pts) {
        if (id === "opacity")
            return { min: 0, max: 1 }
        if (id === "volume")
            return { min: 0, max: 2 }
        if (id === "rotation")
            return { min: -180, max: 180 }
        const fx = effectParam(id)
        if (fx)
            return { min: fx.param.min, max: fx.param.max }
        let lo = Infinity
        let hi = -Infinity
        for (let i = 0; i < pts.length; ++i) {
            const v = Number(pts[i].value)
            lo = Math.min(lo, v)
            hi = Math.max(hi, v)
        }
        if (!isFinite(lo))
            lo = 0
        if (!isFinite(hi))
            hi = 100
        return {
            min: lo - Math.max(1, Math.abs(lo) * 0.1),
            max: hi + Math.max(1, Math.abs(hi) * 0.1)
        }
    }

    // A preview move emits tracksChanged, which would re-evaluate `series` and
    // make the Repeater destroy the delegate owning the active DragHandler —
    // killing the grab on the first mouse move. Freeze the model while dragging
    // and track the in-flight key separately.
    property bool draggingKey: false
    property var frozenSeries: []
    property string dragProp: ""
    property int dragIndex: -1
    property real dragSeconds: 0
    property real dragValue: 0

    // [{ prop, label, color, points, valueMin, valueMax }, ...] — selection order.
    readonly property var series: {
        void EditorState.selectedClipData
        void EditorState.tracks
        if (draggingKey)
            return frozenSeries
        if (!hasClip)
            return []
        const out = []
        const selected = EditorState.keyframeGraphProperties
        for (let i = 0; i < selected.length; ++i) {
            const id = selected[i]
            if (!supportsProperty(id))
                continue
            const pts = EditorState.clipKeyframes(
                          EditorState.selectedTrack, EditorState.selectedClip, id) || []
            // Transform props are always animatable, so they earn a chip even before their first
            // key. An effect param is a static value until it is keyed, and merely dragging its
            // slider selects it — showing an empty series for that would pop the strip open on
            // every effect edit.
            if (pts.length === 0 && id.substring(0, 3) === "fx.")
                continue
            const range = root.valueRangeFor(id, pts)
            out.push({
                prop: id,
                label: root.chipLabel(id),
                color: Theme.keyframeCurveColor(id),
                points: pts,
                valueMin: range.min,
                valueMax: range.max
            })
        }
        return out
    }

    // Flattened key dots so a single Repeater can own every series' handles.
    readonly property var keyHandles: {
        const out = []
        for (let s = 0; s < series.length; ++s) {
            const entry = series[s]
            for (let i = 0; i < entry.points.length; ++i) {
                out.push({
                    seriesIndex: s,
                    pointIndex: i,
                    prop: entry.prop,
                    color: entry.color,
                    seconds: entry.points[i].seconds,
                    value: entry.points[i].value
                })
            }
        }
        return out
    }
    readonly property int keyCount: keyHandles.length

    readonly property real clipStart: clip.start || 0
    readonly property real clipDuration: Math.max(0.1, clip.duration || 1)

    height: visible ? 88 : 0
    visible: (propertiesTab === "transform" || propertiesTab === "effects")
             && hasClip && clip && series.length > 0

    // Absolute timeline X — same mapping as clips on the track.
    function xForSeconds(seconds) {
        return seconds * pxPerSecond
    }
    function secondsForX(x) {
        return x / pxPerSecond
    }
    function yForValue(v, entry) {
        const span = Math.max(1e-6, entry.valueMax - entry.valueMin)
        return graph.height - ((v - entry.valueMin) / span) * graph.height
    }
    function valueForY(y, entry) {
        const span = Math.max(1e-6, entry.valueMax - entry.valueMin)
        const t = (graph.height - y) / Math.max(1, graph.height)
        return entry.valueMin + t * span
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

                // Legend for what the inspector currently has selected — one chip
                // per drawn curve, tinted to match it.
                Flow {
                    width: parent.width
                    spacing: 3
                    Repeater {
                        model: root.series
                        delegate: ThemedChip {
                            required property var modelData
                            text: modelData.label
                            chipHeight: 18
                            horizontalPadding: 3
                            accentColor: modelData.color
                            tooltip: root.series.length > 1
                                     ? qsTr("%1 — click to show only this")
                                       .arg(root.propertyLabel(modelData.prop))
                                     : root.propertyLabel(modelData.prop)
                            selected: true
                            onClicked: EditorState.soloKeyframeGraphProperty(modelData.prop)
                        }
                    }
                }

                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: root.keyCount === 0 ? qsTr("No keys") : (root.keyCount + qsTr(" key(s)"))
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
                            ctx.lineWidth = 1.5
                            for (let s = 0; s < root.series.length; ++s) {
                                const entry = root.series[s]
                                if (entry.points.length < 2)
                                    continue
                                const live = entry.points.map(function (p, i) {
                                    return (entry.prop === root.dragProp && i === root.dragIndex)
                                        ? { seconds: root.dragSeconds, value: root.dragValue }
                                        : p
                                })
                                const sorted = live.sort((a, b) => a.seconds - b.seconds)
                                ctx.strokeStyle = String(entry.color)
                                ctx.beginPath()
                                for (let i = 0; i < sorted.length; ++i) {
                                    const px = root.xForSeconds(sorted[i].seconds)
                                    const py = root.yForValue(sorted[i].value, entry)
                                    if (i === 0)
                                        ctx.moveTo(px, py)
                                    else
                                        ctx.lineTo(px, py)
                                }
                                ctx.stroke()
                            }
                        }
                        Connections {
                            target: root
                            function onSeriesChanged() { curveCanvas.requestPaint() }
                            function onPxPerSecondChanged() { curveCanvas.requestPaint() }
                            function onContentXChanged() { curveCanvas.requestPaint() }
                            function onDragSecondsChanged() { curveCanvas.requestPaint() }
                            function onDragValueChanged() { curveCanvas.requestPaint() }
                        }
                    }

                    Repeater {
                        model: root.keyHandles
                        delegate: Rectangle {
                            id: keyDot
                            required property var modelData
                            readonly property var entry: root.series[modelData.seriesIndex]
                            width: 10
                            height: 10
                            rotation: 45
                            radius: 1
                            color: modelData.color
                            border.width: 1
                            border.color: "#ffffff"
                            x: root.xForSeconds(modelData.seconds) - width / 2 + dragDx
                            y: root.yForValue(modelData.value, entry) - height / 2 + dragDy
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
                                        root.frozenSeries = root.series
                                        root.dragProp = keyDot.modelData.prop
                                        root.dragIndex = keyDot.modelData.pointIndex
                                        root.dragSeconds = keyDot.modelData.seconds
                                        root.dragValue = keyDot.modelData.value
                                        root.draggingKey = true
                                        EditorState.beginPreviewDrag(qsTr("Move keyframe"))
                                    } else {
                                        EditorState.commitPreviewDrag()
                                        root.draggingKey = false
                                        root.dragProp = ""
                                        root.dragIndex = -1
                                        keyDot.dragDx = 0
                                        keyDot.dragDy = 0
                                    }
                                }
                                onTranslationChanged: {
                                    if (!active)
                                        return
                                    const entry = keyDot.entry
                                    const baseSec = keyDot.modelData.seconds
                                    const baseVal = keyDot.modelData.value
                                    const newSec = Math.max(
                                        root.clipStart,
                                        Math.min(root.clipStart + root.clipDuration,
                                                 baseSec + translation.x / root.pxPerSecond))
                                    const newVal = Math.max(
                                        entry.valueMin,
                                        Math.min(entry.valueMax,
                                                 root.valueForY(
                                                     root.yForValue(baseVal, entry) + translation.y,
                                                     entry)))
                                    EditorState.previewMoveClipKeyframe(
                                        EditorState.selectedTrack, EditorState.selectedClip,
                                        keyDot.modelData.prop, keyDot.editSeconds, newSec, newVal)
                                    keyDot.editSeconds = newSec
                                    keyDot.dragDx = root.xForSeconds(newSec) - root.xForSeconds(baseSec)
                                    keyDot.dragDy = root.yForValue(newVal, entry)
                                                  - root.yForValue(baseVal, entry)
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
