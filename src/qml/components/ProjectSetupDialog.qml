import QtQuick
import QtQuick.Controls.Basic
import Drift

ThemedDialog {
    id: root

    property int assetIndex: -1
    property var pendingRunner: null
    property int outWidth: 1920
    property int outHeight: 1080
    property int outFps: 30
    property string aspectMode: "source"
    property string sourceName: ""

    title: qsTr("Project output setup")
    acceptText: qsTr("Create")
    width: 420

    function openForAsset(index, runner) {
        assetIndex = index
        pendingRunner = runner
        const suggested = EditorState.suggestedProjectSetupForAsset(index)
        outWidth = suggested.width || 1920
        outHeight = suggested.height || 1080
        outFps = suggested.fps || 30
        aspectMode = suggested.aspect || "source"
        sourceName = suggested.name || ""
        open()
    }

    function applyAspectPreset(mode) {
        aspectMode = mode
        if (mode === "16:9") {
            outHeight = Math.max(16, Math.round(outWidth * 9 / 16))
        } else if (mode === "9:16") {
            outHeight = Math.max(16, Math.round(outWidth * 16 / 9))
        } else if (mode === "4:3") {
            outHeight = Math.max(16, Math.round(outWidth * 3 / 4))
        } else if (mode === "1:1") {
            outHeight = outWidth
        } else if (mode === "source") {
            const suggested = EditorState.suggestedProjectSetupForAsset(assetIndex)
            outWidth = suggested.width || outWidth
            outHeight = suggested.height || outHeight
        }
    }

    onAccepted: {
        EditorState.setProjectSetup(outWidth, outHeight, outFps)
        if (typeof pendingRunner === "function")
            pendingRunner()
        pendingRunner = null
    }

    onRejected: {
        pendingRunner = null
    }

    contentItem: Column {
        spacing: 12
        width: parent ? parent.width : 400

        ThemedLabel {
            width: parent.width
            size: "sm"
            text: sourceName.length > 0
                  ? qsTr("First clip “%1”. Choose the canvas resolution before it is placed.").arg(sourceName)
                  : qsTr("Choose the canvas resolution before placing the first clip.")
        }

        ThemedLabel {
            text: qsTr("Aspect ratio")
        }

        Flow {
            width: parent.width
            spacing: 6

            Repeater {
                model: [
                    { id: "source", label: qsTr("Match clip") },
                    { id: "16:9", label: "16:9" },
                    { id: "9:16", label: "9:16" },
                    { id: "4:3", label: "4:3" },
                    { id: "1:1", label: "1:1" },
                    { id: "custom", label: qsTr("Custom") }
                ]

                delegate: ThemedChip {
                    required property var modelData
                    text: modelData.label
                    selected: root.aspectMode === modelData.id
                    chipHeight: 28
                    onClicked: root.applyAspectPreset(modelData.id)
                }
            }
        }

        Row {
            width: parent.width
            spacing: 8

            Column {
                width: (parent.width - parent.spacing) / 2
                spacing: 4
                ThemedLabel { text: qsTr("Width") }
                ThemedTextField {
                    width: parent.width
                    text: String(root.outWidth)
                    onEditingFinished: {
                        const v = parseInt(text, 10)
                        if (!isNaN(v) && v > 0) {
                            root.outWidth = v
                            if (root.aspectMode !== "custom" && root.aspectMode !== "source")
                                root.applyAspectPreset(root.aspectMode)
                            else
                                root.aspectMode = "custom"
                        }
                        text = String(root.outWidth)
                    }
                }
            }

            Column {
                width: (parent.width - parent.spacing) / 2
                spacing: 4
                ThemedLabel { text: qsTr("Height") }
                ThemedTextField {
                    width: parent.width
                    text: String(root.outHeight)
                    onEditingFinished: {
                        const v = parseInt(text, 10)
                        if (!isNaN(v) && v > 0) {
                            root.outHeight = v
                            root.aspectMode = "custom"
                        }
                        text = String(root.outHeight)
                    }
                }
            }
        }

        Column {
            width: parent.width
            spacing: 4
            ThemedLabel { text: qsTr("Frame rate") }
            ThemedTextField {
                width: parent.width / 2 - 4
                text: String(root.outFps)
                onEditingFinished: {
                    const v = parseInt(text, 10)
                    if (!isNaN(v) && v > 0)
                        root.outFps = v
                    text = String(root.outFps)
                }
            }
        }

        ThemedLabel {
            width: parent.width
            tone: "default"
            size: "sm"
            font.family: Theme.monoFontFamily
            text: qsTr("Output: %1×%2 @ %3 fps").arg(root.outWidth).arg(root.outHeight).arg(root.outFps)
        }
    }
}
