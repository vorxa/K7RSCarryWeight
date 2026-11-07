#include "core/loader.h"

#include "core/defaults.h"
#include "core/serialize.h"
#include "core/validate.h"
#include "k7/common/config_layers.h"

namespace k7::rscw::core
{
    std::span<const common::Migration> BuiltInMigrations() noexcept
    {
        return {};
    }

    bool EnsureConfigToml(const std::filesystem::path& dir, std::string_view pluginVersion, common::Diagnostics& diagnostics)
    {
        return common::config::EnsureConfigFile(
            dir, kConfigFile, pluginVersion, [&] { return ToConfigToml(MakeDefaultConfig(), pluginVersion); }, diagnostics);
    }

    LoadResult LoadConfig(const std::filesystem::path& dir, const LoadOptions& options)
    {
        LoadResult result;
        result.config = MakeDefaultConfig();

        common::config::LayerOptions layers;
        layers.configFile     = kConfigFile;
        layers.userFile       = kUserConfigFile;
        layers.migrations     = options.migrations;
        layers.currentVersion = options.currentVersion;
        layers.userLayer      = options.userLayer;

        result.appliedFiles = common::config::LoadLayers(dir, layers,
            [&](const toml::table& layer, const std::string& display, common::Diagnostics& diagnostics) {
                ApplyLayer(result.config, layer, display, diagnostics, &result.sources);
            },
            result.diagnostics);

        result.layered = result.config;
        ValidateConfig(result.config, result.sources, result.diagnostics);
        return result;
    }
}
