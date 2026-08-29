import QtQuick
import QtQuick.Controls.Basic
import Drift

// First launch after install: pick an interface language before the rest of startup.
// English is selected by default. Settings keeps the same list for later changes.
ThemedDialog {
    id: root

    title: qsTr("Choose your language")
    acceptText: qsTr("Continue")
    showReject: false
    preferredWidth: Theme.dialogWidthSm
    closePolicy: Popup.NoAutoClose

    property string selectedId: "en"

    function openChooser() {
        selectedId = "en"
        const langs = chooserLanguages
        for (let i = 0; i < langs.length; ++i) {
            if (langs[i].id === "en") {
                languageList.currentIndex = i
                languageList.positionViewAtIndex(i, ListView.Contain)
                break
            }
        }
        open()
    }

    // Concrete languages only — "System default" stays a Settings option. First launch
    // always writes an explicit code (English unless the user picks another).
    readonly property var chooserLanguages: {
        const all = EditorState.uiLanguages
        const out = []
        for (let i = 0; i < all.length; ++i) {
            if (all[i].id !== "")
                out.push(all[i])
        }
        return out
    }

    onAccepted: EditorState.chooseUiLanguage(selectedId)

    contentItem: Column {
        spacing: Theme.spacingXl
        width: parent ? parent.width : 320

        ThemedLabel {
            width: parent.width
            size: "sm"
            wrapMode: Text.WordWrap
            text: qsTr("Pick the language for menus and labels. You can change this later in Settings.")
        }

        Rectangle {
            width: parent.width
            height: Math.min(languageList.contentHeight + 2,
                             Math.min(280, root.availableContentHeight - 48))
            radius: Theme.radiusSm
            color: Theme.appBackground
            border.width: Theme.borderWidth
            border.color: Theme.panelBorder
            clip: true

            ListView {
                id: languageList
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                model: root.chooserLanguages
                interactive: contentHeight > height
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: AppScrollBar { }

                delegate: ItemDelegate {
                    id: row
                    required property var modelData
                    required property int index
                    width: languageList.width
                    height: 44
                    highlighted: modelData.id === root.selectedId
                    hoverEnabled: true

                    background: Rectangle {
                        color: {
                            if (row.highlighted)
                                return Theme.panelSecondaryBg
                            if (row.hovered)
                                return Theme.popoverHover
                            return "transparent"
                        }
                    }

                    contentItem: Item {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12

                        Text {
                            anchors.left: parent.left
                            anchors.right: checkIcon.left
                            anchors.rightMargin: Theme.spacingLg
                            anchors.verticalCenter: parent.verticalCenter
                            text: row.modelData.label
                            elide: Text.ElideRight
                            color: row.highlighted
                                   ? Theme.panelSecondaryForeground
                                   : Theme.panelForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            font.weight: row.highlighted ? Font.Medium : Font.Normal
                        }

                        IconGlyph {
                            id: checkIcon
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            glyph: Theme.icons.check
                            iconSize: 16
                            iconColor: Theme.panelSecondaryForeground
                            visible: row.highlighted
                        }
                    }

                    onClicked: {
                        root.selectedId = modelData.id
                        // Apply now so the dialog (and the rest of the UI) switches
                        // before Continue — same live retranslate as Settings.
                        EditorState.uiLanguage = modelData.id
                    }
                }
            }
        }
    }
}
