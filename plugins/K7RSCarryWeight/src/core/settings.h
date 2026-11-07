#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "k7/common/form_key.h"
#include "k7/common/log_level.h"

// The configuration model of the specification, section 6, without game types.
namespace k7::rscw::core
{
    // Schema version of config.toml / config.user.toml ([meta] config_version). Grows only when the schema
    // changes incompatibly, together with a migration.
    inline constexpr int kCurrentConfigVersion = 1;

    // Allowed ranges (specification, section 6).
    inline constexpr int kMinTotalLow  = 1;
    inline constexpr int kMinTotalHigh = 300;
    inline constexpr int kValueLow     = 1;
    inline constexpr int kValueHigh    = 1000;
    inline constexpr int kSurvivalPenaltyLow  = 0;
    inline constexpr int kSurvivalPenaltyHigh = 1000;

    struct General
    {
        bool             enabled{ true };
        int              minTotal{ 20 };
        common::LogLevel logLevel{ common::LogLevel::Info };
        int              survivalPenalty{ 0 };  // own penalty instead of the Survival Mode one (section 3a)

        bool operator==(const General&) const = default;
    };

    // One race of the configuration, [races.<id>]: the race records that count as this race (a race and its
    // vampire version, or the race of a mod) and the base carry weight of a man and of a woman.
    struct RaceEntry
    {
        std::string                  id;
        std::vector<common::FormKey> forms;
        int                          male{};
        int                          female{};

        bool operator==(const RaceEntry&) const = default;
    };

    struct Config
    {
        int                          configVersion{ kCurrentConfigVersion };
        General                      general;
        std::vector<RaceEntry>       races;       // in the order of the built-in defaults, new ones appended
        std::vector<common::FormKey> beastForms;  // [beast] forms: no recount while the player has one of these
        std::vector<common::FormKey> survivalEffects;  // [survival] effects: magic effects of the Survival Mode penalty

        bool operator==(const Config&) const = default;
    };

    // The race with this id, or null. Ids are compared as written: "nord", "my_mod_race".
    const RaceEntry* FindRace(const Config& config, std::string_view id) noexcept;
    RaceEntry*       FindRace(Config& config, std::string_view id) noexcept;
}
