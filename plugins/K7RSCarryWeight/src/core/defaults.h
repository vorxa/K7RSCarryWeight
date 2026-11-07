#pragma once

#include "core/settings.h"

namespace k7::rscw::core
{
    // The built-in configuration (specification, sections 4–6): ten playable races, each with its ordinary and
    // vampire record, and the two beast forms. config.toml is written from it, and it is what the plugin runs
    // on when no file can be read.
    Config MakeDefaultConfig();
}
