#pragma once

#include <map>
#include <string>
#include <string_view>

#include <toml++/toml.hpp>

#include "core/settings.h"
#include "k7/common/diagnostics.h"
#include "k7/common/form_key.h"

namespace k7::rscw::core
{
    // Where a race record of the configuration was given: file, line and the order in which it was applied
    // (built-in records have order 0). The order decides which race keeps a record named by two races, and
    // the file and line go into the warning about a record that is not loaded.
    struct FormSource
    {
        std::string file;
        int         line{};
        int         order{};
    };

    // Sources of the race records currently in the configuration. The key is SourceKey(...).
    struct Sources
    {
        std::map<std::string, FormSource> forms;
        int                               nextOrder{ 1 };

        // "races.nord|skyrim.esm|0x013746", "beast|dawnguard.esm|0x00283a": plugin names compared without case.
        static std::string Key(std::string_view owner, const common::FormKey& form);
        const FormSource*  Find(std::string_view owner, const common::FormKey& form) const;
    };

    // Applies one parsed configuration file on top of `config` (specification, section 6):
    //  * [general] enabled, min_total (1–300), log_level, survival_penalty (0–1000);
    //  * [races.<id>] merges field by field: `male`, `female` (1–1000) replace the value, `forms` replaces the
    //    list of race records of that race; an id that is not there yet adds a race (checked for completeness
    //    by ValidateConfig after all layers);
    //  * [beast] forms replaces the list of beast forms;
    //  * [survival] effects replaces the list of Survival Mode penalty effects;
    //  * [meta] config_version and plugin_version are read by the loader and ignored here.
    // A wrong type, a value out of range, a bad form or an unknown key is reported with the file and line and
    // skipped; the previous value stays. Nothing throws. If `sources` is given, every race record and beast
    // form that this layer sets is recorded there.
    void ApplyLayer(Config& config, const toml::table& layer, std::string_view file, common::Diagnostics& diagnostics,
        Sources* sources = nullptr);
}
