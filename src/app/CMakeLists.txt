configure_file(
  "${CMAKE_SOURCE_DIR}/cmake/version.h.in"
  "${CMAKE_BINARY_DIR}/generated/version.h"
  @ONLY)

add_executable(yuz-note WIN32
  main.cpp
  MainWindow.h
  MainWindow.cpp
  MarkdownAssociation.h
  MarkdownAssociation.cpp
  FindBar.h
  FindBar.cpp
  SingleInstance.h
  SingleInstance.cpp
  yuz-note.rc
  "${CMAKE_BINARY_DIR}/generated/version.h")

target_include_directories(yuz-note PRIVATE "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(yuz-note PRIVATE Qt6::Widgets Qt6::Network velopack_libc yznote-viewer yznote-update)

# The Velopack DLL must sit beside the exe at runtime (dev tree, windeployqt
# staging, and vpk pack alike). NOTE: the import library records the DLL under
# its upstream internal name `velopack_libc.dll`, while the restored file carries
# a platform suffix — so the copy RENAMES it.
add_custom_command(TARGET yuz-note POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${VELOPACK_ROOT}/lib/velopack_libc_win_x64_msvc.dll"
    $<TARGET_FILE_DIR:yuz-note>/velopack_libc.dll)
