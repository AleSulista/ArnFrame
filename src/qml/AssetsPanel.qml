import QtQuick
import QtQuick.Controls.Basic
import Drift
import "components"

PanelFrame {
    id: root

    Component.onCompleted: AssetLibrary.ensureAllMedia()

    function importMedia() {
        var urls = FileDialogs.openFiles(qsTr("Import Media"), [
            qsTr("Media files (*.mp4 *.mov *.mkv *.avi *.webm *.m4v *.mp3 *.wav *.aac *.flac *.ogg *.m4a *.png *.jpg *.jpeg *.gif *.webp *.bmp)")
        ])
        if (urls.length > 0)
            AssetLibrary.importUrls(urls)
    }

    function kindsForTab(tabId) {
        if (tabId === "media") return ["video", "image"]
        if (tabId === "sounds") return ["audio"]
        return []
    }

    function assetVisible(kind) {
        const tabId = tabsModel.get(activeTab).tabId
        if (tabId === "text" || tabId === "stickers" || tabId === "effects"
                || tabId === "adjustment" || tabId === "settings")
            return false
        const kinds = kindsForTab(tabId)
        return kinds.length === 0 || kinds.indexOf(kind) >= 0
    }

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
    property bool sortByKind: false

    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: (drop) => {
            if (drop.hasUrls)
                AssetLibrary.importUrls(drop.urls)
        }
    }

    Row {
        anchors.fill: parent
        spacing: 0

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
                        onClicked: {
                            if (root.sortByKind)
                                AssetLibrary.sortByName()
                            else
                                AssetLibrary.sortByKind()
                            root.sortByKind = !root.sortByKind
                        }
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
                            onClicked: root.importMedia()
                            onEntered: parent.color = Theme.panelAccent
                            onExited: parent.color = "transparent"
                        }
                    }
                }
            }

            // Text tab panel
            Column {
                visible: tabsModel.get(activeTab).tabId === "text"
                width: parent.width
                height: parent.height - Theme.panelHeaderHeight
                spacing: 8
                padding: 12

                TextField {
                    id: textClipInput
                    width: parent.width - 24
                    placeholderText: "Enter text for timeline clip"
                    color: Theme.panelForeground
                    font.family: Theme.fontFamily
                }

                Rectangle {
                    width: addTextRow.implicitWidth + 20
                    height: 32
                    radius: Theme.radiusSm
                    color: Theme.primary

                    Row {
                        id: addTextRow
                        anchors.centerIn: parent
                        spacing: 6
                        IconGlyph {
                            glyph: Theme.icons.type
                            iconSize: 14
                            iconColor: "white"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: "Add to timeline"
                            color: "white"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            EditorState.addTextClip(textClipInput.text, -1)
                            textClipInput.clear()
                        }
                    }
                }
            }

            Text {
                visible: ["stickers", "effects", "adjustment", "settings"].indexOf(tabsModel.get(activeTab).tabId) >= 0
                anchors.centerIn: parent
                anchors.verticalCenterOffset: Theme.panelHeaderHeight / 2
                text: tabsModel.get(activeTab).label + " — coming soon"
                color: Theme.mutedForeground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
            }

            Flickable {
                id: flick
                visible: kindsForTab(tabsModel.get(activeTab).tabId).length > 0
                width: parent.width
                height: parent.height - Theme.panelHeaderHeight
                contentHeight: assetsContent.gridMode ? grid.height + 24 : listColumn.height + 24
                clip: true
                ScrollBar.vertical: AppScrollBar { }

                Grid {
                    id: grid
                    visible: assetsContent.gridMode
                    x: 12
                    y: 12
                    width: flick.width - 24
                    columns: Math.max(1, Math.floor((width + Theme.assetCardGap) / (Theme.assetCardWidth + Theme.assetCardGap)))
                    columnSpacing: Theme.assetCardGap
                    rowSpacing: Theme.assetCardGap

                    Repeater {
                        model: AssetLibrary
                        delegate: Column {
                            width: Theme.assetCardWidth
                            spacing: 4
                            visible: root.assetVisible(kind)

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
                            Drag.mimeData: { "text/plain": assetIndex.toString() }

                            TapHandler { onTapped: EditorState.addClipFromAsset(assetIndex) }
                            DragHandler {
                                id: assetDrag
                                onActiveChanged: EditorState.draggingAssetIndex = active ? assetIndex : -1
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

                Column {
                    id: listColumn
                    visible: !assetsContent.gridMode
                    x: 12
                    y: 12
                    width: flick.width - 24
                    spacing: 6

                    Repeater {
                        model: AssetLibrary
                        delegate: Rectangle {
                            width: listColumn.width
                            height: 48
                            radius: Theme.radiusSm
                            color: Theme.panelAccent
                            visible: root.assetVisible(kind)

                            required property int index
                            required property string name
                            required property string kind
                            required property string duration
                            required property string thumbnailPath

                            property int assetIndex: index

                            Row {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 10

                                Rectangle {
                                    width: 56
                                    height: 32
                                    radius: 4
                                    color: Theme.panelBackground
                                    clip: true
                                    Image {
                                        anchors.fill: parent
                                        visible: thumbnailPath.length > 0
                                        source: thumbnailPath.length > 0 ? EditorState.imageUrl(thumbnailPath) : ""
                                        fillMode: Image.PreserveAspectCrop
                                    }
                                    IconGlyph {
                                        anchors.centerIn: parent
                                        visible: thumbnailPath.length === 0
                                        glyph: kind === "audio" ? Theme.icons.music : Theme.icons.film
                                        iconSize: 16
                                        iconColor: Theme.mutedForeground
                                    }
                                }

                                Column {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - 66
                                    Text {
                                        text: name
                                        color: Theme.panelForeground
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        elide: Text.ElideRight
                                        width: parent.width
                                    }
                                    Text {
                                        text: kind + (duration.length > 0 ? " · " + duration : "")
                                        color: Theme.mutedForeground
                                        font.pixelSize: Theme.fontSizeXs
                                        font.family: Theme.fontFamily
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: EditorState.addClipFromAsset(assetIndex)
                            }
                        }
                    }
                }
            }
        }
    }
}
