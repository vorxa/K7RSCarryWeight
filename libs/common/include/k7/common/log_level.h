#pragma once

#include <optional>
#include <string_view>

#include <spdlog/common.h>

namespace k7::common
{
    // Values of [diagnostics] log_level (FR-13.1).
    enum class LogLevel
    {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
    };

    // Case-insensitive; surrounding whitespace is not accepted. Empty result means "not a level".
    std::optional<LogLevel> ParseLogLevel(std::string_view text) noexcept;

    std::string_view ToString(LogLevel level) noexcept;

    spdlog::level::level_enum ToSpdlog(LogLevel level) noexcept;
}
