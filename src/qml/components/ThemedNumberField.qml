import QtQuick
import QtQuick.Controls.Basic
import Drift

// Numeric inspector field: arrow up/down nudges the value; commits on focus loss
// or Enter. Avoid binding `text` to the model — set `value` from refresh logic
// while the field is not focused.
ThemedTextField {
    id: root

    property real value: 0
    property real from: -1e9
    property real to: 1e9
    property real step: 1
    property int decimals: 0

    signal edited(real value)

    inputMethodHints: Qt.ImhFormattedNumbersOnly

    function clamp(v) {
        return Math.min(to, Math.max(from, v))
    }

    function format(v) {
        if (decimals > 0)
            return Number(v).toFixed(decimals)
        return String(Math.round(v))
    }

    function applyValue(v) {
        const clamped = clamp(v)
        const changed = decimals === 0
                ? (Math.round(clamped) !== Math.round(value))
                : (Math.abs(clamped - value) > 1e-9)
        value = clamped
        text = format(clamped)
        if (changed)
            edited(clamped)
    }

    function parseAndCommit() {
        const v = parseFloat(text)
        if (isNaN(v)) {
            text = format(value)
            return
        }
        applyValue(v)
    }

    Keys.onUpPressed: function(event) {
        applyValue(value + step)
        event.accepted = true
    }

    Keys.onDownPressed: function(event) {
        applyValue(value - step)
        event.accepted = true
    }

    Keys.onPressed: function(event) {
        if (event.modifiers & Qt.ShiftModifier) {
            if (event.key === Qt.Key_Up) {
                applyValue(value + step * 10)
                event.accepted = true
            } else if (event.key === Qt.Key_Down) {
                applyValue(value - step * 10)
                event.accepted = true
            }
        }
    }

    onActiveFocusChanged: if (!activeFocus)
        parseAndCommit()
    onEditingFinished: parseAndCommit()

    onValueChanged: if (!activeFocus)
        text = format(value)

    Component.onCompleted: text = format(value)
}
