import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Templates as T
import Drift

ApplicationWindow {
    id: window

    width: 1280
    height: 800
    visible: true
    title: "CutWire Drift"
    color: Theme.appBackground

    Shortcut {
        sequence: "Space"
        onActivated: EditorState.playing = !EditorState.playing
    }
    Shortcut {
        sequence: "Delete"
        onActivated: EditorState.deleteSelectedClip()
    }
    Shortcut {
        sequences: [StandardKey.Undo]
        onActivated: EditorState.undo()
    }
    Shortcut {
        sequences: [StandardKey.Redo]
        onActivated: EditorState.redo()
    }
    Shortcut {
        sequence: "Escape"
        onActivated: EditorState.clearSelection()
    }
    Shortcut {
        sequence: "Ctrl+D"
        onActivated: EditorState.duplicateSelectedClip()
    }
    Shortcut {
        sequence: "S"
        onActivated: EditorState.splitAtPlayhead()
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
