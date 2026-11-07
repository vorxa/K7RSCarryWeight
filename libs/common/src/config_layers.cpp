#include "k7/common/config_layers.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <sstream>
#include <system_error>

#include "k7/common/file_io.h"

namespace k7::common::config
{
    namespace fs = std::filesystem;

    namespace
    {
        std::string LowerAscii(std::string text)
        {
            for (auto& c : text) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return text;
        }

        // File name as shown in diagnostics and logs, with forward slashes.
        std::string Display(std::string_view subdir, const fs::path& file)
        {
            const auto name = file.filename().string();
            return subdir.empty() ? name : std::format("{}/{}", subdir, name);
        }

        struct Layer
        {
            std::string display;
            fs::path    path;
        };

        struct LoadedLayer
        {
            toml::table table;
            std::string original;  // the text read from disk, kept to back it up when the file is migrated
            bool        migrated{ false };
            int         fromVersion{ 1 };
        };

        // Reads, parses and migrates one layer. Failures are reported; a skipped file gives nothing.
        std::optional<LoadedLayer> ReadLayer(const Layer& layer, const LayerOptions& options, Diagnostics& diagnostics)
        {
            auto text = io::ReadFile(layer.path);
            if (!text) {
                diagnostics.push_back(Diagnostic{ Severity::Error, layer.display, 0, "cannot read the file, skipped" });
                return std::nullopt;
            }
            auto table = Parse(*text, layer.display, diagnostics);
            if (!table) {
                return std::nullopt;
            }
            const auto migration = MigrateLayer(*table, layer.display, options.migrations, options.currentVersion, diagnostics);
            if (!migration.usable) {
                return std::nullopt;
            }
            return LoadedLayer{ std::move(*table), std::move(*text), migration.changed, migration.fromVersion };
        }

        // config.user.toml is the only file the plugin owns, so only it is rewritten after a migration;
        // the old text is kept next to it. Other files belong to the shipment or to other mods, and under a
        // mod manager a write to them would land in the wrong place, so they are migrated in memory only.
        void PersistMigration(const Layer& layer, const LoadedLayer& loaded, bool isUserFile, int currentVersion, Diagnostics& diagnostics)
        {
            const auto what = std::format("config_version {} -> {}", loaded.fromVersion, currentVersion);
            if (!isUserFile) {
                diagnostics.push_back(Diagnostic{ Severity::Warning, layer.display, 0,
                    std::format("migrated in memory ({}), the file on disk is not changed", what) });
                return;
            }

            auto backup = layer.path;
            backup += ".bak";
            std::ostringstream text;
            text << loaded.table;
            if (io::WriteFile(backup, loaded.original) && io::WriteFile(layer.path, text.str())) {
                diagnostics.push_back(Diagnostic{ Severity::Warning, layer.display, 0,
                    std::format("migrated ({}), the previous version is saved as {}.bak", what, layer.display) });
            } else {
                diagnostics.push_back(Diagnostic{ Severity::Warning, layer.display, 0,
                    std::format("migrated in memory ({}), but the file could not be rewritten", what) });
            }
        }

        std::vector<Layer> OverrideLayers(const fs::path& dir, Diagnostics& diagnostics)
        {
            std::vector<Layer> layers;
            std::error_code    ec;
            const auto         overridesPath = dir / kOverridesDir;
            if (!fs::is_directory(overridesPath, ec)) {
                return layers;
            }
            for (fs::directory_iterator it(overridesPath, ec), end; !ec && it != end; it.increment(ec)) {
                if (!it->is_regular_file(ec)) {
                    continue;
                }
                const auto& path = it->path();
                if (LowerAscii(path.extension().string()) != ".toml") {
                    continue;
                }
                layers.push_back(Layer{ Display(kOverridesDir, path), path });
            }
            if (ec) {
                diagnostics.push_back(Diagnostic{ Severity::Warning, std::string(kOverridesDir), 0,
                    std::format("cannot list the directory: {}", ec.message()) });
            }
            // "By alphabet" without regard to case, so the order does not depend on how a file is spelled.
            std::ranges::sort(layers, [](const Layer& a, const Layer& b) {
                const auto la = LowerAscii(a.display);
                const auto lb = LowerAscii(b.display);
                return la != lb ? la < lb : a.display < b.display;
            });
            return layers;
        }
    }

    bool IsValidPresetName(std::string_view name) noexcept
    {
        return !name.empty() && name.find_first_of("/\\:") == std::string_view::npos && name.find("..") == std::string_view::npos;
    }

    std::optional<toml::table> Parse(const std::string& text, const std::string& display, Diagnostics& diagnostics)
    {
        try {
            return toml::parse(text, display);
        } catch (const toml::parse_error& error) {
            diagnostics.push_back(Diagnostic{ Severity::Error, display, static_cast<int>(error.source().begin.line),
                std::format("syntax error: {}", error.description()) });
        } catch (const std::exception& error) {
            diagnostics.push_back(Diagnostic{ Severity::Error, display, 0, std::format("cannot parse: {}", error.what()) });
        }
        return std::nullopt;
    }

    std::vector<std::string> LoadLayers(const fs::path& dir, const LayerOptions& options, const ApplyLayerFn& apply,
        Diagnostics& diagnostics)
    {
        std::vector<std::string> applied;

        // The user file is read first only to learn the preset name; it is applied last.
        const Layer                userLayer{ std::string(options.userFile), dir / options.userFile };
        std::error_code            ec;
        std::optional<LoadedLayer> user;
        if (options.userLayer && fs::exists(userLayer.path, ec)) {
            user = ReadLayer(userLayer, options, diagnostics);
        }

        std::vector<Layer> layers;
        layers.push_back(Layer{ std::string(options.configFile), dir / options.configFile });
        if (options.overrides) {
            for (auto& layer : OverrideLayers(dir, diagnostics)) {
                layers.push_back(std::move(layer));
            }
        }

        if (user && options.presets) {
            std::string preset;
            if (const auto* meta = user->table["meta"].as_table()) {
                if (const auto* value = meta->get("preset"); value && value->is_string()) {
                    preset = value->as_string()->get();
                }
            }
            if (!preset.empty()) {
                if (!IsValidPresetName(preset)) {
                    diagnostics.push_back(Diagnostic{ Severity::Error, userLayer.display, 0,
                        std::format("meta.preset: '{}' is not a preset name, ignored", preset) });
                } else {
                    layers.push_back(Layer{ std::format("{}/{}.toml", kPresetsDir, preset), dir / kPresetsDir / (preset + ".toml") });
                }
            }
        }

        for (const auto& layer : layers) {
            if (!fs::exists(layer.path, ec)) {
                // A missing config.toml is an error; a missing preset too, the player chose it.
                diagnostics.push_back(Diagnostic{ Severity::Error, layer.display, 0, "file not found, skipped" });
                continue;
            }
            if (auto loaded = ReadLayer(layer, options, diagnostics)) {
                if (loaded->migrated) {
                    PersistMigration(layer, *loaded, false, options.currentVersion, diagnostics);
                }
                apply(loaded->table, layer.display, diagnostics);
                applied.push_back(layer.display);
            }
        }

        if (user) {
            if (user->migrated) {
                PersistMigration(userLayer, *user, true, options.currentVersion, diagnostics);
            }
            apply(user->table, userLayer.display, diagnostics);
            applied.push_back(userLayer.display);
        }
        return applied;
    }

    bool EnsureConfigFile(const fs::path& dir, std::string_view fileName, std::string_view pluginVersion,
        const std::function<std::string()>& makeText, Diagnostics& diagnostics)
    {
        const auto path = dir / fileName;

        // Up to date: parses, and carries our version stamp.
        if (const auto text = io::ReadFile(path)) {
            Diagnostics ignored;
            if (const auto table = Parse(*text, std::string(fileName), ignored)) {
                const auto* meta  = (*table)["meta"].as_table();
                const auto* stamp = meta ? meta->get("plugin_version") : nullptr;
                if (stamp && stamp->is_string() && stamp->as_string()->get() == pluginVersion) {
                    return false;
                }
            }
        }

        std::error_code ec;
        fs::create_directories(dir, ec);
        if (ec || !io::WriteFile(path, makeText())) {
            diagnostics.push_back(Diagnostic{ Severity::Warning, std::string(fileName), 0,
                std::format("cannot write the default {}, built-in defaults are used", fileName) });
            return false;
        }
        return true;
    }
}
