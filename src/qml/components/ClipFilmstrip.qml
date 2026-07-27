import QtQuick
import Drift

// Filmstrip across a video clip body.
//
// Only tiles that intersect the timeline viewport are instantiated. A multi-hour
// clip at default zoom is hundreds of thousands of px wide; creating one Image
// per 120px (or even one huge Image per strip frame) freezes or OOMs the UI,
// especially once play starts auto-scrolling the Flickable.
Item {
    id: root

    property string filmstripPath: ""
    property int frameCount: 8
    property int frameWidth: 120
    property int frameHeight: 68

    // Source window this clip covers, in seconds, plus the full source length. When set, each
    // tile maps to the source time it represents so the strip shows correct-timestamp frames and
    // stays anchored to the source when the clip is trimmed. Left at 0 (e.g. the Speed Curve
    // window) falls back to one plain pass of the strip's frames across the width.
    property real inPoint: 0
    property real outPoint: 0
    property real sourceDuration: 0

    // Timeline viewport in the same coordinate space as this item's parent (clip x).
    // When unset (viewW <= 0), fall back to a small tile cap so Speed Curve etc. stay cheap.
    property real worldX: 0
    property real viewX: 0
    property real viewW: 0

    readonly property bool sourceMapped: sourceDuration > 0 && outPoint > inPoint && width > 0

    readonly property int totalTiles: filmstripPath.length > 0 && width > 0
        ? Math.max(1, Math.ceil(width / Math.max(1, frameWidth)))
        : 0

    // Inclusive range of tile indices that overlap the viewport (+ 1 tile overscan).
    readonly property int firstVisibleTile: {
        if (totalTiles <= 0)
            return 0
        if (viewW <= 0)
            return 0
        var localLeft = viewX - worldX
        return Math.max(0, Math.floor(localLeft / Math.max(1, frameWidth)) - 1)
    }

    readonly property int visibleTileCount: {
        if (totalTiles <= 0)
            return 0
        if (viewW <= 0) {
            // No viewport (Speed Curve): at most one pass of the strip frames.
            return Math.min(totalTiles, Math.max(frameCount, 1))
        }
        var count = Math.ceil(viewW / Math.max(1, frameWidth)) + 3
        return Math.min(totalTiles - firstVisibleTile, Math.max(1, count))
    }

    function frameForTile(tileIndex) {
        if (sourceMapped) {
            var clipFraction = (tileIndex + 0.5) * frameWidth / width
            var srcSec = inPoint + clipFraction * (outPoint - inPoint)
            var f = Math.floor(srcSec / sourceDuration * frameCount)
            return Math.max(0, Math.min(frameCount - 1, f))
        }
        return tileIndex % Math.max(1, frameCount)
    }

    // True while the first frame is still decoding.
    readonly property bool loading: filmstripPath.length > 0 && !firstFrameReady
    property bool firstFrameReady: false

    clip: true

    onFilmstripPathChanged: firstFrameReady = false

    SkeletonBox {
        anchors.fill: parent
        radius: 0
        animated: root.width < 4000
        visible: root.loading
    }

    Repeater {
        model: root.visibleTileCount
        delegate: Image {
            id: frame
            required property int index
            readonly property int tileIndex: root.firstVisibleTile + index

            x: tileIndex * root.frameWidth
            width: root.frameWidth
            height: root.height
            source: EditorState.filmstripFrameUrl(root.filmstripPath,
                                                  root.frameForTile(tileIndex), root.frameCount)
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            // Keep GPU textures small regardless of item layout.
            sourceSize.width: root.frameWidth
            sourceSize.height: root.frameHeight

            opacity: status === Image.Ready ? 1 : 0

            Behavior on opacity {
                NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easing }
            }

            onStatusChanged: {
                if (index !== 0)
                    return
                if (status === Image.Ready || status === Image.Error)
                    root.firstFrameReady = true
            }
        }
    }
}
