import QtQuick
import QtQuick.Controls.Basic
import Drift

// Browse, install and remove addons. Content that used to be baked into the build — fonts,
// stickers, the Whisper model — is downloaded from here instead.
ThemedDialog {
    id: root

    title: qsTr("Addons")
    width: 660
    showAccept: false
    rejectText: qsTr("Close")

    // Which addon to scroll to and highlight when opened from an empty state.
    property string highlightId: ""
    property string kindFilter: "all"

    // id -> { fraction, phase }. Rebuilt wholesale on each signal so the bindings re-evaluate.
    property var transfers: ({})

    function openForKind(kind) {
        root.kindFilter = "all"
        root.highlightId = Addons.firstAddonForKind(kind)
        root.open()
    }

    function formatSize(bytes) {
        if (bytes >= 1e9)
            return (bytes / 1e9).toFixed(1) + " GB"
        if (bytes >= 1e6)
            return Math.round(bytes / 1e6) + " MB"
        return Math.max(1, Math.round(bytes / 1e3)) + " KB"
    }

    onOpened: Addons.refresh(true)

    Connections {
        target: Addons
        function onProgressChanged(id, fraction, phase) {
            var next = root.transfers
            next[id] = { fraction: fraction, phase: phase }
            root.transfers = next
        }
    }

    contentItem: Item {
        implicitHeight: 460

        Row {
            id: filters
            spacing: 6
            anchors.top: parent.top
            anchors.left: parent.left

            Repeater {
                model: [
                    { id: "all", label: qsTr("All") },
                    { id: "fonts", label: qsTr("Fonts") },
                    { id: "stickers", label: qsTr("Stickers") },
                    { id: "whisper-model", label: qsTr("AI models") }
                ]

                ThemedChip {
                    text: modelData.label
                    selected: root.kindFilter === modelData.id
                    onClicked: root.kindFilter = modelData.id
                }
            }
        }

        Text {
            id: statusLine
            anchors.top: filters.bottom
            anchors.topMargin: 10
            anchors.left: parent.left
            anchors.right: parent.right
            visible: Addons.status.length > 0 || Addons.refreshing
            text: Addons.refreshing ? qsTr("Checking for addons…") : Addons.status
            color: Addons.refreshing ? Theme.mutedForeground : Theme.destructive
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            wrapMode: Text.WordWrap
        }

        ListView {
            id: list
            anchors.top: statusLine.visible ? statusLine.bottom : filters.bottom
            anchors.topMargin: 12
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true
            spacing: 8
            model: Addons.catalog.filter(function (addon) {
                return root.kindFilter === "all" || addon.kind === root.kindFilter
            })

            ScrollBar.vertical: AppScrollBar {}

            Text {
                anchors.centerIn: parent
                visible: list.count === 0 && !Addons.refreshing
                text: Addons.status.length > 0
                      ? qsTr("No addons to show while offline.")
                      : qsTr("No addons in this category.")
                color: Theme.mutedForeground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
            }

            delegate: Rectangle {
                id: row

                required property var modelData

                readonly property var transfer: root.transfers[modelData.id]
                readonly property bool active: modelData.state === "downloading"
                                               || modelData.state === "installing"

                width: ListView.view.width
                height: body.implicitHeight + 24
                radius: Theme.radiusMd
                color: Theme.panelSecondaryBg
                border.width: 1
                border.color: modelData.id === root.highlightId ? Theme.primary : Theme.panelSecondaryBorder

                Column {
                    id: body
                    anchors.left: parent.left
                    anchors.right: actions.left
                    anchors.leftMargin: 14
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Row {
                        spacing: 8

                        ThemedLabel {
                            text: row.modelData.name
                            tone: "default"
                            size: "base"
                            font.weight: Font.Medium
                        }

                        ThemedLabel {
                            anchors.verticalCenter: parent.verticalCenter
                            text: row.modelData.state === "update-available"
                                  ? qsTr("%1 → %2").arg(row.modelData.installedVersion).arg(row.modelData.version)
                                  : row.modelData.version
                        }
                    }

                    ThemedLabel {
                        width: body.width
                        text: row.modelData.description
                        size: "sm"
                    }

                    ThemedLabel {
                        text: {
                            if (row.active && row.transfer)
                                return qsTr("%1… %2%").arg(row.transfer.phase)
                                                      .arg(Math.round(row.transfer.fraction * 100))
                            if (row.modelData.state === "failed")
                                return row.modelData.error
                            var parts = [qsTr("%1 download").arg(root.formatSize(row.modelData.downloadSize))]
                            if (row.modelData.items > 0)
                                parts.push(qsTr("%1 items").arg(row.modelData.items))
                            if (row.modelData.license.length > 0)
                                parts.push(row.modelData.license)
                            return parts.join(" · ")
                        }
                        color: row.modelData.state === "failed" ? Theme.destructive : Theme.mutedForeground
                    }
                }

                Row {
                    id: actions
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    CircularProgress {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: row.active
                        // Download and extraction each report their own 0..1, so the ring restarts
                        // when the phase changes.
                        value: row.transfer ? row.transfer.fraction : 0
                    }

                    ThemedButton {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: row.active
                        text: qsTr("Cancel")
                        variant: "ghost"
                        onClicked: Addons.cancel(row.modelData.id)
                    }

                    ThemedButton {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !row.active && row.modelData.state !== "installed"
                        text: row.modelData.state === "update-available" ? qsTr("Update")
                            : row.modelData.state === "failed" ? qsTr("Retry")
                            : qsTr("Install")
                        variant: "primary"
                        onClicked: Addons.install(row.modelData.id)
                    }

                    ThemedButton {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !row.active && row.modelData.installedVersion.length > 0
                        text: qsTr("Remove")
                        variant: "ghost"
                        onClicked: Addons.uninstall(row.modelData.id)
                    }
                }
            }
        }
    }
}
