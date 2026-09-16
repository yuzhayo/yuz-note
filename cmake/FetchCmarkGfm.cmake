# cmark-gfm 0.29.0.gfm.13 — pinned URL+HASH (P2).
# Their cmake_minimum_required(3.0) predates CMake 4: CMAKE_POLICY_VERSION_MINIMUM
# is the official bridge (no source patching). Documented fallback (unused):
# direct-compile the ~30 C files with a hand-written config.h — see DECISIONS.
set(CMARK_TESTS OFF CACHE BOOL "" FORCE)
set(CMARK_SHARED OFF CACHE BOOL "" FORCE)
set(CMARK_STATIC ON CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
# Our root enables AUTOMOC globally for Qt; cmark is pure C — keep its
# configure output clean by scoping AUTOMOC off around its sub-build.
include(FetchContent)
set(YUZNOTE_SAVED_AUTOMOC ${CMAKE_AUTOMOC})
set(CMAKE_AUTOMOC OFF)
FetchContent_Declare(cmark-gfm
  URL https://codeload.github.com/github/cmark-gfm/tar.gz/refs/tags/0.29.0.gfm.13
  URL_HASH SHA256=5ABC61798EBD9DE5660BC076443C07ABAD2B8D15DBC11094A3A79644B8AD243A)
FetchContent_MakeAvailable(cmark-gfm)
set(CMAKE_AUTOMOC ${YUZNOTE_SAVED_AUTOMOC})
