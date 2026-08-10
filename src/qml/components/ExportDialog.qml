import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Drift

ThemedDialog {
    id: root

    title: qsTr("Export")
    acceptText: qsTr("Export")
    preferredWidth: Theme.dialogWidthLg

    property string exportMode: "video" // "video" | "audio"
    property string scaleId: "source"
    property int targetHeight: 0
    property int videoBitrateKbps: 12000
    property string videoCodecId: "h264"
    property string rateControl: "crf"
    property int crf: 18
    property string videoPreset: "medium"
    property string audioCodecId: "aac"
    property int audioBitrateKbps: 192
    property bool advancedOpen: false

    // Bitrate combo: preset kbps values, or "custom"
    property string audioBitrateChoice: "192"
    readonly property var audioBitratePresets: [96, 128, 160, 192, 256, 320]
    readonly property var audioBitrateModel: [
        { label: qsTr("96 kbps"), value: "96" },
        { label: qsTr("128 kbps"), value: "128" },
        { label: qsTr("160 kbps"), value: "160" },
        { label: qsTr("192 kbps"), value: "192" },
        { label: qsTr("256 kbps"), value: "256" },
        { label: qsTr("320 kbps"), value: "320" },
        { label: qsTr("Custom"), value: "custom" }
    ]

    property var videoCodecs: []
    property var audioCodecs: []
    property var scaleOptions: []
    property var currentVideoCodec: ({})
    property var currentAudioCodec: ({})

    readonly property bool videoLossless: !!(currentVideoCodec && currentVideoCodec.lossless)
    readonly property bool videoSupportsCrf: !!(currentVideoCodec && currentVideoCodec.supportsCrf)
    readonly property bool videoSupportsBitrate: !!(currentVideoCodec && currentVideoCodec.supportsBitrate)
    readonly property bool videoSupportsPreset: !!(currentVideoCodec && currentVideoCodec.supportsPreset)
    readonly property var videoPresetList: (currentVideoCodec && currentVideoCodec.presets) ? currentVideoCodec.presets : []
    readonly property bool audioLossless: !!(currentAudioCodec && currentAudioCodec.lossless)
    readonly property bool isAudioOnly: exportMode === "audio"

    function codecById(list, id) {
        for (var i = 0; i < list.length; ++i) {
            if (list[i].id === id)
                return list[i]
        }
        return null
    }

    function firstAvailableId(list, preferred) {
        var pref = codecById(list, preferred)
        if (pref && pref.available)
            return preferred
        for (var i = 0; i < list.length; ++i) {
            if (list[i].available)
                return list[i].id
        }
        return preferred
    }

    function syncAudioBitrateChoice() {
        var s = String(audioBitrateKbps)
        for (var i = 0; i < audioBitratePresets.length; ++i) {
            if (audioBitratePresets[i] === audioBitrateKbps) {
                audioBitrateChoice = s
                return
            }
        }
        audioBitrateChoice = "custom"
    }

    function applyAudioBitrateChoice(choice) {
        audioBitrateChoice = choice
        if (choice !== "custom") {
            var n = parseInt(choice, 10)
            if (!isNaN(n))
                audioBitrateKbps = n
        }
    }

    function refreshCodecMeta() {
        currentVideoCodec = codecById(videoCodecs, videoCodecId) || {}
        currentAudioCodec = codecById(audioCodecs, audioCodecId) || {}
        if (videoLossless)
            rateControl = "crf"
        else if (!videoSupportsCrf && videoSupportsBitrate)
            rateControl = "bitrate"
        else if (videoSupportsCrf && rateControl !== "bitrate")
            rateControl = "crf"
    }

    function applyVideoCodecDefaults(codec) {
        if (!codec)
            return
        if (codec.defaultCrf !== undefined)
            crf = codec.defaultCrf
        if (codec.defaultPreset)
            videoPreset = codec.defaultPreset
        if (codec.lossless)
            rateControl = "crf"
        else if (!codec.supportsCrf && codec.supportsBitrate)
            rateControl = "bitrate"
        else if (codec.supportsCrf)
            rateControl = "crf"
    }

    function openDialog() {
        exportMode = "video"
        videoCodecs = EditorState.exportVideoCodecs()
        audioCodecs = EditorState.exportAudioCodecs()
        scaleOptions = EditorState.exportScaleOptions()

        var defaults = EditorState.exportDefaultSettings()
        var remembered = EditorState.lastExportSettings()
        var hasRemembered = !!(remembered
                               && (remembered.scaleId || remembered.videoCodecId
                                   || remembered.audioCodecId))
        var src = hasRemembered ? remembered : defaults

        exportMode = src.audioOnly ? "audio" : "video"
        videoCodecId = firstAvailableId(videoCodecs, src.videoCodecId || defaults.videoCodecId || "h264")
        audioCodecId = firstAvailableId(audioCodecs, src.audioCodecId || defaults.audioCodecId || "aac")
        rateControl = src.rateControl || defaults.rateControl || "crf"
        crf = (src.crf !== undefined && src.crf !== null) ? src.crf : (defaults.crf || 18)
        videoBitrateKbps = src.videoBitrateKbps || defaults.videoBitrateKbps || 12000
        videoPreset = src.videoPreset || defaults.videoPreset || "medium"
        audioBitrateKbps = src.audioBitrateKbps || defaults.audioBitrateKbps || 192
        syncAudioBitrateChoice()
        advancedOpen = false

        var meta = EditorState.projectMetadata || {}
        tagTitleField.text = meta.title || EditorState.projectName || ""
        tagArtistField.text = meta.author || ""
        tagAlbumField.text = ""
        tagCommentField.text = meta.description || ""

        var restoredScale = false
        if (hasRemembered && src.scaleId && scaleOptions.length > 0) {
            for (var i = 0; i < scaleOptions.length; ++i) {
                if (scaleOptions[i].id === src.scaleId) {
                    scaleId = scaleOptions[i].id
                    targetHeight = scaleOptions[i].targetHeight
                    // Keep the remembered bitrate when Advanced was customized; otherwise use
                    // the chip's suggested bitrate for that scale.
                    if (src.videoBitrateKbps === undefined || src.videoBitrateKbps === null)
                        videoBitrateKbps = scaleOptions[i].videoBitrateKbps || videoBitrateKbps
                    restoredScale = true
                    break
                }
            }
        }
        if (!restoredScale) {
            if (scaleOptions.length > 0) {
                scaleId = scaleOptions[0].id
                targetHeight = scaleOptions[0].targetHeight
                if (!hasRemembered && scaleOptions[0].videoBitrateKbps)
                    videoBitrateKbps = scaleOptions[0].videoBitrateKbps
            } else {
                scaleId = "source"
                targetHeight = 0
            }
        }

        // Codec catalog defaults only on a fresh dialog — otherwise they wipe the remembered CRF.
        if (!hasRemembered)
            applyVideoCodecDefaults(codecById(videoCodecs, videoCodecId))
        refreshCodecMeta()
        syncComboIndices()
        open()
    }

    function syncComboIndices() {
        if (videoCodecCombo)
            videoCodecCombo.currentIndex = Math.max(0, videoCodecCombo.indexOfValue(videoCodecId))
        if (audioCodecCombo)
            audioCodecCombo.currentIndex = Math.max(0, audioCodecCombo.indexOfValue(audioCodecId))
        if (audioOnlyCodecCombo)
            audioOnlyCodecCombo.currentIndex = Math.max(0, audioOnlyCodecCombo.indexOfValue(audioCodecId))
        if (audioBitrateCombo)
            audioBitrateCombo.currentIndex = Math.max(0, audioBitrateCombo.indexOfValue(audioBitrateChoice))
        if (audioOnlyBitrateCombo)
            audioOnlyBitrateCombo.currentIndex = Math.max(0, audioOnlyBitrateCombo.indexOfValue(audioBitrateChoice))
        if (videoSupportsPreset && videoPresetList.length > 0 && presetCombo) {
            var pi = videoPresetList.indexOf(videoPreset)
            presetCombo.currentIndex = pi >= 0 ? pi : Math.max(0, videoPresetList.indexOf(
                                                                   currentVideoCodec.defaultPreset || ""))
            if (presetCombo.currentIndex >= 0 && presetCombo.currentIndex < videoPresetList.length)
                videoPreset = videoPresetList[presetCombo.currentIndex]
        }
    }

    function buildSettings() {
        var s = {
            "scaleId": scaleId,
            "targetHeight": targetHeight,
            "videoCodecId": videoCodecId,
            "rateControl": videoLossless ? "crf" : rateControl,
            "crf": crf,
            "videoBitrateKbps": videoBitrateKbps,
            "videoPreset": videoPreset,
            "audioCodecId": audioCodecId,
            "audioBitrateKbps": audioBitrateKbps,
            "audioOnly": isAudioOnly
        }
        if (isAudioOnly) {
            if (tagTitleField.text.length)
                s.metadataTitle = tagTitleField.text
            if (tagArtistField.text.length)
                s.metadataArtist = tagArtistField.text
            if (tagAlbumField.text.length)
                s.metadataAlbum = tagAlbumField.text
            if (tagCommentField.text.length)
                s.metadataComment = tagCommentField.text
        }
        return s
    }

    onAccepted: {
        var container = isAudioOnly
                ? EditorState.exportPreferredAudioOnlyContainer(audioCodecId)
                : EditorState.exportPreferredContainer(videoCodecId, audioCodecId)
        var filters = EditorState.exportSaveFilters(container, isAudioOnly)
        var suffix = EditorState.exportDefaultSuffix(container, isAudioOnly)
        var dialogTitle = isAudioOnly ? qsTr("Export Audio") : qsTr("Export Video")
        var url = FileDialogs.saveFile(dialogTitle, filters,
                                       EditorState.projectName, suffix,
                                       EditorState.lastExportFolder())
        if (url != "") {
            EditorState.exportWithSettings(url, buildSettings())
            Toasts.info(qsTr("Export started…"))
        } else {
            Toasts.info(qsTr("Export cancelled."))
        }
    }

    // With Advanced open this content is ~800px tall against a 560px minimum
    // window height, so it has to scroll: unscrolled, the dialog overflowed the
    // window and took the Export/Cancel buttons off-screen with it.
    contentItem: Flickable {
        id: contentFlick
        width: parent ? parent.width : Theme.dialogWidthLg
        implicitHeight: Math.min(exportColumn.height, root.availableContentHeight)
        contentWidth: width
        contentHeight: exportColumn.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentHeight > height
        ScrollBar.vertical: AppScrollBar {
            policy: contentFlick.contentHeight > contentFlick.height
                    ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
        }

        Column {
            id: exportColumn
            spacing: Theme.spacingXl
            width: contentFlick.width

            Row {
                spacing: Theme.spacingMd

                ThemedToggleButton {
                    text: qsTr("Video")
                    checked: root.exportMode === "video"
                    onClicked: {
                        if (root.exportMode === "video")
                            return
                        root.exportMode = "video"
                        root.advancedOpen = false
                        root.syncComboIndices()
                    }
                }

                ThemedToggleButton {
                    text: qsTr("Audio")
                    checked: root.exportMode === "audio"
                    onClicked: {
                        if (root.exportMode === "audio")
                            return
                        root.exportMode = "audio"
                        root.advancedOpen = false
                        root.syncComboIndices()
                    }
                }
            }

            // ---- Video mode ----
            Column {
                width: parent.width
                spacing: Theme.spacingXl
                visible: !root.isAudioOnly
                height: visible ? implicitHeight : 0
                clip: true

                ThemedLabel {
                    width: parent.width
                    size: "sm"
                    text: qsTr("Saves what you see in the preview. Pick a size — the picture shape stays the same.")
                }

                ThemedLabel {
                    text: qsTr("Downscale")
                }

                Flow {
                    width: parent.width
                    spacing: Theme.spacingMd

                    Repeater {
                        model: root.scaleOptions

                        delegate: ThemedChip {
                            required property var modelData
                            text: modelData.label
                            selected: root.scaleId === modelData.id
                            tooltip: qsTr("Export at %1×%2").arg(modelData.width).arg(modelData.height)
                            onClicked: {
                                root.scaleId = modelData.id
                                root.targetHeight = modelData.targetHeight
                                if (modelData.videoBitrateKbps)
                                    root.videoBitrateKbps = modelData.videoBitrateKbps
                            }
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: advancedHeader.implicitHeight

                    Row {
                        id: advancedHeader
                        spacing: Theme.spacingSm
                        anchors.left: parent.left

                        IconGlyph {
                            anchors.verticalCenter: parent.verticalCenter
                            glyph: Theme.icons.chevronRight
                            iconSize: Theme.iconSizeSm
                            iconColor: Theme.mutedForeground
                            rotation: root.advancedOpen ? 90 : 0
                            Behavior on rotation {
                                NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                            }
                        }

                        ThemedLabel {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Advanced")
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.advancedOpen = !root.advancedOpen
                    }

                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Advanced")
                    Accessible.checkable: true
                    Accessible.checked: root.advancedOpen
                    Accessible.onPressAction: root.advancedOpen = !root.advancedOpen
                }

                Column {
                    width: parent.width
                    spacing: Theme.spacingXl
                    visible: root.advancedOpen
                    height: visible ? implicitHeight : 0
                    clip: true

                    Column {
                        width: parent.width
                        spacing: Theme.spacingSm

                        ThemedLabel {
                            text: qsTr("Video encoder")
                            size: "sm"
                        }

                        ThemedComboBox {
                            id: videoCodecCombo
                            width: parent.width
                            textRole: "label"
                            valueRole: "id"
                            model: root.videoCodecs
                            onActivated: function (index) {
                                var item = root.videoCodecs[index]
                                if (!item || !item.available) {
                                    currentIndex = Math.max(0, indexOfValue(root.videoCodecId))
                                    return
                                }
                                root.videoCodecId = item.id
                                root.applyVideoCodecDefaults(item)
                                root.refreshCodecMeta()
                                root.syncComboIndices()
                            }
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: Theme.spacingMd
                        visible: !root.videoLossless

                        Row {
                            spacing: Theme.spacingMd

                            ThemedToggleButton {
                                text: qsTr("Constant Quality")
                                checked: root.rateControl === "crf"
                                enabled: root.videoSupportsCrf
                                onClicked: {
                                    if (root.videoSupportsCrf)
                                        root.rateControl = "crf"
                                }
                            }

                            ThemedToggleButton {
                                text: qsTr("Bitrate")
                                checked: root.rateControl === "bitrate"
                                enabled: root.videoSupportsBitrate
                                onClicked: {
                                    if (root.videoSupportsBitrate)
                                        root.rateControl = "bitrate"
                                }
                            }
                        }

                        Column {
                            width: parent.width
                            spacing: Theme.spacingSm
                            visible: root.rateControl === "crf" && root.videoSupportsCrf

                            Row {
                                width: parent.width
                                ThemedLabel {
                                    text: qsTr("RF %1").arg(root.crf)
                                    size: "sm"
                                }
                            }

                            ThemedSlider {
                                width: parent.width
                                label: qsTr("Quality (RF)")
                                from: 0
                                to: 51
                                stepSize: 1
                                value: root.crf
                                valueFormatter: function (v) { return qsTr("RF %1").arg(Math.round(v)) }
                                onMoved: root.crf = Math.round(value)
                            }

                            RowLayout {
                                width: parent.width

                                ThemedLabel {
                                    text: qsTr("Higher quality")
                                    size: "xs"
                                }

                                Item { Layout.fillWidth: true }

                                ThemedLabel {
                                    text: qsTr("Lower quality")
                                    size: "xs"
                                }
                            }
                        }

                        RowLayout {
                            width: parent.width
                            spacing: Theme.spacingMd
                            visible: root.rateControl === "bitrate" && root.videoSupportsBitrate

                            ThemedLabel {
                                text: qsTr("Bitrate (kbps)")
                                size: "sm"
                            }

                            ThemedNumberField {
                                Layout.preferredWidth: 120
                                from: 100
                                to: 100000
                                step: 100
                                unit: qsTr("kbps")
                                value: root.videoBitrateKbps
                                onEdited: function (v) { root.videoBitrateKbps = Math.round(v) }
                            }
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: Theme.spacingSm
                        visible: root.videoSupportsPreset && root.videoPresetList.length > 0

                        ThemedLabel {
                            text: qsTr("Preset")
                            size: "sm"
                        }

                        ThemedComboBox {
                            id: presetCombo
                            width: parent.width
                            model: root.videoPresetList
                            onActivated: function (index) {
                                if (index >= 0 && index < root.videoPresetList.length)
                                    root.videoPreset = root.videoPresetList[index]
                            }
                        }
                    }

                    GridLayout {
                        width: parent.width
                        columns: 2
                        columnSpacing: Theme.spacingLg
                        rowSpacing: Theme.spacingSm

                        ThemedLabel {
                            text: qsTr("Audio encoder")
                            size: "sm"
                            Layout.fillWidth: true
                        }

                        ThemedLabel {
                            text: qsTr("Bitrate")
                            size: "sm"
                            Layout.fillWidth: true
                            visible: !root.audioLossless
                        }

                        ThemedComboBox {
                            id: audioCodecCombo
                            Layout.fillWidth: true
                            textRole: "label"
                            valueRole: "id"
                            model: root.audioCodecs
                            onActivated: function (index) {
                                var item = root.audioCodecs[index]
                                if (!item || !item.available) {
                                    currentIndex = Math.max(0, indexOfValue(root.audioCodecId))
                                    return
                                }
                                root.audioCodecId = item.id
                                root.refreshCodecMeta()
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm
                            visible: !root.audioLossless

                            ThemedComboBox {
                                id: audioBitrateCombo
                                Layout.fillWidth: true
                                textRole: "label"
                                valueRole: "value"
                                model: root.audioBitrateModel
                                onActivated: function (index) {
                                    var item = root.audioBitrateModel[index]
                                    if (!item)
                                        return
                                    root.applyAudioBitrateChoice(item.value)
                                    if (audioOnlyBitrateCombo)
                                        audioOnlyBitrateCombo.currentIndex = currentIndex
                                }
                            }

                            ThemedNumberField {
                                Layout.fillWidth: true
                                visible: root.audioBitrateChoice === "custom"
                                from: 32
                                to: 512
                                step: 16
                                unit: qsTr("kbps")
                                value: root.audioBitrateKbps
                                onEdited: function (v) { root.audioBitrateKbps = Math.round(v) }
                            }
                        }
                    }
                }
            }

            // ---- Audio mode ----
            Column {
                width: parent.width
                spacing: Theme.spacingXl
                visible: root.isAudioOnly
                height: visible ? implicitHeight : 0
                clip: true

                ThemedLabel {
                    width: parent.width
                    size: "sm"
                    text: qsTr("Exports the timeline audio mix only — no video track.")
                }

                Column {
                    width: parent.width
                    spacing: Theme.spacingSm

                    ThemedLabel {
                        text: qsTr("Audio encoder")
                        size: "sm"
                    }

                    ThemedComboBox {
                        id: audioOnlyCodecCombo
                        width: parent.width
                        textRole: "label"
                        valueRole: "id"
                        model: root.audioCodecs
                        onActivated: function (index) {
                            var item = root.audioCodecs[index]
                            if (!item || !item.available) {
                                currentIndex = Math.max(0, indexOfValue(root.audioCodecId))
                                return
                            }
                            root.audioCodecId = item.id
                            root.refreshCodecMeta()
                            if (audioCodecCombo)
                                audioCodecCombo.currentIndex = currentIndex
                        }
                    }
                }

                Column {
                    width: parent.width
                    spacing: Theme.spacingSm
                    visible: !root.audioLossless

                    ThemedLabel {
                        text: qsTr("Bitrate")
                        size: "sm"
                    }

                    ThemedComboBox {
                        id: audioOnlyBitrateCombo
                        width: parent.width
                        textRole: "label"
                        valueRole: "value"
                        model: root.audioBitrateModel
                        onActivated: function (index) {
                            var item = root.audioBitrateModel[index]
                            if (!item)
                                return
                            root.applyAudioBitrateChoice(item.value)
                            if (audioBitrateCombo)
                                audioBitrateCombo.currentIndex = currentIndex
                        }
                    }

                    ThemedNumberField {
                        width: parent.width
                        visible: root.audioBitrateChoice === "custom"
                        from: 32
                        to: 512
                        step: 16
                        unit: qsTr("kbps")
                        value: root.audioBitrateKbps
                        onEdited: function (v) { root.audioBitrateKbps = Math.round(v) }
                    }
                }

                Item {
                    width: parent.width
                    height: audioAdvancedHeader.implicitHeight

                    Row {
                        id: audioAdvancedHeader
                        spacing: Theme.spacingSm
                        anchors.left: parent.left

                        IconGlyph {
                            anchors.verticalCenter: parent.verticalCenter
                            glyph: Theme.icons.chevronRight
                            iconSize: Theme.iconSizeSm
                            iconColor: Theme.mutedForeground
                            rotation: root.advancedOpen ? 90 : 0
                            Behavior on rotation {
                                NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                            }
                        }

                        ThemedLabel {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Advanced")
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.advancedOpen = !root.advancedOpen
                    }

                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Advanced")
                    Accessible.checkable: true
                    Accessible.checked: root.advancedOpen
                    Accessible.onPressAction: root.advancedOpen = !root.advancedOpen
                }

                Column {
                    width: parent.width
                    spacing: Theme.spacingLg
                    visible: root.advancedOpen
                    height: visible ? implicitHeight : 0
                    clip: true

                    ThemedLabel {
                        text: qsTr("Tags")
                        size: "sm"
                    }

                    Column {
                        width: parent.width
                        spacing: Theme.spacingSm

                        ThemedLabel {
                            text: qsTr("Title")
                            size: "xs"
                        }
                        ThemedTextField {
                            id: tagTitleField
                            width: parent.width
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: Theme.spacingSm

                        ThemedLabel {
                            text: qsTr("Artist")
                            size: "xs"
                        }
                        ThemedTextField {
                            id: tagArtistField
                            width: parent.width
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: Theme.spacingSm

                        ThemedLabel {
                            text: qsTr("Album")
                            size: "xs"
                        }
                        ThemedTextField {
                            id: tagAlbumField
                            width: parent.width
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: Theme.spacingSm

                        ThemedLabel {
                            text: qsTr("Comment")
                            size: "xs"
                        }
                        ThemedTextArea {
                            id: tagCommentField
                            width: parent.width
                            height: 96
                        }
                    }
                }
            }
        }
    }
}
