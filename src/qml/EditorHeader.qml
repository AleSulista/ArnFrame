import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
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
        height: Theme.borderWidth
        color: Theme.panelBorder
        opacity: 0.5
    }

    // The three groups used to be independently anchored, so at narrow widths
    // they overlapped instead of compressing. A RowLayout arbitrates the space.
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.pagePadding
        anchors.rightMargin: Theme.pagePadding
        anchors.verticalCenterOffset: 1
        spacing: Theme.spacingLg

        // --- Left: project identity -------------------------------------------
        Row {
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.spacingLg

            Rectangle {
                width: 32
                height: 32
                radius: Theme.radiusSm
                color: logoArea.containsMouse ? Theme.accent : "transparent"

                Behavior on color {
                    ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                }

                IconGlyph {
                    anchors.centerIn: parent
                    glyph: Theme.icons.film
                    iconSize: Theme.iconSizeLg
                    iconColor: Theme.primary
                }

                ThemedToolTip {
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
                    y: parent.height + Theme.spacingMd
                    onOpenFileRequested: root.openProject()
                }
            }

            Rectangle {
                // Bounded: an unbounded implicitWidth let a long project name
                // grow until it collided with the status area and the right group.
                width: Math.min(nameInput.implicitWidth + Theme.spacing2xl, 280)
                height: 32
                radius: Theme.radiusSm
                color: nameArea.containsMouse || nameInput.activeFocus ? Theme.accent : "transparent"
                anchors.verticalCenter: parent.verticalCenter

                Behavior on color {
                    ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                }

                TextInput {
                    id: nameInput
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingLg
                    anchors.rightMargin: Theme.spacingLg
                    verticalAlignment: TextInput.AlignVCenter
                    text: root.projectName
                    color: Theme.foreground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBase
                    selectByMouse: true
                    clip: true
                    onEditingFinished: EditorState.projectName = text

                    // Escape reverts. Committing used to be the only outcome, so
                    // a mistyped name became permanent the moment focus moved.
                    Keys.onEscapePressed: function(event) {
                        text = root.projectName
                        focus = false
                        event.accepted = true
                    }

                    Accessible.role: Accessible.EditableText
                    Accessible.name: qsTr("Project name")
                }

                ThemedToolTip {
                    visible: nameArea.containsMouse && !nameInput.activeFocus
                    text: qsTr("Project name — click to rename, Esc to cancel")
                }

                MouseArea {
                    id: nameArea
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                    cursorShape: Qt.IBeamCursor
                }
            }

            Rectangle {
                width: 7
                height: 7
                radius: 3.5
                color: Theme.primary
                anchors.verticalCenter: parent.verticalCenter
                visible: opacity > 0
                opacity: EditorState.hasUnsavedChanges ? 1 : 0

                Behavior on opacity {
                    NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easing }
                }

                ThemedToolTip {
                    visible: unsavedArea.containsMouse
                    text: qsTr("Unsaved changes")
                }

                MouseArea {
                    id: unsavedArea
                    anchors.fill: parent
                    anchors.margins: -Theme.spacingSm
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }

            IconButton {
                glyph: Theme.icons.save
                variant: "ghost"
                tooltip: qsTr("Save project")
                anchors.verticalCenter: parent.verticalCenter
                onClicked: root.saveProject()
            }
        }

        // Absorbs leftover space so the two groups stay apart but can compress.
        Item {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
        }

        // --- Right: global actions --------------------------------------------
        Row {
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.spacingLg

            IconButton {
                glyph: Theme.icons.puzzle
                variant: "ghost"
                tooltip: qsTr("Addons")
                anchors.verticalCenter: parent.verticalCenter
                onClicked: root.Window.window.openAddonManager()
            }

            Rectangle {
                id: exportProgressBadge
                width: Theme.iconButtonSize
                height: Theme.iconButtonSize
                radius: width / 2
                color: badgeMouse.containsMouse ? Theme.popoverHover : "transparent"
                anchors.verticalCenter: parent.verticalCenter
                visible: opacity > 0
                opacity: EditorState.exportInProgress && root.exportProgressDismissed ? 1 : 0

                Behavior on opacity {
                    NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easing }
                }

                CircularProgress {
                    anchors.centerIn: parent
                    size: Theme.iconSizeXl
                    strokeWidth: 3
                    value: EditorState.exportProgress
                }

                ThemedToolTip {
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

            // Primary CTA. Kept as a bespoke gradient button (the documented
            // exception to Themed* chrome), but it now has the hover, pressed and
            // disabled states every other control has.
            Rectangle {
                id: exportButton

                readonly property bool busy: EditorState.exportInProgress

                width: exportRow.implicitWidth + Theme.spacing3xl
                height: 32
                radius: Theme.radiusMd
                color: Theme.exportGlow
                anchors.verticalCenter: parent.verticalCenter
                opacity: busy ? 0.55 : 1
                scale: exportMouse.pressed && !busy ? 0.97 : 1

                Behavior on opacity {
                    NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                }
                Behavior on scale {
                    NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Export video")

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 2
                    radius: parent.radius - 2
                    // Brightens on hover, darkens on press.
                    opacity: exportButton.busy ? 1
                             : (exportMouse.pressed ? 0.85 : (exportMouse.containsMouse ? 1 : 0.94))

                    Behavior on opacity {
                        NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                    }

                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Theme.exportGradientTop }
                        GradientStop { position: 1.0; color: Theme.exportGradientBottom }
                    }

                    Row {
                        id: exportRow
                        anchors.centerIn: parent
                        spacing: Theme.spacingMd

                        IconGlyph {
                            glyph: Theme.icons.upload
                            iconSize: Theme.iconSizeMd
                            iconColor: Theme.onMedia
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: qsTr("Export")
                            color: Theme.onMedia
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            font.weight: Font.Medium
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        // Fixed-width so ticking percentages do not resize the
                        // button and jolt the whole header row.
                        Text {
                            visible: exportButton.busy
                            width: visible ? 34 : 0
                            text: Math.round(EditorState.exportProgress * 100) + "%"
                            color: Theme.onMedia
                            font.family: Theme.monoFontFamily
                            font.pixelSize: Theme.fontSizeSm
                            horizontalAlignment: Text.AlignRight
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                ThemedToolTip {
                    visible: exportMouse.containsMouse
                    text: exportButton.busy ? qsTr("Export already in progress")
                                            : qsTr("Export video")
                }

                MouseArea {
                    id: exportMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: exportButton.busy ? Qt.ArrowCursor : Qt.PointingHandCursor
                    onClicked: if (!exportButton.busy) root.exportVideo()
                }
            }

            IconButton {
                glyph: Theme.darkMode ? Theme.icons.sun : Theme.icons.moon
                variant: "ghost"
                tooltip: Theme.darkMode ? qsTr("Switch to light mode") : qsTr("Switch to dark mode")
                anchors.verticalCenter: parent.verticalCenter
                onClicked: Theme.toggleDarkMode()
            }
        }
    }
}
