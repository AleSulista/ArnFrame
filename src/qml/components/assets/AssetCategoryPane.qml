import QtQuick
import QtQuick.Controls.Basic
import Drift
import ".."

// Full-height pane split: bottom-left star | vertical rule | all tab content on the right.
Item {
    id: root

    readonly property string favoritesId: "__favorites__"
    readonly property real favoritesGutterWidth:
        categories.length > 0 ? favColumn.width + sectionDivider.width : 0

    property var categories: []
    property string activeCategory: ""
    property bool searching: false

    signal categoryActivated(string categoryId)

    default property alias content: rightPane.children

    Row {
        anchors.fill: parent
        spacing: 0

        Item {
            id: favColumn
            width: Theme.iconButtonSize
            height: parent.height
            visible: root.categories.length > 0

            IconButton {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.bottomMargin: Theme.spacingSm
                glyph: Theme.icons.star
                variant: "ghost"
                active: root.activeCategory === root.favoritesId
                tooltip: qsTr("Favorites")
                onClicked: root.categoryActivated(root.favoritesId)
            }
        }

        Rectangle {
            id: sectionDivider
            width: root.categories.length > 0 ? Theme.borderWidth : 0
            height: parent.height
            visible: root.categories.length > 0
            color: Theme.panelBorder
        }

        Item {
            id: rightPane
            width: parent.width - favColumn.width - sectionDivider.width
            height: parent.height
        }
    }
}
