import QtQuick
import QtQuick.Controls.Basic
import Drift

// Themed modal dialog chrome. Put page content in `contentItem`; use footer buttons
// via acceptText / rejectText (or set showFooter: false and supply your own footer).
Dialog {
    id: root

    property string acceptText: qsTr("OK")
    property string rejectText: qsTr("Cancel")
    property bool showFooter: true
    property bool showAccept: true
    property bool showReject: true
    property string acceptVariant: "primary"
    property string rejectVariant: "secondary"

    modal: true
    anchors.centerIn: Overlay.overlay
    standardButtons: Dialog.NoButton
    padding: 16
    width: 420

    background: Rectangle {
        color: Theme.panelBackground
        border.width: 1
        border.color: Theme.panelBorder
        radius: Theme.radiusMd
    }

    header: Item {
        height: root.title.length > 0 ? 44 : 0
        width: root.width
        visible: root.title.length > 0

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: Theme.panelForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBase
            font.weight: Font.Medium
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.panelBorder
        }
    }

    footer: Item {
        visible: root.showFooter
        height: visible ? 56 : 0
        width: root.width

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: Theme.panelBorder
            visible: root.showFooter
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            ThemedButton {
                visible: root.showReject
                text: root.rejectText
                variant: root.rejectVariant
                onClicked: root.reject()
            }

            ThemedButton {
                visible: root.showAccept
                text: root.acceptText
                variant: root.acceptVariant
                onClicked: root.accept()
            }
        }
    }
}
