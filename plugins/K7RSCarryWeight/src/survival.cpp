#include "pch.h"

#include "survival.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <unordered_set>

#include "carry_service.h"
#include "k7/common/form_key.h"
#include "k7/skse/form_lookup.h"

namespace k7::rscw::survival
{
    namespace
    {
        // Touched on the main thread only (SKSE messages and tasks).
        std::unordered_set<const RE::EffectSetting*> g_effects;

        std::atomic_bool g_anyEffect{ false };   // at least one effect of the list is in the game
        std::atomic_bool g_gameLoaded{ false };  // a save was loaded or a new game started
        std::atomic_bool g_checkQueued{ false }; // a check task is waiting, do not queue another

        void Check()
        {
            g_checkQueued = false;
            auto* ui = RE::UI::GetSingleton();
            if (!ui || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) || ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) {
                return;
            }
            if (!RE::PlayerCharacter::GetSingleton()) {
                return;
            }
            if (GamePenalty() != carry::LastGamePenalty()) {
                carry::Recount("survival penalty");
            }
        }
    }

    void Resolve(const core::Config& config, const core::Sources& sources, std::string_view reason)
    {
        g_effects.clear();
        auto* handler = RE::TESDataHandler::GetSingleton();
        if (!handler) {
            spdlog::error("survival ({}): no data handler, nothing resolved", reason);
            g_anyEffect = false;
            return;
        }
        for (const auto& key : config.survivalEffects) {
            const auto* source = sources.Find("survival", key);
            const auto  where = source ? std::format("{}:{}: ", source->file, source->line) : "built-in: "s;
            const auto* form = k7::skse::LookupForm(*handler, key);
            if (!form) {
                if (!handler->LookupModByName(key.plugin) && !handler->LookupLoadedLightModByName(key.plugin)) {
                    spdlog::debug("survival: {}{} not loaded ({} is not installed)", where, common::ToString(key), key.plugin);
                } else {
                    spdlog::warn("survival: {}{} is not loaded, ignored", where, common::ToString(key));
                }
                continue;
            }
            const auto* effect = form->As<RE::EffectSetting>();
            if (!effect) {
                spdlog::warn("survival: {}{} is not a magic effect, ignored", where, common::ToString(key));
                continue;
            }
            g_effects.insert(effect);
            spdlog::debug("survival: penalty effect {} {}", common::ToString(key), effect->GetFormEditorID());
        }
        g_anyEffect = !g_effects.empty();
        spdlog::info("survival ({}): {} of {} penalty effects found{}", reason, g_effects.size(), config.survivalEffects.size(),
            g_effects.empty() ? ", no polling" : "");
    }

    float GamePenalty()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* target = player ? player->AsMagicTarget() : nullptr;
        auto* list = target && !g_effects.empty() ? target->GetActiveEffectList() : nullptr;
        if (!list) {
            return 0.0f;
        }
        float sum = 0.0f;
        for (const auto* active : *list) {
            if (!active || active->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled)) {
                continue;
            }
            if (g_effects.contains(active->GetBaseObject())) {
                // The game keeps a detrimental magnitude negative (-100 for Survival Mode Improved): the penalty is
                // what it takes off.
                sum -= active->magnitude;
            }
        }
        return sum > 0.0f ? sum : 0.0f;
    }

    void StartPolling()
    {
        std::thread([] {
            for (;;) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (!g_anyEffect || !g_gameLoaded || g_checkQueued.exchange(true)) {
                    continue;
                }
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([] { Check(); });
                } else {
                    g_checkQueued = false;
                }
            }
        }).detach();
    }

    void OnGameLoaded()
    {
        g_gameLoaded = true;
    }
}
