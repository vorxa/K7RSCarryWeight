find_package(Catch2 3 CONFIG REQUIRED)

add_executable(k7_common_tests
    test_diagnostics.cpp
    test_text.cpp
    test_toml_reader.cpp
    test_file_io.cpp
    test_form_key.cpp
    test_log_level.cpp
    test_migrate.cpp
    test_config_layers.cpp
    test_lang.cpp
    test_user_file.cpp
)
target_link_libraries(k7_common_tests PRIVATE k7_common Catch2::Catch2WithMain)
if(MSVC)
    target_compile_options(k7_common_tests PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
endif()

include(Catch)
catch_discover_tests(k7_common_tests)
