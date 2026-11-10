# k7_skse: code shared by the plugins that uses CommonLibSSE-NG: the log file, lookup of forms and
# keywords, the check for SKSE Menu Framework and the language of the game. Headers: #include "k7/skse/<file>.h", namespace k7::skse. Checked in game through the plugins.
add_library(k7_skse STATIC
    src/logging.cpp
    src/form_lookup.cpp
    src/menu.cpp
    src/language.cpp
    src/audio_probe.cpp
)
target_include_directories(k7_skse PUBLIC include)
target_link_libraries(k7_skse PUBLIC k7_common CommonLibSSE::CommonLibSSE)
# The SKSE Menu Framework API header (third_party/SKSEMenuFramework): plugins that draw a menu include it too.
# SYSTEM keeps its warnings out of our build.
target_include_directories(k7_skse SYSTEM PUBLIC ${PROJECT_SOURCE_DIR}/third_party/SKSEMenuFramework)
target_precompile_headers(k7_skse PRIVATE src/pch.h)
if(MSVC)
    target_compile_options(k7_skse PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
endif()
