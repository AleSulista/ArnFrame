import QtQuick
import Drift
import "components"

Rectangle {
    id: root

    height: Theme.headerHeight
    color: Theme.appBackground

    property string projectName: "Untitled Project"

    // --- left: logo + project name -----------------------------------------
    Row {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: 1
        spacing: 8

        Rectangle {
            width: 32
            height: 32
            radius: Theme.radiusSm
            color: logoArea.containsMouse ? Theme.accent : "transparent"

            IconGlyph {
                anchors.centerIn: parent
                glyph: Theme.icons.film
                iconSize: 18
                iconColor: Theme.primary
            }

            MouseArea {
                id: logoArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
            }
        }

        Rectangle {
            width: nameInput.implicitWidth + 16
            height: 32
            radius: Theme.radiusSm
            color: nameArea.containsMouse || nameInput.activeFocus ? Theme.accent : "transparent"
            anchors.verticalCenter: parent.verticalCenter

            TextInput {
                id: nameInput
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                verticalAlignment: TextInput.AlignVCenter
                text: root.projectName
                color: Theme.foreground
                font.family: Theme.fontFamily
                font.pixelSize: Math.round(14.4)
                selectByMouse: true
                onEditingFinished: root.projectName = text
            }

            MouseArea {
                id: nameArea
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
            }
        }
    }

    // --- right: feedback / export / theme toggle ----------------------------
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: 1
        spacing: 8

        IconButton {
            icon: Theme.icons.messageSquare
            variant: "ghost"
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            id: exportButton
            width: exportRow.implicitWidth + 24
            height: 32
            radius: 9.6
            color: "#38bdf8"
            anchors.verticalCenter: parent.verticalCenter

            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: 8
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "#37b6f7" }
                    GradientStop { position: 1.0; color: "#2567ec" }
                }

                Row {
                    id: exportRow
                    anchors.centerIn: parent
                    spacing: 6

                    IconGlyph {
                        glyph: Theme.icons.upload
                        iconSize: 14
                        iconColor: "white"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: "Export"
                        color: "white"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        font.weight: Font.Medium
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
            }
        }

        IconButton {
            icon: Theme.darkMode ? Theme.icons.sun : Theme.icons.moon
            variant: "ghost"
            anchors.verticalCenter: parent.verticalCenter
            onClicked: Theme.toggleDarkMode()
        }
    }
}
