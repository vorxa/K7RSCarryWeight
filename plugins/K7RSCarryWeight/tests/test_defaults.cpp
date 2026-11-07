#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

#include "core/defaults.h"

using namespace k7::rscw::core;
using k7::common::FormKey;

TEST_CASE("the built-in values are those of the specification, section 4", "[defaults]")
{
    const auto config = MakeDefaultConfig();
    struct Expected
    {
        const char* id;
        int         male;
        int         female;
    };
    const Expected table[] = {
        { "orc", 130, 115 }, { "nord", 115, 100 }, { "redguard", 100, 90 }, { "argonian", 100, 85 },
        { "imperial", 100, 90 }, { "dunmer", 95, 80 }, { "breton", 90, 80 }, { "altmer", 85, 75 },
        { "bosmer", 80, 70 }, { "khajiit", 80, 70 },
    };
    REQUIRE(config.races.size() == std::size(table));
    for (const auto& row : table) {
        INFO(row.id);
        const auto* race = FindRace(config, row.id);
        REQUIRE(race);
        CHECK(race->male == row.male);
        CHECK(race->female == row.female);
    }
}

TEST_CASE("every race has its ordinary and vampire record, and no record is used twice", "[defaults]")
{
    const auto config = MakeDefaultConfig();
    std::set<std::uint32_t> ids;
    for (const auto& race : config.races) {
        INFO(race.id);
        REQUIRE(race.forms.size() == 2);
        for (const auto& form : race.forms) {
            CHECK(form.plugin == "Skyrim.esm");
            CHECK(ids.insert(form.id).second);
        }
    }
    CHECK(ids.size() == 20);
    // Seen in game on stand 16 (specification, section 7, item 7).
    CHECK(FindRace(config, "nord")->forms == std::vector<FormKey>{ { "Skyrim.esm", 0x013746 }, { "Skyrim.esm", 0x088794 } });
    CHECK(FindRace(config, "imperial")->forms.front() == FormKey{ "Skyrim.esm", 0x013744 });
    CHECK(FindRace(config, "breton")->forms.front() == FormKey{ "Skyrim.esm", 0x013741 });
}

TEST_CASE("the beast forms are the werewolf and the vampire lord", "[defaults]")
{
    const auto config = MakeDefaultConfig();
    CHECK(config.beastForms == std::vector<FormKey>{ { "Skyrim.esm", 0x0CDD84 }, { "Dawnguard.esm", 0x00283A } });
}

TEST_CASE("the general settings default as the specification says", "[defaults]")
{
    const auto config = MakeDefaultConfig();
    CHECK(config.configVersion == kCurrentConfigVersion);
    CHECK(config.general.enabled);
    CHECK(config.general.minTotal == 20);
    CHECK(config.general.logLevel == k7::common::LogLevel::Info);
    CHECK(config.general.survivalPenalty == 0);
    CHECK(FindRace(config, "no_such_race") == nullptr);
}

TEST_CASE("the Survival Mode penalty effect is built in (specification, section 3a)", "[defaults]")
{
    const auto config = MakeDefaultConfig();
    CHECK(config.survivalEffects == std::vector<FormKey>{ { "ccqdrsse001-survivalmode.esl", 0x00084C } });
}
