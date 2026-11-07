#pragma once

#include <optional>

// The calculation model of the specification, section 3, without game types.
namespace k7::rscw::core
{
    // The configured value of a race and sex, not below min_total.
    int ClampValue(int value, int minTotal) noexcept;

    // What the plugin adds to the player's carry weight: the clamped value minus the base carry weight of the
    // player's race (TESRace::data.baseCarryWeight; 300 for every vanilla race). A race that is not in the
    // configuration (no value) contributes nothing.
    float Contribution(std::optional<int> value, int minTotal, float raceBase) noexcept;

    // The change to apply to the permanent CarryWeight modifier so that the applied contribution becomes
    // `wanted`. Zero when nothing changed, so a recount is idempotent.
    float Delta(float applied, float wanted) noexcept;
}
