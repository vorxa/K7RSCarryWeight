#pragma once

#include <string_view>

// The plugin's contribution to the player's carry weight in the game (specification, sections 3, 7–9): applied
// to the permanent CarryWeight modifier, remembered in the SKSE co-save, recounted when the race or sex may have
// changed.
namespace k7::rscw::carry
{
    // At plugin load: the co-save callbacks must be set before the first save is loaded.
    void RegisterSerialization();

    // At kDataLoaded: the character creation menu and the race switch event of the player.
    void RegisterEvents();

    // A new game has no contribution of the plugin yet; the first recount comes when the character creation
    // menu closes.
    void OnNewGame();

    // The Survival Mode penalty the last recount counted with (specification, section 3a).
    float LastGamePenalty() noexcept;

    // What the plugin has in the permanent CarryWeight modifier of the current game (for the menu).
    float Applied() noexcept;

    // Recounts now. Must run on the game's main thread (SKSE messages do; events queue a task).
    void Recount(std::string_view reason);
}
