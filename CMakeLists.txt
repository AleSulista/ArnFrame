cmake_minimum_required(VERSION 3.21)
project(Drift VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(WITH_BGREMOVAL "Enable OpenCV-based background removal" OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Multimedia Test Concurrent)
find_package(FFmpeg REQUIRED)

if(WITH_BGREMOVAL)
    find_package(OpenCV REQUIRED)
endif()

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

# --- engine: FFmpeg-facing code, no QML deps -------------------------------
add_library(driftengine STATIC
    src/engine/MediaProbe.h
    src/engine/MediaProbe.cpp
    src/engine/MediaThumbnail.h
    src/engine/MediaThumbnail.cpp
    src/engine/MediaWaveform.h
    src/engine/MediaWaveform.cpp
    src/engine/TimelineExporter.h
    src/engine/TimelineExporter.cpp
)
target_include_directories(driftengine PUBLIC src)
target_link_libraries(driftengine PUBLIC Qt6::Core Qt6::Gui FFmpeg::FFmpeg)

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
    src/models/AssetLibrary.h
    src/models/AssetLibrary.cpp
    src/models/EditorState.h
    src/models/EditorState.cpp
    src/models/EditorStateOps.cpp
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
    RESOURCES
        resources/fonts/Inter.ttf
        resources/fonts/lucide.ttf
)

target_link_libraries(drift PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::Gui
    Qt6::Concurrent
    driftengine
)

# --- CLI tools (headless engine testing, no GUI) ----------------------------
add_subdirectory(tools)

# --- tests -------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
