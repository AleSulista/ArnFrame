cmake_minimum_required(VERSION 3.21)
project(Drift VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(WITH_BGREMOVAL "Enable OpenCV-based background removal" OFF)

# Fonts, stickers and the Whisper model used to be fetched or extracted here and shipped inside
# the build tree — roughly 800 MB of assets every user paid for whether or not they used them.
# They are now addons: signed .driftpkg archives downloaded at runtime into <AppDataLocation>/
# addons/ (src/engine/AddonPackage, src/models/AddonManager). Nothing asset-shaped is bundled,
# so a fresh build starts with system fonts, an empty sticker tab and no transcriber, and the
# user installs what they want from the Addon Manager. The packaging and publishing tooling
# lives in the drift-addons repo.
#
# For development without downloading, point DRIFT_FONTS_DIR, DRIFT_STICKERS_DIR or
# DRIFT_WHISPER_MODEL_DIR at a local directory; those still take priority over installed addons.

# --- Addon service ----------------------------------------------------------
# Where the Addon Manager fetches from. These are the only place either value is defined —
# src/models/AddonEndpoint.h just reads them — so a fork can point at its own Worker with:
#
#   cmake -B build -DDRIFT_ADDON_INDEX_URL=https://addons.example.com/v1/index \
#                  -DDRIFT_ADDON_CLIENT_TOKEN=...
#
# Set DRIFT_ADDON_INDEX_URL to an empty string to build with the addon service disabled: the
# manager then lists nothing and installs nothing, and the app runs on locally installed or
# side-loaded content only.
#
# The token is NOT a secret. It ships inside every binary and is trivially extractable; it exists
# so the bucket cannot be crawled or hotlinked, which would cost operations for someone else's
# benefit. That is also why it is fine in CMakeCache.txt.
set(DRIFT_ADDON_INDEX_URL "https://drift-addons.cutwire.org/v1/index"
    CACHE STRING "Addon index endpoint; empty disables the addon service")
set(DRIFT_ADDON_CLIENT_TOKEN "79a388bba9301cacc81cc6eb0724c81e948e1b27c69aee1b"
    CACHE STRING "Client token sent to the addon service as X-Drift-Client")

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Multimedia Test Concurrent Widgets OpenGL Network)
find_package(FFmpeg REQUIRED)

# ONNX Runtime powers the Whisper auto-subtitle transcriber (src/engine/WhisperTranscriber).
option(DRIFT_FETCH_ONNXRUNTIME
    "Download a prebuilt ONNX Runtime when it is not installed (cmake/FetchOnnxRuntime.cmake)" ON)
find_package(OnnxRuntime QUIET)
if(NOT OnnxRuntime_FOUND AND DRIFT_FETCH_ONNXRUNTIME)
    include(FetchOnnxRuntime)
    find_package(OnnxRuntime REQUIRED)
endif()
if(NOT OnnxRuntime_FOUND)
    message(FATAL_ERROR
        "ONNX Runtime not found. Install it and set ONNXRUNTIME_ROOT, "
        "or enable DRIFT_FETCH_ONNXRUNTIME to download it automatically.")
endif()
message(STATUS "ONNX Runtime found: ${OnnxRuntime_LIBRARIES} — auto subtitles enabled")

if(WITH_BGREMOVAL)
    # Link only modules needed for background removal — not the full OpenCV
    # distribution (opencv_viz pulls in VTK and breaks linking on many systems).
    find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
endif()

# Addon packages (.driftpkg) are solid-zstd compressed and Ed25519 signed. zstd upstream only
# ships a CMake config in recent releases, so fall back to pkg-config.
find_package(zstd CONFIG QUIET)
if(TARGET zstd::libzstd_shared)
    set(DRIFT_ZSTD_TARGET zstd::libzstd_shared)
elseif(TARGET zstd::libzstd)
    set(DRIFT_ZSTD_TARGET zstd::libzstd)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd)
    set(DRIFT_ZSTD_TARGET PkgConfig::ZSTD)
endif()
find_package(OpenSSL REQUIRED COMPONENTS Crypto)

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

# --- core: domain model, no GUI ---------------------------------------------
add_library(driftcore STATIC
    src/core/Effect.h
    src/core/Effect.cpp
    src/core/EffectPreset.h
    src/core/MediaAsset.h
    src/core/MediaAsset.cpp
    src/core/Clip.h
    src/core/Clip.cpp
    src/core/SubtitleCue.h
    src/core/SubtitleCue.cpp
    src/core/Mask.h
    src/core/Mask.cpp
    src/core/Transition.h
    src/core/Transition.cpp
    src/core/TextStyle.h
    src/core/TextStyle.cpp
    src/core/ShapeStyle.h
    src/core/ShapeStyle.cpp
    src/core/Track.h
    src/core/Track.cpp
    src/core/Project.h
    src/core/Project.cpp
    src/core/TimelineOps.h
    src/core/TimelineOps.cpp
    src/core/commands/ProjectCommands.h
    src/core/commands/ProjectCommands.cpp
)
target_include_directories(driftcore PUBLIC src)
target_link_libraries(driftcore PUBLIC Qt6::Core Qt6::Gui)

# --- engine: FFmpeg-facing code, no QML deps -------------------------------
add_library(driftengine STATIC
    src/engine/AddonPackage.h
    src/engine/AddonPackage.cpp
    src/engine/AddonRegistry.h
    src/engine/AddonRegistry.cpp
    src/engine/AddonSigningKey.h
    src/engine/MediaProbe.h
    src/engine/MediaProbe.cpp
    src/engine/MediaThumbnail.h
    src/engine/MediaThumbnail.cpp
    src/engine/MediaWaveform.h
    src/engine/MediaWaveform.cpp
    src/engine/Exporter.h
    src/engine/Exporter.cpp
    src/engine/ClipReader.h
    src/engine/ClipReader.cpp
    src/engine/ClipReaderWorker.h
    src/engine/ClipReaderWorker.cpp
    src/engine/ClipReaderPool.h
    src/engine/ClipReaderPool.cpp
    src/engine/EffectProcessor.h
    src/engine/EffectProcessor.cpp
    src/engine/EffectCatalog.h
    src/engine/EffectCatalog.cpp
    src/engine/FontCatalog.h
    src/engine/FontCatalog.cpp
    src/engine/StickerCatalog.h
    src/engine/StickerCatalog.cpp
    src/engine/TextRaster.h
    src/engine/TextRaster.cpp
    src/engine/EffectPackageLoader.h
    src/engine/EffectPackageLoader.cpp
    src/engine/TransitionCatalog.h
    src/engine/TransitionCatalog.cpp
    src/engine/TransitionPackageLoader.h
    src/engine/TransitionPackageLoader.cpp
    src/engine/GpuPackageParse.h
    src/engine/GpuPackageParse.cpp
    src/engine/GpuEffectDefinition.h
    src/engine/GpuCompositor.h
    src/engine/GpuCompositor.cpp
    src/engine/GlRuntime.h
    src/engine/GlRuntime.cpp
    src/engine/GpuEffectExecutor.h
    src/engine/GpuEffectExecutor.cpp
    src/engine/CompositorFrameHistory.h
    src/engine/CompositorFrameHistory.cpp
    src/engine/FrameCompositor.h
    src/engine/FrameCompositor.cpp
    src/engine/MaskApplier.h
    src/engine/MaskApplier.cpp
    src/engine/AudioAtempo.h
    src/engine/AudioAtempo.cpp
    src/engine/AudioMixer.h
    src/engine/AudioMixer.cpp
    src/engine/WhisperTokenizer.h
    src/engine/WhisperTokenizer.cpp
    src/engine/WhisperTranscriber.h
    src/engine/WhisperTranscriber.cpp
)
target_include_directories(driftengine PUBLIC src)
target_link_libraries(driftengine PUBLIC Qt6::Core Qt6::Gui Qt6::OpenGL FFmpeg::FFmpeg driftcore
    onnxruntime::onnxruntime)
target_link_libraries(driftengine PRIVATE ${DRIFT_ZSTD_TARGET} OpenSSL::Crypto)

if(WITH_BGREMOVAL)
    target_include_directories(driftengine PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(driftengine PRIVATE ${OpenCV_LIBS})
    target_compile_definitions(driftengine PUBLIC DRIFT_WITH_BGREMOVAL)
endif()

# --- application ------------------------------------------------------------
qt_add_executable(drift
    src/main.cpp
    src/DriftImageProvider.h
    src/DriftImageProvider.cpp
    src/models/AddonEndpoint.h
    src/models/AddonManager.h
    src/models/AddonManager.cpp
    src/models/AppController.h
    src/models/AppController.cpp
    src/models/EditorState.h
    src/models/FileDialogs.h
    src/models/FileDialogs.cpp
    src/models/AssetLibrary.h
    src/models/AssetLibrary.cpp
    src/models/TimelineModel.h
    src/models/TimelineModel.cpp
    src/models/ClipListModel.h
    src/models/ClipListModel.cpp
    src/playback/PlaybackEngine.h
    src/playback/PlaybackEngine.cpp
    src/playback/PlaybackClock.h
    src/playback/PlaybackClock.cpp
    src/playback/CompositorService.h
    src/playback/CompositorService.cpp
    src/preview/PreviewItem.h
    src/preview/PreviewItem.cpp
)

set_source_files_properties(src/qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE true)

qt_add_qml_module(drift
    URI Drift
    VERSION 1.0
    QML_FILES
        src/qml/Main.qml
        src/qml/Theme.qml
        src/qml/EditorHeader.qml
        src/qml/AssetsPanel.qml
        src/qml/PreviewPanel.qml
        src/qml/PropertiesPanel.qml
        src/qml/TimelinePanel.qml
        src/qml/components/IconGlyph.qml
        src/qml/components/IconButton.qml
        src/qml/components/PanelFrame.qml
        src/qml/components/ClipFilmstrip.qml
        src/qml/components/AppScrollBar.qml
        src/qml/components/KeyframeDiamond.qml
        src/qml/components/PropertyKeyframeRow.qml
        src/qml/components/KeyframeGraph.qml
        src/qml/components/SubtitleCueLane.qml
        src/qml/components/ShortcutCaptureField.qml
        src/qml/components/ShapePreview.qml
        src/qml/components/EffectBrowser.qml
        src/qml/components/ThemedSlider.qml
        src/qml/components/ThemedTextField.qml
        src/qml/components/ThemedNumberField.qml
        src/qml/components/ThemedTextArea.qml
        src/qml/components/SubtitleEditor.qml
        src/qml/components/ThemedComboBox.qml
        src/qml/components/FontPicker.qml
        src/qml/components/ThemedButton.qml
        src/qml/components/ThemedChip.qml
        src/qml/components/ThemedLabel.qml
        src/qml/components/ThemedDialog.qml
        src/qml/components/AddonManagerDialog.qml
        src/qml/components/ProjectSetupDialog.qml
        src/qml/components/ExportDialog.qml
        src/qml/components/ExportProgressDialog.qml
        src/qml/components/SubtitleProgressDialog.qml
        src/qml/components/CircularProgress.qml
        src/qml/components/RecoveryDialog.qml
        src/qml/components/RecentProjectsPopup.qml
        src/qml/components/NewTrackMenu.qml
    RESOURCES
        # UI chrome, not catalog content: the interface font and the icon font are always needed,
        # including before any addon is installed.
        resources/fonts/Inter.ttf
        resources/fonts/lucide.ttf
)

target_link_libraries(drift PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Network
    driftcore
    driftengine
)
target_include_directories(drift PRIVATE src src/preview)

# Defined here rather than in the header so there is exactly one place to change them.
target_compile_definitions(drift PRIVATE
    DRIFT_ADDON_INDEX_URL="${DRIFT_ADDON_INDEX_URL}"
    DRIFT_ADDON_CLIENT_TOKEN="${DRIFT_ADDON_CLIENT_TOKEN}"
)

# Ship file-based effect packages next to binaries that load the catalog.
function(drift_copy_effects target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/effects"
                "$<TARGET_FILE_DIR:${target}>/effects"
        COMMENT "Copy effects packages next to ${target}"
    )
endfunction()

# Same, for file-based transition packages.
function(drift_copy_transitions target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/transitions"
                "$<TARGET_FILE_DIR:${target}>/transitions"
        COMMENT "Copy transition packages next to ${target}"
    )
endfunction()

# Deploy the ONNX Runtime shared library next to the binary (needed on Windows; harmless
# elsewhere and keeps the app runnable from the build tree without a system install).
function(drift_copy_onnxruntime target)
    if(ONNXRUNTIME_RUNTIME_LIBS)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${ONNXRUNTIME_RUNTIME_LIBS}
                    "$<TARGET_FILE_DIR:${target}>"
            COMMENT "Copy onnxruntime runtime libs next to ${target}"
        )
    endif()
endfunction()

drift_copy_effects(drift)
drift_copy_transitions(drift)
drift_copy_onnxruntime(drift)

# --- CLI tools (headless engine testing, no GUI) ----------------------------
add_subdirectory(tools)

# --- tests -------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
