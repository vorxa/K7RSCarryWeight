#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

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

TEST_CASE("configuration backups are unique, verified and outside the source directory", "[file_io]")
{
    TempDir dir;
    const auto previous = io::EnvironmentPath("K7_CONFIG_BACKUP_ROOT");
    const auto root = dir.path / "backups";
    const auto config = dir.path / "config" / "user.toml";
    std::filesystem::create_directories(config.parent_path());
#ifdef _MSC_VER
    REQUIRE(_putenv_s("K7_CONFIG_BACKUP_ROOT", root.string().c_str()) == 0);
#else
    REQUIRE(setenv("K7_CONFIG_BACKUP_ROOT", root.string().c_str(), 1) == 0);
#endif
    REQUIRE(io::WriteFile(config, "first"));
    CHECK(io::BackupExisting(config));
    REQUIRE(io::WriteFile(config, "second"));
    CHECK(io::BackupExisting(config));
    std::vector<std::string> contents;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file()) { contents.push_back(*io::ReadFile(entry.path())); }
    }
    CHECK(contents.size() == 2);
    CHECK(std::ranges::find(contents, "first") != contents.end());
    CHECK(std::ranges::find(contents, "second") != contents.end());
#ifdef _MSC_VER
    REQUIRE(_putenv_s("K7_CONFIG_BACKUP_ROOT", (config.parent_path() / "inside").string().c_str()) == 0);
#else
    REQUIRE(setenv("K7_CONFIG_BACKUP_ROOT", (config.parent_path() / "inside").string().c_str(), 1) == 0);
#endif
    CHECK_FALSE(io::BackupExisting(config));
    CHECK(io::ReadFile(config) == "second");
#ifdef _MSC_VER
    _putenv_s("K7_CONFIG_BACKUP_ROOT", previous ? previous->string().c_str() : "");
#else
    if (previous) { setenv("K7_CONFIG_BACKUP_ROOT", previous->string().c_str(), 1); } else { unsetenv("K7_CONFIG_BACKUP_ROOT"); }
#endif
}
