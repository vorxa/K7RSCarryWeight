#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "k7/common/diagnostics.h"

// The player's own configuration file (config.user.toml by default), without the plugin's schema. The same
// safety floor for every plugin:
//   * a file that does not parse is never written over: the player's settings would be lost;
//   * a file is replaced atomically, through a temporary file and a rename;
//   * before every replacement a unique verified copy is saved outside the configuration tree.
// What stays with the plugin: what to write (its own "only the differences" text), whether to compare the file
// with the text the editor read, and how often to keep the backup.
// Log lines here are English, like the rest of the log; nothing here is shown to the player.
namespace k7::common::config
{
    struct UserFileState
    {
        std::optional<std::string> text;    // nothing if the file is missing
        bool                       parses{ true };
        Diagnostics                diagnostics;  // the syntax error of a file that does not parse
    };

    // The file on disk as the editor reads it.
    UserFileState ReadUserFile(const std::filesystem::path& a_dir, std::string_view a_fileName);

    enum class BackupPolicy
    {
        EachWrite,        // ensure a legacy .bak exists; never overwrite it
        OncePerSession,   // ensure a legacy .bak exists on the first write of the session
    };

    enum class UserFileResult
    {
        Written,
        Changed,  // the file on disk is not what the editor read: ask to overwrite or read it again
        Broken,   // the file on disk does not parse: never written over, the player's settings would be lost
        Failed,   // could not write; the reason is in the diagnostics
    };

    // Writes `a_text` as `a_dir`/`a_fileName` through a temporary file and a rename. `a_expectedText` is the
    // content the editor read; if the file on disk differs, nothing is written and the result is Changed, unless
    // `a_overwrite`. A missing file is created. `a_backupDone` belongs to the caller's session: with
    // OncePerSession the legacy .bak is ensured on the first write and the flag remembers it. Every replacement
    // also requires a unique verified backup outside the config directory; an existing legacy .bak is retained.
    UserFileResult WriteUserFile(const std::filesystem::path& a_dir, std::string_view a_fileName, std::string_view a_text,
        const std::optional<std::string>& a_expectedText, bool a_overwrite, BackupPolicy a_policy, bool& a_backupDone,
        Diagnostics& a_diagnostics);

    // "Start with a clean file": the current text is backed up and an empty file
    // takes its place. False with an error in the diagnostics if the files could not be changed.
    bool StartCleanUserFile(const std::filesystem::path& a_dir, std::string_view a_fileName, Diagnostics& a_diagnostics,
        const std::optional<std::string>* a_expectedText = nullptr);

    // Names (without ".toml") of the files in `a_dir`/presets, alphabetically without regard to case.
    std::vector<std::string> ListPresets(const std::filesystem::path& a_dir);

    // Chooses a preset: writes `[meta] preset = "<name>"` into the user file, keeping everything else in it; an
    // empty name clears the choice. Refuses (returns false, reason in the diagnostics, file untouched) for an
    // invalid name, a preset that does not exist, or a user file that does not parse. Comments in the user file do
    // not survive a rewrite; the previous text is kept next to it.
    bool SetPreset(const std::filesystem::path& a_dir, std::string_view a_fileName, std::string_view a_name,
        Diagnostics& a_diagnostics, const std::optional<std::string>* a_expectedText = nullptr);
}
