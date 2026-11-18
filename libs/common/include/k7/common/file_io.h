#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>

// Small file helpers shared by the loader and the user-config editor. Nothing here throws.
namespace k7::common::io
{
    inline std::optional<std::string> ReadFile(const std::filesystem::path& path);
    inline std::optional<std::filesystem::path> EnvironmentPath(const char* name)
    {
#ifdef _MSC_VER
        char* value = nullptr;
        std::size_t size = 0;
        if (_dupenv_s(&value, &size, name) != 0 || !value) { return std::nullopt; }
        const std::filesystem::path result(value);
        std::free(value);
        return result.empty() ? std::nullopt : std::optional(result);
#else
        const auto* value = std::getenv(name);
        return value && *value ? std::optional(std::filesystem::path(value)) : std::nullopt;
#endif
    }

    // A unique, verified copy outside the config tree before any replacement.
    // The environment override is useful for isolated test installations.
    inline bool BackupExisting(const std::filesystem::path& path)
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        const bool exists = fs::exists(path, ec);
        if (ec) { return false; }
        if (!exists) { return true; }
        auto root = EnvironmentPath("K7_CONFIG_BACKUP_ROOT");
        if (!root) {
            const auto local = EnvironmentPath("LOCALAPPDATA");
            if (!local) { return false; }
            root = *local / "K7SkyrimPlugins" / "ConfigBackups";
        }
        const auto source = ReadFile(path);
        if (!source) { return false; }
        const auto sourceDir = fs::weakly_canonical(path.parent_path(), ec);
        if (ec) { return false; }
        const auto backupRoot = fs::weakly_canonical(*root, ec);
        if (ec) { return false; }
        const auto rel = backupRoot.lexically_relative(sourceDir);
        if (rel.empty() && backupRoot.root_name() == sourceDir.root_name()) { return false; }
        if (!rel.empty() && *rel.begin() != "..") { return false; }
        const auto now = std::chrono::system_clock::now();
        const auto time = std::chrono::system_clock::to_time_t(now);
        std::tm localTime{};
#ifdef _MSC_VER
        if (localtime_s(&localTime, &time) != 0) { return false; }
#else
        if (!localtime_r(&time, &localTime)) { return false; }
#endif
        static std::atomic_uint64_t sequence{0};
        std::ostringstream stamp;
        stamp << std::put_time(&localTime, "%Y%m%d-%H%M%S") << '-' << now.time_since_epoch().count() << '-' << sequence++;
        const auto folder = backupRoot / stamp.str();
        if (!fs::create_directories(folder, ec) || ec) { return false; }
        const auto backup = folder / path.filename();
        if (!fs::copy_file(path, backup, fs::copy_options::none, ec) || ec) { return false; }
        return ReadFile(backup) == source && ReadFile(path) == source;
    }
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
        static std::atomic_uint64_t sequence{0};
        std::filesystem::path folder;
        std::error_code ec;
        for (int attempt = 0; attempt < 100; ++attempt) {
            folder = path;
            folder += ".tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(sequence++);
            if (std::filesystem::create_directory(folder, ec)) { break; }
            if (ec && ec != std::errc::file_exists) { return false; }
            folder.clear();
            ec.clear();
        }
        if (folder.empty()) { return false; }
        auto temp = folder / path.filename();
        if (!WriteFile(temp, content)) {
            std::error_code ignored;
            std::filesystem::remove(temp, ignored);
            std::filesystem::remove(folder, ignored);
            return false;
        }
        std::filesystem::rename(temp, path, ec);
        if (ec) {
            std::error_code ignored;
            std::filesystem::remove(temp, ignored);
            std::filesystem::remove(folder, ignored);
            return false;
        }
        std::filesystem::remove(folder, ec);
        return true;
    }
}
