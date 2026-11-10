# The plugin version lives here and nowhere else: the DLL resource, the SKSE plugin info and the log line
# all take it from PROJECT_VERSION. Rules: docs/VERSIONING.md (in this folder and at the repository root).
project(K7RSCarryWeight
    VERSION 0.2.1
    DESCRIPTION "Base carry weight by race and sex for Skyrim (SKSE plugin)"
    LANGUAGES CXX
)

# --- K7RSCarryWeight_core: game-independent logic, unit-tested ------------------------------------
add_library(K7RSCarryWeight_core STATIC
    src/core/carry.cpp
    src/core/defaults.cpp
    src/core/apply.cpp
    src/core/validate.cpp
    src/core/serialize.cpp
    src/core/loader.cpp
    src/core/recount.cpp
    src/core/user_edit.cpp
)
target_include_directories(K7RSCarryWeight_core PUBLIC src)
target_link_libraries(K7RSCarryWeight_core PUBLIC k7_common)
if(MSVC)
    target_compile_options(K7RSCarryWeight_core PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
endif()

if(K7_BUILD_TESTS)
    add_subdirectory(tests)
endif()

k7_add_plugin(${PROJECT_NAME}
    SOURCES
        src/main.cpp
        src/config_service.cpp
        src/race_lookup.cpp
        src/carry_service.cpp
        src/survival.cpp
        src/ui/lang_service.cpp
        src/ui/state.cpp
        src/ui/menu.cpp
        src/reload.cpp
    BUILD_ID_SOURCES
        src/main.cpp
        src/config_service.cpp
    PCH src/pch.h
)

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE K7RSCarryWeight_core k7_skse)
