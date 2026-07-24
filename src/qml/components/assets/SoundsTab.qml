import QtQuick
import QtQuick.Controls.Basic
import Drift
import ".."

// Sounds tab: placeholder for the upcoming royalty-free sound effects library.
Item {
    id: root

    // Uses the shared empty state rather than bare centered text, so
    // it reads the same as every other "nothing here" surface.
    EmptyState {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.spacing3xl * 2, 260)
        glyph: Theme.icons.headphones
        title: qsTr("Sound effects")
        hint: qsTr("Royalty-free sound effects will appear here.")
    }
}
