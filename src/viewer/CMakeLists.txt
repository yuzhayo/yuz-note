add_library(yznote-viewer STATIC
  MarkdownView.h
  MarkdownView.cpp
  ViewerTheme.h
  ViewerTheme.cpp)

# Public: consumers include "viewer/*.h" and link Qt Widgets (signals in headers).
target_include_directories(yznote-viewer PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(yznote-viewer PUBLIC Qt6::Widgets PRIVATE yznote-markdown)
