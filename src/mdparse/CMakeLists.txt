add_executable(mdparse main.cpp)
target_include_directories(mdparse PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(mdparse PRIVATE yznote-markdown)
