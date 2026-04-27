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
)
target_include_directories(tst_editorstate PRIVATE ../src)
target_link_libraries(tst_editorstate PRIVATE Qt6::Test Qt6::Gui Qt6::Concurrent driftcore driftengine)
add_test(NAME EditorState COMMAND tst_editorstate)
