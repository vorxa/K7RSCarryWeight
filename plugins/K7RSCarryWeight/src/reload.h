#pragma once

#include <string_view>

namespace k7::rscw
{
    // Reads the configuration again, so that edited files take effect without a restart (specification, section 6),
    // and resolves its races and Survival Mode penalty effects. Main thread only: on a save load, a new game, and
    // after the menu writes config.user.toml. Does not recount; the caller does.
    void Reload(std::string_view reason);
}
