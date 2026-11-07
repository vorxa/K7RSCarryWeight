#pragma once

#include <string_view>

#include "core/apply.h"
#include "core/settings.h"

// The Survival Mode carry weight penalty on the player (specification, section 3a).
namespace k7::rscw::survival
{
    // Resolves [survival] effects to the game's magic effects and logs a summary. An effect whose plugin is not
    // loaded (Survival Mode not installed) is logged at debug level only. Call after kDataLoaded and again
    // whenever the configuration is read.
    void Resolve(const core::Config& config, const core::Sources& sources, std::string_view reason);

    // The penalty on the player now: the sum of the magnitudes of the player's active effects from the list,
    // as a positive number; 0 if there are none. Main thread only.
    float GamePenalty();

    // At kDataLoaded: once a second, while a game is loaded and at least one effect of the list is in the game,
    // compares the penalty with the one of the last recount and recounts when it changed (the game gives no
    // event for this ability and switches Survival on about a second after a load or a menu: section 7).
    void StartPolling();

    // A game is loaded or started: polling may run.
    void OnGameLoaded();
}
