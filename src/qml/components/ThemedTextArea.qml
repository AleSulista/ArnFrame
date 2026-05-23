import QtQuick
import QtQuick.Controls.Basic
import Drift

// Multi-line text field matching ThemedTextField chrome.
TextArea {
    id: root

    color: Theme.panelForeground
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeSm
    wrapMode: TextArea.Wrap
    selectByMouse: true
    selectedTextColor: Theme.primaryForeground
    selectionColor: Theme.primary
    leftPadding: 8
    rightPadding: 8
    topPadding: 6
    bottomPadding: 6

    background: Rectangle {
        radius: Theme.radiusSm
        color: Theme.panelAccent
        border.width: root.activeFocus ? 1 : 0
        border.color: Theme.panelSecondaryBorder
    }
}
