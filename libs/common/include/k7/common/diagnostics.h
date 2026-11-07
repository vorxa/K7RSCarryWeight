#pragma once

#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace k7::common
{
    enum class Severity
    {
        Note,    // expected and harmless, worth a line only when debugging
        Warning, // something is ignored or suspicious, the rest of the file still applies
        Error    // a file or a value was rejected
    };

    // A problem found while loading configuration. Never thrown: the loader collects them and the
    // plugin writes them to the log (FR-5.4, FR-13.4).
    struct Diagnostic
    {
        Severity    severity{ Severity::Warning };
        std::string file;    // as shown to the user, e.g. "overrides/20_x.toml"
        int         line{};  // 1-based, 0 when unknown
        std::string message;
    };

    using Diagnostics = std::vector<Diagnostic>;

    // File name used in diagnostics that come from the merged configuration rather than one file.
    inline constexpr std::string_view kMergedConfig = "(merged configuration)";

    // "overrides/20_x.toml:7: unknown key 'colour'" (the line is left out when unknown).
    inline std::string Format(const Diagnostic& diagnostic)
    {
        if (diagnostic.line > 0) {
            return std::format("{}:{}: {}", diagnostic.file, diagnostic.line, diagnostic.message);
        }
        return std::format("{}: {}", diagnostic.file, diagnostic.message);
    }
}
