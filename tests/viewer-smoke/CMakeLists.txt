add_executable(viewer-smoke main.cpp)
target_link_libraries(viewer-smoke PRIVATE yznote-viewer Qt6::Widgets)
