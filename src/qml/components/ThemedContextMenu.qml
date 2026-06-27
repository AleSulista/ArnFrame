import QtQuick
import QtQuick.Controls.Basic
import Drift

// Themed right-click menu.
//
// The app previously had no context menus at all: right-clicking a clip, track,
// ruler or bookmark did nothing, so several actions were reachable only by
// unlabelled keyboard shortcut or not at all.
//
// Use with ThemedMenuItem for entries.
Menu {
    id: root

    implicitWidth: 200
    padding: Theme.spacingSm
    // Escape closes; clicking outside dismisses.
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        radius: Theme.radiusSm
        color: Theme.panelBackground
        border.width: Theme.borderWidth
        border.color: Theme.panelBorder
    }

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: Theme.durationFast
            easing.type: Theme.easing
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: Theme.durationFast
            easing.type: Theme.easing
        }
    }

    delegate: MenuItem {
        id: menuItem

        implicitHeight: Theme.controlHeightSm + Theme.spacingSm
        hoverEnabled: true

        contentItem: Row {
            spacing: Theme.spacingLg
            leftPadding: Theme.spacingLg
            rightPadding: Theme.spacingLg

            IconGlyph {
                anchors.verticalCenter: parent.verticalCenter
                visible: menuItem.icon.name.length > 0
                width: visible ? implicitWidth : 0
                glyph: menuItem.icon.name
                iconSize: Theme.iconSizeMd
                iconColor: menuItem.enabled ? Theme.panelForeground : Theme.mutedForeground
                opacity: menuItem.enabled ? 1 : 0.5
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: menuItem.text
                color: Theme.panelForeground
                opacity: menuItem.enabled ? 1 : 0.5
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                elide: Text.ElideRight
            }
        }

        background: Rectangle {
            radius: Theme.radiusXs
            color: menuItem.highlighted && menuItem.enabled ? Theme.panelAccent : "transparent"

            Behavior on color {
                ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
            }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            cursorShape: menuItem.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        }
    }
}
