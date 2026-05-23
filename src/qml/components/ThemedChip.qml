import QtQuick
import Drift

// Selectable pill/chip used for aspect presets, speed presets, category filters, etc.
// Variants:
//   "primary"   — selected fill = Theme.primary
//   "secondary" — selected fill = Theme.panelSecondaryBg
//   "outline"   — transparent / accent fill; border always drawn
Rectangle {
    id: root

    property string text: ""
    property bool selected: false
    property string variant: "primary"
    property real chipHeight: 26
    property real horizontalPadding: 14

    signal clicked()

    implicitWidth: label.implicitWidth + horizontalPadding
    implicitHeight: chipHeight
    width: implicitWidth
    height: chipHeight
    radius: Theme.radiusSm
    color: _fill
    border.width: 1
    border.color: _border

    readonly property color _fill: {
        if (selected) {
            switch (variant) {
            case "secondary": return Theme.panelSecondaryBg
            case "outline": return Theme.panelAccent
            default: return Theme.primary
            }
        }
        if (chipMouse.containsMouse)
            return Theme.popoverHover
        return variant === "outline" ? "transparent" : Theme.panelAccent
    }

    readonly property color _border: {
        if (selected) {
            switch (variant) {
            case "secondary": return Theme.panelSecondaryBorder
            case "outline": return Theme.panelBorder
            default: return Theme.primary
            }
        }
        return Theme.panelBorder
    }

    readonly property color _fg: {
        if (selected && variant === "primary")
            return Theme.primaryForeground
        if (selected && variant === "secondary")
            return Theme.panelSecondaryForeground
        return Theme.panelForeground
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root._fg
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeXs
        font.weight: root.selected ? Font.Medium : Font.Normal
    }

    MouseArea {
        id: chipMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
