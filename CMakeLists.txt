cmake_minimum_required(VERSION 3.21)
project(Drift VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(WITH_BGREMOVAL "Enable OpenCV-based background removal" OFF)
option(DRIFT_FETCH_FONTS "Download the bundled font packages into fonts/ when absent" ON)

# The font bundle is ~17 MB of TTFs, so it is fetched rather than committed. A failure here is
# only a warning: the app falls back to system fonts and the font tests skip, so an offline
# clone still builds.
if(DRIFT_FETCH_FONTS AND NOT EXISTS "${CMAKE_SOURCE_DIR}/fonts/inter/family.json")
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
        message(STATUS "Fetching bundled fonts (scripts/fetch-fonts.py)...")
        execute_process(
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/fetch-fonts.py"
            RESULT_VARIABLE drift_fetch_fonts_result
            OUTPUT_VARIABLE drift_fetch_fonts_output
            ERROR_VARIABLE drift_fetch_fonts_output
        )
        if(NOT drift_fetch_fonts_result EQUAL 0)
            message(WARNING "Font fetch failed; Drift will fall back to system fonts.\n"
                            "Re-run later with: cmake --build <dir> --target fonts\n"
                            "${drift_fetch_fonts_output}")
        endif()
    else()
        message(WARNING "python3 not found — skipping font fetch. Drift will fall back to system fonts.")
    endif()
endif()

add_custom_target(fonts
    COMMAND ${CMAKE_COMMAND} -E env python3 "${CMAKE_SOURCE_DIR}/scripts/fetch-fonts.py"
    COMMENT "Fetch the bundled font packages into fonts/"
)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Multimedia Test Concurrent Widgets OpenGL)
find_package(FFmpeg REQUIRED)

if(WITH_BGREMOVAL)
    # Link only modules needed for background removal — not the full OpenCV
    # distribution (opencv_viz pulls in VTK and breaks linking on many systems).
    find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
endif()

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
)
target_include_directories(driftengine PUBLIC src)
target_link_libraries(driftengine PUBLIC Qt6::Core Qt6::Gui Qt6::OpenGL FFmpeg::FFmpeg driftcore)

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
        src/qml/components/ShortcutCaptureField.qml
        src/qml/components/ShapePreview.qml
        src/qml/components/EffectBrowser.qml
        src/qml/components/ThemedSlider.qml
        src/qml/components/ThemedTextField.qml
        src/qml/components/ThemedTextArea.qml
        src/qml/components/SubtitleEditor.qml
        src/qml/components/ThemedComboBox.qml
        src/qml/components/FontPicker.qml
        src/qml/components/ThemedButton.qml
        src/qml/components/ThemedChip.qml
        src/qml/components/ThemedLabel.qml
        src/qml/components/ThemedDialog.qml
        src/qml/components/ProjectSetupDialog.qml
        src/qml/components/ExportDialog.qml
        src/qml/components/ExportProgressDialog.qml
        src/qml/components/CircularProgress.qml
        src/qml/components/RecoveryDialog.qml
        src/qml/components/RecentProjectsPopup.qml
    RESOURCES
        resources/fonts/Inter.ttf
        resources/fonts/lucide.ttf
        resources/stickers/star.png
        resources/stickers/heart.png
        resources/stickers/arrow.png
        resources/stickers/circle.png
        resources/stickers/check.png
        resources/stickers/fire.png
)

target_link_libraries(drift PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    driftcore
    driftengine
)
target_include_directories(drift PRIVATE src src/preview)

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

# Same, for the font packages. Unlike effects/ and transitions/ these are not committed —
# they are fetched below — so tolerate the directory being absent.
function(drift_copy_fonts target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/fonts"
                "$<TARGET_FILE_DIR:${target}>/fonts"
        COMMENT "Copy font packages next to ${target}"
    )
endfunction()

drift_copy_effects(drift)
drift_copy_transitions(drift)
drift_copy_fonts(drift)

# --- CLI tools (headless engine testing, no GUI) ----------------------------
add_subdirectory(tools)

# --- tests -------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
