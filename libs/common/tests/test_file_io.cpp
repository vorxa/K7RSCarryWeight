#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <string>

#include "k7/common/file_io.h"

using namespace k7::common;

namespace
{
    // A fresh folder in the temp directory, removed at the end of the test.
    struct TempDir
    {
        std::filesystem::path path;
        TempDir()
        {
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            path = std::filesystem::temp_directory_path() / ("k7_common_tests_" + std::to_string(stamp));
            std::filesystem::create_directories(path);
        }
        ~TempDir()
        {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    };
}

TEST_CASE("a written file reads back byte for byte", "[file_io]")
{
    TempDir dir;
    const auto file = dir.path / "a.toml";
    const std::string content = "line 1\r\nline 2\n\xC3\xA9";
    REQUIRE(io::WriteFile(file, content));
    CHECK(io::ReadFile(file) == content);
}

TEST_CASE("reading a missing file gives nothing", "[file_io]")
{
    TempDir dir;
    CHECK_FALSE(io::ReadFile(dir.path / "missing.toml").has_value());
}

TEST_CASE("an atomic write replaces the file and leaves no temporary file", "[file_io]")
{
    TempDir dir;
    const auto file = dir.path / "config.user.toml";
    REQUIRE(io::WriteFile(file, "old"));
    REQUIRE(io::WriteFileAtomic(file, "new"));
    CHECK(io::ReadFile(file) == "new");
    CHECK_FALSE(std::filesystem::exists(dir.path / "config.user.toml.tmp"));
}

TEST_CASE("an atomic write into a missing folder fails and creates nothing", "[file_io]")
{
    TempDir dir;
    const auto file = dir.path / "no_such_folder" / "config.user.toml";
    CHECK_FALSE(io::WriteFileAtomic(file, "x"));
    CHECK_FALSE(std::filesystem::exists(file));
}
