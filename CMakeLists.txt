cmake_minimum_required(VERSION 3.25)

# K7 Skyrim plugins: one repository, one CommonLibSSE-NG, one plugin per folder in plugins/.
# Each plugin has its own version in its own project() (plugins/<name>/CMakeLists.txt).
project(K7SkyrimPlugins LANGUAGES CXX)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed.")
endif()

# --- Build identifier: short commit hash, with "-dirty" if the working tree has uncommitted changes.
# Shown next to the version in the plugin logs, so that two builds of one version on different test
# installations can be told apart. It is not part of any version and never goes into config files.
set(K7_BUILD_ID "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=7 HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} OUTPUT_VARIABLE K7_GIT_HASH RESULT_VARIABLE K7_GIT_RESULT
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(K7_GIT_RESULT EQUAL 0)
        set(K7_BUILD_ID "${K7_GIT_HASH}")
        execute_process(COMMAND ${GIT_EXECUTABLE} status --porcelain
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} OUTPUT_VARIABLE K7_GIT_STATUS
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(K7_GIT_STATUS)
            set(K7_BUILD_ID "${K7_GIT_HASH}-dirty")
        endif()
    endif()
endif()
message(STATUS "Build id: ${K7_BUILD_ID}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Must be set before CommonLibSSE-NG is added: both must agree on the CRT.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# --- CommonLibSSE-NG: one DLL for SE, AE and VR -------------------------------------------
set(ENABLE_SKYRIM_SE ON  CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON  CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR ON  CACHE BOOL "" FORCE)
set(BUILD_TESTS      OFF CACHE BOOL "" FORCE)
set(SKSE_SUPPORT_XBYAK ON CACHE BOOL "" FORCE)
add_subdirectory(lib/commonlibsse-ng EXCLUDE_FROM_ALL)
# In a source build the library does not provide add_commonlibsse_plugin() itself.
include(lib/commonlibsse-ng/cmake/CommonLibSSE.cmake)
include(cmake/K7Plugin.cmake)

# --- Deploy into Mod Organizer 2 (machine-specific, set in CMakeUserPresets.json) -----------
# A ';'-separated list of MO2 "mods" folders, one per test installation; every plugin is copied into
# "<mods>/<plugin> (dev)".
set(K7_MO2_MODS_DIRS "$ENV{K7_MO2_MODS_DIRS}" CACHE STRING "MO2 mods folders to deploy the plugins into (optional)")

option(K7_BUILD_TESTS "Build the unit tests of the plugins" ON)
if(K7_BUILD_TESTS)
    enable_testing()
    add_test(NAME deploy_data_backup
        COMMAND ${CMAKE_COMMAND} "-DTEST_ROOT=${CMAKE_BINARY_DIR}"
            -P "${CMAKE_SOURCE_DIR}/tools/test_deploy_data.cmake")
endif()

# --- Shared libraries, before the plugins that link them ----------------------------------
add_subdirectory(libs/common)
add_subdirectory(libs/skse)

# --- Plugins: every plugins/<name> with a CMakeLists.txt. A folder without one (documents only) is
# skipped, so the tree builds with any subset of the plugins present.
file(GLOB k7PluginDirs LIST_DIRECTORIES true "${PROJECT_SOURCE_DIR}/plugins/*")
list(SORT k7PluginDirs)
foreach(dir IN LISTS k7PluginDirs)
    if(EXISTS "${dir}/CMakeLists.txt")
        add_subdirectory("${dir}")
    endif()
endforeach()
