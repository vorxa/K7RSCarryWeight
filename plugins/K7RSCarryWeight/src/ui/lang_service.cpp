#include "pch.h"

#include "ui/lang_service.h"

#include "k7/skse/language.h"
#include "k7/skse/logging.h"

namespace k7::rscw::lang
{
    namespace
    {
        const std::filesystem::path kPluginDir = "Data/SKSE/Plugins/K7RSCarryWeight";

        common::lang::Localization g_localization;
    }

    void Load()
    {
        try {
            common::Diagnostics diagnostics;
            const auto          language = k7::skse::GameLanguage();
            g_localization.Load(kPluginDir, language, diagnostics);
            for (const auto& diagnostic : diagnostics) {
                k7::skse::logging::LogDiagnostic(diagnostic);
            }
            spdlog::info("lang: language of the game {}, {} keys in {}.toml, {} in {}.toml", language, g_localization.LanguageKeys(),
                g_localization.Language(), g_localization.FallbackKeys(), common::lang::kFallbackLanguage);
        } catch (const std::exception& error) {
            spdlog::error("lang: unexpected failure while reading the tables: {}", error.what());
        }
    }

    const common::lang::Localization& Get() noexcept
    {
        return g_localization;
    }
}
