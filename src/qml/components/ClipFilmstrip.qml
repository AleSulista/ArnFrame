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
            source: EditorState.imageUrl(root.filmstripPath)
            sourceClipRect: Qt.rect((index % root.frameCount) * root.frameWidth, 0,
                                    root.frameWidth, root.frameHeight)
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
