pragma Singleton
import QtQuick

// Design tokens for the app shell and panel surfaces (dark + light), plus
// shared layout/typography/iconography constants used across the UI.
QtObject {
    id: theme

    property FontLoader _interLoader: FontLoader { source: "qrc:/qt/qml/Drift/resources/fonts/Inter.ttf" }
    property FontLoader _lucideLoader: FontLoader { source: "qrc:/qt/qml/Drift/resources/fonts/lucide.ttf" }

    readonly property string fontFamily: _interLoader.name || "sans-serif"
    readonly property string iconFontFamily: _lucideLoader.name || "sans-serif"
    readonly property string monoFontFamily: "monospace"

    // --- Light/dark mode: follows the OS by default, overridable at runtime ----
    // Qt.styleHints.colorScheme is live-updated by the platform theme (Qt 6.5+).
    readonly property bool systemPrefersDark: Qt.styleHints.colorScheme !== Qt.Light
    property bool _userOverride: false
    property bool _userDarkMode: true
    readonly property bool darkMode: _userOverride ? _userDarkMode : systemPrefersDark

    function toggleDarkMode() {
        _userDarkMode = !darkMode;
        _userOverride = true;
    }

    // --- Color palettes: app shell vs. panel surfaces, light and dark ------------
    readonly property var _dark: ({
        appBackground: "#0d0d0d",
        foreground: "#dedede",
        border: "#292929",
        accent: "#242424",
        accentForeground: "#f2f2f2",
        mutedForeground: "#808080",
        popoverHover: "#212121",
        panelBackground: "#1a1a1a",
        panelForeground: "#d9d9d9",
        panelBorder: "#2e2e2e",
        panelAccent: "#262626",
        panelAccentForeground: "#ededed",
        panelMuted: "#383838",
        panelSecondaryBg: "#081a26",
        panelSecondaryBorder: "#002b47",
        panelSecondaryForeground: "#44bffd"
    })
    readonly property var _light: ({
        appBackground: "#ffffff",
        foreground: "#1c1c1c",
        border: "#e8e8e8",
        accent: "#f5f5f5",
        accentForeground: "#050505",
        mutedForeground: "#7a7a7a",
        popoverHover: "#f5f5f5",
        panelBackground: "#f9fafb",
        panelForeground: "#212121",
        panelBorder: "#dedede",
        panelAccent: "#ededed",
        panelAccentForeground: "#0d0d0d",
        panelMuted: "#d4d4d4",
        panelSecondaryBg: "#e5f5ff",
        panelSecondaryBorder: "#d6efff",
        panelSecondaryForeground: "#027dbb"
    })
    readonly property var _palette: darkMode ? _dark : _light

    // --- Colors: app shell ---------------------------------------------------
    readonly property color appBackground: _palette.appBackground
    readonly property color foreground: _palette.foreground
    readonly property color border: _palette.border
    readonly property color accent: _palette.accent
    readonly property color accentForeground: _palette.accentForeground
    readonly property color mutedForeground: _palette.mutedForeground
    readonly property color popoverHover: _palette.popoverHover

    // --- Colors: panel surfaces ------------------------------------------------
    readonly property color panelBackground: _palette.panelBackground
    readonly property color panelForeground: _palette.panelForeground
    readonly property color panelBorder: _palette.panelBorder
    readonly property color panelAccent: _palette.panelAccent
    readonly property color panelAccentForeground: _palette.panelAccentForeground
    readonly property color panelMuted: _palette.panelMuted
    readonly property color panelSecondaryBg: _palette.panelSecondaryBg
    readonly property color panelSecondaryBorder: _palette.panelSecondaryBorder
    readonly property color panelSecondaryForeground: _palette.panelSecondaryForeground

    // --- Colors: shared semantic (identical in both themes) -----------------------
    readonly property color primary: "#16a9f3"
    readonly property color primaryForeground: "#ffffff"
    readonly property color destructive: "#e91616"
    readonly property color constructive: "#23d160"

    // --- Colors: timeline clip types (fixed regardless of app theme) ---------------
    readonly property color clipText: "#5DBAA0"
    readonly property color clipAudio: "#8F5DBA"
    readonly property color clipGraphic: "#BA5D7A"
    readonly property color clipEffect: "#5d93ba"
    readonly property color waveformColor: "#ffffffb3" // rgba(255,255,255,0.7)
    // Video clips normally show real thumbnails (always photographic/dark-ish); until
    // thumbnail generation exists, use a fixed dark placeholder so the white filename
    // scrim stays legible in light mode too instead of following panelAccent.
    readonly property color clipVideoPlaceholder: "#2b2b2b"

    // --- Radius --------------------------------------------------------------
    readonly property real radiusSm: 5.6
    readonly property real radiusMd: 10.4
    readonly property real radiusLg: 13.12

    // --- Typography ------------------------------------------------------------
    readonly property real fontSizeXs: 11.52
    readonly property real fontSizeSm: 12.64
    readonly property real fontSizeBase: 14.72
    readonly property real fontSizeTiny: 9.6   // 0.6rem clip captions
    readonly property real fontSizeTick: 10    // literal 10px ruler tick labels
    readonly property real fontSizeCard: 11.2  // 0.7rem asset card filenames

    // --- Layout: chrome ------------------------------------------------------
    readonly property real headerHeight: 54.4
    readonly property real panelGap: 3
    readonly property real pagePadding: 12

    // --- Layout: assets panel -----------------------------------------------
    readonly property real panelHeaderHeight: 44
    readonly property real tabRailWidth: 40
    readonly property real assetCardWidth: 112
    readonly property real assetCardGap: 16

    // --- Layout: preview panel -----------------------------------------------
    readonly property real previewToolbarPaddingTop: 20
    readonly property real previewToolbarPaddingBottom: 12

    // --- Layout: timeline ------------------------------------------------------
    readonly property real timelineToolbarHeight: 40
    readonly property real timelineRulerHeight: 22
    readonly property real timelineBookmarkRowHeight: 16
    readonly property real trackHeightVideo: 65
    readonly property real trackHeightAudio: 50
    readonly property real trackHeightText: 25
    readonly property real trackGap: 6
    readonly property real trackLabelsWidth: 112
    readonly property real pixelsPerSecondBase: 50
    readonly property real playheadLineWidth: 2
    readonly property real playheadHandleSize: 12
    readonly property real clipSelectionRingWidth: 1.5

    // --- Iconography (Lucide glyph codepoints; ISC-licensed, see
    // resources/licenses/LICENSE-lucide.txt) ------------------------------------
    readonly property var icons: ({
        scissors: "",
        alignLeft: "",
        alignRight: "",
        copy: "",
        trash: "",
        snowflake: "",
        bookmark: "",
        layers: "",
        magnet: "",
        linkTwo: "",
        zoomOut: "",
        zoomIn: "",
        play: "",
        pause: "",
        maximize: "",
        folder: "",
        headphones: "",
        type: "",
        smile: "",
        wand: "",
        sliders: "",
        settings: "",
        upload: "",
        plus: "",
        volumeHigh: "",
        volumeOff: "",
        eye: "",
        eyeOff: "",
        film: "",
        music: "",
        image: "",
        chevronDown: "",
        x: "",
        search: "",
        messageSquare: "",
        moon: "",
        sun: "",
        grid: "",
        list: "",
        sort: ""
    })
}
