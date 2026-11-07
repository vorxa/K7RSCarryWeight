#include "pch.h"

#include "ui/menu.h"

#include <array>
#include <atomic>
#include <map>

// The header resolves every function through GetProcAddress; it needs <atomic>, <filesystem> and RE/ first.
#include <SKSEMenuFramework.h>

#include "core/user_edit.h"
#include "k7/common/form_key.h"
#include "k7/skse/menu.h"
#include "ui/keys.h"
#include "ui/lang_service.h"
#include "ui/state.h"

namespace k7::rscw::ui
{
    namespace
    {
        namespace I = ImGuiMCP;

        // Targets of a record the menu asked the game to check (ActionResult::formTarget).
        constexpr int kTargetBeast = 1;
        constexpr int kTargetSurvival = 2;
        constexpr int kTargetRaceFirst = 100;

        using Buffer = std::array<char, 128>;

        // The menu's working copy (render thread only).
        struct Work
        {
            bool                       active{};
            std::uint64_t              configRevision{};
            core::Config               base;
            core::Config               edited;
            std::optional<std::string> readText;  // config.user.toml as it was when the working copy was taken
            bool                       dirty{};
            std::uint64_t              actionSeen{};
            std::string                message;
            std::map<std::string, Buffer> inputs;  // the "add a record" fields by list
            std::map<int, std::string>    raceTargets;  // target -> race id
            int                           nextRaceTarget{ kTargetRaceFirst };
            bool                          openChanged{};
            bool                          openBroken{};
        };
        Work g_work;

        std::string T(std::string_view key)
        {
            return lang::Get().Text(key);
        }

        std::string F(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string_view>> values)
        {
            return lang::Get().Format(key, values);
        }

        std::string Number(float value)
        {
            return std::format("{:g}", value);
        }

        // "+5", "-85", "0": the parts of a sum. ASCII only: the menu's font has no typographic minus.
        std::string Signed(float value)
        {
            if (value > 0.0f) {
                return "+" + Number(value);
            }
            return value < 0.0f ? "-" + Number(-value) : "0";
        }

        // "Text##id": the text shown, the id unique in the window.
        std::string Label(std::string_view text, std::string_view id)
        {
            return std::format("{}##{}", text, id);
        }

        void Line(const std::string& text)
        {
            I::TextUnformatted(text.c_str());
        }

        void Wrapped(const std::string& text)
        {
            I::TextWrapped("%s", text.c_str());
        }

        void Hint(const std::string& text)
        {
            if (I::IsItemHovered()) {
                I::SetTooltip("%s", text.c_str());
            }
        }

        void TakeWorkingCopy(const Snapshot& snapshot)
        {
            g_work.active = true;
            g_work.configRevision = snapshot.configRevision;
            g_work.base = snapshot.base;
            g_work.edited = snapshot.layered;
            g_work.readText = snapshot.userFile.text;
            g_work.dirty = false;
            g_work.raceTargets.clear();
        }

        std::string RaceName(const core::RaceEntry& race)
        {
            std::string key = "K7RS_RACE_";
            for (const char c : race.id) {
                key += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            return lang::Get().Has(key) ? T(key) : race.id;
        }

        std::string SexText(core::Sex sex)
        {
            return T(sex == core::Sex::Male ? keys::SEX_MALE : sex == core::Sex::Female ? keys::SEX_FEMALE : keys::SEX_UNKNOWN);
        }

        int RaceTarget(const std::string& id)
        {
            for (const auto& [target, raceId] : g_work.raceTargets) {
                if (raceId == id) {
                    return target;
                }
            }
            const auto target = g_work.nextRaceTarget++;
            g_work.raceTargets[target] = id;
            return target;
        }

        std::vector<common::FormKey>* ListOf(int target)
        {
            if (target == kTargetBeast) {
                return &g_work.edited.beastForms;
            }
            if (target == kTargetSurvival) {
                return &g_work.edited.survivalEffects;
            }
            const auto it = g_work.raceTargets.find(target);
            auto*      race = it == g_work.raceTargets.end() ? nullptr : core::FindRace(g_work.edited, it->second);
            return race ? &race->forms : nullptr;
        }

        void HandleAction(const Snapshot& snapshot)
        {
            if (!snapshot.lastAction || snapshot.lastAction->id <= g_work.actionSeen) {
                return;
            }
            const auto& action = *snapshot.lastAction;
            g_work.actionSeen = action.id;
            using Kind = ActionResult::Kind;
            switch (action.kind) {
            case Kind::Saved:
                g_work.message = T(keys::SAVED);
                g_work.active = false;  // take the configuration as the game read it again
                break;
            case Kind::SaveChanged:
                g_work.openChanged = true;
                break;
            case Kind::SaveBroken:
                g_work.openBroken = true;
                break;
            case Kind::SaveFailed:
            case Kind::CleanFailed:
                g_work.message = F(keys::SAVE_FAILED, { { "reason", action.reason } });
                break;
            case Kind::Reloaded:
            case Kind::CleanStarted:
                g_work.message.clear();
                g_work.active = false;
                break;
            case Kind::FormChecked: {
                const auto text = common::ToString(action.form);
                using Check = ActionResult::FormCheck;
                if (action.formCheck == Check::NotFound) {
                    g_work.message = F(keys::FORM_NOT_FOUND, { { "form", text } });
                } else if (action.formCheck == Check::NotRace) {
                    g_work.message = F(keys::FORM_NOT_RACE, { { "form", text } });
                } else if (action.formCheck == Check::NotEffect) {
                    g_work.message = F(keys::FORM_NOT_EFFECT, { { "form", text } });
                } else if (auto* list = ListOf(action.formTarget)) {
                    if (core::AddRecord(*list, action.form) == core::AddRecordResult::Added) {
                        g_work.dirty = true;
                        g_work.message.clear();
                    } else {
                        g_work.message = F(keys::FORM_DUPLICATE, { { "form", text } });
                    }
                }
                break;
            }
            }
        }

        // An integer field that takes only values in [low, high]: anything else keeps the old value. The value is
        // taken when Enter is pressed or the field is left, not at every key: typing 5000 must not pass 500 on the way.
        // `width` < 0 fills the rest of the line or of the table cell.
        void IntField(const std::string& label, int& value, int low, int high, const std::string& hint, float width = 200.0f)
        {
            int edit = value;
            I::SetNextItemWidth(width);
            const bool entered = I::InputInt(label.c_str(), &edit, 1, 100, I::ImGuiInputTextFlags_EnterReturnsTrue);
            if ((entered || I::IsItemDeactivatedAfterEdit()) && edit >= low && edit <= high && edit != value) {
                value = edit;
                g_work.dirty = true;
            }
            Hint(hint);
        }

        // The records of a list with "remove" (not for those of the base) and a field to add one, checked by the game.
        void FormList(std::string_view id, std::vector<common::FormKey>& list, const std::vector<common::FormKey>* baseList, int target,
            bool wantEffect)
        {
            I::PushID(std::string(id).c_str());
            std::optional<common::FormKey> remove;
            for (std::size_t i = 0; i < list.size(); ++i) {
                I::PushID(static_cast<int>(i));
                const bool builtIn = baseList && core::IsInList(*baseList, list[i]);
                Line(common::ToString(list[i]));
                I::SameLine();
                I::BeginDisabled(builtIn);
                if (I::SmallButton(T(keys::FORM_REMOVE).c_str())) {
                    remove = list[i];
                }
                I::EndDisabled();
                if (builtIn && I::IsItemHovered(I::ImGuiHoveredFlags_AllowWhenDisabled)) {
                    I::SetTooltip("%s", T(keys::FORM_BUILT_IN).c_str());
                }
                I::PopID();
            }
            if (remove && core::RemoveRecord(list, *remove)) {
                g_work.dirty = true;
            }

            auto& buffer = g_work.inputs[std::string(id)];
            I::SetNextItemWidth(320.0f);
            I::InputTextWithHint("##input", T(keys::FORM_INPUT_HINT).c_str(), buffer.data(), buffer.size());
            I::SameLine();
            if (I::Button(T(keys::FORM_ADD).c_str())) {
                const auto form = common::ParseFormKey(buffer.data());
                if (!form) {
                    g_work.message = T(keys::FORM_BAD_FORMAT);
                } else if (core::IsInList(list, *form)) {
                    g_work.message = F(keys::FORM_DUPLICATE, { { "form", common::ToString(*form) } });
                } else {
                    ui::CheckForm(*form, wantEffect, target);
                    buffer.fill('\0');
                }
            }
            I::PopID();
        }

        void DrawPlayer(const PlayerView& p)
        {
            if (!I::CollapsingHeader(Label(T(keys::PLAYER_HEADER), "player").c_str(), I::ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }
            if (!p.loaded) {
                Wrapped(T(keys::PLAYER_NONE));
                return;
            }
            Line(F(keys::PLAYER_RACE, { { "race", p.raceName }, { "form", p.raceForm } }));
            using Kind = PlayerView::Kind;
            Line(p.kind == Kind::Race  ? F(keys::PLAYER_KIND_RACE, { { "id", p.raceId } }) :
                 p.kind == Kind::Beast ? T(keys::PLAYER_KIND_BEAST) :
                                         T(keys::PLAYER_KIND_NONE));
            Line(F(keys::PLAYER_SEX, { { "sex", SexText(p.sex) } }));
            Line(p.value ? F(keys::PLAYER_VALUE, { { "value", std::to_string(*p.value) } }) : T(keys::PLAYER_VALUE_NONE));
            Hint(T(keys::PLAYER_VALUE_HINT));

            // The carry weight as a sum the player can follow: base + adjustment - Survival penalty + the rest.
            I::Spacing();
            Line(T(keys::PLAYER_BREAKDOWN));
            I::Indent();
            Line(F(keys::PLAYER_BASE, { { "base", Number(p.base) } }));
            Line(F(keys::PLAYER_CONTRIBUTION, { { "contribution", Signed(p.contribution) } }));
            Hint(T(keys::PLAYER_CONTRIBUTION_HINT));
            if (p.gamePenalty > 0.0f) {
                Line(p.penaltyCancelled ?
                         F(keys::PLAYER_SURVIVAL, { { "penalty", Signed(-p.gamePenalty) },
                                                      { "own", std::to_string(g_work.edited.general.survivalPenalty) } }) :
                         F(keys::PLAYER_SURVIVAL_LEFT, { { "penalty", Signed(-p.gamePenalty) } }));
            }
            const float other = p.total - p.base - p.contribution + p.gamePenalty;
            if (other != 0.0f) {
                Line(F(keys::PLAYER_OTHER, { { "other", Signed(other) } }));
            }
            I::Unindent();
            Line(F(keys::PLAYER_TOTAL, { { "total", Number(p.total) } }));
        }

        void DrawFiles(const Snapshot& snapshot)
        {
            std::string files;
            for (const auto& file : snapshot.files) {
                files += (files.empty() ? "" : ", ") + file;
            }
            Line(F(keys::FILES, { { "files", files } }));
            if (snapshot.problems.empty()) {
                Line(T(keys::ERRORS_NONE));
                return;
            }
            I::TextColored(I::ImVec4{ 1.0f, 0.55f, 0.35f, 1.0f }, "%s", T(keys::ERRORS_HEADER).c_str());
            for (const auto& problem : snapshot.problems) {
                Wrapped(common::Format(problem));
            }
        }

        void DrawGeneral()
        {
            if (!I::CollapsingHeader(Label(T(keys::GENERAL_HEADER), "general").c_str(), I::ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }
            auto& g = g_work.edited.general;
            if (I::Checkbox(Label(T(keys::GENERAL_ENABLED), "enabled").c_str(), &g.enabled)) {
                g_work.dirty = true;
            }
            Hint(T(keys::GENERAL_ENABLED_HINT));
            IntField(Label(T(keys::GENERAL_MIN_TOTAL), "min_total"), g.minTotal, core::kMinTotalLow, core::kMinTotalHigh,
                F(keys::GENERAL_MIN_TOTAL_HINT, { { "low", std::to_string(core::kMinTotalLow) }, { "high", std::to_string(core::kMinTotalHigh) } }));
            IntField(Label(T(keys::GENERAL_SURVIVAL_PENALTY), "survival_penalty"), g.survivalPenalty, core::kSurvivalPenaltyLow,
                core::kSurvivalPenaltyHigh,
                F(keys::GENERAL_SURVIVAL_PENALTY_HINT,
                    { { "low", std::to_string(core::kSurvivalPenaltyLow) }, { "high", std::to_string(core::kSurvivalPenaltyHigh) } }));

            static constexpr std::array levels{
                std::pair{ common::LogLevel::Trace, keys::LOG_TRACE }, std::pair{ common::LogLevel::Debug, keys::LOG_DEBUG },
                std::pair{ common::LogLevel::Info, keys::LOG_INFO }, std::pair{ common::LogLevel::Warn, keys::LOG_WARN },
                std::pair{ common::LogLevel::Error, keys::LOG_ERROR },
            };
            std::string current;
            for (const auto& [level, key] : levels) {
                if (level == g.logLevel) {
                    current = T(key);
                }
            }
            I::SetNextItemWidth(200.0f);
            if (I::BeginCombo(Label(T(keys::GENERAL_LOG_LEVEL), "log_level").c_str(), current.c_str())) {
                for (const auto& [level, key] : levels) {
                    if (I::Selectable(Label(T(key), std::string(key)).c_str(), level == g.logLevel) && level != g.logLevel) {
                        g.logLevel = level;
                        g_work.dirty = true;
                    }
                }
                I::EndCombo();
            }
        }

        void DrawRaces(const PlayerView& player)
        {
            if (!I::CollapsingHeader(Label(T(keys::RACES_HEADER), "races").c_str(), I::ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }
            const auto hint = F(keys::RACES_VALUE_HINT, { { "low", std::to_string(core::kValueLow) }, { "high", std::to_string(core::kValueHigh) } });
            std::optional<std::string> revert;
            std::optional<std::string> remove;
            constexpr int kFlags = I::ImGuiTableFlags_RowBg | I::ImGuiTableFlags_Borders;
            if (I::BeginTable("races", 4, kFlags)) {
                // Fixed columns for the values, so that the field with its -/+ buttons has room for the number.
                I::TableSetupColumn(T(keys::RACES_COLUMN_RACE).c_str(), I::ImGuiTableColumnFlags_WidthStretch);
                I::TableSetupColumn(T(keys::RACES_COLUMN_MALE).c_str(), I::ImGuiTableColumnFlags_WidthFixed, 190.0f);
                I::TableSetupColumn(T(keys::RACES_COLUMN_FEMALE).c_str(), I::ImGuiTableColumnFlags_WidthFixed, 190.0f);
                I::TableSetupColumn("", I::ImGuiTableColumnFlags_WidthFixed);
                I::TableHeadersRow();
                for (auto& race : g_work.edited.races) {
                    const auto* known = core::FindRace(g_work.base, race.id);
                    const bool  edited = !known || known->male != race.male || known->female != race.female || known->forms != race.forms;
                    I::PushID(race.id.c_str());
                    I::TableNextRow();
                    I::TableNextColumn();
                    Line(RaceName(race));
                    if (edited) {
                        I::SameLine();
                        I::TextColored(I::ImVec4{ 1.0f, 0.8f, 0.3f, 1.0f }, "*");
                        Hint(T(keys::RACES_EDITED));
                    }
                    I::TableNextColumn();
                    IntField("##male", race.male, core::kValueLow, core::kValueHigh, hint, -1.0f);
                    I::TableNextColumn();
                    IntField("##female", race.female, core::kValueLow, core::kValueHigh, hint, -1.0f);
                    I::TableNextColumn();
                    if (known) {
                        I::BeginDisabled(!edited);
                        if (I::SmallButton(T(keys::RACES_REVERT).c_str())) {
                            revert = race.id;
                        }
                        I::EndDisabled();
                        if (I::IsItemHovered(I::ImGuiHoveredFlags_AllowWhenDisabled)) {
                            I::SetTooltip("%s", T(keys::RACES_REVERT_HINT).c_str());
                        }
                    } else if (I::SmallButton(T(keys::RACES_REMOVE).c_str())) {
                        remove = race.id;
                    }
                    I::PopID();
                }
                I::EndTable();
            }
            if (revert && core::RevertRace(g_work.edited, g_work.base, *revert)) {
                g_work.dirty = true;
            }
            if (remove && core::RemoveRace(g_work.edited, g_work.base, *remove)) {
                g_work.dirty = true;
            }

            // Records of the races the player added (those of the base are changed only in the file: section 13.3).
            for (auto& race : g_work.edited.races) {
                if (core::FindRace(g_work.base, race.id)) {
                    continue;
                }
                I::Spacing();
                Line(std::format("{}: {}", RaceName(race), T(keys::RACES_FORMS)));
                FormList("forms." + race.id, race.forms, nullptr, RaceTarget(race.id), false);
            }

            if (player.loaded && player.kind == PlayerView::Kind::None && player.raceKey) {
                I::Spacing();
                if (I::Button(T(keys::RACES_ADD_CURRENT).c_str())) {
                    core::AddRace(g_work.edited, player.raceEditorId, *player.raceKey, player.base);
                    g_work.dirty = true;
                }
            }
        }

        void DrawBeast(const PlayerView& player)
        {
            if (!I::CollapsingHeader(Label(T(keys::BEAST_HEADER), "beast").c_str())) {
                return;
            }
            Wrapped(T(keys::BEAST_HINT));
            FormList("beast", g_work.edited.beastForms, &g_work.base.beastForms, kTargetBeast, false);
            // Only for a race the configuration does not know: a race from the settings is not a beast form.
            if (player.loaded && player.kind == PlayerView::Kind::None && player.raceKey &&
                !core::IsInList(g_work.edited.beastForms, *player.raceKey)) {
                if (I::Button(T(keys::BEAST_ADD_CURRENT).c_str())) {
                    core::AddRecord(g_work.edited.beastForms, *player.raceKey);
                    g_work.dirty = true;
                }
            }
        }

        void DrawSurvival()
        {
            if (!I::CollapsingHeader(Label(T(keys::SURVIVAL_HEADER), "survival").c_str())) {
                return;
            }
            Wrapped(T(keys::SURVIVAL_HINT));
            FormList("survival", g_work.edited.survivalEffects, &g_work.base.survivalEffects, kTargetSurvival, true);
        }

        void SaveNow(bool overwrite)
        {
            ui::Save(core::ToUserToml(g_work.base, g_work.edited), g_work.readText, overwrite);
        }

        void DrawButtons(const Snapshot& snapshot)
        {
            I::Separator();
            I::BeginDisabled(!g_work.dirty);
            if (I::Button(T(keys::SAVE).c_str())) {
                if (!snapshot.userFile.parses) {
                    g_work.openBroken = true;
                } else {
                    SaveNow(false);
                }
            }
            I::SameLine();
            if (I::Button(T(keys::DISCARD).c_str())) {
                g_work.active = false;
                g_work.message.clear();
            }
            I::EndDisabled();
            I::SameLine();
            if (I::Button(T(keys::RELOAD).c_str())) {
                ui::Reload();
            }
            if (g_work.dirty) {
                I::TextColored(I::ImVec4{ 1.0f, 0.8f, 0.3f, 1.0f }, "%s", T(keys::UNSAVED).c_str());
            }
            if (!g_work.message.empty()) {
                Wrapped(g_work.message);
            }
        }

        void DrawPopups()
        {
            const auto changed = Label(T(keys::CHANGED_TITLE), "changed");
            const auto broken = Label(T(keys::BROKEN_TITLE), "broken");
            if (g_work.openChanged) {
                I::OpenPopup(changed.c_str());
                g_work.openChanged = false;
            }
            if (g_work.openBroken) {
                I::OpenPopup(broken.c_str());
                g_work.openBroken = false;
            }
            constexpr int kAutoResize = I::ImGuiWindowFlags_AlwaysAutoResize;
            if (I::BeginPopupModal(changed.c_str(), nullptr, kAutoResize)) {
                Line(T(keys::CHANGED_TEXT));
                if (I::Button(T(keys::CHANGED_OVERWRITE).c_str())) {
                    SaveNow(true);
                    I::CloseCurrentPopup();
                }
                I::SameLine();
                if (I::Button(T(keys::CHANGED_RELOAD).c_str())) {
                    ui::Reload();
                    I::CloseCurrentPopup();
                }
                I::SameLine();
                if (I::Button(T(keys::CANCEL).c_str())) {
                    I::CloseCurrentPopup();
                }
                I::EndPopup();
            }
            if (I::BeginPopupModal(broken.c_str(), nullptr, kAutoResize)) {
                I::PushTextWrapPos(520.0f);
                Wrapped(T(keys::BROKEN_TEXT));
                I::PopTextWrapPos();
                if (I::Button(T(keys::BROKEN_CLEAN).c_str())) {
                    ui::StartClean();
                    I::CloseCurrentPopup();
                }
                I::SameLine();
                if (I::Button(T(keys::CANCEL).c_str())) {
                    I::CloseCurrentPopup();
                }
                I::EndPopup();
            }
        }

        void Draw()
        {
            ui::RequestRefresh();
            const auto snapshot = ui::Get();
            if (snapshot.revision == 0) {
                return;  // the first refresh has not run yet
            }
            HandleAction(snapshot);
            if (!g_work.active || (!g_work.dirty && snapshot.configRevision != g_work.configRevision)) {
                TakeWorkingCopy(snapshot);
            }

            DrawPlayer(snapshot.player);
            DrawFiles(snapshot);
            if (!snapshot.userFile.parses) {
                I::TextColored(I::ImVec4{ 1.0f, 0.55f, 0.35f, 1.0f }, "%s", T(keys::BROKEN_TITLE).c_str());
                I::SameLine();
                if (I::SmallButton(T(keys::BROKEN_CLEAN).c_str())) {
                    g_work.openBroken = true;
                }
            }
            DrawGeneral();
            DrawRaces(snapshot.player);
            DrawBeast(snapshot.player);
            DrawSurvival();
            DrawButtons(snapshot);
            DrawPopups();

            for (const auto& key : lang::Get().TakeMissing()) {
                spdlog::warn("lang: no text for key {} in either table", key);
            }
        }

        void __stdcall Render()
        {
            try {
                Draw();
            } catch (const std::exception& error) {
                spdlog::error("menu: unexpected failure while drawing: {}", error.what());
            }
        }
    }

    void RegisterMenu()
    {
        if (!k7::skse::menu::Available()) {
            return;
        }
        SKSEMenuFramework::SetSection(T(keys::MENU_SECTION));
        SKSEMenuFramework::AddSectionItem(T(keys::MENU_SETTINGS), Render);
        spdlog::info("menu: section registered in SKSE Menu Framework");
    }
}
