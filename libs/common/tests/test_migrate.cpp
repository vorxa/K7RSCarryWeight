#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "k7/common/migrate.h"

using namespace k7::common;

namespace
{
    bool Mentions(const Diagnostics& diagnostics, std::string_view text)
    {
        return std::ranges::any_of(diagnostics, [&](const Diagnostic& d) { return d.message.find(text) != std::string::npos; });
    }

    std::size_t Count(const Diagnostics& diagnostics, Severity severity)
    {
        return static_cast<std::size_t>(std::ranges::count_if(diagnostics, [&](const Diagnostic& d) { return d.severity == severity; }));
    }
}

namespace
{
    // A schema that went through two upgrades: 1 -> 2 renames [general] old_flag to enabled,
    // 2 -> 3 adds a ui hotkey.
    std::vector<Migration> TestChain()
    {
        return {
            Migration{ 1, [](toml::table& table) {
                          if (auto* general = table["general"].as_table()) {
                              if (const auto* old = general->get("old_flag")) {
                                  const bool value = old->value_or(true);
                                  general->erase("old_flag");
                                  general->insert_or_assign("enabled", value);
                              }
                          }
                      } },
            Migration{ 2, [](toml::table& table) {
                          table.insert_or_assign("ui", toml::table{ { "hotkey", "F9" } });
                      } },
        };
    }
}

TEST_CASE("MigrateLayer follows FR-5.6", "[migrate]")
{
    const auto chain = TestChain();
    Diagnostics diagnostics;

    const auto run = [&](std::string_view text, int current = 3) {
        auto table = toml::parse(text);
        auto result = MigrateLayer(table, "f.toml", chain, current, diagnostics);
        return std::make_pair(std::move(table), result);
    };

    SECTION("no version is version 1: current when the plugin is at 1")
    {
        const auto [table, result] = run("[general]\nenabled = true\n", 1);
        CHECK(result.usable);
        CHECK_FALSE(result.changed);
        CHECK(diagnostics.empty());
    }
    SECTION("no version is version 1: the whole chain runs and the file is stamped")
    {
        const auto [table, result] = run("[general]\nold_flag = false\n");
        CHECK(result.usable);
        CHECK(result.changed);
        CHECK(result.fromVersion == 1);
        CHECK(table["general"]["enabled"].value_or(true) == false);
        CHECK_FALSE(table["general"]["old_flag"]);
        CHECK(table["ui"]["hotkey"].value_or(std::string{}) == "F9");
        CHECK(table["meta"]["config_version"].value_or(0) == 3);
        CHECK(diagnostics.empty());
    }
    SECTION("the current version is untouched")
    {
        const auto [table, result] = run("[meta]\nconfig_version = 3\n");
        CHECK(result.usable);
        CHECK_FALSE(result.changed);
    }
    SECTION("an older file runs the chain step by step and is stamped")
    {
        const auto [table, result] = run("[meta]\nconfig_version = 1\n[general]\nold_flag = false\n");
        CHECK(result.usable);
        CHECK(result.changed);
        CHECK(result.fromVersion == 1);
        CHECK(table["general"]["enabled"].value<bool>() == false);
        CHECK_FALSE(table["general"].as_table()->contains("old_flag"));
        CHECK(table["ui"]["hotkey"].value<std::string>() == "F9");
        CHECK(table["meta"]["config_version"].value<int>() == 3);
    }
    SECTION("an older file starts from its own version")
    {
        const auto [table, result] = run("[meta]\nconfig_version = 2\n");
        CHECK(result.changed);
        CHECK(result.fromVersion == 2);
        CHECK(table["ui"]["hotkey"].value<std::string>() == "F9");
    }
    SECTION("a newer file is not applied and the error says to update the plugin")
    {
        const auto [table, result] = run("[meta]\nconfig_version = 4\n");
        CHECK_FALSE(result.usable);
        REQUIRE(diagnostics.size() == 1);
        CHECK(diagnostics[0].severity == Severity::Error);
        CHECK(diagnostics[0].line == 2);
        CHECK(Mentions(diagnostics, "update the plugin"));
    }
    SECTION("garbage versions are errors")
    {
        CHECK_FALSE(run("[meta]\nconfig_version = \"two\"\n").second.usable);
        CHECK_FALSE(run("[meta]\nconfig_version = 0\n").second.usable);
        CHECK_FALSE(run("[meta]\nconfig_version = 1.5\n").second.usable);
        CHECK(Count(diagnostics, Severity::Error) == 3);
    }
    SECTION("a gap in the chain is an error, not a silent skip")
    {
        auto table = toml::parse("[meta]\nconfig_version = 1\n");
        const std::vector<Migration> gap{ Migration{ 2, [](toml::table&) {} } };
        const auto result = MigrateLayer(table, "f.toml", gap, 3, diagnostics);
        CHECK_FALSE(result.usable);
        CHECK(Mentions(diagnostics, "no migration from config_version 1"));
    }
}
