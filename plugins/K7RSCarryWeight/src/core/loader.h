#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/apply.h"
#include "core/settings.h"
#include "k7/common/diagnostics.h"
#include "k7/common/migrate.h"

namespace k7::rscw::core
{
    inline constexpr std::string_view kConfigFile     = "config.toml";
    inline constexpr std::string_view kUserConfigFile = "config.user.toml";

    // The schema migrations of this plugin: none while the schema is at version 1.
    std::span<const common::Migration> BuiltInMigrations() noexcept;

    struct LoadResult
    {
        Config                   config;        // built-in defaults with every readable file applied, validated
        Config                   layered;       // the same merged before validation: what the menu edits, so a save never
                                                // writes what validation left out of a race
        common::Diagnostics      diagnostics;   // problems found, in the order they were met
        Sources                  sources;       // file and line of the race records the files set
        std::vector<std::string> appliedFiles;  // files that were read, in order
    };

    struct LoadOptions
    {
        std::span<const common::Migration> migrations     = BuiltInMigrations();
        int                                currentVersion = kCurrentConfigVersion;
        bool                               userLayer      = true;  // false: built-in values and config.toml only
    };

    // Writes `dir`/config.toml from the built-in defaults if it is missing, does not parse, or was written by
    // another plugin version. Returns true if it wrote the file; a failure to write is a warning.
    bool EnsureConfigToml(const std::filesystem::path& dir, std::string_view pluginVersion, common::Diagnostics& diagnostics);

    // Reads config.toml and config.user.toml from `dir` (specification, section 6) through the shared layers
    // (common::config::LoadLayers): each file is migrated first, a broken or unreadable one is skipped with an
    // error, config.user.toml wins. The merged result is validated and always usable.
    LoadResult LoadConfig(const std::filesystem::path& dir, const LoadOptions& options = {});
}
