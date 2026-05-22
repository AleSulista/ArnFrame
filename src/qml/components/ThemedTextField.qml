import QtQuick
import QtQuick.Controls.Basic
import Drift

TextField {
    id: root

    color: Theme.panelForeground
    font.family: Theme.monoFontFamily
    font.pixelSize: Theme.fontSizeSm
    selectByMouse: true
    selectedTextColor: Theme.primaryForeground
    selectionColor: Theme.primary
    leftPadding: 8
    rightPadding: 8
    topPadding: 6
    bottomPadding: 6
    implicitHeight: 30

    background: Rectangle {
        radius: Theme.radiusSm
        color: Theme.panelAccent
        border.width: root.activeFocus ? 1 : 0
        border.color: Theme.panelSecondaryBorder
    }
}
