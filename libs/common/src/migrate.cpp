#include "k7/common/migrate.h"

#include <algorithm>
#include <cstdint>
#include <format>

namespace k7::common
{
    MigrationResult MigrateLayer(toml::table& layer, std::string_view file, std::span<const Migration> chain,
        int currentVersion, Diagnostics& diagnostics)
    {
        MigrationResult result;

        const auto fail = [&](int line, std::string message) {
            diagnostics.push_back(Diagnostic{ Severity::Error, std::string(file), line, std::move(message) });
            result.usable = false;
            return result;
        };

        const auto* meta = layer["meta"].as_table();
        const auto* node = meta ? meta->get("config_version") : nullptr;
        // No version: the file is version 1 and goes through the chain like an explicit 1. A player's
        // config.user.toml rarely carries the field, and its keys are exactly what a migration has to move.
        std::int64_t version = 1;
        int          line    = 0;
        if (node) {
            line = static_cast<int>(node->source().begin.line);
            const auto* value = node->as_integer();
            if (!value) {
                return fail(line, "meta.config_version: expected a whole number, file skipped");
            }
            version = value->get();
        }
        if (version < 1) {
            return fail(line, std::format("meta.config_version: {} is not a valid version, file skipped", version));
        }
        if (version > currentVersion) {
            return fail(line, std::format("file has config_version {} but this plugin understands up to {}: "
                                          "update the plugin; the file is skipped",
                                  version, currentVersion));
        }

        result.fromVersion = static_cast<int>(version);
        for (int v = result.fromVersion; v < currentVersion; ++v) {
            const auto step = std::ranges::find_if(chain, [&](const Migration& m) { return m.from == v; });
            if (step == chain.end() || !step->run) {
                return fail(line, std::format("no migration from config_version {} to {}, file skipped", v, v + 1));
            }
            step->run(layer);
            result.changed = true;
        }
        if (result.changed) {
            auto* writable = layer["meta"].as_table();
            if (!writable) {
                layer.insert_or_assign("meta", toml::table{});
                writable = layer["meta"].as_table();
            }
            writable->insert_or_assign("config_version", static_cast<int64_t>(currentVersion));
        }
        return result;
    }
}
