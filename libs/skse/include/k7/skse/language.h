#pragma once

#include <string>

namespace k7::skse
{
    // The language of the game, lowered ("russian", "english"): sLanguage:General of Skyrim.ini, the name of the
    // localization table to load (k7::common::lang). English if the setting is not there. Call after kDataLoaded.
    std::string GameLanguage();
}
