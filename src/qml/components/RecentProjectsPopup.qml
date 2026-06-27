import QtQuick
import QtQuick.Controls.Basic
import Drift

// Dropdown listing recently opened/saved projects. Emits openFileRequested()
// when the user wants the full file picker instead of a recent entry.
Popup {
    id: root

    signal openFileRequested()

    width: Theme.dialogWidthSm
    padding: Theme.spacingMd
    modal: true
    dim: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property var items: EditorState.recentProjects

    background: Rectangle {
        color: Theme.panelBackground
        border.width: Theme.borderWidth
        border.color: Theme.panelBorder
        radius: Theme.radiusMd
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

    contentItem: Column {
        spacing: Theme.spacingXs

        Item {
            width: parent.width
            height: Theme.iconButtonSize

            Text {
                anchors.left: parent.left
                anchors.leftMargin: Theme.spacingLg
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Recent projects")
                color: Theme.mutedForeground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                font.weight: Font.Medium
            }

            IconButton {
                anchors.right: parent.right
                anchors.rightMargin: Theme.spacingXs
                anchors.verticalCenter: parent.verticalCenter
                glyph: Theme.icons.trash
                variant: "ghost"
                tooltip: qsTr("Clear recent")
                visible: root.items.length > 0
                onClicked: EditorState.clearRecentProjects()
            }
        }

        Rectangle {
            width: parent.width
            height: Theme.borderWidth
            color: Theme.panelBorder
            opacity: 0.5
        }

        Text {
            width: parent.width
            leftPadding: Theme.spacingLg
            topPadding: Theme.spacingLg
            bottomPadding: Theme.spacingLg
            visible: root.items.length === 0
            text: qsTr("No recent projects")
            color: Theme.mutedForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
        }

        // Height-capped and scrollable. This was an unbounded Repeater inside a
        // Column, so a long recents list ran off the bottom of the screen with
        // no way to reach the entries below.
        ListView {
            id: recentList
            width: parent.width
            height: Math.min(contentHeight, 42 * 6)
            visible: root.items.length > 0
            clip: true
            model: root.items
            // Arrow keys move the highlight; Enter opens.
            keyNavigationEnabled: true
            focus: true
            currentIndex: -1

            ScrollBar.vertical: AppScrollBar { }

            Keys.onReturnPressed: recentList.openCurrent()
            Keys.onEnterPressed: recentList.openCurrent()

            function openCurrent() {
                if (currentIndex < 0 || currentIndex >= root.items.length)
                    return
                const entry = root.items[currentIndex]
                if (!entry.exists)
                    return
                EditorState.openRecentProject(entry.path)
                root.close()
            }

            delegate: Rectangle {
                id: recentRow
                required property var modelData
                required property int index

                width: ListView.view.width
                height: 42
                radius: Theme.radiusSm
                color: rowArea.containsMouse || recentList.currentIndex === index
                       ? Theme.accent : "transparent"

                Behavior on color {
                    ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                }

                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.spacingLg
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.spacingLg
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1

                    Text {
                        width: parent.width
                        text: recentRow.modelData.name
                              + (recentRow.modelData.exists ? "" : qsTr(" (missing)"))
                        color: recentRow.modelData.exists ? Theme.foreground : Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        elide: Text.ElideRight
                    }

                    Text {
                        width: parent.width
                        text: recentRow.modelData.path
                        color: Theme.mutedForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        elide: Text.ElideLeft
                    }
                }

                ThemedToolTip {
                    text: recentRow.modelData.exists
                          ? recentRow.modelData.path
                          : qsTr("This file has been moved or deleted:\n%1").arg(recentRow.modelData.path)
                    visible: rowArea.containsMouse
                }

                MouseArea {
                    id: rowArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: recentRow.modelData.exists ? Qt.PointingHandCursor : Qt.ArrowCursor
                    // Kept hover-enabled even when missing, so the tooltip can
                    // explain why the entry is not clickable.
                    onClicked: {
                        if (!recentRow.modelData.exists)
                            return
                        EditorState.openRecentProject(recentRow.modelData.path)
                        root.close()
                    }
                }
            }
        }

        Rectangle {
            width: parent.width
            height: Theme.borderWidth
            color: Theme.panelBorder
            opacity: 0.5
        }

        Rectangle {
            width: parent.width
            height: Theme.controlHeight + Theme.spacingSm
            radius: Theme.radiusSm
            color: openArea.containsMouse ? Theme.accent : "transparent"

            Row {
                anchors.left: parent.left
                anchors.leftMargin: Theme.spacingLg
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.spacingLg

                IconGlyph {
                    glyph: Theme.icons.folder
                    iconSize: Theme.iconSizeMd
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
