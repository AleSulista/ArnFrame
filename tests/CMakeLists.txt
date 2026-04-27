qt_add_executable(tst_mediaprobe tst_mediaprobe.cpp)
target_link_libraries(tst_mediaprobe PRIVATE Qt6::Test driftengine)
add_test(NAME MediaProbe COMMAND tst_mediaprobe)

qt_add_executable(tst_editorstate
    tst_editorstate.cpp
    ../src/models/EditorState.cpp
    ../src/models/EditorStateOps.cpp
    ../src/models/AssetLibrary.cpp
)
target_include_directories(tst_editorstate PRIVATE ../src)
target_link_libraries(tst_editorstate PRIVATE Qt6::Test Qt6::Gui Qt6::Concurrent driftengine)
add_test(NAME EditorState COMMAND tst_editorstate)
