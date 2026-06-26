import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import Drift
import "components"

Rectangle {
    id: root

    height: Theme.headerHeight
    color: Theme.appBackground

    property string projectName: EditorState.projectName

    function openProject() {
        var url = FileDialogs.openFile(qsTr("Open Project"), [qsTr("Drift project (*.drift.json)")])
        if (url != "")
            EditorState.loadProject(url)
    }

    function saveProject() {
        if (EditorState.currentProjectPath && EditorState.currentProjectPath.length > 0) {
            EditorState.saveProject(EditorState.fileUrl(EditorState.currentProjectPath))
            return
        }
        var url = FileDialogs.saveFile(qsTr("Save Project"), [qsTr("Drift project (*.drift.json)")], "drift.json")
        if (url != "")
            EditorState.saveProject(url)
    }

    function exportVideo() {
        exportDialog.openDialog()
    }

    // True once the user has dismissed the progress dialog while an export is
    // still running; drives the circular-progress badge next to Export.
    property bool exportProgressDismissed: false

    Connections {
        target: EditorState
        function onProjectNameChanged() { root.projectName = EditorState.projectName }
        function onExportInProgressChanged() {
            if (EditorState.exportInProgress) {
                root.exportProgressDismissed = false
                exportProgressDialog.openDialog()
            }
        }
    }

    ExportDialog {
        id: exportDialog
    }

    ExportProgressDialog {
        id: exportProgressDialog
        onClosed: if (EditorState.exportInProgress) root.exportProgressDismissed = true
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.panelBorder
        opacity: 0.5
    }

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

            ToolTip {
                visible: logoArea.containsMouse
                text: qsTr("Open project / recent")
            }

            MouseArea {
                id: logoArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: recentPopup.open()
            }

            RecentProjectsPopup {
                id: recentPopup
                y: parent.height + 6
                onOpenFileRequested: root.openProject()
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
                onEditingFinished: EditorState.projectName = text
            }

            MouseArea {
                id: nameArea
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
            }
        }

        Rectangle {
            width: 7
            height: 7
            radius: 3.5
            color: Theme.primary
            anchors.verticalCenter: parent.verticalCenter
            visible: EditorState.hasUnsavedChanges

            ToolTip {
                visible: unsavedArea.containsMouse
                text: qsTr("Unsaved changes")
            }

            MouseArea {
                id: unsavedArea
                anchors.fill: parent
                anchors.margins: -4
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
            }
        }

        IconButton {
            icon: Theme.icons.copy
            variant: "ghost"
            tooltip: qsTr("Save project")
            onClicked: root.saveProject()
        }
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        text: EditorState.lastMessage
        color: Theme.mutedForeground
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeXs
        visible: text.length > 0
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: 1
        spacing: 8

        IconButton {
            icon: Theme.icons.layers
            variant: "ghost"
            tooltip: qsTr("Addons")
            anchors.verticalCenter: parent.verticalCenter
            onClicked: root.Window.window.openAddonManager()
        }

        IconButton {
            icon: Theme.icons.messageSquare
            variant: "ghost"
            tooltip: qsTr("Messages")
            anchors.verticalCenter: parent.verticalCenter
            onClicked: EditorState.lastMessage = EditorState.lastMessage.length > 0
                        ? EditorState.lastMessage
                        : "No messages"
        }

        Rectangle {
            id: exportProgressBadge
            width: 28
            height: 28
            radius: 14
            color: badgeMouse.containsMouse ? Theme.popoverHover : "transparent"
            anchors.verticalCenter: parent.verticalCenter
            visible: EditorState.exportInProgress && root.exportProgressDismissed

            CircularProgress {
                anchors.centerIn: parent
                size: 22
                strokeWidth: 3
                value: EditorState.exportProgress
            }

            ToolTip {
                visible: badgeMouse.containsMouse
                text: qsTr("Export in progress (%1%) — click to view").arg(Math.round(EditorState.exportProgress * 100))
            }

            MouseArea {
                id: badgeMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.exportProgressDismissed = false
                    exportProgressDialog.openDialog()
                }
            }
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
                    Text {
                        visible: EditorState.exportInProgress
                        text: Math.round(EditorState.exportProgress * 100) + "%"
                        color: "white"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            ToolTip {
                visible: exportMouse.containsMouse
                text: qsTr("Export video")
            }

            MouseArea {
                id: exportMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                enabled: !EditorState.exportInProgress
                onClicked: root.exportVideo()
            }
        }

        IconButton {
            icon: Theme.darkMode ? Theme.icons.sun : Theme.icons.moon
            variant: "ghost"
            tooltip: Theme.darkMode ? qsTr("Switch to light mode") : qsTr("Switch to dark mode")
            anchors.verticalCenter: parent.verticalCenter
            onClicked: Theme.toggleDarkMode()
        }
    }
}
