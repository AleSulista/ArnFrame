qt_add_executable(tst_mediaprobe tst_mediaprobe.cpp)
target_link_libraries(tst_mediaprobe PRIVATE Qt6::Test driftengine)
add_test(NAME MediaProbe COMMAND tst_mediaprobe)

qt_add_executable(tst_core tst_core.cpp)
target_include_directories(tst_core PRIVATE ../src)
target_link_libraries(tst_core PRIVATE Qt6::Test driftcore)
add_test(NAME Core COMMAND tst_core)

qt_add_executable(tst_editorstate
    tst_editorstate.cpp
    ../src/models/AppController.cpp
    ../src/models/AssetLibrary.cpp
    ../src/models/TimelineModel.cpp
    ../src/models/ClipListModel.cpp
    ../src/playback/PlaybackEngine.cpp
    ../src/playback/PlaybackClock.cpp
    ../src/playback/CompositorService.cpp
)
target_include_directories(tst_editorstate PRIVATE ../src)
target_link_libraries(tst_editorstate PRIVATE Qt6::Test Qt6::Gui Qt6::Multimedia Qt6::Concurrent driftcore driftengine)
add_test(NAME EditorState COMMAND tst_editorstate)
drift_copy_effects(tst_editorstate)
drift_copy_transitions(tst_editorstate)

qt_add_executable(tst_playback
    tst_playback.cpp
    ../src/playback/PlaybackClock.cpp
)
target_include_directories(tst_playback PRIVATE ../src)
target_link_libraries(tst_playback PRIVATE Qt6::Test driftcore)
add_test(NAME Playback COMMAND tst_playback)

qt_add_executable(tst_engine tst_engine.cpp)
target_include_directories(tst_engine PRIVATE ../src)
target_link_libraries(tst_engine PRIVATE Qt6::Test Qt6::Gui Qt6::OpenGL driftengine driftcore)
target_compile_definitions(tst_engine PRIVATE
    DRIFT_TEST_EFFECTS_DIR="${CMAKE_SOURCE_DIR}/effects"
    DRIFT_TEST_TRANSITIONS_DIR="${CMAKE_SOURCE_DIR}/transitions"
)
add_test(NAME Engine COMMAND tst_engine)
drift_copy_effects(tst_engine)
drift_copy_transitions(tst_engine)
