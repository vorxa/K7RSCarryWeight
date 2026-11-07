#pragma once

#include <string>
#include <string_view>

#include "core/settings.h"

namespace k7::rscw::core
{
    // config.toml: the configuration as text with a comment for every key, stamped with the plugin version
    // ([meta] plugin_version). Reading it back gives the same configuration.
    std::string ToConfigToml(const Config& config, std::string_view pluginVersion);

    // Pieces of TOML shared with config.user.toml written by the menu.
    std::string QuoteToml(std::string_view text);                         // "text" with \ and " escaped
    std::string FormsToToml(const std::vector<common::FormKey>& forms);   // ["Plugin.esp|0x000800", ...]
    std::string TomlKey(std::string_view key);                            // a bare key if possible, else quoted
}
