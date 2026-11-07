#include "k7/common/log_level.h"

#include <array>
#include <cctype>

namespace k7::common
{
    namespace
    {
        constexpr std::array<std::pair<std::string_view, LogLevel>, 5> kNames{ {
            { "trace", LogLevel::Trace },
            { "debug", LogLevel::Debug },
            { "info", LogLevel::Info },
            { "warn", LogLevel::Warn },
            { "error", LogLevel::Error },
        } };

        bool EqualsIgnoreCase(std::string_view a, std::string_view lowerB) noexcept
        {
            if (a.size() != lowerB.size()) {
                return false;
            }
            for (std::size_t i = 0; i < a.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(a[i])) != lowerB[i]) {
                    return false;
                }
            }
            return true;
        }
    }

    std::optional<LogLevel> ParseLogLevel(std::string_view text) noexcept
    {
        for (const auto& [name, level] : kNames) {
            if (EqualsIgnoreCase(text, name)) {
                return level;
            }
        }
        return std::nullopt;
    }

    std::string_view ToString(LogLevel level) noexcept
    {
        for (const auto& [name, value] : kNames) {
            if (value == level) {
                return name;
            }
        }
        return "info";
    }

    spdlog::level::level_enum ToSpdlog(LogLevel level) noexcept
    {
        switch (level) {
        case LogLevel::Trace: return spdlog::level::trace;
        case LogLevel::Debug: return spdlog::level::debug;
        case LogLevel::Info:  return spdlog::level::info;
        case LogLevel::Warn:  return spdlog::level::warn;
        case LogLevel::Error: return spdlog::level::err;
        }
        return spdlog::level::info;
    }
}
