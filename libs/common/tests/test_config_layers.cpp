#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "k7/common/config_layers.h"

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
            path = fs::temp_directory_path() / ("k7_common_layers_" + std::to_string(stamp));
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
    };

    // What the layers did to a model that is just "the last value of `v`" and the order of the files.
    struct Recorded
    {
        std::vector<std::string> order;
        std::string              v;
        std::vector<std::string> applied;
        Diagnostics              diagnostics;
    };

    Recorded Load(const fs::path& dir, const config::LayerOptions& options)
    {
        Recorded result;
        result.applied = config::LoadLayers(dir, options,
            [&](const toml::table& layer, const std::string& display, Diagnostics&) {
                result.order.push_back(display);
                if (const auto* v = layer.get("v"); v && v->is_string()) {
                    result.v = v->as_string()->get();
                }
            },
            result.diagnostics);
        return result;
    }

    bool HasError(const Diagnostics& diagnostics, std::string_view file)
    {
        return std::ranges::any_of(diagnostics, [&](const Diagnostic& d) { return d.severity == Severity::Error && d.file == file; });
    }

    config::LayerOptions AllLayers()
    {
        config::LayerOptions options;
        options.overrides = true;
        options.presets   = true;
        return options;
    }
}

TEST_CASE("preset names are plain file names", "[config]")
{
    CHECK(config::IsValidPresetName("hardcore"));
    CHECK(config::IsValidPresetName("my preset 2"));
    CHECK_FALSE(config::IsValidPresetName(""));
    CHECK_FALSE(config::IsValidPresetName("../evil"));
    CHECK_FALSE(config::IsValidPresetName("a/b"));
    CHECK_FALSE(config::IsValidPresetName("a\\b"));
    CHECK_FALSE(config::IsValidPresetName("C:evil"));
    CHECK_FALSE(config::IsValidPresetName(".."));
}

TEST_CASE("layers apply in the order config, overrides, preset, user", "[config]")
{
    TempDir dir;
    dir.Write("config.toml", "v = 'config'");
    dir.Write("overrides/b.toml", "v = 'b'");
    dir.Write("overrides/A.toml", "v = 'a'");
    dir.Write("overrides/notes.txt", "v = 'not a layer'");
    dir.Write("presets/hard.toml", "v = 'preset'");
    dir.Write("config.user.toml", "v = 'user'\n[meta]\npreset = 'hard'");

    const auto r = Load(dir.path, AllLayers());
    const std::vector<std::string> expected{ "config.toml", "overrides/A.toml", "overrides/b.toml", "presets/hard.toml", "config.user.toml" };
    CHECK(r.order == expected);
    CHECK(r.applied == expected);
    CHECK(r.v == "user");
    CHECK(r.diagnostics.empty());
}

TEST_CASE("without overrides and presets only the two files are read", "[config]")
{
    TempDir dir;
    dir.Write("config.toml", "v = 'config'");
    dir.Write("overrides/a.toml", "v = 'a'");
    dir.Write("presets/hard.toml", "v = 'preset'");
    dir.Write("config.user.toml", "[meta]\npreset = 'hard'");

    const auto r = Load(dir.path, config::LayerOptions{});
    CHECK(r.order == std::vector<std::string>{ "config.toml", "config.user.toml" });
    CHECK(r.v == "config");
    CHECK(r.diagnostics.empty());
}

TEST_CASE("a missing config.toml is an error, a missing user file is not", "[config]")
{
    TempDir dir;
    const auto r = Load(dir.path, config::LayerOptions{});
    CHECK(r.order.empty());
    CHECK(HasError(r.diagnostics, "config.toml"));
    CHECK_FALSE(HasError(r.diagnostics, "config.user.toml"));
}

TEST_CASE("a broken user file is skipped with its line, config.toml still applies", "[config]")
{
    TempDir dir;
    dir.Write("config.toml", "v = 'config'");
    dir.Write("config.user.toml", "v = 'user'\nthis is not toml");

    const auto r = Load(dir.path, config::LayerOptions{});
    CHECK(r.order == std::vector<std::string>{ "config.toml" });
    CHECK(r.v == "config");
    REQUIRE(HasError(r.diagnostics, "config.user.toml"));
    CHECK(r.diagnostics.front().line == 2);
    CHECK(dir.Read("config.user.toml") == "v = 'user'\nthis is not toml");
}

TEST_CASE("a preset name with a path is refused and a missing preset is an error", "[config]")
{
    TempDir dir;
    dir.Write("config.toml", "v = 'config'");

    SECTION("path")
    {
        dir.Write("evil.toml", "v = 'evil'");
        dir.Write("config.user.toml", "[meta]\npreset = '../evil'");
        const auto r = Load(dir.path, AllLayers());
        CHECK(r.v == "config");
        CHECK(HasError(r.diagnostics, "config.user.toml"));
    }
    SECTION("missing")
    {
        dir.Write("config.user.toml", "[meta]\npreset = 'none'");
        const auto r = Load(dir.path, AllLayers());
        CHECK(HasError(r.diagnostics, "presets/none.toml"));
    }
}

TEST_CASE("migration: the user file is rewritten with a backup, other files only in memory", "[config][migrate]")
{
    const std::vector<Migration> chain{ Migration{ 1, [](toml::table& table) { table.insert_or_assign("v", "migrated"); } } };
    config::LayerOptions options;
    options.migrations     = chain;
    options.currentVersion = 2;

    TempDir dir;
    // No config_version: both files are version 1.
    const std::string config = "v = 'config'";
    const std::string user   = "v = 'user'";
    dir.Write("config.toml", config);
    dir.Write("config.user.toml", user);

    const auto r = Load(dir.path, options);
    CHECK(r.v == "migrated");
    CHECK(dir.Read("config.toml") == config);
    CHECK(dir.Read("config.user.toml.bak") == user);
    const auto rewritten = toml::parse(dir.Read("config.user.toml"));
    CHECK(rewritten["meta"]["config_version"].value_or(0) == 2);
    CHECK(rewritten["v"].value_or(std::string{}) == "migrated");
}

TEST_CASE("a file from a newer schema is skipped, the others still apply", "[config][migrate]")
{
    TempDir dir;
    dir.Write("config.toml", "v = 'config'");
    dir.Write("config.user.toml", "v = 'user'\n[meta]\nconfig_version = 5");

    const auto r = Load(dir.path, config::LayerOptions{});
    CHECK(r.v == "config");
    CHECK(HasError(r.diagnostics, "config.user.toml"));
}

TEST_CASE("EnsureConfigFile writes the defaults once per plugin version", "[config]")
{
    TempDir    dir;
    const auto sub = dir.path / "nested" / "dir";
    int        calls = 0;
    const auto make = [&] {
        ++calls;
        return std::string("[meta]\nplugin_version = '1.0.0'\n");
    };
    Diagnostics diagnostics;

    CHECK(config::EnsureConfigFile(sub, "config.toml", "1.0.0", make, diagnostics));
    CHECK(fs::exists(sub / "config.toml"));
    CHECK_FALSE(config::EnsureConfigFile(sub, "config.toml", "1.0.0", make, diagnostics));
    CHECK(calls == 1);

    // Another version, a missing stamp, a broken file: all rewritten.
    CHECK(config::EnsureConfigFile(sub, "config.toml", "1.1.0", make, diagnostics));
    std::ofstream(sub / "config.toml", std::ios::trunc) << "x = 1";
    CHECK(config::EnsureConfigFile(sub, "config.toml", "1.0.0", make, diagnostics));
    std::ofstream(sub / "config.toml", std::ios::trunc) << "[[[";
    CHECK(config::EnsureConfigFile(sub, "config.toml", "1.0.0", make, diagnostics));
    CHECK(diagnostics.empty());
}
