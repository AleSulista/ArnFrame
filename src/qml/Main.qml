import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Templates as T
import QtQuick.Window
import Drift
import "components"

ApplicationWindow {
    id: window

    width: 1280
    height: 800
    // Below this the split minimums cannot all be satisfied and panels overlap.
    minimumWidth: Theme.windowMinimumWidth
    minimumHeight: Theme.windowMinimumHeight
    visible: true
    title: "CutWire Drift"
    color: Theme.appBackground

    // Fullscreen preview: the window goes fullscreen *and* every panel around the
    // preview collapses, so the video actually fills the display. Toggling the
    // window alone just scaled the same three-pane layout up.
    property bool previewFullscreen: false
    // Restoring to Windowed unconditionally would silently un-maximize a window
    // that was maximized before going fullscreen.
    property int _preFullscreenVisibility: Window.Windowed

    function togglePreviewFullscreen() {
        if (previewFullscreen) {
            previewFullscreen = false
            visibility = _preFullscreenVisibility
        } else {
            _preFullscreenVisibility = visibility
            previewFullscreen = true
            visibility = Window.FullScreen
        }
    }

    Shortcut {
        sequence: "Esc"
        enabled: window.previewFullscreen
        onActivated: window.togglePreviewFullscreen()
    }

    function configureAndAddAsset(assetIndex, runner) {
        if (!EditorState.shouldConfigureProjectForAsset(assetIndex)) {
            runner()
            return
        }
        projectSetupDialog.openForAsset(assetIndex, runner)
    }

    ProjectSetupDialog {
        id: projectSetupDialog
    }

    RecoveryDialog {
        id: recoveryDialog
    }

    SubtitleProgressDialog {
        id: subtitleProgressDialog
    }

    AddonManagerDialog {
        id: addonManagerDialog
    }

    MissingAddonsDialog {
        id: missingAddonsDialog
    }

    SegmentationWindow {
        id: segmentationWindow
    }

    Connections {
        target: EditorState
        function onOpenSegmentationWindowRequested(track, clip, startSeconds, durationSeconds) {
            segmentationWindow.openFor(track, clip, startSeconds, durationSeconds, true)
        }
    }

    DenoiseWindow {
        id: denoiseWindow
    }

    SpeedCurveWindow {
        id: speedCurveWindow
    }

    // Opened from the clip inspector; a window rather than a dialog so the timeline stays visible.
    function openSegmentation(track, clip, startSeconds, durationSeconds) {
        segmentationWindow.openFor(track, clip, startSeconds, durationSeconds)
    }

    function openDenoise(track, clip, durationSeconds) {
        denoiseWindow.openFor(track, clip, durationSeconds)
    }

    function openSpeedCurve(track, clip) {
        speedCurveWindow.openFor(track, clip)
    }

    // Opened from the header, and from every empty state that a missing addon causes.
    function openAddonManager(kind) {
        if (kind === undefined)
            addonManagerDialog.open()
        else
            addonManagerDialog.openForKind(kind)
    }

    function promptRecoveryIfNeeded() {
        if (!EditorState.recoveryAvailable || recoveryDialog.visible)
            return
        recoveryDialog.open()
    }

    // Ask every launch while the previous session left an autosave snapshot
    // (unsaved work — whether the app crashed or was closed normally).
    Timer {
        id: recoveryOpenTimer
        interval: 150
        repeat: true
        triggeredOnStart: false
        property int attempts: 0
        onTriggered: {
            if (!EditorState.recoveryAvailable) {
                stop()
                attempts = 0
                return
            }
            promptRecoveryIfNeeded()
            if (recoveryDialog.visible || ++attempts >= 20)
                stop()
        }
    }

    Component.onCompleted: {
        if (EditorState.recoveryAvailable)
            recoveryOpenTimer.start()
    }

    onVisibilityChanged: {
        if (visible && EditorState.recoveryAvailable)
            recoveryOpenTimer.start()
    }

    Connections {
        target: EditorState
        function onRecoveryChanged() {
            if (EditorState.recoveryAvailable)
                recoveryOpenTimer.start()
        }

        // --- Error and status surfacing -------------------------------------
        // These signals previously had no handler anywhere in QML, so a failed
        // export or transcription was reported only to the console.
        function onExportFinished(success) {
            if (success)
                Toasts.success(qsTr("Export finished."))
            else
                Toasts.error(qsTr("Export failed. Check the output path and disk space."))
        }

        function onMissingAddons(addons) {
            missingAddonsDialog.openFor(addons)
        }

        function onPackageFinished(ok, message) {
            if (ok)
                Toasts.success(message)
            else
                Toasts.error(qsTr("Packaging failed: %1").arg(message))
        }

        function onSubtitleGenerationFinished(ok, message) {
            if (ok)
                Toasts.success(message.length > 0 ? message : qsTr("Subtitles generated."))
            else
                Toasts.error(message.length > 0
                             ? qsTr("Subtitle generation failed: %1").arg(message)
                             : qsTr("Subtitle generation failed."))
        }

        // `lastMessage` is the backend's general-purpose status line. It used to
        // render as static muted text in the header that one message silently
        // overwrote; it now goes through the toast queue like everything else.
        function onLastMessageChanged() {
            const message = EditorState.lastMessage
            if (message.length === 0)
                return
            // The backend has no severity channel, so infer it from the wording.
            if (/fail|error|could not|unable|invalid|denied/i.test(message))
                Toasts.error(message)
            else
                Toasts.info(message)
        }

        // Raised when an edit is refused (e.g. transforming a locked clip).
        // Only PreviewPanel used to show this, so the same block initiated from
        // the timeline was silent.
        function onTransformBlocked(reason) {
            Toasts.warning(reason)
        }
    }

    // Shortcut is not an Item, so wrap each binding in a zero-size host.
    Repeater {
        model: EditorState.actions
        Item {
            required property var modelData
            width: 0
            height: 0
            Shortcut {
                sequence: modelData.shortcut
                context: Qt.ApplicationShortcut
                onActivated: EditorState.triggerAction(modelData.id)
            }
        }
    }

    Column {
        anchors.fill: parent
        spacing: 0

        EditorHeader {
            width: parent.width
            visible: !window.previewFullscreen
        }

        Item {
            width: parent.width
            // Clamped so the editor body never takes a negative height while the
            // window is being resized toward its minimum.
            height: Math.max(0, parent.height
                                - (window.previewFullscreen ? 0 : Theme.headerHeight))

            SplitView {
                id: outerSplit
                anchors.fill: parent
                // Edge-to-edge in fullscreen; the usual page padding otherwise.
                anchors.margins: window.previewFullscreen ? 0 : Theme.pagePadding
                anchors.topMargin: 0
                orientation: Qt.Vertical
                spacing: Theme.panelGap

                // Handles were 1px and transparent at rest, so they were both
                // undiscoverable and nearly impossible to hit. They are now a
                // real target with a visible grip.
                handle: Rectangle {
                    id: outerHandle
                    implicitWidth: outerSplit.orientation === Qt.Horizontal ? Theme.spacingMd : outerSplit.width
                    implicitHeight: outerSplit.orientation === Qt.Horizontal ? outerSplit.height : Theme.spacingMd
                    color: T.SplitHandle.pressed ? Theme.primary
                        : (T.SplitHandle.hovered ? Theme.panelBorder : "transparent")

                    Behavior on color {
                        ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                    }

                    // Grip dots: a resting affordance that reads as draggable.
                    Row {
                        anchors.centerIn: parent
                        spacing: Theme.spacingSm
                        opacity: T.SplitHandle.hovered || T.SplitHandle.pressed ? 0 : 1

                        Behavior on opacity {
                            NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                        }

                        Repeater {
                            model: 3
                            Rectangle {
                                width: 2
                                height: 2
                                radius: 1
                                color: Theme.panelBorder
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton
                        cursorShape: Qt.SplitVCursor
                    }
                }

                SplitView {
                    id: innerSplit
                    // Sized against the enclosing SplitView by id. `parent` here
                    // is the SplitView's own content item, which makes these
                    // percentage constraints self-referential during a resize.
                    // In fullscreen the timeline is hidden, so the normal 30–85%
                    // band would leave a dead strip below the preview.
                    SplitView.preferredHeight: window.previewFullscreen
                                               ? outerSplit.height : outerSplit.height * 0.5
                    SplitView.minimumHeight: window.previewFullscreen
                                             ? outerSplit.height : outerSplit.height * 0.3
                    SplitView.maximumHeight: window.previewFullscreen
                                             ? outerSplit.height : outerSplit.height * 0.85
                    orientation: Qt.Horizontal
                    spacing: Theme.panelGap

                    handle: Rectangle {
                        implicitWidth: innerSplit.orientation === Qt.Horizontal ? Theme.spacingMd : innerSplit.width
                        implicitHeight: innerSplit.orientation === Qt.Horizontal ? innerSplit.height : Theme.spacingMd
                        color: T.SplitHandle.pressed ? Theme.primary
                            : (T.SplitHandle.hovered ? Theme.panelBorder : "transparent")

                        Behavior on color {
                            ColorAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                        }

                        Column {
                            anchors.centerIn: parent
                            spacing: Theme.spacingSm
                            opacity: T.SplitHandle.hovered || T.SplitHandle.pressed ? 0 : 1

                            Behavior on opacity {
                                NumberAnimation { duration: Theme.durationFast; easing.type: Theme.easing }
                            }

                            Repeater {
                                model: 3
                                Rectangle {
                                    width: 2
                                    height: 2
                                    radius: 1
                                    color: Theme.panelBorder
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.NoButton
                            cursorShape: Qt.SplitHCursor
                        }
                    }

                    AssetsPanel {
                        id: assetsPanel
                        visible: !window.previewFullscreen
                        SplitView.preferredWidth: innerSplit.width * 0.25
                        SplitView.minimumWidth: 200
                        SplitView.maximumWidth: innerSplit.width * 0.4
                    }

                    PreviewPanel {
                        previewFullscreen: window.previewFullscreen
                        onFullscreenRequested: window.togglePreviewFullscreen()
                        SplitView.fillWidth: true
                        SplitView.minimumWidth: 320
                    }

                    PropertiesPanel {
                        id: propertiesPanel
                        visible: !window.previewFullscreen
                        SplitView.preferredWidth: innerSplit.width * 0.25
                        SplitView.minimumWidth: 240
                        SplitView.maximumWidth: innerSplit.width * 0.4
                        // "Browse effects" in the empty Effects tab jumps the
                        // assets panel to its Effects library.
                        onBrowseEffectsRequested: assetsPanel.showTab("effects")
                    }
                }

                TimelinePanel {
                    visible: !window.previewFullscreen
                    propertiesTab: propertiesPanel.currentTabId
                    SplitView.preferredHeight: outerSplit.height * 0.5
                    SplitView.minimumHeight: 140
                    SplitView.maximumHeight: outerSplit.height * 0.7
                }
            }
        }
    }

    // Notification host — above all panels, so any message lands in one place.
    ToastHost { }
}
