import QtQuick
import QtQuick.Controls.Basic
import Drift
import ".."

// Horizontal category chips (no star — that lives in AssetCategoryPane).
Item {
    id: root

    property var categories: []
    property string activeCategory: ""
    property bool searching: false

    signal categoryActivated(string categoryId)

    width: parent ? parent.width : 0
    height: visible ? Theme.controlHeightSm : 0
    visible: !searching && categories.length > 0

    Flickable {
        anchors.fill: parent
        contentWidth: categoryRow.width
        flickableDirection: Flickable.HorizontalFlick
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentWidth > width

        Row {
            id: categoryRow
            height: parent.height
            spacing: Theme.spacingSm

            Repeater {
                model: root.categories
                delegate: ThemedChip {
                    required property var modelData
                    required property int index
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.label
                    variant: "secondary"
                    accentColor: Theme.categoryColor(index)
                    selected: root.activeCategory === modelData.id
                    onClicked: root.categoryActivated(modelData.id)
                }
            }
        }
    }
}
