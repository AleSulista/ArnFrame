import QtQuick
import Drift

// Repeating filmstrip strip drawn across the body of a video clip.
//
// Frames load asynchronously; until they arrive (or if the strip file is
// missing) the clip used to show nothing at all, which was indistinguishable
// from a clip that simply had no thumbnails.
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

    // Strip frame a tile should show. In source-mapped mode the tile's centre is projected into
    // the clip's [inPoint, outPoint] source window and quantised to one of the baked frames.
    function frameForIndex(index) {
        if (root.sourceDuration > 0 && root.outPoint > root.inPoint && root.width > 0) {
            var clipFraction = (index + 0.5) * root.frameWidth / root.width;
            var srcSec = root.inPoint + clipFraction * (root.outPoint - root.inPoint);
            var f = Math.floor(srcSec / root.sourceDuration * root.frameCount);
            return Math.max(0, Math.min(root.frameCount - 1, f));
        }
        return index % root.frameCount;
    }

    // True while the first frame is still decoding.
    readonly property bool loading: filmstripPath.length > 0 && !firstFrameReady
    property bool firstFrameReady: false

    clip: true

    onFilmstripPathChanged: firstFrameReady = false

    // Placeholder until the first frame decodes.
    SkeletonBox {
        anchors.fill: parent
        radius: 0
        visible: root.loading
    }

    Repeater {
        model: root.filmstripPath.length > 0
               ? Math.max(1, Math.ceil(root.width / root.frameWidth))
               : 0
        delegate: Image {
            id: frame
            required property int index

            x: index * root.frameWidth
            width: root.frameWidth
            height: root.height
            // The provider crops the requested frame server-side, so each tile gets a distinct
            // image rather than sharing one strip URL and slicing it client-side.
            source: EditorState.filmstripFrameUrl(root.filmstripPath,
                                                  root.frameForIndex(index), root.frameCount)
            fillMode: Image.PreserveAspectCrop
            asynchronous: true

            // Fades in rather than popping to full opacity.
            opacity: status === Image.Ready ? 1 : 0

            Behavior on opacity {
                NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easing }
            }

            onStatusChanged: {
                if (index !== 0)
                    return
                // A missing strip clears the skeleton too, so the clip falls back
                // to its flat colour instead of shimmering forever.
                if (status === Image.Ready || status === Image.Error)
                    root.firstFrameReady = true
            }
        }
    }
}
