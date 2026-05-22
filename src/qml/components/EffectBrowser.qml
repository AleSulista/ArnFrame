import QtQuick
import QtQuick.Controls.Basic
import Drift

// Browsable effect preset picker: category chips + card grid.
// Drag a card onto a timeline clip, or click / tap + to apply to the selection.
Column {
    id: root
    spacing: 0

    readonly property var categories: EditorState.effectCategories()
    property string activeCategory: categories.length > 0 ? categories[0].id : ""

    function applyPreset(effectId) {
        if (EditorState.selectedClip < 0)
            return
        EditorState.addEffect(EditorState.selectedTrack, EditorState.selectedClip, effectId)
    }

    Text {
        id: browserTip
        width: parent.width - 24
        leftPadding: 12
        rightPadding: 12
        topPadding: 8
        bottomPadding: 4
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        text: EditorState.selectedClip >= 0
              ? qsTr("Drag a preset onto a clip, or click to apply to the selection")
              : qsTr("Drag a preset onto a clip in the timeline")
        color: Theme.mutedForeground
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeXs
    }

    Flickable {
        id: categoryFlick
        width: parent.width
        height: 34
        contentWidth: categoryRow.width + 24
        clip: true

        Row {
            id: categoryRow
            x: 12
            height: parent.height
            spacing: 6

            Repeater {
                model: root.categories
                delegate: Rectangle {
                    id: categoryChip
                    required property var modelData
                    height: 26
                    radius: Theme.radiusSm
                    color: categoryChip.modelData.id === root.activeCategory
                           ? Theme.panelSecondaryBg : Theme.panelAccent
                    border.width: categoryChip.modelData.id === root.activeCategory ? 1 : 0
                    border.color: Theme.panelSecondaryBorder

                    Text {
                        id: chipLabel
                        anchors.centerIn: parent
                        anchors.margins: 8
                        text: categoryChip.modelData.label
                        color: categoryChip.modelData.id === root.activeCategory
                               ? Theme.panelSecondaryForeground : Theme.panelForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                    }

                    width: chipLabel.implicitWidth + 16

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.activeCategory = categoryChip.modelData.id
                    }
                }
            }
        }
    }

    Flickable {
        width: parent.width
        height: Math.max(0, root.height - browserTip.height - categoryFlick.height)
        contentHeight: presetGrid.height + 24
        clip: true
        ScrollBar.vertical: AppScrollBar { }

        Grid {
            id: presetGrid
            x: 12
            y: 12
            width: parent.width - 24
            columns: Math.max(1, Math.floor((width + Theme.assetCardGap) / (Theme.assetCardWidth + Theme.assetCardGap)))
            columnSpacing: Theme.assetCardGap
            rowSpacing: Theme.assetCardGap

            Repeater {
                model: EditorState.effectCatalog()
                delegate: Rectangle {
                    id: presetCard
                    required property var modelData
                    visible: presetCard.modelData.category === root.activeCategory
                    width: visible ? Theme.assetCardWidth : 0
                    height: visible ? 64 : 0
                    radius: Theme.radiusSm
                    color: cardHover.hovered ? Theme.panelSecondaryBg : Theme.panelAccent
                    border.width: presetDrag.active ? 1 : 0
                    border.color: Theme.primary
                    opacity: presetDrag.active ? 0.85 : 1

                    Drag.active: presetDrag.active
                    Drag.dragType: Drag.Automatic
                    Drag.supportedActions: Qt.CopyAction
                    Drag.keys: ["application/x-drift-effect"]
                    Drag.mimeData: ({ "application/x-drift-effect": presetCard.modelData.id })
                    Drag.hotSpot.x: width / 2
                    Drag.hotSpot.y: height / 2

                    HoverHandler { id: cardHover }

                    TapHandler {
                        enabled: !presetDrag.active
                        onTapped: root.applyPreset(presetCard.modelData.id)
                    }

                    DragHandler {
                        id: presetDrag
                        target: null
                        acceptedButtons: Qt.LeftButton
                    }

                    Column {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 2

                        Text {
                            width: parent.width
                            text: presetCard.modelData.label
                            color: Theme.panelForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeCard
                            font.weight: Font.Medium
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            width: parent.width
                            visible: presetCard.modelData.compositorOnly === true
                            text: qsTr("Compositor")
                            color: Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs - 1
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    IconButton {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 3
                        icon: Theme.icons.plus
                        variant: "ghost"
                        buttonSize: 18
                        iconSize: 12
                        tooltip: qsTr("Apply to selected clip")
                        buttonEnabled: EditorState.selectedClip >= 0
                        onClicked: root.applyPreset(presetCard.modelData.id)
                    }
                }
            }
        }
    }
}
