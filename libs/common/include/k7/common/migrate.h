#pragma once

#include <functional>
#include <span>
#include <string_view>

#include <toml++/toml.hpp>

#include "k7/common/diagnostics.h"

namespace k7::common
{
    // Upgrades a parsed file from schema version `from` to `from + 1`.
    struct Migration
    {
        int                                from{};
        std::function<void(toml::table&)>  run;
    };

    struct MigrationResult
    {
        bool usable{ true };       // false: the file must not be applied
        bool changed{ false };     // the table was upgraded and differs from what is on disk
        int  fromVersion{ 1 };
    };

    // Schema versioning of one parsed configuration file, `[meta] config_version`:
    //  * no `config_version`: the file is version 1 and is migrated like an explicit 1;
    //  * older than `currentVersion`: the chain of migrations is run and `config_version` is updated;
    //  * newer than `currentVersion`, not a whole number, below 1, or a migration step is missing:
    //    an error, the file is not usable.
    MigrationResult MigrateLayer(toml::table& layer, std::string_view file, std::span<const Migration> chain,
        int currentVersion, Diagnostics& diagnostics);
}
