import QtQuick
import QtQuick.Controls.Basic
import Drift

// Dropdown for inserting an empty timeline track by type.
Popup {
    id: root

    width: 180
    padding: 6
    modal: true
    dim: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property var trackTypes: [
        { type: "video", label: qsTr("Video"), icon: Theme.icons.film },
        { type: "audio", label: qsTr("Audio"), icon: Theme.icons.music },
        { type: "text", label: qsTr("Text"), icon: Theme.icons.type },
        { type: "subtitle", label: qsTr("Subtitle"), icon: Theme.icons.messageSquare },
        { type: "shape", label: qsTr("Shape"), icon: Theme.icons.shapes },
    ]

    background: Rectangle {
        color: Theme.panelBackground
        border.width: 1
        border.color: Theme.panelBorder
        radius: Theme.radiusMd
    }

    contentItem: Column {
        spacing: 2

        Text {
            width: parent.width
            leftPadding: 8
            rightPadding: 8
            topPadding: 4
            bottomPadding: 6
            text: qsTr("New track")
            color: Theme.mutedForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            font.weight: Font.Medium
        }

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.panelBorder
            opacity: 0.5
        }

        Repeater {
            model: root.trackTypes

            delegate: Rectangle {
                required property var modelData
                width: parent.width
                height: 32
                radius: Theme.radiusSm
                color: itemMouse.containsMouse ? Theme.popoverHover : "transparent"

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 10

                    IconGlyph {
                        anchors.verticalCenter: parent.verticalCenter
                        glyph: modelData.icon
                        iconSize: 14
                        iconColor: Theme.mutedForeground
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        color: Theme.panelForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                    }
                }

                MouseArea {
                    id: itemMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        EditorState.addTrack(modelData.type)
                        root.close()
                    }
                }
            }
        }
    }
}
