import QtQuick
import QtQuick.Controls.Basic
import Drift

// Text button with standard Drift variants. Prefer this over inline Button chrome.
// Variants: "primary" | "secondary" | "ghost" | "destructive"
Button {
    id: root

    property string variant: "secondary"
    // Named `glyph` — Button.icon is FINAL in Qt Quick Controls.
    property string glyph: ""
    property real glyphSize: 14

    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeSm
    font.weight: variant === "primary" ? Font.Medium : Font.Normal
    leftPadding: glyph.length > 0 ? 12 : 14
    rightPadding: 14
    topPadding: 7
    bottomPadding: 7
    hoverEnabled: true

    readonly property color _fg: {
        switch (variant) {
        case "primary": return Theme.primaryForeground
        case "destructive": return Theme.destructive
        default: return Theme.panelForeground
        }
    }

    readonly property color _bg: {
        if (!enabled)
            return Theme.panelAccent
        switch (variant) {
        case "primary":
            return down ? Theme.panelSecondaryForeground
                        : (hovered ? Qt.lighter(Theme.primary, 1.08) : Theme.primary)
        case "ghost":
            return down ? Theme.panelMuted
                        : (hovered ? Theme.popoverHover : "transparent")
        case "destructive":
            return down ? Theme.panelMuted
                        : (hovered ? Theme.popoverHover : "transparent")
        default: // secondary
            return down ? Theme.panelMuted
                        : (hovered ? Theme.popoverHover : Theme.panelAccent)
        }
    }

    readonly property bool _drawBorder: variant === "secondary" || variant === "ghost"
                                        || variant === "destructive"

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: Math.max(row.implicitHeight, 16)

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 6

            IconGlyph {
                visible: root.glyph.length > 0
                width: visible ? implicitWidth : 0
                glyph: root.glyph
                iconSize: root.glyphSize
                iconColor: root._fg
                anchors.verticalCenter: parent.verticalCenter
                opacity: root.enabled ? 1 : 0.5
            }

            Text {
                text: root.text
                font: root.font
                color: root._fg
                opacity: root.enabled ? 1 : 0.5
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    background: Rectangle {
        implicitHeight: 30
        radius: Theme.radiusSm
        color: root._bg
        border.width: root._drawBorder ? 1 : 0
        border.color: Theme.panelBorder
        opacity: root.enabled ? 1 : 0.6
    }
}
