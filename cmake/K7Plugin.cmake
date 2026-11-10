# k7_add_plugin(): what every SKSE plugin of this repository shares.
#
#   k7_add_plugin(<name>
#       SOURCES <files...>                  plugin sources, relative to the plugin folder
#       [BUILD_ID_SOURCES <files...>]       the sources that print the build id (get K7_BUILD_ID)
#       [PCH <header>])                     precompiled header
#
# Call it from the plugin's CMakeLists.txt after the plugin's own project(): the version is PROJECT_VERSION.
# It adds:
#   * the CommonLibSSE-NG plugin target <name> (DLL, version resource, SKSE plugin info) and the define
#     K7_PLUGIN_VERSION="<version>";
#   * K7_BUILD_ID="<id>" only on BUILD_ID_SOURCES, so that a new commit does not rebuild the whole plugin;
#   * MSVC warnings and flags, debug symbols in Release for Crash Logger;
#   * deployment into "<mods>/<name> (dev)" for every Mod Organizer 2 mods folder in K7_MO2_MODS_DIRS:
#     the DLL and the PDB when the DLL is linked, and the plugin's dist/SKSE/Plugins (if it has one) on every
#     build, so that a change of a data file such as lang/*.toml reaches the stands without relinking.

function(k7_add_plugin name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "PCH" "SOURCES;BUILD_ID_SOURCES")
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "k7_add_plugin(${name}): SOURCES is required")
    endif()

    add_commonlibsse_plugin(${name}
        NAME ${name}
        VERSION ${PROJECT_VERSION}
        SOURCES ${ARG_SOURCES}
    )
    target_compile_definitions(${name} PRIVATE K7_PLUGIN_VERSION="${PROJECT_VERSION}")
    if(ARG_BUILD_ID_SOURCES)
        set_source_files_properties(${ARG_BUILD_ID_SOURCES} PROPERTIES COMPILE_DEFINITIONS K7_BUILD_ID="${K7_BUILD_ID}")
    endif()
    if(ARG_PCH)
        target_precompile_headers(${name} PRIVATE ${ARG_PCH})
    endif()

    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
        # Keep debug symbols in Release so Crash Logger can resolve our frames.
        target_compile_options(${name} PRIVATE /Zi)
        target_link_options(${name} PRIVATE /DEBUG /OPT:REF /OPT:ICF)
    endif()

    set(copyDistCommands "")
    foreach(modsDir IN LISTS K7_MO2_MODS_DIRS)
        set(deployDir "${modsDir}/${name} (dev)")
        add_custom_command(TARGET ${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${deployDir}/SKSE/Plugins"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:${name}>" "$<TARGET_PDB_FILE:${name}>"
                    "${deployDir}/SKSE/Plugins"
            COMMENT "Deploying ${name} to ${deployDir}"
        )
        if(EXISTS "${PROJECT_SOURCE_DIR}/dist/SKSE/Plugins")
            list(APPEND copyDistCommands COMMAND ${CMAKE_COMMAND}
                "-DSOURCE=${PROJECT_SOURCE_DIR}/dist/SKSE/Plugins"
                "-DDESTINATION=${deployDir}/SKSE/Plugins"
                "-DBACKUP_ROOT=${modsDir}/.k7-deploy-backups/${name}"
                -P "${CMAKE_SOURCE_DIR}/tools/deploy_data.cmake")
        endif()
    endforeach()
    if(copyDistCommands)
        add_custom_target(${name}_deploy_dist ALL ${copyDistCommands}
            COMMENT "Deploying the data files of ${name}")
    endif()
endfunction()
