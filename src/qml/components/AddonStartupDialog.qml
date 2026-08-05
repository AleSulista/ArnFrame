import QtQuick
import QtQuick.Controls.Basic
import Drift

// Startup nudge: install the essential video / transitions / audio packs when they are not yet
// recorded as addons, and offer updates for anything already installed. Both sections honour a
// "don't remind me" preference so a later dismiss stays quiet across launches.
ThemedDialog {
    id: root

    property var essentialAddons: []
    property var updateAddons: []
    property var transfers: ({})
    property bool dontRemindEssential: false
    property bool dontRemindUpdates: false
    // Session latch so catalog churn after a dismiss does not reopen the dialog.
    property bool promptedThisSession: false

    readonly property bool showEssential: essentialAddons.length > 0
    readonly property bool showUpdates: updateAddons.length > 0
    readonly property var actionIds: {
        var ids = []
        var i
        for (i = 0; i < essentialAddons.length; ++i)
            ids.push(essentialAddons[i].id)
        for (i = 0; i < updateAddons.length; ++i)
            ids.push(updateAddons[i].id)
        return ids
    }

    title: {
        if (showEssential && showUpdates)
            return qsTr("Extra packs")
        if (showUpdates)
            return qsTr("Pack updates available")
        return qsTr("Recommended packs")
    }
    preferredWidth: Theme.dialogWidthMd
    showFooter: false
    acceptOnReturn: false

    // Returns true when the dialog was opened (or when there is nothing left to ask this session).
    function considerOpen() {
        if (promptedThisSession || visible)
            return promptedThisSession
        if (Addons.refreshing)
            return false

        essentialAddons = Addons.remindEssential ? Addons.missingEssentialAddons() : []
        updateAddons = Addons.remindUpdates ? Addons.updatableAddons() : []
        dontRemindEssential = false
        dontRemindUpdates = false
        transfers = ({})

        if (!showEssential && !showUpdates) {
            promptedThisSession = true
            return true
        }

        open()
        promptedThisSession = true
        return true
    }

    function persistReminders() {
        if (dontRemindEssential && showEssential)
            Addons.remindEssential = false
        if (dontRemindUpdates && showUpdates)
            Addons.remindUpdates = false
    }

    function installListed() {
        persistReminders()
        for (var i = 0; i < actionIds.length; ++i)
            Addons.install(actionIds[i])
        close()
    }

    function dismiss() {
        persistReminders()
        close()
    }

    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: root.visible
        onActivated: root.installListed()
    }

    Connections {
        target: Addons
        function onProgressChanged(id, fraction, phase) {
            var next = root.transfers
            next[id] = { fraction: fraction, phase: phase }
            root.transfers = next
        }
    }

    contentItem: Column {
        spacing: Theme.spacingLg
        width: parent ? parent.width : Theme.dialogWidthMd

        ThemedLabel {
            width: parent.width
            size: "sm"
            visible: root.showEssential
            text: qsTr("Install the essential packs for effects, transitions, and audio. "
                       + "You can keep using Drift without them — installing unlocks updates "
                       + "when they improve.")
        }

        Repeater {
            model: root.essentialAddons

            Rectangle {
                id: essentialRow

                required property var modelData

                readonly property var transfer: root.transfers[modelData.id]

                width: parent ? parent.width : 0
                height: essentialBody.implicitHeight + Theme.spacingXl
                radius: Theme.radiusMd
                color: Theme.panelSecondaryBg
                border.width: Theme.borderWidth
                border.color: Theme.panelSecondaryBorder

                Column {
                    id: essentialBody
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: Theme.spacingXl
                    anchors.rightMargin: Theme.spacingXl
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.spacingXs

                    ThemedLabel {
                        width: parent.width
                        text: essentialRow.modelData.name
                        tone: "default"
                        size: "sm"
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                    }

                    ThemedLabel {
                        width: parent.width
                        text: essentialRow.transfer
                              ? qsTr("%1… %2%").arg(essentialRow.transfer.phase)
                                                .arg(Math.round(essentialRow.transfer.fraction * 100))
                              : (essentialRow.modelData.description || essentialRow.modelData.id)
                        elide: Text.ElideRight
                    }
                }
            }
        }

        ThemedCheckBox {
            width: parent.width
            visible: root.showEssential
            checked: root.dontRemindEssential
            text: qsTr("Don't remind me of essential addons")
            onToggled: root.dontRemindEssential = checked
        }

        Rectangle {
            width: parent.width
            height: Theme.borderWidth
            color: Theme.panelBorder
            visible: root.showEssential && root.showUpdates
        }

        ThemedLabel {
            width: parent.width
            size: "sm"
            visible: root.showUpdates
            text: qsTr("Updates are available for packs you already have installed.")
        }

        Repeater {
            model: root.updateAddons

            Rectangle {
                id: updateRow

                required property var modelData

                readonly property var transfer: root.transfers[modelData.id]

                width: parent ? parent.width : 0
                height: updateBody.implicitHeight + Theme.spacingXl
                radius: Theme.radiusMd
                color: Theme.panelSecondaryBg
                border.width: Theme.borderWidth
                border.color: Theme.panelSecondaryBorder

                Column {
                    id: updateBody
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: Theme.spacingXl
                    anchors.rightMargin: Theme.spacingXl
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.spacingXs

                    ThemedLabel {
                        width: parent.width
                        text: updateRow.modelData.name
                        tone: "default"
                        size: "sm"
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                    }

                    ThemedLabel {
                        width: parent.width
                        text: {
                            if (updateRow.transfer)
                                return qsTr("%1… %2%").arg(updateRow.transfer.phase)
                                                      .arg(Math.round(updateRow.transfer.fraction * 100))
                            return qsTr("%1 → %2").arg(updateRow.modelData.installedVersion)
                                                  .arg(updateRow.modelData.version)
                        }
                        elide: Text.ElideRight
                    }
                }
            }
        }

        ThemedCheckBox {
            width: parent.width
            visible: root.showUpdates
            checked: root.dontRemindUpdates
            text: qsTr("Don't remind me of future addon updates")
            onToggled: root.dontRemindUpdates = checked
        }

        Item {
            width: parent.width
            height: installButton.height

            ThemedButton {
                anchors.left: parent.left
                variant: "ghost"
                text: qsTr("Later")
                onClicked: root.dismiss()
            }

            ThemedButton {
                id: installButton
                anchors.right: parent.right
                variant: "primary"
                glyph: Theme.icons.download
                text: {
                    if (root.showEssential && root.showUpdates)
                        return qsTr("Install & update")
                    if (root.showUpdates)
                        return root.updateAddons.length === 1 ? qsTr("Update") : qsTr("Update all")
                    return root.essentialAddons.length === 1 ? qsTr("Install") : qsTr("Install all")
                }
                onClicked: root.installListed()
            }
        }
    }

    onOpened: installButton.forceActiveFocus()
}
