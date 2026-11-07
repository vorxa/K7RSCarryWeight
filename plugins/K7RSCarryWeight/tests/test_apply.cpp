#include <catch2/catch_test_macros.hpp>

#include "core/validate.h"
#include "test_helpers.h"

using namespace k7::rscw;
using namespace k7::rscw::test;
using k7::common::Diagnostics;
using k7::common::FormKey;
using k7::common::Severity;

TEST_CASE("one field of a race changes, the others stay (specification, section 6)", "[apply]")
{
    auto        config = core::MakeDefaultConfig();
    const auto  before = *core::FindRace(config, "nord");
    Diagnostics diagnostics;
    Apply(config, "[races.nord]\nmale = 120\n", diagnostics);

    const auto* nord = core::FindRace(config, "nord");
    CHECK(nord->male == 120);
    CHECK(nord->female == before.female);
    CHECK(nord->forms == before.forms);
    CHECK(diagnostics.empty());
}

TEST_CASE("a race of a mod is added with its own values", "[apply]")
{
    auto        config = core::MakeDefaultConfig();
    Diagnostics diagnostics;
    core::Sources sources;
    Apply(config, "[races.my_mod_race]\nforms = [\"MyRace.esp|0x000800\"]\nmale = 110\nfemale = 95\n", diagnostics, &sources);
    core::ValidateConfig(config, sources, diagnostics);

    const auto* race = core::FindRace(config, "my_mod_race");
    REQUIRE(race);
    CHECK(race->forms == std::vector<FormKey>{ { "MyRace.esp", 0x000800 } });
    CHECK(race->male == 110);
    CHECK(race->female == 95);
    CHECK(config.races.size() == 11);
    CHECK(diagnostics.empty());
    const auto* source = sources.Find("races.my_mod_race", race->forms.front());
    REQUIRE(source);
    CHECK(source->file == "config.user.toml");
    CHECK(source->line == 2);
}

TEST_CASE("forms given in a layer replace the race's list", "[apply]")
{
    auto        config = core::MakeDefaultConfig();
    Diagnostics diagnostics;
    Apply(config, "[races.nord]\nforms = [\"Skyrim.esm|0x013746\", \"MyNord.esp|0x000801\"]\n", diagnostics);
    CHECK(core::FindRace(config, "nord")->forms == std::vector<FormKey>{ { "Skyrim.esm", 0x013746 }, { "MyNord.esp", 0x000801 } });
    CHECK(diagnostics.empty());
}

TEST_CASE("general settings are read with their ranges", "[apply]")
{
    auto        config = core::MakeDefaultConfig();
    Diagnostics diagnostics;
    Apply(config, "[general]\nenabled = false\nmin_total = 50\nlog_level = \"debug\"\n", diagnostics);
    CHECK_FALSE(config.general.enabled);
    CHECK(config.general.minTotal == 50);
    CHECK(config.general.logLevel == k7::common::LogLevel::Debug);
    CHECK(diagnostics.empty());

    SECTION("boundaries are accepted")
    {
        Apply(config, "[general]\nmin_total = 1\n[races.orc]\nmale = 1000\nfemale = 1\n", diagnostics);
        CHECK(config.general.minTotal == 1);
        CHECK(core::FindRace(config, "orc")->male == 1000);
        CHECK(diagnostics.empty());
    }
}

TEST_CASE("a wrong value is skipped with the file, key and line; the rest applies (T16)", "[apply]")
{
    auto        config = core::MakeDefaultConfig();
    Diagnostics diagnostics;
    Apply(config,
        "[general]\n"
        "min_total = 301\n"         // 2: out of range
        "enabled = \"yes\"\n"       // 3: wrong type
        "[races.nord]\n"
        "male = 0\n"                // 5: out of range
        "female = 101\n"            // 6: fine
        "forms = [\"NoBar\", 5]\n"  // 7: both elements bad
        "colour = \"red\"\n",       // 8: unknown key
        diagnostics);

    CHECK(config.general.minTotal == 20);
    CHECK(config.general.enabled);
    const auto* nord = core::FindRace(config, "nord");
    CHECK(nord->male == 115);
    CHECK(nord->female == 101);
    CHECK(nord->forms.empty());  // an array was given: it replaces, without the bad elements
    CHECK(Count(diagnostics, Severity::Error) == 5);
    CHECK(Count(diagnostics, Severity::Warning) == 1);
    for (const auto& d : diagnostics) {
        CHECK(d.file == "config.user.toml");
        CHECK(d.line > 0);
    }
    CHECK(Mentions(diagnostics, "general.min_total: must be between 1 and 300"));
    CHECK(Mentions(diagnostics, "races.nord.male: must be between 1 and 1000"));
    CHECK(Mentions(diagnostics, "races.nord.colour"));
}

TEST_CASE("an unknown section and a race that is not a table are reported", "[apply]")
{
    auto        config = core::MakeDefaultConfig();
    Diagnostics diagnostics;
    Apply(config, "races.nord = 5\n[colours]\nred = 1\n", diagnostics);
    CHECK(core::FindRace(config, "nord")->male == 115);
    CHECK(Mentions(diagnostics, "expected a table [races.nord]"));
    CHECK(Mentions(diagnostics, "unknown key 'colours'"));
}

TEST_CASE("an incomplete new race is dropped after the merge", "[validate]")
{
    auto          config = core::MakeDefaultConfig();
    Diagnostics   diagnostics;
    core::Sources sources;
    Apply(config, "[races.half]\nmale = 100\n", diagnostics, &sources);
    core::ValidateConfig(config, sources, diagnostics);
    CHECK(core::FindRace(config, "half") == nullptr);
    CHECK(Mentions(diagnostics, "races.half: missing forms female"));

    SECTION("but a race completed by a later file is kept")
    {
        auto          again = core::MakeDefaultConfig();
        Diagnostics   d;
        core::Sources s;
        Apply(again, "[races.half]\nmale = 100\n", d, &s, "a.toml");
        Apply(again, "[races.half]\nfemale = 90\nforms = [\"Half.esp|0x000800\"]\n", d, &s, "b.toml");
        core::ValidateConfig(again, s, d);
        CHECK(core::FindRace(again, "half"));
        CHECK(d.empty());
    }
}

TEST_CASE("a record named by two races stays with the later one (decision 2026-11-06)", "[validate]")
{
    auto          config = core::MakeDefaultConfig();
    Diagnostics   diagnostics;
    core::Sources sources;

    SECTION("a mod race that takes the Nord record from the built-in nord")
    {
        Apply(config, "[races.big_nord]\nforms = [\"Skyrim.esm|0x013746\"]\nmale = 200\nfemale = 180\n", diagnostics, &sources);
        core::ValidateConfig(config, sources, diagnostics);
        CHECK(core::FindRace(config, "big_nord")->forms == std::vector<FormKey>{ { "Skyrim.esm", 0x013746 } });
        CHECK(core::FindRace(config, "nord")->forms == std::vector<FormKey>{ { "Skyrim.esm", 0x088794 } });
        REQUIRE(Count(diagnostics, Severity::Warning) == 1);
        CHECK(diagnostics.front().file == "config.user.toml");
        CHECK(Mentions(diagnostics, "races.big_nord, keeps it"));
    }
    SECTION("an earlier race in the list that names the record later wins too")
    {
        // orc comes before nord in the list, but its forms are set by the file, after the built-in nord.
        Apply(config, "[races.orc]\nforms = [\"Skyrim.esm|0x013747\", \"skyrim.esm|0x13746\"]\n", diagnostics, &sources);
        core::ValidateConfig(config, sources, diagnostics);
        CHECK(core::FindRace(config, "orc")->forms.size() == 2);
        CHECK(core::FindRace(config, "nord")->forms == std::vector<FormKey>{ { "Skyrim.esm", 0x088794 } });
    }
    SECTION("between two files the second one wins")
    {
        Apply(config, "[races.a]\nforms = [\"X.esp|0x000800\"]\nmale = 100\nfemale = 100\n", diagnostics, &sources, "a.toml");
        Apply(config, "[races.b]\nforms = [\"X.esp|0x000800\"]\nmale = 100\nfemale = 100\n", diagnostics, &sources, "b.toml");
        Apply(config, "[races.a]\nmale = 120\n", diagnostics, &sources, "c.toml");  // not the forms: does not count
        core::ValidateConfig(config, sources, diagnostics);
        CHECK(core::FindRace(config, "a")->forms.empty());
        CHECK(core::FindRace(config, "b")->forms.size() == 1);
    }
}

TEST_CASE("a race record that is also a beast form is a beast form only", "[validate]")
{
    auto          config = core::MakeDefaultConfig();
    Diagnostics   diagnostics;
    core::Sources sources;
    Apply(config, "[beast]\nforms = [\"Skyrim.esm|0x0CDD84\", \"Skyrim.esm|0x088794\"]\n", diagnostics, &sources);
    core::ValidateConfig(config, sources, diagnostics);
    CHECK(core::FindRace(config, "nord")->forms == std::vector<FormKey>{ { "Skyrim.esm", 0x013746 } });
    CHECK(config.beastForms.size() == 2);
    CHECK(Mentions(diagnostics, "is also a beast form"));
}

TEST_CASE("the built-in configuration passes validation untouched", "[validate]")
{
    auto          config = core::MakeDefaultConfig();
    Diagnostics   diagnostics;
    core::ValidateConfig(config, core::Sources{}, diagnostics);
    CHECK(config == core::MakeDefaultConfig());
    CHECK(diagnostics.empty());
}

TEST_CASE("survival_penalty is read with its range (specification, section 3a)", "[apply]")
{
    auto        config = core::MakeDefaultConfig();
    Diagnostics diagnostics;
    Apply(config, "[general]\nsurvival_penalty = 30\n", diagnostics);
    CHECK(config.general.survivalPenalty == 30);
    CHECK(diagnostics.empty());

    Apply(config, "[general]\nsurvival_penalty = 0\n", diagnostics);
    CHECK(config.general.survivalPenalty == 0);
    CHECK(diagnostics.empty());

    Apply(config, "[general]\nsurvival_penalty = -1\n", diagnostics);
    CHECK(config.general.survivalPenalty == 0);
    CHECK(Mentions(diagnostics, "survival_penalty: must be between 0 and 1000"));
}

TEST_CASE("[survival] effects in a layer replace the list", "[apply]")
{
    auto          config = core::MakeDefaultConfig();
    Diagnostics   diagnostics;
    core::Sources sources;
    Apply(config, "[survival]\neffects = [\"MySurvival.esp|0x000801\", \"ccqdrsse001-survivalmode.esl|0x00084C\"]\n", diagnostics,
        &sources);
    CHECK(config.survivalEffects ==
          std::vector<FormKey>{ { "MySurvival.esp", 0x000801 }, { "ccqdrsse001-survivalmode.esl", 0x00084C } });
    CHECK(sources.Find("survival", FormKey{ "MySurvival.esp", 0x000801 }) != nullptr);
    CHECK(diagnostics.empty());
}
