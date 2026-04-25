import QtQuick
import Drift

// Icon-only button, variants "text" / "ghost" / "secondary".
Rectangle {
    id: root

    property string icon: ""
    property real buttonSize: 28
    property real iconSize: 16
    property bool active: false
    property bool buttonEnabled: true
    // "text": transparent, hover = reduced opacity (toolbar buttons)
    // "ghost": transparent, hover = accent background (tab rail, view toggles)
    property string variant: "text"

    signal clicked()

    width: buttonSize
    height: buttonSize
    radius: Theme.radiusSm
    color: active ? Theme.panelSecondaryBg
                  : (variant === "ghost" && mouseArea.containsMouse ? Theme.panelAccent : "transparent")
    border.width: active ? 1 : 0
    border.color: Theme.panelSecondaryBorder
    opacity: !buttonEnabled ? 0.5 : (variant === "text" && !active && mouseArea.containsMouse ? 0.75 : 1)

    IconGlyph {
        anchors.centerIn: parent
        glyph: root.icon
        iconSize: root.iconSize
        iconColor: root.active ? Theme.panelSecondaryForeground : Theme.mutedForeground
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.buttonEnabled
        cursorShape: root.buttonEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
