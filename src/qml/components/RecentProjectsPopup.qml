import QtQuick
import QtQuick.Controls.Basic
import Drift

// Dropdown listing recently opened/saved projects. Emits openFileRequested()
// when the user wants the full file picker instead of a recent entry.
Popup {
    id: root

    signal openFileRequested()

    width: 340
    padding: 6
    modal: true
    dim: false

    readonly property var items: EditorState.recentProjects

    background: Rectangle {
        color: Theme.panelBackground
        border.width: 1
        border.color: Theme.panelBorder
        radius: Theme.radiusMd
    }

    contentItem: Column {
        spacing: 2

        Item {
            width: parent.width
            height: 28

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Recent projects")
                color: Theme.mutedForeground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                font.weight: Font.Medium
            }

            IconButton {
                anchors.right: parent.right
                anchors.rightMargin: 2
                anchors.verticalCenter: parent.verticalCenter
                icon: Theme.icons.trash
                variant: "ghost"
                tooltip: qsTr("Clear recent")
                visible: root.items.length > 0
                onClicked: EditorState.clearRecentProjects()
            }
        }

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.panelBorder
            opacity: 0.5
        }

        Text {
            width: parent.width
            leftPadding: 8
            topPadding: 8
            bottomPadding: 8
            visible: root.items.length === 0
            text: qsTr("No recent projects")
            color: Theme.mutedForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
        }

        Repeater {
            model: root.items

            delegate: Rectangle {
                required property var modelData
                width: parent.width
                height: 42
                radius: Theme.radiusSm
                color: rowArea.containsMouse ? Theme.accent : "transparent"

                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1

                    Text {
                        width: parent.width
                        text: modelData.name + (modelData.exists ? "" : qsTr(" (missing)"))
                        color: modelData.exists ? Theme.foreground : Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        elide: Text.ElideRight
                    }

                    Text {
                        width: parent.width
                        text: modelData.path
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        elide: Text.ElideLeft
                    }
                }

                MouseArea {
                    id: rowArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: modelData.exists ? Qt.PointingHandCursor : Qt.ArrowCursor
                    enabled: modelData.exists
                    onClicked: {
                        EditorState.openRecentProject(modelData.path)
                        root.close()
                    }
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.panelBorder
            opacity: 0.5
        }

        Rectangle {
            width: parent.width
            height: 34
            radius: Theme.radiusSm
            color: openArea.containsMouse ? Theme.accent : "transparent"

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                IconGlyph {
                    glyph: Theme.icons.folder
                    iconSize: 14
                    iconColor: Theme.foreground
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: qsTr("Open project…")
                    color: Theme.foreground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                id: openArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.close()
                    root.openFileRequested()
                }
            }
        }
    }
}
