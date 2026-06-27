import QtQuick
import QtQuick.Shapes
import Drift

// Thumbnail for the built-in shape clips.
//
// The paths are authored on a fixed 0..100 grid, so the Shape is scaled to fit
// the item. Previously the paths were drawn at their raw coordinates inside a
// full-size Shape, which meant the preview only rendered correctly at exactly
// 100×100 — at any other size it was clipped or floated in a corner.
Item {
    id: root

    required property string shapeKind

    // Authoring grid the paths below are drawn on.
    readonly property real designSize: 100

    Item {
        // Square, centred, so shapes keep their aspect ratio in any container.
        readonly property real side: Math.min(root.width, root.height)
        width: root.designSize
        height: root.designSize
        anchors.centerIn: parent
        scale: side / root.designSize

        Shape {
            anchors.fill: parent
            antialiasing: true

            ShapePath {
                strokeColor: Theme.onMedia
                strokeWidth: 2
                // Shape clips are drawn with the graphic clip colour on the
                // timeline; the preview now uses the same family instead of an
                // unrelated five-colour palette.
                fillColor: {
                    switch (root.shapeKind) {
                    case "square": return Theme.clipGraphic
                    case "triangle": return Qt.lighter(Theme.clipGraphic, 1.25)
                    case "pentagon": return Qt.darker(Theme.clipGraphic, 1.2)
                    case "hexagon": return Qt.lighter(Theme.clipGraphic, 1.1)
                    default: return Qt.darker(Theme.clipGraphic, 1.05)
                    }
                }

                PathSvg {
                    path: {
                        switch (root.shapeKind) {
                        case "square":
                            return "M 4,4 L 96,4 L 96,96 L 4,96 Z"
                        case "triangle":
                            return "M 50,6 L 96,94 L 4,94 Z"
                        case "pentagon":
                            return "M 50,4 L 94,36 L 78,92 L 22,92 L 6,36 Z"
                        case "hexagon":
                            return "M 50,4 L 90,26 L 90,74 L 50,96 L 10,74 L 10,26 Z"
                        default:
                            return "M 4,20 L 96,20 L 96,80 L 4,80 Z"
                        }
                    }
                }
            }
        }
    }
}
