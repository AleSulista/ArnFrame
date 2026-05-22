import QtQuick
import QtQuick.Controls.Basic
import Drift

ComboBox {
    id: root

    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeSm
    implicitHeight: 30
    leftPadding: 8
    rightPadding: 28

    contentItem: Text {
        leftPadding: root.leftPadding
        rightPadding: root.indicator.width + root.spacing
        text: root.displayText
        font: root.font
        color: Theme.panelForeground
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: Theme.panelAccent
        border.width: root.activeFocus || root.down ? 1 : 0
        border.color: Theme.panelSecondaryBorder
    }

    indicator: IconGlyph {
        x: root.width - width - 8
        y: root.topPadding + (root.availableHeight - height) / 2
        glyph: Theme.icons.chevronDown
        iconSize: 12
        iconColor: Theme.mutedForeground
    }

    popup: Popup {
        y: root.height + 2
        width: root.width
        implicitHeight: Math.min(240, contentItem.implicitHeight + 2)
        padding: 1

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            ScrollBar.vertical: AppScrollBar { }
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: Theme.panelBackground
            border.width: 1
            border.color: Theme.panelBorder
        }
    }

    delegate: ItemDelegate {
        width: root.width
        height: 28
        highlighted: root.highlightedIndex === index

        contentItem: Text {
            text: root.textRole ? (modelData[root.textRole] !== undefined
                                   ? modelData[root.textRole]
                                   : (model[root.textRole] !== undefined ? model[root.textRole] : modelData))
                                : modelData
            color: Theme.panelForeground
            font: root.font
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            leftPadding: 8
        }

        background: Rectangle {
            color: highlighted ? Theme.panelSecondaryBg : (hovered ? Theme.panelAccent : "transparent")
        }
    }
}
