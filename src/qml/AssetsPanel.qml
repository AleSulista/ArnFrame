import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import Drift
import "components"

PanelFrame {
    id: root

    Component.onCompleted: AssetLibrary.ensureAllMedia()

    ListModel {
        id: tabsModel
        ListElement { tabId: "media"; icon: 0; label: "Media" }
        ListElement { tabId: "sounds"; icon: 1; label: "Sounds" }
        ListElement { tabId: "text"; icon: 2; label: "Text" }
        ListElement { tabId: "stickers"; icon: 3; label: "Stickers" }
        ListElement { tabId: "effects"; icon: 4; label: "Effects" }
        ListElement { tabId: "adjustment"; icon: 5; label: "Adjustment" }
        ListElement { tabId: "settings"; icon: 6; label: "Settings" }
    }
    property var tabIcons: [
        Theme.icons.folder,
        Theme.icons.headphones,
        Theme.icons.type,
        Theme.icons.smile,
        Theme.icons.wand,
        Theme.icons.sliders,
        Theme.icons.settings
    ]
    property int activeTab: 0

    FileDialog {
        id: importDialog
        title: qsTr("Import Media")
        fileMode: FileDialog.OpenFiles
        nameFilters: [
            qsTr("Media files (*.mp4 *.mov *.mkv *.avi *.webm *.m4v *.mp3 *.wav *.aac *.flac *.ogg *.m4a *.png *.jpg *.jpeg *.gif *.webp *.bmp)")
        ]
        onAccepted: AssetLibrary.importUrls(selectedFiles)
    }

    Row {
        anchors.fill: parent
        spacing: 0

        // --- vertical tab rail --------------------------------------------------
        Column {
            width: Theme.tabRailWidth
            height: parent.height
            topPadding: 4
            spacing: 2

            Repeater {
                model: tabsModel
                delegate: IconButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    icon: root.tabIcons[model.icon]
                    variant: "ghost"
                    active: root.activeTab === index
                    onClicked: root.activeTab = index
                }
            }
        }

        Rectangle {
            width: 1
            height: parent.height
            color: Theme.panelBorder
        }

        // --- header + content -----------------------------------------------------
        Column {
            id: assetsContent
            width: parent.width - Theme.tabRailWidth - 1
            height: parent.height
            property bool gridMode: true

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
                    text: tabsModel.get(root.activeTab).label
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                }

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6

                    IconButton {
                        icon: Theme.icons.grid
                        variant: "ghost"
                        active: assetsContent.gridMode
                        onClicked: assetsContent.gridMode = true
                    }
                    IconButton {
                        icon: Theme.icons.list
                        variant: "ghost"
                        active: !assetsContent.gridMode
                        onClicked: assetsContent.gridMode = false
                    }
                    IconButton {
                        icon: Theme.icons.sort
                        variant: "ghost"
                    }

                    Rectangle {
                        width: importRow.implicitWidth + 20
                        height: 28
                        radius: Theme.radiusSm
                        color: "transparent"
                        border.width: 1
                        border.color: Theme.panelBorder
                        anchors.verticalCenter: parent.verticalCenter

                        Row {
                            id: importRow
                            anchors.centerIn: parent
                            spacing: 6

                            IconGlyph {
                                glyph: Theme.icons.upload
                                iconSize: 16
                                iconColor: Theme.panelForeground
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: "Import"
                                color: Theme.panelForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            onClicked: importDialog.open()
                            onEntered: parent.color = Theme.panelAccent
                            onExited: parent.color = "transparent"
                        }
                    }
                }
            }

            Flickable {
                id: flick
                width: parent.width
                height: parent.height - Theme.panelHeaderHeight
                contentHeight: grid.height + 24
                clip: true
                ScrollBar.vertical: AppScrollBar { }

                Grid {
                    id: grid
                    x: 12
                    y: 12
                    width: flick.width - 24
                    columns: Math.max(1, Math.floor((width + Theme.assetCardGap) / (Theme.assetCardWidth + Theme.assetCardGap)))
                    columnSpacing: Theme.assetCardGap
                    rowSpacing: Theme.assetCardGap

                    Repeater {
                        model: AssetLibrary
                        delegate: Column {
                            id: assetCard
                            width: Theme.assetCardWidth
                            spacing: 4

                            required property int index
                            required property string name
                            required property string kind
                            required property string duration
                            required property double durationSeconds
                            required property string path
                            required property string thumbnailPath
                            required property string filmstripPath

                            property int assetIndex: index

                            Drag.active: assetDrag.active
                            Drag.dragType: Drag.Automatic
                            Drag.supportedActions: Qt.CopyAction
                            Drag.mimeData: {
                                "text/plain": assetIndex.toString()
                            }

                            TapHandler {
                                onTapped: EditorState.addClipFromAsset(assetIndex)
                            }

                            DragHandler {
                                id: assetDrag
                            }

                            Rectangle {
                                width: Theme.assetCardWidth
                                height: Theme.assetCardWidth * 9 / 16
                                radius: Theme.radiusSm
                                color: Theme.panelAccent
                                clip: true

                                Image {
                                    anchors.fill: parent
                                    visible: thumbnailPath.length > 0
                                    source: thumbnailPath.length > 0 ? EditorState.imageUrl(thumbnailPath) : ""
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                }

                                IconGlyph {
                                    anchors.centerIn: parent
                                    visible: thumbnailPath.length === 0
                                    glyph: kind === "audio" ? Theme.icons.music
                                         : kind === "image" ? Theme.icons.image
                                         : Theme.icons.film
                                    iconSize: 24
                                    iconColor: Theme.mutedForeground
                                }

                                Rectangle {
                                    visible: duration.length > 0
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 4
                                    color: "#000000b3"
                                    radius: 3
                                    width: durationLabel.implicitWidth + 8
                                    height: durationLabel.implicitHeight + 4

                                    Text {
                                        id: durationLabel
                                        anchors.centerIn: parent
                                        text: duration
                                        color: "white"
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                }
                            }

                            Text {
                                width: parent.width
                                text: name
                                color: Theme.mutedForeground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeCard
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }
    }
}
