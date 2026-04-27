add_executable(probe probe.cpp)
target_link_libraries(probe PRIVATE Qt6::Core driftengine)

add_executable(renderframe renderframe.cpp)
target_include_directories(renderframe PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(renderframe PRIVATE Qt6::Core Qt6::Gui driftengine driftcore)
