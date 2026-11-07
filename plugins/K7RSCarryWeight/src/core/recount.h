#pragma once

#include <optional>

#include "core/settings.h"

// One recount of the player's carry weight (specification, sections 3, 5, 7 and 9), without game types: the
// engine collects the inputs, this decides, the engine applies the delta and remembers the new contribution.
namespace k7::rscw::core
{
    enum class Sex
    {
        Male,
        Female,
        Unknown,
    };

    struct RecountInput
    {
        const RaceEntry* race{};        // the configured race of the player's race record, null if none
        bool             beast{};       // the player's race is a beast form of the configuration
        Sex              sex{ Sex::Unknown };
        float            raceBase{};    // TESRace::data.baseCarryWeight of the player's race
        float            applied{};     // the contribution applied before (from the co-save)
        bool             enabled{ true };
        int              minTotal{ 20 };
        float            gamePenalty{};      // the Survival Mode penalty on the player now (positive, 0 if none)
        int              survivalPenalty{};  // general.survival_penalty
    };

    enum class RecountKind
    {
        Apply,        // apply `delta` to the permanent CarryWeight modifier (may be 0)
        SkipBeast,    // beast form: keep what was applied before the transformation
        SkipUnknown,  // the sex cannot be read: change nothing
    };

    struct RecountResult
    {
        RecountKind        kind{ RecountKind::Apply };
        std::optional<int> value;           // the configured value after min_total, if the race is configured
        float              contribution{};  // the contribution after this recount
        float              delta{};         // contribution - applied
    };

    // * beast form: skip, the contribution stays;
    // * enabled = false: the contribution becomes 0, so the plugin takes back what it applied (section 9);
    // * a race that is not configured: the contribution becomes 0;
    // * otherwise the value of the race and sex, not below min_total, minus the base of the race;
    // * while a Survival Mode penalty is on (section 3a): the value minus survival_penalty, not below
    //   min_total, minus the base of the race, plus the game's penalty (which the plugin cancels).
    RecountResult Recount(const RecountInput& input) noexcept;
}
