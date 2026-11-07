#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "core/apply.h"
#include "core/settings.h"
#include "k7/common/diagnostics.h"

namespace k7::rscw::config
{
    // Writes config.toml if needed and reads the configuration (specification, section 6). Called at plugin
    // load and again on every save load and new game, so that edited files take effect without a restart.
    // `reason` goes into the log. Never throws: on an unexpected failure the built-in values are used.
    void Load(std::string_view reason);

    const core::Config&  Get() noexcept;
    const core::Sources& GetSources() noexcept;

    // For the menu (specification, section 13): the configuration without config.user.toml (what the menu compares
    // with), the problems of the last read and the files it read. Main thread only.
    const core::Config&              GetBase() noexcept;
    const core::Config&              GetLayered() noexcept;
    const common::Diagnostics&       GetDiagnostics() noexcept;
    const std::vector<std::string>&  GetFiles() noexcept;
    const std::filesystem::path&     Dir() noexcept;
}
