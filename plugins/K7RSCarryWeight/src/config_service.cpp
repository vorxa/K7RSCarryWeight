#include "pch.h"

#include "config_service.h"

#include "core/defaults.h"
#include "core/loader.h"
#include "k7/common/diagnostics.h"
#include "k7/skse/logging.h"

namespace k7::rscw::config
{
    namespace
    {
        // Relative to the game directory, like every SKSE plugin's data (a mod manager maps it into place).
        const std::filesystem::path kConfigDir = "Data/SKSE/Plugins/K7RSCarryWeight";

        core::Config  g_config = core::MakeDefaultConfig();
        core::Sources g_sources;
        core::Config  g_base = core::MakeDefaultConfig();
        core::Config  g_layered = core::MakeDefaultConfig();
        common::Diagnostics      g_diagnostics;
        std::vector<std::string> g_files;
    }

    void Load(std::string_view a_reason)
    {
        try {
            common::Diagnostics ensureDiagnostics;
            if (core::EnsureConfigToml(kConfigDir, K7_PLUGIN_VERSION, ensureDiagnostics)) {
                spdlog::info("config: wrote {}/config.toml for plugin version {}", kConfigDir.generic_string(), K7_PLUGIN_VERSION);
            }
            for (const auto& diagnostic : ensureDiagnostics) {
                skse::logging::LogDiagnostic(diagnostic);
            }

            auto result = core::LoadConfig(kConfigDir);
            for (const auto& diagnostic : result.diagnostics) {
                skse::logging::LogDiagnostic(diagnostic);
            }
            g_config  = std::move(result.config);
            g_layered = std::move(result.layered);
            g_sources = std::move(result.sources);
            g_diagnostics = ensureDiagnostics;
            g_diagnostics.insert(g_diagnostics.end(), result.diagnostics.begin(), result.diagnostics.end());
            g_files = result.appliedFiles;
            core::LoadOptions baseOptions;
            baseOptions.userLayer = false;
            g_base = core::LoadConfig(kConfigDir, baseOptions).config;
            // The log starts at info so that the errors above are not lost; now the player's level applies.
            skse::logging::SetLevel(g_config.general.logLevel);

            std::string files;
            for (const auto& file : result.appliedFiles) {
                files += (files.empty() ? "" : ", ") + file;
            }
            spdlog::info("config ({}): loaded {} file(s): {}; config_version {}, enabled {}, min_total {}, {} races, {} beast forms, log level {}",
                a_reason, result.appliedFiles.size(), files.empty() ? "none"s : files, g_config.configVersion,
                g_config.general.enabled, g_config.general.minTotal, g_config.races.size(), g_config.beastForms.size(),
                common::ToString(g_config.general.logLevel));
            for (const auto& race : g_config.races) {
                spdlog::debug("config: race {} male {} female {}, {} record(s)", race.id, race.male, race.female, race.forms.size());
            }
        } catch (const std::exception& error) {
            // Nothing in the loader is meant to throw; this keeps the game alive if something does.
            g_config  = core::MakeDefaultConfig();
            g_sources = {};
            g_base    = core::MakeDefaultConfig();
            g_layered = core::MakeDefaultConfig();
            spdlog::error("config: unexpected failure, built-in values are used: {}", error.what());
        }
    }

    const core::Config& Get() noexcept
    {
        return g_config;
    }

    const core::Sources& GetSources() noexcept
    {
        return g_sources;
    }

    const core::Config& GetLayered() noexcept
    {
        return g_layered;
    }

    const core::Config& GetBase() noexcept
    {
        return g_base;
    }

    const common::Diagnostics& GetDiagnostics() noexcept
    {
        return g_diagnostics;
    }

    const std::vector<std::string>& GetFiles() noexcept
    {
        return g_files;
    }

    const std::filesystem::path& Dir() noexcept
    {
        return kConfigDir;
    }
}
