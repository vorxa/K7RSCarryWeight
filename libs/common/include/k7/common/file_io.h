#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

// Small file helpers shared by the loader and the user-config editor. Nothing here throws.
namespace k7::common::io
{
    inline std::optional<std::string> ReadFile(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) {
            return std::nullopt;
        }
        std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        if (stream.bad()) {
            return std::nullopt;
        }
        return content;
    }

    inline bool WriteFile(const std::filesystem::path& path, std::string_view content)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream) {
            return false;
        }
        stream.write(content.data(), static_cast<std::streamsize>(content.size()));
        stream.flush();
        return static_cast<bool>(stream);
    }

    // Writes next to the target and renames over it, so that a crash or a full disk never leaves
    // a half-written file where the player's settings were.
    inline bool WriteFileAtomic(const std::filesystem::path& path, std::string_view content)
    {
        auto temp = path;
        temp += ".tmp";
        if (!WriteFile(temp, content)) {
            std::error_code ignored;
            std::filesystem::remove(temp, ignored);
            return false;
        }
        std::error_code ec;
        std::filesystem::rename(temp, path, ec);
        if (ec) {
            std::filesystem::remove(temp, ec);
            return false;
        }
        return true;
    }
}
