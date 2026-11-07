#include "core/defaults.h"

#include <algorithm>
#include <cstdint>

namespace k7::rscw::core
{
    namespace
    {
        common::FormKey Skyrim(std::uint32_t id)
        {
            return common::FormKey{ "Skyrim.esm", id };
        }

        // Form ids from the RACE records of Skyrim.esm and Dawnguard.esm (EditorID in the comment).
        RaceEntry Race(const char* id, std::uint32_t race, std::uint32_t vampire, int male, int female)
        {
            return RaceEntry{ id, { Skyrim(race), Skyrim(vampire) }, male, female };
        }
    }

    Config MakeDefaultConfig()
    {
        Config config;
        // Values: specification, section 4.
        config.races = {
            Race("orc", 0x013747, 0x0A82B9, 130, 115),       // OrcRace, OrcRaceVampire
            Race("nord", 0x013746, 0x088794, 115, 100),      // NordRace, NordRaceVampire
            Race("redguard", 0x013748, 0x088846, 100, 90),   // RedguardRace, RedguardRaceVampire
            Race("argonian", 0x013740, 0x08883A, 100, 85),   // ArgonianRace, ArgonianRaceVampire
            Race("imperial", 0x013744, 0x088844, 100, 90),   // ImperialRace, ImperialRaceVampire
            Race("dunmer", 0x013742, 0x08883D, 95, 80),      // DarkElfRace, DarkElfRaceVampire
            Race("breton", 0x013741, 0x08883C, 90, 80),      // BretonRace, BretonRaceVampire
            Race("altmer", 0x013743, 0x088840, 85, 75),      // HighElfRace, HighElfRaceVampire
            Race("bosmer", 0x013749, 0x088884, 80, 70),      // WoodElfRace, WoodElfRaceVampire
            Race("khajiit", 0x013745, 0x088845, 80, 70),     // KhajiitRace, KhajiitRaceVampire
        };
        config.beastForms = {
            Skyrim(0x0CDD84),                              // WerewolfBeastRace
            common::FormKey{ "Dawnguard.esm", 0x00283A },  // DLC1VampireBeastRace
        };
        // Specification, section 3a: the magic effect of Survival_abLowerCarryWeightSpell.
        config.survivalEffects = {
            common::FormKey{ "ccqdrsse001-survivalmode.esl", 0x00084C },  // Survival_AbLowerCarryWeight
        };
        return config;
    }

    const RaceEntry* FindRace(const Config& config, std::string_view id) noexcept
    {
        const auto it = std::ranges::find(config.races, id, &RaceEntry::id);
        return it == config.races.end() ? nullptr : &*it;
    }

    RaceEntry* FindRace(Config& config, std::string_view id) noexcept
    {
        const auto it = std::ranges::find(config.races, id, &RaceEntry::id);
        return it == config.races.end() ? nullptr : &*it;
    }
}
