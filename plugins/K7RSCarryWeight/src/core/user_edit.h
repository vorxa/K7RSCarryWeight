#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/settings.h"
#include "k7/common/diagnostics.h"
#include "k7/common/form_key.h"

// What the settings menu does to config.user.toml (specification, section 13.3–13.4), without the game: the menu
// keeps a working copy of the configuration, changes it with these operations and writes back only what
// differs from the base (built-in values and config.toml).
namespace k7::rscw::core
{
    // config.user.toml as text: only the differences between `edited` and `base`.
    //  * [general]: every key whose value differs;
    //  * a race of the base: `male`, `female` and `forms` that differ;
    //  * a race that is not in the base: all of it;
    //  * [beast] forms and [survival] effects: the whole list if it differs.
    // A race of the base that `edited` lacks cannot be expressed (layers only add and replace) and is left out.
    std::string ToUserToml(const Config& base, const Config& edited);

    // The menu's operations on the working copy. Ids are compared as written.
    // * Built-in values: male, female and forms of the race as in the base. False if the base has no such race.
    bool RevertRace(Config& edited, const Config& base, std::string_view id);

    // * A race only the player added can be removed; one of the base cannot (the layers cannot remove it).
    bool CanRemoveRace(const Config& base, std::string_view id) noexcept;
    bool RemoveRace(Config& edited, const Config& base, std::string_view id);

    // * The id for a race added from its EditorID: lower case, letters, digits and `_` (anything else becomes
    //   `_`), "race" if nothing is left; `_2`, `_3`, ... if the id is taken.
    std::string NewRaceId(const Config& config, std::string_view editorId);

    // * Adds the race of the character: one record, both values = the base carry weight of that race (clamped
    //   to 1–1000), so that its contribution is 0 until the player changes them. Returns its id.
    std::string AddRace(Config& edited, std::string_view editorId, const common::FormKey& form, float raceBase);

    // * Lists of records ([races.<id>] forms, [beast] forms, [survival] effects).
    enum class AddRecordResult
    {
        Added,
        Duplicate,  // already in this list
    };
    AddRecordResult AddRecord(std::vector<common::FormKey>& list, const common::FormKey& form);
    bool          RemoveRecord(std::vector<common::FormKey>& list, const common::FormKey& form);

    // * A record of a base list (built in, or from config.toml) is removed only by editing the file.
    bool IsInList(const std::vector<common::FormKey>& list, const common::FormKey& form) noexcept;

    // config.user.toml on disk as the menu read it.
    struct UserFileState
    {
        std::optional<std::string> text;    // nothing if the file is missing
        bool                       parses{ true };
        common::Diagnostics        diagnostics;  // the syntax error of a file that does not parse
    };
    UserFileState ReadUserFile(const std::filesystem::path& dir);

    enum class WriteUserResult
    {
        Written,
        Changed,  // the file on disk is not what the menu read: ask to overwrite or read it again
        Broken,   // the file on disk does not parse: never written over, the player's settings would be lost
        Failed,   // could not write; the reason is in the diagnostics
    };

    // Writes `text` as config.user.toml through a temporary file and a rename. `readText` is the content the menu
    // read; if the file on disk differs, nothing is written and the result is Changed, unless `overwrite`.
    // Before the first write of a game session (`backupDone` false) the old file is copied to
    // config.user.toml.bak and `backupDone` becomes true.
    WriteUserResult WriteUserFile(const std::filesystem::path& dir, std::string_view text, const std::optional<std::string>& readText,
        bool overwrite, bool& backupDone, common::Diagnostics& diagnostics);

    // "Start with a clean file": the current config.user.toml is copied to config.user.toml.bak (replacing an older
    // backup) and an empty one takes its place. False with an error in the diagnostics if the files could not be
    // changed.
    bool StartCleanUserFile(const std::filesystem::path& dir, common::Diagnostics& diagnostics);
}
