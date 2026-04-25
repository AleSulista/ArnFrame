import QtQuick
import Drift

Text {
    id: root

    property string glyph: ""
    property real iconSize: 16
    property color iconColor: Theme.mutedForeground

    text: glyph
    color: iconColor
    font.family: Theme.iconFontFamily
    font.pixelSize: iconSize
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    renderType: Text.NativeRendering
}
