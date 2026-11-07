#include "pch.h"

#include "ui/state.h"

#include <atomic>
#include <chrono>
#include <mutex>

#include "carry_service.h"
#include "config_service.h"
#include "k7/skse/form_lookup.h"
#include "race_lookup.h"
#include "reload.h"
#include "survival.h"

namespace k7::rscw::ui
{
    namespace
    {
        std::mutex       g_mutex;
        Snapshot         g_snapshot;          // guarded by g_mutex
        std::atomic_bool g_refreshQueued{ false };
        std::atomic<std::int64_t> g_lastRefreshMs{ 0 };

        // Main thread only.
        bool          g_backupDone = false;    // config.user.toml.bak made in this game session
        std::uint64_t g_actionId = 0;
        std::uint64_t g_configRevision = 1;

        std::int64_t NowMs()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }

        void Queue(std::function<void()> task)
        {
            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([task = std::move(task)] {
                    try {
                        task();
                    } catch (const std::exception& error) {
                        spdlog::error("menu: unexpected failure in a game task: {}", error.what());
                    }
                });
            }
        }

        PlayerView ReadPlayer()
        {
            PlayerView view;
            auto*      ui = RE::UI::GetSingleton();
            auto*      player = RE::PlayerCharacter::GetSingleton();
            if (!ui || !player || ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || !player->Is3DLoaded()) {
                return view;
            }
            const auto* race = player->GetRace();
            const auto* npc = player->GetActorBase();
            if (!race || !npc) {
                return view;
            }
            view.loaded = true;
            view.raceName = race->GetName();
            view.raceForm = k7::skse::FormText(*race);
            view.raceEditorId = race->GetFormEditorID();
            view.raceKey = k7::skse::FormKeyOf(*race);
            const auto* entry = races::Find(race);
            view.kind = races::IsBeast(race) ? PlayerView::Kind::Beast : entry ? PlayerView::Kind::Race : PlayerView::Kind::None;
            if (entry) {
                view.raceId = entry->id;
            }
            const auto sex = npc->GetSex();
            view.sex = sex == RE::SEX::kMale ? core::Sex::Male : sex == RE::SEX::kFemale ? core::Sex::Female : core::Sex::Unknown;
            view.base = race->data.baseCarryWeight;

            // The value the plugin counts with now: the same recount as the game's, without applying it.
            const auto&        config = config::Get();
            core::RecountInput input;
            input.race = entry;
            input.beast = view.kind == PlayerView::Kind::Beast;
            input.sex = view.sex;
            input.raceBase = view.base;
            input.applied = carry::Applied();
            input.enabled = config.general.enabled;
            input.minTotal = config.general.minTotal;
            input.gamePenalty = survival::GamePenalty();
            input.survivalPenalty = config.general.survivalPenalty;
            const auto result = core::Recount(input);
            view.value = result.value;
            view.gamePenalty = input.gamePenalty;
            view.penaltyCancelled = input.gamePenalty > 0.0f && result.value.has_value() && !input.beast;

            auto* values = player->AsActorValueOwner();
            view.contribution = carry::Applied();
            view.modifier = player->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kCarryWeight);
            view.total = values->GetActorValue(RE::ActorValue::kCarryWeight);
            return view;
        }

        // Main thread only.
        void Refresh()
        {
            Snapshot next;
            next.player = ReadPlayer();
            next.base = config::GetBase();
            next.config = config::Get();
            next.layered = config::GetLayered();
            next.problems = config::GetDiagnostics();
            next.files = config::GetFiles();
            next.userFile = core::ReadUserFile(config::Dir());
            next.configRevision = g_configRevision;

            std::scoped_lock lock(g_mutex);
            next.revision = g_snapshot.revision + 1;
            next.lastAction = std::move(g_snapshot.lastAction);
            g_snapshot = std::move(next);
        }

        void Publish(ActionResult result)
        {
            result.id = ++g_actionId;
            {
                std::scoped_lock lock(g_mutex);
                g_snapshot.lastAction = std::move(result);
            }
            Refresh();
        }

        void ReloadAndRecount(std::string_view reason)
        {
            rscw::Reload(reason);
            ++g_configRevision;
            // In the main menu no game is loaded: the configuration is read and the menu is written, nothing is counted
            // (specification, section 13.4). The recount comes with the next load of a save.
            auto* ui = RE::UI::GetSingleton();
            if (ui && !ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) {
                carry::Recount(reason);
            }
        }

        std::string Reasons(const common::Diagnostics& diagnostics)
        {
            std::string text;
            for (const auto& diagnostic : diagnostics) {
                text += (text.empty() ? "" : "; ") + diagnostic.message;
            }
            return text;
        }
    }

    Snapshot Get()
    {
        std::scoped_lock lock(g_mutex);
        return g_snapshot;
    }

    void RequestRefresh()
    {
        const auto now = NowMs();
        if (now - g_lastRefreshMs.load() < 500 || g_refreshQueued.exchange(true)) {
            return;
        }
        g_lastRefreshMs = now;
        Queue([] {
            g_refreshQueued = false;
            Refresh();
        });
    }

    void Save(std::string text, std::optional<std::string> readText, bool overwrite)
    {
        Queue([text = std::move(text), readText = std::move(readText), overwrite] {
            common::Diagnostics diagnostics;
            const auto          written = core::WriteUserFile(config::Dir(), text, readText, overwrite, g_backupDone, diagnostics);
            ActionResult        result;
            switch (written) {
            case core::WriteUserResult::Written:
                spdlog::info("menu: config.user.toml written");
                ReloadAndRecount("menu");
                result.kind = ActionResult::Kind::Saved;
                break;
            case core::WriteUserResult::Changed:
                spdlog::info("menu: config.user.toml changed on disk since the menu read it, not written");
                result.kind = ActionResult::Kind::SaveChanged;
                break;
            case core::WriteUserResult::Broken:
                spdlog::warn("menu: config.user.toml does not parse, not written over");
                result.kind = ActionResult::Kind::SaveBroken;
                break;
            case core::WriteUserResult::Failed:
                result.kind = ActionResult::Kind::SaveFailed;
                result.reason = Reasons(diagnostics);
                spdlog::error("menu: config.user.toml not written: {}", result.reason);
                break;
            }
            Publish(std::move(result));
        });
    }

    void Reload()
    {
        Queue([] {
            ReloadAndRecount("menu");
            ActionResult result;
            result.kind = ActionResult::Kind::Reloaded;
            Publish(std::move(result));
        });
    }

    void StartClean()
    {
        Queue([] {
            common::Diagnostics diagnostics;
            ActionResult        result;
            if (core::StartCleanUserFile(config::Dir(), diagnostics)) {
                spdlog::info("menu: config.user.toml kept as config.user.toml.bak, a clean file started");
                ReloadAndRecount("menu");
                result.kind = ActionResult::Kind::CleanStarted;
            } else {
                result.kind = ActionResult::Kind::CleanFailed;
                result.reason = Reasons(diagnostics);
                spdlog::error("menu: could not start a clean config.user.toml: {}", result.reason);
            }
            Publish(std::move(result));
        });
    }

    void CheckForm(common::FormKey form, bool wantEffect, int target)
    {
        Queue([form = std::move(form), wantEffect, target] {
            ActionResult result;
            result.kind = ActionResult::Kind::FormChecked;
            result.form = form;
            result.formTarget = target;
            auto*       handler = RE::TESDataHandler::GetSingleton();
            const auto* found = handler ? k7::skse::LookupForm(*handler, form) : nullptr;
            if (!found) {
                result.formCheck = ActionResult::FormCheck::NotFound;
            } else if (wantEffect && !found->Is(RE::FormType::MagicEffect)) {
                result.formCheck = ActionResult::FormCheck::NotEffect;
            } else if (!wantEffect && !found->Is(RE::FormType::Race)) {
                result.formCheck = ActionResult::FormCheck::NotRace;
            }
            Publish(std::move(result));
        });
    }

    void Init()
    {
        Queue([] { Refresh(); });
    }
}
