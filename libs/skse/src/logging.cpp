#include "pch.h"

#include "k7/skse/logging.h"
#include "k7/common/diagnostics.h"

#include <spdlog/sinks/basic_file_sink.h>

namespace k7::skse::logging
{
    void Init(const char* pluginName)
    {
        // The game folder (SE/AE or VR) is chosen by CommonLibSSE-NG from the running runtime.
        auto dir = SKSE::log::log_directory();
        if (!dir) {
            return;
        }
        *dir /= std::format("{}.log", pluginName);

        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(dir->string(), true);
        auto log = std::make_shared<spdlog::logger>("global", std::move(sink));
        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        SetLevel(common::LogLevel::Info);
    }

    void SetLevel(common::LogLevel level)
    {
        const auto spdLevel = common::ToSpdlog(level);
        spdlog::set_level(spdLevel);
        // Flush at least info, and also trace/debug lines when they are enabled: they matter most after a crash.
        spdlog::flush_on((std::min)(spdLevel, spdlog::level::info)); // parentheses: windows.h defines min
    }

    void LogDiagnostic(const common::Diagnostic& diagnostic)
    {
        const auto text = common::Format(diagnostic);
        switch (diagnostic.severity) {
        case common::Severity::Error: spdlog::error("config: {}", text); break;
        case common::Severity::Warning: spdlog::warn("config: {}", text); break;
        case common::Severity::Note: spdlog::debug("config: {}", text); break;
        }
    }
}
