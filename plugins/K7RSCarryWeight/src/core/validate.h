#pragma once

#include "core/apply.h"
#include "core/settings.h"
#include "k7/common/diagnostics.h"

namespace k7::rscw::core
{
    // Checks the configuration after all layers are merged and repairs what cannot be used, with a warning
    // for each repair; the result is always usable:
    //  * a race added by a file without `forms`, `male` or `female` is dropped;
    //  * a race record named by two races stays with the race that named it last in the order of application
    //    (built-in records count as first); the user decided so on 2026-11-06;
    //  * a race record that is also a beast form stays a beast form only: the plugin does not recount there.
    void ValidateConfig(Config& config, const Sources& sources, common::Diagnostics& diagnostics);
}
