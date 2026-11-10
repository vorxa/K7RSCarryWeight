#include "k7/common/user_file.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <sstream>
#include <system_error>

#include <toml++/toml.hpp>

#include "k7/common/config_layers.h"
#include "k7/common/file_io.h"

namespace k7::common::config
{
    namespace fs = std::filesystem;

    namespace
    {
        std::string LowerAscii(std::string text)
        {
            for (auto& c : text) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return text;
        }

        void Error(Diagnostics& a_diagnostics, std::string_view a_fileName, std::string a_message)
        {
            a_diagnostics.push_back(Diagnostic{ Severity::Error, std::string(a_fileName), 0, std::move(a_message) });
        }

        std::optional<std::string> ParseErrorOf(const std::string& a_text, std::string_view a_fileName)
        {
            Diagnostics diagnostics;
            const auto parsed = Parse(a_text, std::string(a_fileName), diagnostics);
            if (parsed) {
                return std::nullopt;
            }
            return diagnostics.empty() ? std::string("does not parse") : diagnostics.front().message;
        }

        // Copied, not renamed, so that the new file replaces the old one in a single atomic step.
        bool SaveBackup(const fs::path& a_userPath, std::string_view a_fileName, Diagnostics& a_diagnostics)
        {
            std::error_code ec;
            if (!fs::exists(a_userPath, ec)) {
                return true;
            }
            const auto text = io::ReadFile(a_userPath);
            auto       backup = a_userPath;
            backup += ".bak";
            if (!text || !io::WriteFileAtomic(backup, *text)) {
                Error(a_diagnostics, a_fileName, std::format("cannot save {}.bak, nothing written", a_fileName));
                return false;
            }
            return true;
        }
    }

    UserFileState ReadUserFile(const fs::path& a_dir, std::string_view a_fileName)
    {
        UserFileState state;
        state.text = io::ReadFile(a_dir / a_fileName);
        if (state.text) {
            state.parses = Parse(*state.text, std::string(a_fileName), state.diagnostics).has_value();
        }
        return state;
    }

    UserFileResult WriteUserFile(const fs::path& a_dir, std::string_view a_fileName, std::string_view a_text,
        const std::optional<std::string>& a_expectedText, bool a_overwrite, BackupPolicy a_policy, bool& a_backupDone,
        Diagnostics& a_diagnostics)
    {
        const auto userPath = a_dir / a_fileName;
        const auto current  = io::ReadFile(userPath);
        if (current) {
            if (ParseErrorOf(*current, a_fileName)) {
                return UserFileResult::Broken;
            }
        }
        if (!a_overwrite && current != a_expectedText) {
            return UserFileResult::Changed;
        }

        std::error_code ec;
        fs::create_directories(a_dir, ec);
        if (a_policy == BackupPolicy::EachWrite || !a_backupDone) {
            if (!SaveBackup(userPath, a_fileName, a_diagnostics)) {
                return UserFileResult::Failed;
            }
            a_backupDone = true;
        }
        if (!io::WriteFileAtomic(userPath, a_text)) {
            Error(a_diagnostics, a_fileName, "cannot write the file");
            return UserFileResult::Failed;
        }
        return UserFileResult::Written;
    }

    bool StartCleanUserFile(const fs::path& a_dir, std::string_view a_fileName, Diagnostics& a_diagnostics)
    {
        const auto      userPath = a_dir / a_fileName;
        std::error_code ec;
        fs::create_directories(a_dir, ec);
        if (!SaveBackup(userPath, a_fileName, a_diagnostics)) {
            return false;
        }
        if (!io::WriteFileAtomic(userPath, "")) {
            Error(a_diagnostics, a_fileName, "cannot create an empty file");
            return false;
        }
        return true;
    }

    std::vector<std::string> ListPresets(const fs::path& a_dir)
    {
        std::vector<std::string> names;
        std::error_code          ec;
        const auto               presets = a_dir / kPresetsDir;
        if (!fs::is_directory(presets, ec)) {
            return names;
        }
        for (fs::directory_iterator it(presets, ec), end; !ec && it != end; it.increment(ec)) {
            if (!it->is_regular_file(ec) || LowerAscii(it->path().extension().string()) != ".toml") {
                continue;
            }
            names.push_back(it->path().stem().string());
        }
        std::ranges::sort(names, [](const std::string& a, const std::string& b) {
            const auto la = LowerAscii(a);
            const auto lb = LowerAscii(b);
            return la != lb ? la < lb : a < b;
        });
        return names;
    }

    bool SetPreset(const fs::path& a_dir, std::string_view a_fileName, std::string_view a_name, Diagnostics& a_diagnostics)
    {
        const std::string display(a_fileName);
        const auto        userPath = a_dir / a_fileName;

        if (!a_name.empty()) {
            if (!IsValidPresetName(a_name)) {
                Error(a_diagnostics, display, std::format("'{}' is not a preset name", a_name));
                return false;
            }
            std::error_code ec;
            if (!fs::is_regular_file(a_dir / kPresetsDir / (std::string(a_name) + ".toml"), ec)) {
                Error(a_diagnostics, display, std::format("preset '{}' does not exist", a_name));
                return false;
            }
        }

        std::optional<std::string> original;
        toml::table                table;
        std::error_code            ec;
        if (fs::exists(userPath, ec)) {
            original = io::ReadFile(userPath);
            if (!original) {
                Error(a_diagnostics, display, "cannot read the file, preset not changed");
                return false;
            }
            try {
                table = toml::parse(*original, display);
            } catch (const std::exception&) {
                Error(a_diagnostics, display, "the file does not parse, preset not changed (fix it or reset to defaults)");
                return false;
            }
        }

        if (a_name.empty()) {
            if (auto* meta = table["meta"].as_table()) {
                meta->erase("preset");
            }
        } else {
            auto* meta = table["meta"].as_table();
            if (!meta) {
                table.insert_or_assign("meta", toml::table{});
                meta = table["meta"].as_table();
            }
            meta->insert_or_assign("preset", std::string(a_name));
        }

        std::ostringstream out;
        out << table;
        bool       backupDone = false;
        const auto result     = WriteUserFile(a_dir, a_fileName, out.str(), original, false, BackupPolicy::EachWrite,
            backupDone, a_diagnostics);
        return result == UserFileResult::Written;
    }
}
