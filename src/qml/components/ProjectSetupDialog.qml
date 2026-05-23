import QtQuick
import QtQuick.Controls.Basic
import Drift

Dialog {
    id: root

    property int assetIndex: -1
    property var pendingRunner: null
    property int outWidth: 1920
    property int outHeight: 1080
    property int outFps: 30
    property string aspectMode: "source"
    property string sourceName: ""

    modal: true
    anchors.centerIn: Overlay.overlay
    title: qsTr("Project output setup")
    standardButtons: Dialog.Ok | Dialog.Cancel
    width: 420
    padding: 16

    background: Rectangle {
        color: Theme.panelBg
        border.width: 1
        border.color: Theme.panelBorder
        radius: Theme.radiusMd
    }

    header: Item {
        height: 44
        width: root.width
        Text {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: Theme.panelForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.Medium
        }
    }

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

        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            text: sourceName.length > 0
                  ? qsTr("First clip “%1”. Choose the canvas resolution before it is placed.").arg(sourceName)
                  : qsTr("Choose the canvas resolution before placing the first clip.")
            color: Theme.mutedForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
        }

        Text {
            text: qsTr("Aspect ratio")
            color: Theme.mutedForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
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

                delegate: Rectangle {
                    required property var modelData
                    width: labelText.implicitWidth + 16
                    height: 28
                    radius: Theme.radiusSm
                    color: root.aspectMode === modelData.id ? Theme.primary : Theme.panelAccent
                    border.width: 1
                    border.color: Theme.panelBorder

                    Text {
                        id: labelText
                        anchors.centerIn: parent
                        text: modelData.label
                        color: root.aspectMode === modelData.id ? "#ffffff" : Theme.panelForeground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.applyAspectPreset(modelData.id)
                    }
                }
            }
        }

        Row {
            width: parent.width
            spacing: 8

            Column {
                width: (parent.width - parent.spacing) / 2
                spacing: 4
                Text {
                    text: qsTr("Width")
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }
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
                Text {
                    text: qsTr("Height")
                    color: Theme.mutedForeground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                }
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
            Text {
                text: qsTr("Frame rate")
                color: Theme.mutedForeground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
            }
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

        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("Output: %1×%2 @ %3 fps").arg(root.outWidth).arg(root.outHeight).arg(root.outFps)
            color: Theme.panelForeground
            font.family: Theme.monoFontFamily
            font.pixelSize: Theme.fontSizeSm
        }
    }
}
