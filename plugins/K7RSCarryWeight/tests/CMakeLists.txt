find_package(Catch2 3 CONFIG REQUIRED)

add_executable(K7RSCarryWeight_tests
    test_carry.cpp
    test_defaults.cpp
    test_apply.cpp
    test_loader.cpp
    test_recount.cpp
    test_lang_tables.cpp
    test_user_edit.cpp
)
target_link_libraries(K7RSCarryWeight_tests PRIVATE K7RSCarryWeight_core Catch2::Catch2WithMain)
target_compile_definitions(K7RSCarryWeight_tests PRIVATE
    K7RS_LANG_DIR="${PROJECT_SOURCE_DIR}/dist/SKSE/Plugins/K7RSCarryWeight/lang")
if(MSVC)
    target_compile_options(K7RSCarryWeight_tests PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
endif()

include(Catch)
catch_discover_tests(K7RSCarryWeight_tests)
