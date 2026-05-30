import QtQuick
import Drift

// Small circular progress ring drawn on a Canvas. value is 0..1.
Item {
    id: root

    property real value: 0
    property real size: 28
    property real strokeWidth: 3
    property color trackColor: Theme.panelMuted
    property color progressColor: Theme.primary

    width: size
    height: size

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()

            var cx = width / 2
            var cy = height / 2
            var radius = Math.min(width, height) / 2 - root.strokeWidth / 2
            var start = -Math.PI / 2
            var end = start + Math.max(0, Math.min(1, root.value)) * Math.PI * 2

            ctx.lineWidth = root.strokeWidth
            ctx.lineCap = "round"

            ctx.strokeStyle = root.trackColor
            ctx.beginPath()
            ctx.arc(cx, cy, radius, 0, Math.PI * 2, false)
            ctx.stroke()

            ctx.strokeStyle = root.progressColor
            ctx.beginPath()
            ctx.arc(cx, cy, radius, start, end, false)
            ctx.stroke()
        }
    }

    onValueChanged: canvas.requestPaint()
    onSizeChanged: canvas.requestPaint()
    onStrokeWidthChanged: canvas.requestPaint()
    onTrackColorChanged: canvas.requestPaint()
    onProgressColorChanged: canvas.requestPaint()
    Component.onCompleted: canvas.requestPaint()
}
