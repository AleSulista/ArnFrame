import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import Drift
import ".."

Item {
    id: root

    // Raised by the empty effects state; Main wires it to the Sounds library.
    signal browseAudioEffectsRequested()

    property int clipDataRevision: 0
    readonly property var clipData: {
        void clipDataRevision
        return EditorState.selectedClipData
    }
    readonly property bool hasSelection: !!clipData && Object.keys(clipData).length > 0
    readonly property string clipKind: hasSelection ? (clipData.kind || "") : ""
    readonly property bool hasAudio: clipKind === "audio" || clipKind === "video"
    readonly property var selectedAudioEffects: EditorState.selectedClipAudioEffects
    readonly property var audioFxCatalog: hasAudio ? EditorState.audioEffectCatalog() : []
    readonly property var propVolume: { "key": "volume", "label": "Volume", "def": 1.0, "decimals": 2 }

    height: audioTabColumn.height
    implicitHeight: audioTabColumn.height

    function refreshFields() {}

    Connections {
        target: EditorState
        function onSelectionChanged() { root.clipDataRevision++ }
        function onSelectedClipDataChanged() { root.clipDataRevision++ }
        function onTracksChanged() { root.clipDataRevision++ }
    }

    Column {
        id: audioTabColumn
        width: root.width
        spacing: Theme.spacingXl

        EmptyState {
            visible: root.clipKind !== "audio" && root.clipKind !== "video"
            width: parent.width
            compact: true
            glyph: Theme.icons.volumeOff
            title: qsTr("No audio")
            hint: qsTr("This clip has no audio track.")
        }

        PropertyKeyframeRow {
            width: root.width
            visible: root.clipKind === "audio" || root.clipKind === "video"
            propDef: root.propVolume
            keyframeList: (root.clipData.keyframes && root.clipData.keyframes.volume && root.clipData.keyframes.volume.points) || []
            useSlider: true
            sliderFrom: 0
            sliderTo: 2
            percent: true
        }

        Rectangle {
            visible: root.clipKind === "audio" || root.clipKind === "video"
            width: parent.width
            height: 1
            color: Theme.panelBorder
            opacity: 0.5
        }

        // ----- Noise removal ---------------------------------------------
        Column {
            id: denoiseSection
            width: parent.width
            spacing: Theme.spacingSm
            visible: root.clipKind === "audio" || root.clipKind === "video"

            // Whether the model is on disk is a one-shot filesystem answer, not a
            // binding, hence the reset below when an addon of this kind appears.
            // The runtime that runs it is a second, separate addon.
            property bool denoiseReady: EditorState.denoiseAvailable()
            property bool runtimeReady: Addons.runtimeAvailable()

            Connections {
                target: Addons
                function onKindChanged(kind) {
                    if (kind === "denoise-model")
                        denoiseSection.denoiseReady = EditorState.denoiseAvailable()
                    else if (kind === "onnxruntime")
                        denoiseSection.runtimeReady = Addons.runtimeAvailable()
                }
            }

            Text {
                width: parent.width
                text: qsTr("Noise")
                color: Theme.mutedForeground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
            }

            ThemedButton {
                visible: denoiseSection.denoiseReady && denoiseSection.runtimeReady
                width: parent.width
                text: qsTr("Remove noise…")
                enabled: !EditorState.denoising
                onClicked: {
                    const data = EditorState.selectedClipData
                    root.Window.window.openDenoise(
                        EditorState.selectedTrack, EditorState.selectedClip,
                        data.duration !== undefined ? data.duration : 0)
                }
            }

            ThemedButton {
                visible: !denoiseSection.denoiseReady || !denoiseSection.runtimeReady
                width: parent.width
                text: denoiseSection.runtimeReady
                      ? qsTr("Download noise removal (about 9 MB)")
                      : qsTr("Install AI engine first")
                variant: "primary"
                onClicked: root.Window.window.openAddonManager(
                    denoiseSection.runtimeReady ? "denoise-model" : "onnxruntime")
            }
        }

        Rectangle {
            visible: root.clipKind === "audio" || root.clipKind === "video"
            width: parent.width
            height: 1
            color: Theme.panelBorder
            opacity: 0.5
        }

        // ----- Audio effects (browse in Sounds; edit here) ---------------
        Text {
            visible: root.hasAudio
            text: qsTr("Audio effects")
            color: Theme.mutedForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
        }

        Text {
            visible: root.hasAudio && root.audioFxCatalog.length === 0
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("No audio effects installed. Get the Audio Effects pack from Extras.")
            color: Theme.mutedForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
        }

        ThemedButton {
            visible: root.hasAudio && root.audioFxCatalog.length === 0
            width: parent.width
            text: qsTr("Install audio effects")
            variant: "primary"
            onClicked: root.Window.window.openAddonManager("audio-effects")
        }

        EmptyState {
            width: parent.width
            visible: root.hasAudio && root.audioFxCatalog.length > 0
                     && root.selectedAudioEffects.length === 0
            glyph: Theme.icons.headphones
            title: qsTr("No audio effects yet")
            hint: qsTr("Drag a preset from the Sounds library onto this clip, or click a preset card.")
            actionText: qsTr("Browse sounds")
            onActionTriggered: root.browseAudioEffectsRequested()
        }

        // Integer models: previewSet* rebuilds selectedClipAudioEffects as a new
        // QVariantList on every tick. A list model would regenerate delegates and
        // destroy the pressed slider; a count only changes when effects are added/removed.
        Repeater {
            model: root.selectedAudioEffects.length
            delegate: Column {
                id: audioEffectCard
                required property int index
                readonly property var effectData: root.selectedAudioEffects[index] || ({})
                readonly property var effectParams: effectData.params || []
                readonly property bool effectEnabled: effectData.enabled !== false
                width: root.width
                spacing: 6

                Rectangle {
                    width: parent.width
                    height: audioEffectHeader.implicitHeight + 8
                    radius: Theme.radiusSm
                    color: Theme.panelAccent

                    Row {
                        id: audioEffectHeader
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 8
                        anchors.rightMargin: 4
                        spacing: 2

                        IconGlyph {
                            anchors.verticalCenter: parent.verticalCenter
                            glyph: audioEffectCard.effectData.icon || "audio-lines"
                            iconSize: 14
                            iconColor: Theme.mutedForeground
                            opacity: audioEffectCard.effectEnabled ? 1 : 0.5
                        }
                        Text {
                            text: audioEffectCard.effectData.missing
                                  ? qsTr("%1 (not installed)").arg(audioEffectCard.effectData.label)
                                  : audioEffectCard.effectData.label
                            color: audioEffectCard.effectEnabled
                                   ? Theme.panelForeground : Theme.mutedForeground
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            font.weight: Font.Medium
                            width: parent.width - 22 * 4 - 20 - 8
                            elide: Text.ElideRight
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        IconButton {
                            glyph: Theme.icons.chevronUp
                            variant: "ghost"
                            buttonSize: 22
                            iconSize: 12
                            enabled: audioEffectCard.index > 0
                            tooltip: qsTr("Move audio effect up")
                            onClicked: EditorState.moveAudioEffect(
                                           EditorState.selectedTrack, EditorState.selectedClip,
                                           audioEffectCard.index, audioEffectCard.index - 1)
                        }
                        IconButton {
                            glyph: Theme.icons.chevronDown
                            variant: "ghost"
                            buttonSize: 22
                            iconSize: 12
                            enabled: audioEffectCard.index < root.selectedAudioEffects.length - 1
                            tooltip: qsTr("Move audio effect down")
                            onClicked: EditorState.moveAudioEffect(
                                           EditorState.selectedTrack, EditorState.selectedClip,
                                           audioEffectCard.index, audioEffectCard.index + 1)
                        }
                        IconButton {
                            glyph: audioEffectCard.effectEnabled ? Theme.icons.eye : Theme.icons.eyeOff
                            variant: "ghost"
                            buttonSize: 22
                            iconSize: 12
                            tooltip: audioEffectCard.effectEnabled
                                     ? qsTr("Disable audio effect") : qsTr("Enable audio effect")
                            onClicked: EditorState.setAudioEffectEnabled(
                                           EditorState.selectedTrack, EditorState.selectedClip,
                                           audioEffectCard.index, !audioEffectCard.effectEnabled)
                        }
                        IconButton {
                            glyph: Theme.icons.x
                            variant: "ghost"
                            buttonSize: 22
                            iconSize: 12
                            tooltip: qsTr("Remove audio effect")
                            onClicked: EditorState.removeAudioEffect(
                                           EditorState.selectedTrack, EditorState.selectedClip,
                                           audioEffectCard.index)
                        }
                    }
                }

                Column {
                    width: parent.width
                    spacing: 4
                    opacity: audioEffectCard.effectEnabled ? 1 : 0.45

                    Repeater {
                        model: audioEffectCard.effectParams.length
                        delegate: Column {
                            id: audioParamRow
                            required property int index
                            readonly property var paramData: audioEffectCard.effectParams[index] || ({})
                            width: root.width
                            spacing: 4

                            Row {
                                width: parent.width
                                spacing: 8
                                Text {
                                    width: parent.width - 48
                                    elide: Text.ElideRight
                                    text: audioParamRow.paramData.label
                                    color: Theme.mutedForeground
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    width: 40
                                    horizontalAlignment: Text.AlignRight
                                    text: audioParamRow.paramData.isBoolean
                                          ? (audioParamRow.paramData.value ? qsTr("On") : qsTr("Off"))
                                          : Number(audioParamSlider.value).toFixed(
                                                Math.abs(audioParamRow.paramData.max - audioParamRow.paramData.min) >= 10 ? 1 : 2)
                                    color: Theme.panelForeground
                                    font.family: Theme.monoFontFamily
                                    font.pixelSize: Theme.fontSizeXs
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            ThemedSwitch {
                                visible: !!audioParamRow.paramData.isBoolean
                                checked: !!audioParamRow.paramData.value
                                onToggled: EditorState.previewSetAudioEffectParam(
                                               EditorState.selectedTrack, EditorState.selectedClip,
                                               audioEffectCard.index, audioParamRow.paramData.key,
                                               checked ? 1 : 0)
                            }

                            ThemedSlider {
                                id: audioParamSlider
                                label: audioParamRow.paramData.label
                                visible: !audioParamRow.paramData.isBoolean
                                width: parent.width
                                from: audioParamRow.paramData.min
                                to: audioParamRow.paramData.max
                                // Same pattern as PreviewPanel scrub: keep the model binding
                                // off while pressed so preview ticks cannot fight the drag.
                                Binding on value {
                                    when: !audioParamSlider.pressed
                                    value: audioParamRow.paramData.value
                                }
                                onMoved: EditorState.previewSetAudioEffectParam(
                                             EditorState.selectedTrack, EditorState.selectedClip,
                                             audioEffectCard.index, audioParamRow.paramData.key, value)
                                onPressedChanged: {
                                    if (pressed)
                                        EditorState.beginPreviewDrag(qsTr("Edit audio effect"))
                                    else
                                        EditorState.commitPreviewDrag()
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            visible: root.clipKind === "audio" || root.clipKind === "video"
            width: parent.width
            height: 1
            color: Theme.panelBorder
            opacity: 0.5
        }

        Text {
            visible: root.clipKind === "audio" || root.clipKind === "video"
            text: qsTr("Auto subtitles")
            color: Theme.mutedForeground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
        }

        // The transcriber is an addon, and so is the runtime it needs; without
        // both there are no languages to list and nothing to run, so offer the
        // download in place of the controls.
        property bool whisperReady: Addons.hasKind("whisper-model")
                                    && Addons.runtimeAvailable()
        property bool runtimeReady: Addons.runtimeAvailable()

        Connections {
            target: Addons
            function onKindChanged(kind) {
                if (kind !== "whisper-model" && kind !== "onnxruntime")
                    return
                const section = subtitleLanguageBox.parent
                section.runtimeReady = Addons.runtimeAvailable()
                section.whisperReady = Addons.hasKind("whisper-model")
                                       && section.runtimeReady
            }
        }

        ThemedComboBox {
            id: subtitleLanguageBox
            visible: parent.whisperReady
                     && (root.clipKind === "audio" || root.clipKind === "video")
            width: parent.width
            enabled: !EditorState.subtitleGenerating
            textRole: "label"
            valueRole: "code"
            model: EditorState.whisperLanguages()
            Component.onCompleted: currentIndex = 0
        }

        ThemedButton {
            visible: parent.whisperReady
                     && (root.clipKind === "audio" || root.clipKind === "video")
            width: parent.width
            text: EditorState.subtitleGenerating
                  ? qsTr("Creating captions… %1%").arg(Math.round(EditorState.subtitleGenProgress * 100))
                  : qsTr("Create captions from speech")
            enabled: !EditorState.subtitleGenerating
            onClicked: {
                const lang = subtitleLanguageBox.currentValue !== undefined
                             ? subtitleLanguageBox.currentValue
                             : ""
                EditorState.generateSubtitlesForClip(
                    EditorState.selectedTrack, EditorState.selectedClip, lang)
            }
        }

        ThemedButton {
            visible: !parent.whisperReady
                     && (root.clipKind === "audio" || root.clipKind === "video")
            width: parent.width
            text: parent.runtimeReady
                  ? qsTr("Download speech recognition (about 670 MB)")
                  : qsTr("Install AI engine first")
            variant: "primary"
            onClicked: root.Window.window.openAddonManager(
                parent.runtimeReady ? "whisper-model" : "onnxruntime")
        }
    }
}
