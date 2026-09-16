# Velopack is restored from its official release into generated output. App +
# update link the same imported target from this parent scope.
set(VELOPACK_ROOT "${CMAKE_SOURCE_DIR}/release/artifacts/dependencies/velopack")
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "yuz-note: only win-x64 Velopack is supported")
endif()
add_library(velopack_libc SHARED IMPORTED GLOBAL)
set_target_properties(velopack_libc PROPERTIES
  IMPORTED_LOCATION "${VELOPACK_ROOT}/lib/velopack_libc_win_x64_msvc.dll"
  IMPORTED_IMPLIB "${VELOPACK_ROOT}/lib/velopack_libc_win_x64_msvc.dll.lib"
  INTERFACE_INCLUDE_DIRECTORIES "${VELOPACK_ROOT}/include")

add_subdirectory(app)
add_subdirectory(markdown)
add_subdirectory(mdparse)
add_subdirectory(viewer)
add_subdirectory(update)
