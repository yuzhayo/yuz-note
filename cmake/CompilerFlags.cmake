# MSVC baseline. /utf-8: sources are UTF-8 (Velopack strings are UTF-8 by contract).
if(MSVC)
  add_compile_options(/W4 /EHsc /permissive- /utf-8)
endif()
