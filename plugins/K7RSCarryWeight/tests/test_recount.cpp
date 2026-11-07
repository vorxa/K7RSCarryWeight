#include <catch2/catch_test_macros.hpp>

#include "core/defaults.h"
#include "core/recount.h"

using namespace k7::rscw::core;

namespace
{
    RecountInput For(const Config& config, const char* race, Sex sex, float applied = 0.0f)
    {
        RecountInput input;
        input.race     = FindRace(config, race);
        input.sex      = sex;
        input.raceBase = 300.0f;
        input.applied  = applied;
        input.enabled  = config.general.enabled;
        input.minTotal = config.general.minTotal;
        return input;
    }
}

TEST_CASE("every race and sex of the defaults ends at its value for a new character (T1)", "[recount]")
{
    const auto config = MakeDefaultConfig();
    for (const auto& race : config.races) {
        for (const auto sex : { Sex::Male, Sex::Female }) {
            INFO(race.id << (sex == Sex::Male ? " male" : " female"));
            const auto result = Recount(For(config, race.id.c_str(), sex));
            const int  value  = sex == Sex::Male ? race.male : race.female;
            REQUIRE(result.kind == RecountKind::Apply);
            CHECK(result.value == value);
            // base 300 + contribution = the configured value
            CHECK(300.0f + result.contribution == static_cast<float>(value));
            CHECK(result.delta == result.contribution);
        }
    }
}

TEST_CASE("a value below min_total gives min_total (T5)", "[recount]")
{
    auto config = MakeDefaultConfig();
    FindRace(config, "bosmer")->female = 10;
    config.general.minTotal = 50;
    const auto result = Recount(For(config, "bosmer", Sex::Female));
    CHECK(result.value == 50);
    CHECK(result.contribution == -250.0f);
}

TEST_CASE("a recount without a change is idempotent (T8, T9)", "[recount]")
{
    const auto config = MakeDefaultConfig();
    const auto first  = Recount(For(config, "nord", Sex::Male));
    const auto second = Recount(For(config, "nord", Sex::Male, first.contribution));
    CHECK(second.delta == 0.0f);
    CHECK(second.contribution == first.contribution);
}

TEST_CASE("a change of race, sex or value applies only the difference (T4, T6, T7)", "[recount]")
{
    auto       config  = MakeDefaultConfig();
    const auto nord    = Recount(For(config, "nord", Sex::Male));                       // 115: -185
    const auto orc     = Recount(For(config, "orc", Sex::Male, nord.contribution));     // 130: -170
    CHECK(orc.delta == 15.0f);
    const auto woman = Recount(For(config, "orc", Sex::Female, orc.contribution));      // 115: -185
    CHECK(woman.delta == -15.0f);
    FindRace(config, "orc")->female = 120;
    const auto edited = Recount(For(config, "orc", Sex::Female, woman.contribution));   // 120: -180
    CHECK(edited.delta == 5.0f);
}

TEST_CASE("the base of the race is the starting point, also when a mod changed it", "[recount]")
{
    const auto config = MakeDefaultConfig();
    auto       input  = For(config, "nord", Sex::Male);
    input.raceBase    = 250.0f;
    const auto result = Recount(input);
    CHECK(result.contribution == -135.0f);  // 250 - 135 = 115
}

TEST_CASE("a race that is not configured contributes nothing (T14)", "[recount]")
{
    const auto config = MakeDefaultConfig();
    auto       input  = For(config, "no_such_race", Sex::Male, -185.0f);
    REQUIRE(input.race == nullptr);
    const auto result = Recount(input);
    CHECK(result.kind == RecountKind::Apply);
    CHECK_FALSE(result.value);
    CHECK(result.contribution == 0.0f);
    CHECK(result.delta == 185.0f);  // what a configured race applied before is taken back
}

TEST_CASE("a beast form is skipped and keeps the contribution (T11)", "[recount]")
{
    const auto config = MakeDefaultConfig();
    RecountInput input;
    input.beast    = true;
    input.raceBase = 200.0f;
    input.applied  = -185.0f;
    const auto result = Recount(input);
    CHECK(result.kind == RecountKind::SkipBeast);
    CHECK(result.contribution == -185.0f);
    CHECK(result.delta == 0.0f);
}

TEST_CASE("enabled = false takes the contribution back, enabling again applies it once (T22, T23)", "[recount]")
{
    auto       config  = MakeDefaultConfig();
    const auto applied = Recount(For(config, "nord", Sex::Male)).contribution;

    config.general.enabled = false;
    const auto off = Recount(For(config, "nord", Sex::Male, applied));
    CHECK(off.contribution == 0.0f);
    CHECK(off.delta == -applied);
    CHECK(Recount(For(config, "nord", Sex::Male, off.contribution)).delta == 0.0f);

    config.general.enabled = true;
    const auto on = Recount(For(config, "nord", Sex::Male, off.contribution));
    CHECK(on.delta == applied);
    CHECK(Recount(For(config, "nord", Sex::Male, on.contribution)).delta == 0.0f);
}

TEST_CASE("an unknown sex changes nothing", "[recount]")
{
    const auto config = MakeDefaultConfig();
    const auto result = Recount(For(config, "nord", Sex::Unknown, -185.0f));
    CHECK(result.kind == RecountKind::SkipUnknown);
    CHECK(result.contribution == -185.0f);
    CHECK(result.delta == 0.0f);
}

TEST_CASE("a Survival Mode penalty is cancelled and survival_penalty applied instead (specification, section 3a)", "[recount][survival]")
{
    auto config = MakeDefaultConfig();
    auto input  = For(config, "nord", Sex::Male);  // value 115, base 300

    SECTION("survival_penalty 0: the carry weight stays at the value")
    {
        input.gamePenalty = 100.0f;
        const auto result = Recount(input);
        CHECK(result.value == 115);
        CHECK(result.contribution == -85.0f);  // -185 + 100
        // base 300 + contribution - the game's penalty = the value
        CHECK(300.0f + result.contribution - 100.0f == 115.0f);
    }
    SECTION("survival_penalty 30: the value minus 30")
    {
        input.gamePenalty     = 150.0f;
        input.survivalPenalty = 30;
        const auto result     = Recount(input);
        CHECK(result.value == 85);
        CHECK(300.0f + result.contribution - 150.0f == 85.0f);
    }
    SECTION("not below min_total")
    {
        input.gamePenalty     = 100.0f;
        input.survivalPenalty = 200;
        const auto result     = Recount(input);
        CHECK(result.value == 20);
        CHECK(300.0f + result.contribution - 100.0f == 20.0f);
    }
    SECTION("survival_penalty does nothing without the game's penalty")
    {
        input.survivalPenalty = 30;
        const auto result     = Recount(input);
        CHECK(result.value == 115);
        CHECK(result.contribution == -185.0f);
    }
}

TEST_CASE("the penalty coming and going changes only the difference", "[recount][survival]")
{
    auto config = MakeDefaultConfig();
    auto input  = For(config, "nord", Sex::Male, -185.0f);
    input.gamePenalty = 100.0f;
    const auto on     = Recount(input);
    CHECK(on.delta == 100.0f);

    input.applied     = on.contribution;
    const auto again  = Recount(input);
    CHECK(again.delta == 0.0f);

    input.gamePenalty = 0.0f;
    const auto off    = Recount(input);
    CHECK(off.delta == -100.0f);
    CHECK(off.contribution == -185.0f);
}

TEST_CASE("with a Survival Mode penalty, a race that is not configured and enabled = false leave the penalty alone", "[recount][survival]")
{
    auto config = MakeDefaultConfig();
    auto input  = For(config, "no_such_race", Sex::Male, 0.0f);
    input.gamePenalty     = 100.0f;
    input.survivalPenalty = 30;
    CHECK(Recount(input).contribution == 0.0f);

    input             = For(config, "nord", Sex::Male, -85.0f);
    input.gamePenalty = 100.0f;
    input.enabled     = false;
    const auto off    = Recount(input);
    CHECK(off.contribution == 0.0f);
    CHECK(off.delta == 85.0f);

    input.enabled = true;
    input.beast   = true;
    CHECK(Recount(input).kind == RecountKind::SkipBeast);
}
