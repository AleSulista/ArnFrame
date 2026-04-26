import QtQuick
import Drift

Item {
    id: root

    property string filmstripPath: ""
    property int frameCount: 8
    property int frameWidth: 120
    property int frameHeight: 68

    clip: true

    Repeater {
        model: root.filmstripPath.length > 0
               ? Math.max(1, Math.ceil(root.width / root.frameWidth))
               : 0
        delegate: Image {
            x: index * root.frameWidth
            width: root.frameWidth
            height: root.height
            source: EditorState.imageUrl(root.filmstripPath)
            sourceClipRect: Qt.rect((index % root.frameCount) * root.frameWidth, 0,
                                    root.frameWidth, root.frameHeight)
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
        }
    }
}
