#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "core/loader.h"
#include "core/serialize.h"
#include "test_helpers.h"

using namespace k7::rscw;
using namespace k7::rscw::test;
using k7::common::Diagnostics;
using k7::common::Severity;

namespace
{
    constexpr auto kVersion = "0.0.1";

    void WriteDefaults(const TempDir& dir)
    {
        Diagnostics ignored;
        core::EnsureConfigToml(dir.Path(), kVersion, ignored);
    }
}

TEST_CASE("the generated config.toml reads back as the built-in configuration", "[loader][generate]")
{
    TempDir dir;
    WriteDefaults(dir);
    const auto result = core::LoadConfig(dir.Path());
    CHECK(result.config == core::MakeDefaultConfig());
    CHECK(result.diagnostics.empty());
    CHECK(result.appliedFiles == std::vector<std::string>{ "config.toml" });

    SECTION("and a changed configuration survives the text too")
    {
        auto config                 = core::MakeDefaultConfig();
        config.general.enabled      = false;
        config.general.minTotal     = 33;
        config.general.logLevel     = k7::common::LogLevel::Trace;
        config.races.front().male   = 999;
        config.beastForms.pop_back();
        dir.Write("config.toml", core::ToConfigToml(config, kVersion));
        const auto again = core::LoadConfig(dir.Path());
        CHECK(again.config == config);
        CHECK(again.diagnostics.empty());
    }
}

TEST_CASE("config.user.toml wins over config.toml (T3)", "[loader]")
{
    TempDir dir;
    WriteDefaults(dir);
    dir.Write("config.user.toml", "[races.nord]\nmale = 120\n[general]\nmin_total = 40\n");
    const auto result = core::LoadConfig(dir.Path());
    CHECK(core::FindRace(result.config, "nord")->male == 120);
    CHECK(core::FindRace(result.config, "nord")->female == 100);
    CHECK(result.config.general.minTotal == 40);
    CHECK(result.appliedFiles == std::vector<std::string>{ "config.toml", "config.user.toml" });
    CHECK(result.diagnostics.empty());
}

TEST_CASE("a broken config.user.toml is not applied and not rewritten (T17)", "[loader]")
{
    TempDir dir;
    WriteDefaults(dir);
    const std::string broken = "[races.nord]\nmale = = 120\n";
    dir.Write("config.user.toml", broken);
    const auto result = core::LoadConfig(dir.Path());
    CHECK(result.config == core::MakeDefaultConfig());
    CHECK(Count(result.diagnostics, Severity::Error) == 1);
    CHECK(result.diagnostics.front().file == "config.user.toml");
    CHECK(result.diagnostics.front().line == 2);
    CHECK(dir.Read("config.user.toml") == broken);
}

TEST_CASE("a missing or broken config.toml: built-in values, and the file is written again (T18)", "[loader]")
{
    TempDir dir;

    SECTION("missing")
    {
        const auto result = core::LoadConfig(dir.Path());
        CHECK(result.config == core::MakeDefaultConfig());
        CHECK(Count(result.diagnostics, Severity::Error) == 1);
    }
    SECTION("broken")
    {
        dir.Write("config.toml", "[[[ not toml");
        const auto result = core::LoadConfig(dir.Path());
        CHECK(result.config == core::MakeDefaultConfig());
        CHECK(Count(result.diagnostics, Severity::Error) == 1);
    }
    Diagnostics diagnostics;
    CHECK(core::EnsureConfigToml(dir.Path(), kVersion, diagnostics));
    CHECK(core::LoadConfig(dir.Path()).diagnostics.empty());
}

TEST_CASE("a new plugin version rewrites config.toml and leaves config.user.toml alone (T19)", "[loader]")
{
    TempDir dir;
    WriteDefaults(dir);
    dir.Write("config.user.toml", "[races.nord]\nmale = 120\n");
    Diagnostics diagnostics;
    CHECK_FALSE(core::EnsureConfigToml(dir.Path(), kVersion, diagnostics));
    CHECK(core::EnsureConfigToml(dir.Path(), "0.0.2", diagnostics));
    CHECK(dir.Read("config.toml").find("plugin_version = \"0.0.2\"") != std::string::npos);
    CHECK(dir.Read("config.user.toml") == "[races.nord]\nmale = 120\n");
    CHECK(diagnostics.empty());
}

TEST_CASE("an older config_version is migrated, the old file kept as .bak (T20)", "[loader][migrate]")
{
    const std::vector<k7::common::Migration> chain{ k7::common::Migration{ 1, [](toml::table& table) {
        // A test step: version 1 called the key "minimum".
        if (auto* general = table["general"].as_table()) {
            if (const auto* old = general->get("minimum")) {
                const auto value = old->value_or<int64_t>(20);
                general->erase("minimum");
                general->insert_or_assign("min_total", value);
            }
        }
    } } };
    core::LoadOptions options;
    options.migrations     = chain;
    options.currentVersion = 2;

    TempDir dir;
    dir.Write("config.toml", "[meta]\nconfig_version = 2\n");
    const std::string old = "[general]\nminimum = 45\n";
    dir.Write("config.user.toml", old);
    const auto result = core::LoadConfig(dir.Path(), options);
    CHECK(result.config.general.minTotal == 45);
    CHECK(dir.Read("config.user.toml.bak") == old);
    CHECK(Count(result.diagnostics, Severity::Warning) == 1);
    CHECK(Mentions(result.diagnostics, "config.user.toml.bak"));
}

TEST_CASE("a newer config_version is not applied (T21)", "[loader][migrate]")
{
    TempDir dir;
    WriteDefaults(dir);
    dir.Write("config.user.toml", "[meta]\nconfig_version = 2\n[races.nord]\nmale = 120\n");
    const auto result = core::LoadConfig(dir.Path());
    CHECK(core::FindRace(result.config, "nord")->male == 115);
    CHECK(Count(result.diagnostics, Severity::Error) == 1);
    CHECK(Mentions(result.diagnostics, "update the plugin"));
}
