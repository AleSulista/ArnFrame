import QtQuick
import QtQuick.Shapes

Item {
    id: root

    required property string shapeKind

    Shape {
        anchors.fill: parent
        antialiasing: true

        ShapePath {
            strokeColor: "#ffffff"
            strokeWidth: 2
            fillColor: {
                switch (root.shapeKind) {
                case "square": return "#ff7840"
                case "triangle": return "#ffd60a"
                case "pentagon": return "#a060ff"
                case "hexagon": return "#50dc8c"
                default: return "#00b4ff"
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
