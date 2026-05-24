import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Templates as T
import Drift
import "components"

ApplicationWindow {
    id: window

    width: 1280
    height: 800
    visible: true
    title: "CutWire Drift"
    color: Theme.appBackground

    function configureAndAddAsset(assetIndex, runner) {
        if (!EditorState.shouldConfigureProjectForAsset(assetIndex)) {
            runner()
            return
        }
        projectSetupDialog.openForAsset(assetIndex, runner)
    }

    ProjectSetupDialog {
        id: projectSetupDialog
    }

    RecoveryDialog {
        id: recoveryDialog
    }

    // Offer to recover unsaved work if the previous session crashed. Deferred so
    // the window is up before the modal appears.
    Component.onCompleted: if (EditorState.recoveryAvailable) recoveryTimer.start()

    Timer {
        id: recoveryTimer
        interval: 300
        onTriggered: if (EditorState.recoveryAvailable) recoveryDialog.open()
    }

    // Shortcut is not an Item, so wrap each binding in a zero-size host.
    Repeater {
        model: EditorState.actions
        Item {
            required property var modelData
            width: 0
            height: 0
            Shortcut {
                sequence: modelData.shortcut
                context: Qt.ApplicationShortcut
                onActivated: EditorState.triggerAction(modelData.id)
            }
        }
    }

    Column {
        anchors.fill: parent
        spacing: 0

        EditorHeader {
            width: parent.width
        }

        Item {
            width: parent.width
            height: parent.height - Theme.headerHeight

            SplitView {
                id: outerSplit
                anchors.fill: parent
                anchors.margins: Theme.pagePadding
                anchors.topMargin: 0
                orientation: Qt.Vertical
                spacing: Theme.panelGap

                handle: Rectangle {
                    implicitWidth: outerSplit.orientation === Qt.Horizontal ? 1 : outerSplit.width
                    implicitHeight: outerSplit.orientation === Qt.Horizontal ? outerSplit.height : 1
                    color: T.SplitHandle.pressed ? Theme.primary
                        : (T.SplitHandle.hovered ? Theme.panelBorder : "transparent")
                }

                SplitView {
                    id: innerSplit
                    SplitView.preferredHeight: parent.height * 0.5
                    SplitView.minimumHeight: parent.height * 0.3
                    SplitView.maximumHeight: parent.height * 0.85
                    orientation: Qt.Horizontal
                    spacing: Theme.panelGap

                    handle: Rectangle {
                        implicitWidth: innerSplit.orientation === Qt.Horizontal ? 1 : innerSplit.width
                        implicitHeight: innerSplit.orientation === Qt.Horizontal ? innerSplit.height : 1
                        color: T.SplitHandle.pressed ? Theme.primary
                            : (T.SplitHandle.hovered ? Theme.panelBorder : "transparent")
                    }

                    AssetsPanel {
                        SplitView.preferredWidth: parent.width * 0.25
                        SplitView.minimumWidth: 180
                        SplitView.maximumWidth: parent.width * 0.4
                    }

                    PreviewPanel {
                        SplitView.fillWidth: true
                        SplitView.minimumWidth: 320
                    }

                    PropertiesPanel {
                        SplitView.preferredWidth: parent.width * 0.25
                        SplitView.minimumWidth: 220
                        SplitView.maximumWidth: parent.width * 0.4
                    }
                }

                TimelinePanel {
                    SplitView.preferredHeight: parent.height * 0.5
                    SplitView.minimumHeight: 140
                    SplitView.maximumHeight: parent.height * 0.7
                }
            }
        }
    }
}
