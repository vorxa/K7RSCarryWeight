#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "core/loader.h"
#include "core/user_edit.h"
#include "test_helpers.h"

using namespace k7::rscw;
using namespace k7::rscw::test;
using k7::common::Diagnostics;
using k7::common::FormKey;

namespace
{
    constexpr auto kVersion = "0.0.1";

    void WriteDefaults(const TempDir& dir)
    {
        Diagnostics ignored;
        core::EnsureConfigToml(dir.Path(), kVersion, ignored);
    }

    // Races in a stable order: a race added by a layer comes in alphabetical order, the menu appends it.
    core::Config Sorted(core::Config config)
    {
        std::ranges::sort(config.races, {}, &core::RaceEntry::id);
        return config;
    }

    core::Config LoadBase(const TempDir& dir)
    {
        core::LoadOptions options;
        options.userLayer = false;
        return core::LoadConfig(dir.Path(), options).config;
    }
}

TEST_CASE("the menu's text reads back as the edited configuration", "[user_edit]")
{
    TempDir dir;
    WriteDefaults(dir);
    const auto base   = LoadBase(dir);
    auto       edited = base;
    edited.general.minTotal        = 30;
    edited.general.survivalPenalty = 25;
    edited.general.logLevel        = k7::common::LogLevel::Debug;
    core::FindRace(edited, "nord")->male = 125;
    core::AddRace(edited, "aaaHalfDragonRace", FormKey{ "halfdragonrace.esp", 0x0125AA }, 300.0f);
    core::AddRecord(core::FindRace(edited, "aaahalfdragonrace")->forms, FormKey{ "halfdragonrace.esp", 0x013B4C });
    core::AddRecord(edited.beastForms, FormKey{ "MyWerebear.esp", 0x000900 });
    core::AddRecord(edited.survivalEffects, FormKey{ "MySurvival.esp", 0x000801 });

    dir.Write("config.user.toml", core::ToUserToml(base, edited));
    const auto loaded = core::LoadConfig(dir.Path());
    CHECK(loaded.diagnostics.empty());
    CHECK(Sorted(loaded.config) == Sorted(edited));
}

TEST_CASE("only the differences are written", "[user_edit]")
{
    const auto base   = core::MakeDefaultConfig();
    auto       edited = base;
    SECTION("nothing changed: no table at all")
    {
        const auto text = core::ToUserToml(base, edited);
        CHECK(text.find('[') == std::string::npos);
    }
    SECTION("one value of a vanilla race")
    {
        core::FindRace(edited, "nord")->female = 105;
        const auto text = core::ToUserToml(base, edited);
        CHECK(text.find("[races.nord]\nfemale = 105\n") != std::string::npos);
        CHECK(text.find("male   =") == std::string::npos);
        CHECK(text.find("forms") == std::string::npos);
        CHECK(text.find("[general]") == std::string::npos);
        CHECK(text.find("[beast]") == std::string::npos);
        CHECK(text.find("[survival]") == std::string::npos);
    }
    SECTION("a general key")
    {
        edited.general.enabled = false;
        const auto text = core::ToUserToml(base, edited);
        CHECK(text.find("[general]\nenabled = false\n") != std::string::npos);
        CHECK(text.find("min_total") == std::string::npos);
    }
}

TEST_CASE("built-in values bring a race back to the base", "[user_edit]")
{
    const auto base   = core::MakeDefaultConfig();
    auto       edited = base;
    core::FindRace(edited, "orc")->male = 200;
    REQUIRE(core::RevertRace(edited, base, "orc"));
    CHECK(*core::FindRace(edited, "orc") == *core::FindRace(base, "orc"));
    CHECK(core::ToUserToml(base, edited).find("[races.orc]") == std::string::npos);
    CHECK_FALSE(core::RevertRace(edited, base, "no_such_race"));
}

TEST_CASE("only a race the player added can be removed", "[user_edit]")
{
    const auto base   = core::MakeDefaultConfig();
    auto       edited = base;
    const auto id     = core::AddRace(edited, "MyRace", FormKey{ "MyRace.esp", 0x000800 }, 300.0f);
    CHECK(core::CanRemoveRace(base, id));
    CHECK_FALSE(core::CanRemoveRace(base, "nord"));
    CHECK_FALSE(core::RemoveRace(edited, base, "nord"));
    CHECK(core::RemoveRace(edited, base, id));
    CHECK(core::FindRace(edited, id) == nullptr);
}

TEST_CASE("the id of an added race comes from its EditorID", "[user_edit]")
{
    auto config = core::MakeDefaultConfig();
    CHECK(core::NewRaceId(config, "aaaHalfDragonRace") == "aaahalfdragonrace");
    CHECK(core::NewRaceId(config, "My Race-01!") == "my_race_01");
    CHECK(core::NewRaceId(config, "") == "race");
    CHECK(core::NewRaceId(config, "Nord") == "nord_2");
    const auto id = core::AddRace(config, "Nord", FormKey{ "NewNord.esp", 0x000800 }, 250.0f);
    CHECK(id == "nord_2");
    CHECK(core::FindRace(config, id)->male == 250);
    CHECK(core::FindRace(config, id)->female == 250);
    CHECK(core::NewRaceId(config, "Nord") == "nord_3");
}

TEST_CASE("a record is added once and removed", "[user_edit]")
{
    std::vector<FormKey> list{ { "Skyrim.esm", 0x0CDD84 } };
    CHECK(core::AddRecord(list, FormKey{ "skyrim.esm", 0x0CDD84 }) == core::AddRecordResult::Duplicate);
    CHECK(core::AddRecord(list, FormKey{ "Mod.esp", 0x000801 }) == core::AddRecordResult::Added);
    CHECK(list.size() == 2);
    CHECK(core::IsInList(list, FormKey{ "MOD.ESP", 0x000801 }));
    CHECK(core::RemoveRecord(list, FormKey{ "Mod.esp", 0x000801 }));
    CHECK_FALSE(core::RemoveRecord(list, FormKey{ "Mod.esp", 0x000801 }));
}

TEST_CASE("writing: a backup once, a changed file is not overwritten unless asked", "[user_edit]")
{
    TempDir dir;
    dir.Write("config.user.toml", "[general]\nmin_total = 40\n");
    auto        state      = core::ReadUserFile(dir.Path());
    bool        backupDone = false;
    Diagnostics diagnostics;
    REQUIRE(state.parses);

    CHECK(core::WriteUserFile(dir.Path(), "# one\n", state.text, false, backupDone, diagnostics) == core::WriteUserResult::Written);
    CHECK(backupDone);
    CHECK(dir.Read("config.user.toml.bak") == "[general]\nmin_total = 40\n");
    CHECK(dir.Read("config.user.toml") == "# one\n");

    // The menu still holds the old content: the file changed under it.
    CHECK(core::WriteUserFile(dir.Path(), "# two\n", state.text, false, backupDone, diagnostics) == core::WriteUserResult::Changed);
    CHECK(dir.Read("config.user.toml") == "# one\n");

    // Overwrite: written, the backup of the session stays as it was.
    CHECK(core::WriteUserFile(dir.Path(), "# two\n", state.text, true, backupDone, diagnostics) == core::WriteUserResult::Written);
    CHECK(dir.Read("config.user.toml") == "# two\n");
    CHECK(dir.Read("config.user.toml.bak") == "[general]\nmin_total = 40\n");
    CHECK(diagnostics.empty());
}

TEST_CASE("writing: a missing file is created", "[user_edit]")
{
    TempDir     dir;
    auto        state      = core::ReadUserFile(dir.Path());
    bool        backupDone = false;
    Diagnostics diagnostics;
    CHECK_FALSE(state.text);
    CHECK(core::WriteUserFile(dir.Path(), "# new\n", state.text, false, backupDone, diagnostics) == core::WriteUserResult::Written);
    CHECK(dir.Read("config.user.toml") == "# new\n");
}

TEST_CASE("a file that does not parse is never written over; a clean file keeps it as .bak", "[user_edit]")
{
    TempDir dir;
    dir.Write("config.user.toml", "[general]\n[[[\n");
    auto state = core::ReadUserFile(dir.Path());
    CHECK_FALSE(state.parses);
    CHECK(Mentions(state.diagnostics, "syntax error"));

    bool        backupDone = false;
    Diagnostics diagnostics;
    CHECK(core::WriteUserFile(dir.Path(), "# x\n", state.text, true, backupDone, diagnostics) == core::WriteUserResult::Broken);
    CHECK(dir.Read("config.user.toml") == "[general]\n[[[\n");

    REQUIRE(core::StartCleanUserFile(dir.Path(), diagnostics));
    CHECK(dir.Read("config.user.toml").empty());
    CHECK(dir.Read("config.user.toml.bak") == "[general]\n[[[\n");
}

TEST_CASE("the base leaves config.user.toml out", "[user_edit][loader]")
{
    TempDir dir;
    WriteDefaults(dir);
    dir.Write("config.user.toml", "[races.nord]\nmale = 150\n");
    CHECK(core::FindRace(LoadBase(dir), "nord")->male == 115);
    CHECK(core::FindRace(core::LoadConfig(dir.Path()).config, "nord")->male == 150);
}

// The menu edits the layers, not the validated configuration: a race record that is also a beast form is dropped
// from the race only after validation, and a save must not write that drop into config.user.toml.
TEST_CASE("the menu saves the beast form without dropping the race record", "[user_edit]")
{
    TempDir dir;
    WriteDefaults(dir);
    dir.Write("config.user.toml", "[beast]\nforms = [\"Skyrim.esm|0x0CDD84\", \"Dawnguard.esm|0x00283A\", \"Skyrim.esm|0x013746\"]\n");
    const auto base   = LoadBase(dir);
    const auto loaded = core::LoadConfig(dir.Path());
    const FormKey normal{ "Skyrim.esm", 0x013746 };

    CHECK_FALSE(core::IsInList(core::FindRace(loaded.config, "nord")->forms, normal));
    REQUIRE(core::FindRace(loaded.layered, "nord"));
    CHECK(core::IsInList(core::FindRace(loaded.layered, "nord")->forms, normal));

    const auto text = core::ToUserToml(base, loaded.layered);
    CHECK(text.find("[races.nord]") == std::string::npos);
    CHECK(text.find("0x013746") != std::string::npos);
}
