cmake_minimum_required(VERSION 3.28)

file(READ "${CMAKE_SOURCE_DIR}/version.txt" YUZNOTE_VERSION_RAW)
string(STRIP "${YUZNOTE_VERSION_RAW}" YUZNOTE_VERSION)

project(YuzNote VERSION ${YUZNOTE_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerFlags)
include(FetchCmarkGfm)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Network)

add_subdirectory(src)
add_subdirectory(tests)
