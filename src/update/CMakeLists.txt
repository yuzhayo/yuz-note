add_library(yznote-update STATIC
  UpdateService.h
  UpdateService.cpp)

# Public: consumers include "update/*.h" and use QObject/signals in headers.
target_include_directories(yznote-update PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(yznote-update PUBLIC Qt6::Widgets PRIVATE velopack_libc)
target_include_directories(yznote-update PRIVATE "${CMAKE_BINARY_DIR}/generated")
