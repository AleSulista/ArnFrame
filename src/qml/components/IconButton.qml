import QtQuick
import QtQuick.Controls.Basic
import Drift

// Icon-only button, variants "text" / "ghost" / "secondary".
Rectangle {
    id: root

    property string icon: ""
    property string tooltip: ""
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
                  : (variant === "ghost" && buttonEnabled && mouseArea.containsMouse ? Theme.panelAccent : "transparent")
    border.width: active ? 1 : 0
    border.color: Theme.panelSecondaryBorder
    opacity: !buttonEnabled ? 0.5 : (variant === "text" && !active && mouseArea.containsMouse ? 0.75 : 1)

    IconGlyph {
        anchors.centerIn: parent
        glyph: root.icon
        iconSize: root.iconSize
        iconColor: root.active ? Theme.panelSecondaryForeground : Theme.mutedForeground
    }

    ToolTip {
        visible: root.tooltip.length > 0 && mouseArea.containsMouse
        text: root.tooltip
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.buttonEnabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: if (root.buttonEnabled) root.clicked()
    }
}
