import QtQuick
import QtQuick.Controls.Basic
import Drift

// A font selector that previews each family in its own face and groups them by catalog category.
// ThemedComboBox cannot do this: its delegate binds a single `root.font` for every row.
Item {
    id: root

    property string family: ""
    signal familyPicked(string family)

    readonly property string displayFamily: family === "" ? "Select a font" : family

    implicitHeight: 30
    implicitWidth: 200

    // One model shape whatever the source, so the delegate has a single contract: name is what the
    // user picks, previewFamily is the face to render it in, group is the section header.
    ListModel {
        id: familyModel
    }

    Component.onCompleted: {
        const catalog = EditorState.fontCatalog()
        for (let i = 0; i < catalog.length; ++i) {
            familyModel.append({
                "name": catalog[i].family,
                "previewFamily": catalog[i].qtFamily,
                "group": catalog[i].categoryLabel
            })
        }
        if (familyModel.count > 0)
            return

        // The bundle is fetched rather than committed, so fall back to system fonts when absent.
        const system = Qt.fontFamilies()
        for (let i = 0; i < system.length; ++i)
            familyModel.append({ "name": system[i], "previewFamily": system[i], "group": "System fonts" })
    }

    Rectangle {
        id: trigger
        anchors.fill: parent
        radius: Theme.radiusSm
        color: Theme.panelAccent
        border.width: popup.visible ? 1 : 0
        border.color: Theme.panelSecondaryBorder

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.right: chevron.left
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            text: root.displayFamily
            // Preview the current selection in the face it actually is.
            font.family: root.family === "" ? Theme.fontFamily : root.family
            font.pixelSize: Theme.fontSizeSm
            color: Theme.panelForeground
            elide: Text.ElideRight
        }

        IconGlyph {
            id: chevron
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            glyph: Theme.icons.chevronDown
            iconSize: 12
            iconColor: Theme.mutedForeground
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: popup.visible ? popup.close() : popup.open()
        }
    }

    Popup {
        id: popup
        y: root.height + 2
        width: root.width
        implicitHeight: 320
        padding: 1

        contentItem: ListView {
            id: list
            clip: true
            model: familyModel
            currentIndex: -1
            ScrollBar.vertical: AppScrollBar {}

            section.property: "group"
            section.delegate: Rectangle {
                required property string section
                width: list.width
                height: 24
                color: Theme.panelSecondaryBg

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: parent.section
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }
            }

            // Roles come in as required properties. Declaring any required property puts the
            // delegate in required-properties mode, where the `model` context object is not
            // injected — so every role the delegate reads has to be declared here.
            delegate: Rectangle {
                id: row
                required property string name
                required property string previewFamily

                width: list.width
                height: 34
                color: rowMouse.containsMouse ? Theme.panelAccent
                                              : (row.name === root.family ? Theme.panelSecondaryBg
                                                                          : "transparent")

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.name
                    font.family: row.previewFamily
                    font.pixelSize: 18
                    color: Theme.panelForeground
                    elide: Text.ElideRight
                }

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.familyPicked(row.name)
                        popup.close()
                    }
                }
            }
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: Theme.panelBackground
            border.width: 1
            border.color: Theme.panelBorder
        }
    }
}
