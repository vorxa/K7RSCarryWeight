#include "core/user_edit.h"

#include <algorithm>
#include <cctype>
#include <format>

#include "core/loader.h"
#include "core/serialize.h"

namespace k7::rscw::core
{
    namespace
    {
        namespace fs = std::filesystem;

        bool SameForms(const std::vector<common::FormKey>& a, const std::vector<common::FormKey>& b)
        {
            return a.size() == b.size() && std::ranges::equal(a, b, [](const common::FormKey& x, const common::FormKey& y) {
                return common::Equivalent(x, y);
            });
        }
    }

    std::string ToUserToml(const Config& base, const Config& edited)
    {
        std::string out;
        out += "# K7 - Race&Sex Carry Weight: your settings, written by the in-game menu.\n";
        out += "# Only what differs from config.toml is here. Comments are not kept when the menu writes this file;\n";
        out += "# the previous version is in config.user.toml.bak.\n";

        std::string general;
        if (edited.general.enabled != base.general.enabled) {
            general += std::format("enabled = {}\n", edited.general.enabled ? "true" : "false");
        }
        if (edited.general.minTotal != base.general.minTotal) {
            general += std::format("min_total = {}\n", edited.general.minTotal);
        }
        if (edited.general.logLevel != base.general.logLevel) {
            general += std::format("log_level = {}\n", QuoteToml(common::ToString(edited.general.logLevel)));
        }
        if (edited.general.survivalPenalty != base.general.survivalPenalty) {
            general += std::format("survival_penalty = {}\n", edited.general.survivalPenalty);
        }
        if (!general.empty()) {
            out += "\n[general]\n" + general;
        }

        for (const auto& race : edited.races) {
            const auto* known = FindRace(base, race.id);
            std::string fields;
            if (!known || !SameForms(race.forms, known->forms)) {
                fields += std::format("forms  = {}\n", FormsToToml(race.forms));
            }
            if (!known || race.male != known->male) {
                fields += std::format("male   = {}\n", race.male);
            }
            if (!known || race.female != known->female) {
                fields += std::format("female = {}\n", race.female);
            }
            if (!fields.empty()) {
                out += std::format("\n[races.{}]\n", TomlKey(race.id)) + fields;
            }
        }

        if (!SameForms(edited.beastForms, base.beastForms)) {
            out += std::format("\n[beast]\nforms = {}\n", FormsToToml(edited.beastForms));
        }
        if (!SameForms(edited.survivalEffects, base.survivalEffects)) {
            out += std::format("\n[survival]\neffects = {}\n", FormsToToml(edited.survivalEffects));
        }
        return out;
    }

    bool RevertRace(Config& edited, const Config& base, std::string_view id)
    {
        const auto* known = FindRace(base, id);
        auto*       race = FindRace(edited, id);
        if (!known || !race) {
            return false;
        }
        race->forms = known->forms;
        race->male = known->male;
        race->female = known->female;
        return true;
    }

    bool CanRemoveRace(const Config& base, std::string_view id) noexcept
    {
        return FindRace(base, id) == nullptr;
    }

    bool RemoveRace(Config& edited, const Config& base, std::string_view id)
    {
        if (!CanRemoveRace(base, id)) {
            return false;
        }
        return std::erase_if(edited.races, [&](const RaceEntry& race) { return race.id == id; }) > 0;
    }

    std::string NewRaceId(const Config& config, std::string_view editorId)
    {
        std::string id;
        for (const char c : editorId) {
            const auto u = static_cast<unsigned char>(c);
            if (std::isalnum(u) && u < 0x80) {
                id += static_cast<char>(std::tolower(u));
            } else if (!id.empty() && id.back() != '_') {
                id += '_';
            }
        }
        while (!id.empty() && id.back() == '_') {
            id.pop_back();
        }
        if (id.empty()) {
            id = "race";
        }
        if (!FindRace(config, id)) {
            return id;
        }
        for (int n = 2;; ++n) {
            auto candidate = std::format("{}_{}", id, n);
            if (!FindRace(config, candidate)) {
                return candidate;
            }
        }
    }

    std::string AddRace(Config& edited, std::string_view editorId, const common::FormKey& form, float raceBase)
    {
        const auto value = std::clamp(static_cast<int>(raceBase + 0.5f), kValueLow, kValueHigh);
        auto       id = NewRaceId(edited, editorId);
        edited.races.push_back(RaceEntry{ id, { form }, value, value });
        return id;
    }

    bool IsInList(const std::vector<common::FormKey>& list, const common::FormKey& form) noexcept
    {
        return std::ranges::any_of(list, [&](const common::FormKey& f) { return common::Equivalent(f, form); });
    }

    AddRecordResult AddRecord(std::vector<common::FormKey>& list, const common::FormKey& form)
    {
        if (IsInList(list, form)) {
            return AddRecordResult::Duplicate;
        }
        list.push_back(form);
        return AddRecordResult::Added;
    }

    bool RemoveRecord(std::vector<common::FormKey>& list, const common::FormKey& form)
    {
        return std::erase_if(list, [&](const common::FormKey& f) { return common::Equivalent(f, form); }) > 0;
    }

    UserFileState ReadUserFile(const fs::path& dir)
    {
        return common::config::ReadUserFile(dir, kUserConfigFile);
    }

    WriteUserResult WriteUserFile(const fs::path& dir, std::string_view text, const std::optional<std::string>& readText,
        bool overwrite, bool& backupDone, common::Diagnostics& diagnostics)
    {
        return common::config::WriteUserFile(dir, kUserConfigFile, text, readText, overwrite,
            common::config::BackupPolicy::OncePerSession, backupDone, diagnostics);
    }

    bool StartCleanUserFile(const fs::path& dir, common::Diagnostics& diagnostics)
    {
        return common::config::StartCleanUserFile(dir, kUserConfigFile, diagnostics);
    }
}
