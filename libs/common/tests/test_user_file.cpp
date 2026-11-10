#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "k7/common/user_file.h"

using namespace k7::common;

namespace
{
    namespace fs = std::filesystem;

    // A fresh folder in the temp directory, removed at the end of the test.
    struct TempDir
    {
        fs::path path;
        TempDir()
        {
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            path = fs::temp_directory_path() / ("k7_common_userfile_" + std::to_string(stamp));
            fs::create_directories(path);
        }
        ~TempDir()
        {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
        void Write(const std::string& relative, std::string_view content) const
        {
            const auto file = path / relative;
            fs::create_directories(file.parent_path());
            std::ofstream stream(file, std::ios::binary | std::ios::trunc);
            stream.write(content.data(), static_cast<std::streamsize>(content.size()));
        }
        std::string Read(const std::string& relative) const
        {
            std::ifstream stream(path / relative, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(stream), {});
        }
        bool Exists(const std::string& relative) const
        {
            std::error_code ec;
            return fs::exists(path / relative, ec);
        }
    };

    constexpr std::string_view kUser = "config.user.toml";
}

TEST_CASE("user file: a missing file is created without a backup", "[user_file]")
{
    TempDir          dir;
    Diagnostics      diagnostics;
    bool             backupDone = false;
    const auto state          = config::ReadUserFile(dir.path, kUser);
    CHECK_FALSE(state.text);
    CHECK(state.parses);

    const auto result = config::WriteUserFile(dir.path, kUser, "# new\n", state.text, false,
        config::BackupPolicy::EachWrite, backupDone, diagnostics);
    CHECK(result == config::UserFileResult::Written);
    CHECK(dir.Read("config.user.toml") == "# new\n");
    CHECK_FALSE(dir.Exists("config.user.toml.bak"));
    CHECK(diagnostics.empty());
}

TEST_CASE("user file: each write keeps the previous text, once per session keeps the first", "[user_file]")
{
    TempDir dir;
    dir.Write("config.user.toml", "# one\n");
    Diagnostics diagnostics;

    SECTION("each write")
    {
        bool       backupDone = false;
        const auto first      = config::WriteUserFile(dir.path, kUser, "# two\n", std::string("# one\n"), false,
            config::BackupPolicy::EachWrite, backupDone, diagnostics);
        REQUIRE(first == config::UserFileResult::Written);
        CHECK(dir.Read("config.user.toml.bak") == "# one\n");

        const auto second = config::WriteUserFile(dir.path, kUser, "# three\n", std::string("# two\n"), false,
            config::BackupPolicy::EachWrite, backupDone, diagnostics);
        REQUIRE(second == config::UserFileResult::Written);
        CHECK(dir.Read("config.user.toml.bak") == "# two\n");
    }

    SECTION("once per session")
    {
        bool       backupDone = false;
        const auto first      = config::WriteUserFile(dir.path, kUser, "# two\n", std::string("# one\n"), false,
            config::BackupPolicy::OncePerSession, backupDone, diagnostics);
        REQUIRE(first == config::UserFileResult::Written);
        CHECK(backupDone);
        CHECK(dir.Read("config.user.toml.bak") == "# one\n");

        const auto second = config::WriteUserFile(dir.path, kUser, "# three\n", std::string("# two\n"), false,
            config::BackupPolicy::OncePerSession, backupDone, diagnostics);
        REQUIRE(second == config::UserFileResult::Written);
        CHECK(dir.Read("config.user.toml.bak") == "# one\n");
    }
}

TEST_CASE("user file: a file that changed after reading is left alone unless overwritten", "[user_file]")
{
    TempDir      dir;
    Diagnostics  diagnostics;
    bool         backupDone = false;
    const auto   state      = config::ReadUserFile(dir.path, kUser);
    dir.Write("config.user.toml", "# changed elsewhere\n");

    const auto refused = config::WriteUserFile(dir.path, kUser, "# mine\n", state.text, false,
        config::BackupPolicy::EachWrite, backupDone, diagnostics);
    CHECK(refused == config::UserFileResult::Changed);
    CHECK(dir.Read("config.user.toml") == "# changed elsewhere\n");
    CHECK_FALSE(dir.Exists("config.user.toml.bak"));

    const auto written = config::WriteUserFile(dir.path, kUser, "# mine\n", state.text, true,
        config::BackupPolicy::EachWrite, backupDone, diagnostics);
    CHECK(written == config::UserFileResult::Written);
    CHECK(dir.Read("config.user.toml") == "# mine\n");
    CHECK(dir.Read("config.user.toml.bak") == "# changed elsewhere\n");
}

TEST_CASE("user file: a file that does not parse is never written over", "[user_file]")
{
    TempDir     dir;
    Diagnostics diagnostics;
    bool        backupDone = false;
    dir.Write("config.user.toml", "this is not toml");

    const auto result = config::WriteUserFile(dir.path, kUser, "# mine\n", std::string("this is not toml"), true,
        config::BackupPolicy::EachWrite, backupDone, diagnostics);
    CHECK(result == config::UserFileResult::Broken);
    CHECK(dir.Read("config.user.toml") == "this is not toml");
    CHECK_FALSE(dir.Exists("config.user.toml.bak"));
}

TEST_CASE("user file: a clean start keeps the old text next to the empty file", "[user_file]")
{
    TempDir     dir;
    Diagnostics diagnostics;
    dir.Write("config.user.toml", "# mine\n");

    REQUIRE(config::StartCleanUserFile(dir.path, kUser, diagnostics));
    CHECK(dir.Read("config.user.toml").empty());
    CHECK(dir.Read("config.user.toml.bak") == "# mine\n");
    CHECK(diagnostics.empty());
}

TEST_CASE("presets: the list is the toml files in presets, sorted without regard to case", "[user_file]")
{
    TempDir dir;
    dir.Write("presets/Balanced.toml", "");
    dir.Write("presets/hardcore.toml", "");
    dir.Write("presets/rab_2_05_defaults.toml", "");
    dir.Write("presets/notes.txt", "");
    dir.Write("presets/Sub/nested.toml", "");

    CHECK(config::ListPresets(dir.path) == std::vector<std::string>{ "Balanced", "hardcore", "rab_2_05_defaults" });
    CHECK(config::ListPresets(dir.path / "missing").empty());
}

TEST_CASE("presets: the choice is written beside the player's keys and the old text is kept", "[user_file]")
{
    TempDir     dir;
    Diagnostics diagnostics;
    dir.Write("presets/hardcore.toml", "");
    dir.Write("config.user.toml", "[general]\nenabled = false\n");

    REQUIRE(config::SetPreset(dir.path, kUser, "hardcore", diagnostics));
    const auto text = dir.Read("config.user.toml");
    CHECK(text.find("preset") != std::string::npos);
    CHECK(text.find("hardcore") != std::string::npos);
    CHECK(text.find("enabled") != std::string::npos);
    CHECK(dir.Read("config.user.toml.bak") == "[general]\nenabled = false\n");

    // An empty name clears the choice and keeps the rest.
    REQUIRE(config::SetPreset(dir.path, kUser, "", diagnostics));
    const auto cleared = dir.Read("config.user.toml");
    CHECK(cleared.find("preset") == std::string::npos);
    CHECK(cleared.find("enabled") != std::string::npos);
}

TEST_CASE("presets: an invalid or missing preset and a broken file leave the file alone", "[user_file]")
{
    TempDir     dir;
    Diagnostics diagnostics;
    dir.Write("presets/hardcore.toml", "");

    CHECK_FALSE(config::SetPreset(dir.path, kUser, "nope", diagnostics));
    CHECK_FALSE(config::SetPreset(dir.path, kUser, "../hardcore", diagnostics));
    CHECK_FALSE(dir.Exists("config.user.toml"));

    dir.Write("config.user.toml", "this is not toml");
    CHECK_FALSE(config::SetPreset(dir.path, kUser, "hardcore", diagnostics));
    CHECK(dir.Read("config.user.toml") == "this is not toml");
    CHECK_FALSE(dir.Exists("config.user.toml.bak"));
}
