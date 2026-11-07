#pragma once

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>

#include <toml++/toml.hpp>

#include "core/apply.h"
#include "core/defaults.h"
#include "k7/common/diagnostics.h"

namespace k7::rscw::test
{
    namespace fs = std::filesystem;

    // A scratch directory removed at the end of the test.
    class TempDir
    {
    public:
        TempDir()
        {
            path_ = fs::temp_directory_path() /
                    std::format("k7rscw_tests_{}", std::chrono::steady_clock::now().time_since_epoch().count());
            fs::create_directories(path_);
        }
        ~TempDir()
        {
            std::error_code ec;
            fs::remove_all(path_, ec);
        }
        TempDir(const TempDir&)            = delete;
        TempDir& operator=(const TempDir&) = delete;

        const fs::path& Path() const noexcept { return path_; }

        void Write(const std::string& relative, std::string_view content) const
        {
            const auto file = path_ / relative;
            fs::create_directories(file.parent_path());
            std::ofstream stream(file, std::ios::binary | std::ios::trunc);
            stream.write(content.data(), static_cast<std::streamsize>(content.size()));
        }

        std::string Read(const std::string& relative) const
        {
            std::ifstream stream(path_ / relative, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(stream), {});
        }

    private:
        fs::path path_;
    };

    // Applies TOML text as one layer named `file` on top of `config`.
    inline void Apply(core::Config& config, std::string_view text, common::Diagnostics& diagnostics,
        core::Sources* sources = nullptr, std::string_view file = "config.user.toml")
    {
        const auto table = toml::parse(text, std::string(file));
        core::ApplyLayer(config, table, file, diagnostics, sources);
    }

    inline std::size_t Count(const common::Diagnostics& diagnostics, common::Severity severity)
    {
        return static_cast<std::size_t>(std::ranges::count(diagnostics, severity, &common::Diagnostic::severity));
    }

    inline bool Mentions(const common::Diagnostics& diagnostics, std::string_view text)
    {
        return std::ranges::any_of(diagnostics, [&](const common::Diagnostic& d) {
            return d.message.find(text) != std::string::npos || d.file.find(text) != std::string::npos;
        });
    }
}
