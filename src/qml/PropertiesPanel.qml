import QtQuick
import Drift
import "components"

PanelFrame {
    id: root

    Rectangle {
        width: parent.width
        height: Theme.panelHeaderHeight
        color: Theme.appBackground

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.panelBorder
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: "Properties"
            color: Theme.mutedForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
        }
    }

    Text {
        anchors.centerIn: parent
        text: "No clip selected"
        color: Theme.mutedForeground
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSm
    }
}
