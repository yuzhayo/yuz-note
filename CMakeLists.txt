cmake_minimum_required(VERSION 3.28)

if (NOT DEFINED YUZNOTE_VERSION)
  file(READ "${CMAKE_SOURCE_DIR}/version.txt" YUZNOTE_VERSION_RAW)
  string(STRIP "${YUZNOTE_VERSION_RAW}" YUZNOTE_VERSION)
endif()
string(REPLACE "." "," YUZNOTE_VERSION_COMMA "${YUZNOTE_VERSION}")

project(YuzNote VERSION ${YUZNOTE_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerFlags)
include(FetchCmarkGfm)

set(YUZNOTE_QT_ROOT "${CMAKE_SOURCE_DIR}/development/Qt/6.8.3/msvc2022_64")
if(EXISTS "${YUZNOTE_QT_ROOT}/lib/cmake/Qt6/Qt6Config.cmake")
  list(PREPEND CMAKE_PREFIX_PATH "${YUZNOTE_QT_ROOT}")
endif()
find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Network)

add_subdirectory(src)
add_subdirectory(tests)
