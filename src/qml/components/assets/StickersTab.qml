import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import Drift
import ".."

// Stickers tab: category page chips over a grid of the built-in sticker set.
Item {
    id: root

    // Stickers come from an addon, so these are refreshed on install rather than
    // being fixed at load: a fresh install has no packs and no pages at all.
    property var categories: EditorState.builtinStickerCategories()
    property var allStickers: EditorState.builtinStickers()
    readonly property var pages: categories
    readonly property bool hasStickers: allStickers.length > 0
    property int pageIndex: 0
    readonly property string currentPageId: pages[pageIndex] ? pages[pageIndex].id : ""
    readonly property string query: search.text.trim().toLowerCase()

    // Search spans every category — once you have a name, the chips are in the way.
    readonly property var currentStickers: {
        const q = root.query
        if (q.length > 0) {
            return root.allStickers.filter(function(s) {
                const label = (s.label || "").toLowerCase()
                const id = (s.id || "").toLowerCase()
                return label.indexOf(q) >= 0 || id.indexOf(q) >= 0
            })
        }
        return root.allStickers.filter(function(s) { return s.category === root.currentPageId })
    }

    Connections {
        target: Addons
        function onKindChanged(kind) {
            if (kind !== "stickers")
                return
            root.categories = EditorState.builtinStickerCategories()
            root.allStickers = EditorState.builtinStickers()
            root.pageIndex = 0
        }
    }

    ThemedTextField {
        id: search
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 12
        visible: root.hasStickers
        placeholderText: qsTr("Search stickers")
        font.family: Theme.fontFamily
    }

    Flow {
        id: stickerPageBar
        anchors.top: search.visible ? search.bottom : parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: search.visible ? Theme.spacingMd : 12
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 6
        visible: root.hasStickers && root.query.length === 0
        height: visible ? implicitHeight : 0

        Repeater {
            model: root.pages
            delegate: ThemedChip {
                required property var modelData
                required property int index
                text: modelData.label
                variant: "secondary"
                selected: root.pageIndex === index
                onClicked: root.pageIndex = index
            }
        }
    }

    Flickable {
        anchors.top: stickerPageBar.visible ? stickerPageBar.bottom
                                            : (search.visible ? search.bottom : parent.top)
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 12
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        contentHeight: stickerPageContent.height + 24
        clip: true
        ScrollBar.vertical: AppScrollBar { }

        Column {
            id: stickerPageContent
            width: parent.width - 12
            spacing: 16

            EmptyState {
                visible: !root.hasStickers
                width: parent.width
                compact: true
                glyph: Theme.icons.smile
                title: qsTr("No sticker packs installed")
                hint: qsTr("Install the emoji pack to add stickers.")
                actionText: qsTr("Get extras")
                actionVariant: "primary"
                onActionTriggered: root.Window.window.openAddonManager("stickers")
            }

            EmptyState {
                visible: root.hasStickers && root.currentStickers.length === 0
                width: parent.width
                compact: true
                glyph: Theme.icons.smile
                title: root.query.length > 0
                       ? qsTr("No stickers match “%1”").arg(search.text.trim())
                       : qsTr("Nothing in this category")
                hint: root.query.length > 0
                      ? qsTr("Try a different name.")
                      : qsTr("Pick another category above.")
            }

            // Sticker grid for the selected category page (or search results).
            Grid {
                width: parent.width
                visible: root.currentStickers.length > 0
                columns: Math.max(1, Math.floor((width + Theme.assetCardGap) / (Theme.assetCardWidth + Theme.assetCardGap)))
                columnSpacing: Theme.assetCardGap
                rowSpacing: Theme.assetCardGap

                Repeater {
                    model: root.currentStickers
                    delegate: Column {
                        required property var modelData
                        width: Theme.assetCardWidth
                        spacing: Theme.spacingSm

                        Rectangle {
                            width: Theme.assetCardWidth
                            height: Theme.assetCardWidth
                            radius: Theme.radiusSm
                            color: stickerMouse.containsMouse ? Theme.popoverHover : Theme.panelAccent
                            clip: true

                            Behavior on color {
                                ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                            }

                            SkeletonBox {
                                anchors.fill: parent
                                anchors.margins: Theme.pagePadding
                                visible: stickerImage.status === Image.Loading
                            }

                            Image {
                                id: stickerImage
                                anchors.fill: parent
                                anchors.margins: Theme.pagePadding
                                source: EditorState.imageUrl(modelData.path)
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                                opacity: status === Image.Ready ? 1 : 0

                                Behavior on opacity {
                                    NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easing }
                                }
                            }

                            IconGlyph {
                                anchors.centerIn: parent
                                visible: stickerImage.status === Image.Error
                                glyph: Theme.icons.error
                                iconSize: Theme.iconSizeLg
                                iconColor: Theme.mutedForeground
                            }

                            ThemedToolTip {
                                text: modelData.label
                                visible: stickerMouse.containsMouse
                            }

                            MouseArea {
                                id: stickerMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: EditorState.addStickerClip(modelData.id, -1)
                            }
                        }

                        Text {
                            width: parent.width
                            text: modelData.label
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeCard
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }
}
