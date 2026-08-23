import QtQuick
import QtQuick.Controls.Basic
import Drift

// Name prompt shared by "save this clip's style" and "rename a saved style". Both are a single
// short field, so they are one dialog rather than two near-identical ones.
ThemedDialog {
    id: root

    // Emitted with the trimmed name; never fires empty.
    signal submitted(string name)

    acceptText: qsTr("Save")
    preferredWidth: Theme.dialogWidthSm

    function openWith(dialogTitle, initialName) {
        root.title = dialogTitle
        nameField.text = initialName
        open()
        nameField.forceActiveFocus()
        nameField.selectAll()
    }

    onAccepted: {
        const name = nameField.text.trim()
        if (name.length > 0)
            root.submitted(name)
    }

    contentItem: Column {
        spacing: Theme.spacingLg
        width: parent ? parent.width : 320

        ThemedLabel {
            text: qsTr("Name")
        }

        ThemedTextField {
            id: nameField
            width: parent.width
            placeholderText: qsTr("My style")
        }
    }
}
