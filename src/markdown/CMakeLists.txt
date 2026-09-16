add_library(yznote-markdown STATIC
  MarkdownParse.h
  MarkdownParse.cpp
  MarkdownDetails.h
  MarkdownDetails.cpp
  MarkdownSanitize.h
  MarkdownSanitize.cpp)

# Headers live in cmark's source tree; config/export headers are generated
# into its binary tree. Both pairs needed (verified against 0.29.0.gfm.13).
target_include_directories(yznote-markdown PUBLIC
  "${cmark-gfm_SOURCE_DIR}/src"
  "${cmark-gfm_SOURCE_DIR}/extensions"
  "${cmark-gfm_BINARY_DIR}/src"
  "${cmark-gfm_BINARY_DIR}/extensions")

target_link_libraries(yznote-markdown PRIVATE
  libcmark-gfm_static
  libcmark-gfm-extensions_static)

# Static link: opt out of dllimport decoration (matches upstream static build).
target_compile_definitions(yznote-markdown PRIVATE
  CMARK_GFM_STATIC_DEFINE
  CMARK_GFM_EXTENSIONS_STATIC_DEFINE)
