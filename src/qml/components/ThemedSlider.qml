import QtQuick
import QtQuick.Controls.Basic
import Drift

// Panel-themed slider that keeps parent Flickables from stealing mouse drags.
Slider {
    id: root

    property var _flickable: null
    property bool _flickableWasInteractive: true

    from: 0
    to: 1
    live: true
    implicitHeight: 22
    padding: 0
    topPadding: 0
    bottomPadding: 0

    function findFlickable(item) {
        var node = item
        while (node) {
            if (node.contentX !== undefined && node.interactive !== undefined && node.contentHeight !== undefined)
                return node
            node = node.parent
        }
        return null
    }

    onPressedChanged: {
        if (pressed) {
            _flickable = findFlickable(parent)
            if (_flickable) {
                _flickableWasInteractive = _flickable.interactive
                _flickable.interactive = false
            }
        } else if (_flickable) {
            _flickable.interactive = _flickableWasInteractive
            _flickable = null
        }
    }

    background: Rectangle {
        x: root.leftPadding
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: root.availableWidth
        height: 4
        radius: 2
        color: Theme.panelMuted

        Rectangle {
            width: root.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: Theme.primary
        }
    }

    handle: Rectangle {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: 14
        height: 14
        radius: 7
        color: root.pressed ? Theme.panelSecondaryForeground : Theme.primary
        border.width: 2
        border.color: Theme.primaryForeground
    }
}
