#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/recount.h"
#include "core/settings.h"
#include "core/user_edit.h"
#include "k7/common/diagnostics.h"
#include "k7/common/form_key.h"

// The bridge between the menu and the game (stage 2 plan, step 5). The menu is drawn on the render thread; the game
// is read and changed only on the main thread. So the menu reads a snapshot that a game task refreshes while the
// menu is drawn, and sends its actions as game tasks; their results come back in the snapshot.
namespace k7::rscw::ui
{
    // What the menu shows of the character (specification, section 13.2).
    struct PlayerView
    {
        bool loaded{};  // a game is loaded (not the main menu)

        enum class Kind
        {
            Race,   // a race of the configuration
            Beast,  // a beast form: no recount
            None,   // not in the configuration
        };

        std::string                    raceName;      // the race's name in the game
        std::string                    raceForm;      // "Skyrim.esm|0x013746"
        std::string                    raceEditorId;  // for "add the character's race"
        std::optional<common::FormKey> raceKey;
        Kind                           kind{ Kind::None };
        std::string                    raceId;        // [races.<id>] when kind is Race
        core::Sex                      sex{ core::Sex::Unknown };
        float                          base{};
        std::optional<int>             value;         // after min_total and survival_penalty
        float                          contribution{};
        float                          modifier{};    // the permanent CarryWeight modifier
        float                          total{};       // the carry weight
        float                          gamePenalty{}; // the Survival Mode penalty on the character, 0 if none
        bool                           penaltyCancelled{};
    };

    // The result of the last action of the menu.
    struct ActionResult
    {
        enum class Kind
        {
            Saved,
            SaveChanged,     // config.user.toml changed on disk: ask to overwrite or read it again
            SaveBroken,      // config.user.toml does not parse: not written
            SaveFailed,
            Reloaded,
            CleanStarted,
            CleanFailed,
            FormChecked,
        };
        enum class FormCheck
        {
            Ok,
            NotFound,
            NotRace,
            NotEffect,
        };

        std::uint64_t        id{};  // grows with every action; the menu reacts to a new id once
        Kind                 kind{ Kind::Reloaded };
        std::string          reason;  // why a write failed, for the message
        FormCheck            formCheck{ FormCheck::Ok };
        common::FormKey      form;
        int                  formTarget{};  // what the menu wanted the form for (its own tag)
    };

    struct Snapshot
    {
        std::uint64_t              revision{};      // grows with every refresh
        std::uint64_t              configRevision{};  // grows when the configuration is read again
        PlayerView                 player;
        core::Config               base;            // without config.user.toml: what the menu compares with
        core::Config               config;          // as the game uses it now
        core::Config               layered;         // before validation: the menu edits this one
        common::Diagnostics        problems;        // of the last read of the files
        std::vector<std::string>   files;           // the files read
        core::UserFileState        userFile;        // config.user.toml on disk now
        std::optional<ActionResult> lastAction;
    };

    // A copy of the current snapshot. Any thread.
    Snapshot Get();

    // Asks for a refresh: queues a game task at most twice a second. Call from the menu's render function.
    void RequestRefresh();

    // Actions of the menu, done in a game task. Any thread.
    // * Writes `text` as config.user.toml (core::WriteUserFile, `readText` = what the menu read); when written, reads
    //   the configuration again, resolves races and effects and recounts (section 13.4).
    void Save(std::string text, std::optional<std::string> readText, bool overwrite);
    // * Reads the files again and recounts: the same as a save load, without loading.
    void Reload();
    // * "Start with a clean file" (core::StartCleanUserFile), then reads the files again.
    void StartClean();
    // * Checks that a record is in the game and is a race (`wantEffect` false) or a magic effect (true).
    void CheckForm(common::FormKey form, bool wantEffect, int target);

    // At kDataLoaded: one refresh so that the menu has data when first opened.
    void Init();
}
