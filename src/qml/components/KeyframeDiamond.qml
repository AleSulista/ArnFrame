import QtQuick
import Drift

// Diamond keyframe toggle: filled when a keyframe exists at the current
// playhead position, empty otherwise. Click adds/removes a key; the owner
// decides what "add"/"remove" means for its property.
Item {
    id: root

    property bool hasKey: false
    signal toggled()

    width: 12
    height: 12

    Rectangle {
        anchors.centerIn: parent
        width: 9
        height: 9
        rotation: 45
        radius: 2
        color: root.hasKey ? Theme.primary : "transparent"
        border.width: 1.5
        border.color: Theme.primary
    }

    TapHandler {
        onTapped: root.toggled()
    }
}
