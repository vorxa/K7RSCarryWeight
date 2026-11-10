# k7_common: code shared by the plugins that does not touch the game (no RE::, no SKSE::), unit-tested.
# Depends on spdlog (log levels) and toml++ (TomlReader) only.
# Headers: #include "k7/common/<file>.h", namespace k7::common.
find_package(spdlog CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)

add_library(k7_common STATIC
    src/config_layers.cpp
    src/form_key.cpp
    src/lang.cpp
    src/log_level.cpp
    src/migrate.cpp
    src/toml_reader.cpp
    src/user_file.cpp
)
target_include_directories(k7_common PUBLIC include)
target_link_libraries(k7_common PUBLIC spdlog::spdlog tomlplusplus::tomlplusplus)
if(MSVC)
    target_compile_options(k7_common PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
endif()

if(K7_BUILD_TESTS)
    add_subdirectory(tests)
endif()
