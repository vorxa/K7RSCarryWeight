#pragma once

#include <string>
#include <string_view>

#include "core/apply.h"
#include "core/settings.h"

// Race records of the configuration resolved to the game's races (specification, section 5): by FormKey only.
namespace k7::rscw::races
{
    // Resolves every race record and beast form of `config` against the loaded plugins and logs a summary
    // (specification, section 10). A record that is not found is a warning with the file and line it came
    // from; a record of Dawnguard.esm while Dawnguard is not loaded only at debug level. Call after
    // kDataLoaded, and again whenever the configuration is read.
    void Resolve(const core::Config& config, const core::Sources& sources, std::string_view reason);

    // The race of the configuration a race record belongs to, or null (a race that is not configured).
    const core::RaceEntry* Find(const RE::TESRace* race) noexcept;

    bool IsBeast(const RE::TESRace* race) noexcept;

    // "nord", "beast form", "not in configuration": for the log.
    std::string Describe(const RE::TESRace* race);
}
