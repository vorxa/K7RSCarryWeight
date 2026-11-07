#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <toml++/toml.hpp>

#include "k7/common/diagnostics.h"
#include "k7/common/migrate.h"

// The layers of a plugin's configuration, without the plugin's schema. Every K7 plugin keeps its settings the
// same way: config.toml written by the plugin from its built-in defaults, the player's config.user.toml on top,
// and optionally patches in overrides/ and a preset chosen in config.user.toml. Each file is migrated to the
// current schema version first; a broken or unreadable file is skipped with an error, the rest still applies.
// The plugin applies each layer to its own model through a callback and validates the result itself.
namespace k7::common::config
{
    inline constexpr std::string_view kOverridesDir = "overrides";
    inline constexpr std::string_view kPresetsDir   = "presets";

    struct LayerOptions
    {
        std::string_view           configFile     = "config.toml";
        std::string_view           userFile       = "config.user.toml";
        bool                       overrides      = false;  // read overrides/*.toml after config.toml
        bool                       presets        = false;  // read presets/<[meta] preset of the user file>.toml
        bool                       userLayer      = true;   // false: leave config.user.toml out (the base a settings menu compares with)
        std::span<const Migration> migrations     = {};
        int                        currentVersion = 1;
    };

    // Applies one parsed layer to the plugin's model. `display` names the file in diagnostics
    // ("config.toml", "overrides/a.toml").
    using ApplyLayerFn = std::function<void(const toml::table& layer, const std::string& display, Diagnostics& diagnostics)>;

    // A preset name is a plain file name: no path separators, no "..", not empty.
    bool IsValidPresetName(std::string_view name) noexcept;

    // Parses text into a table. A syntax error becomes a diagnostic with its line and an empty result.
    std::optional<toml::table> Parse(const std::string& text, const std::string& display, Diagnostics& diagnostics);

    // Reads the layers from `dir` and passes each to `apply`, in this order: config.toml, overrides/*.toml
    // alphabetically without regard to case, the preset, config.user.toml. The user file is read first, to
    // learn the preset, but applied last. A missing config.toml or preset is an error, a missing user file
    // is not. Only config.user.toml is rewritten after a migration, with the old text kept as
    // config.user.toml.bak; other files belong to the shipment or to other mods and are migrated in memory.
    // Returns the names of the layers that were applied, in order. Nothing throws.
    std::vector<std::string> LoadLayers(const std::filesystem::path& dir, const LayerOptions& options,
        const ApplyLayerFn& apply, Diagnostics& diagnostics);

    // Makes sure `dir`/`fileName` exists and belongs to this plugin version: writes `makeText()` if the file
    // is missing, does not parse, or carries another `[meta] plugin_version` (a stale copy would otherwise
    // shadow the new defaults). Returns true if it wrote the file. A failure to write is a warning; the
    // plugin still runs on its built-in defaults.
    bool EnsureConfigFile(const std::filesystem::path& dir, std::string_view fileName, std::string_view pluginVersion,
        const std::function<std::string()>& makeText, Diagnostics& diagnostics);
}
