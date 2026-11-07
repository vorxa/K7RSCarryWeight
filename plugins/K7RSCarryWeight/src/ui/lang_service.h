#pragma once

#include "k7/common/lang.h"

// The localization tables of the plugin (specification, section 13.5; AGENTS.md, "Локализация"): read once
// at kDataLoaded from Data/SKSE/Plugins/K7RSCarryWeight/lang/, in the language of the game with English to
// fall back on. Read-only afterwards, so the menu can use them from the render thread.
namespace k7::rscw::lang
{
    // At kDataLoaded, on the main thread. Logs the language, the number of keys read and every problem.
    void Load();

    const common::lang::Localization& Get() noexcept;
}
