#include "pch.h"

#include "carry_service.h"

#include "config_service.h"
#include "core/recount.h"
#include "k7/skse/form_lookup.h"
#include "race_lookup.h"
#include "survival.h"

namespace k7::rscw::carry
{
    namespace
    {
        constexpr std::uint32_t kUniqueId = 'K7RS';
        constexpr std::uint32_t kRecordType = 'APPL';
        constexpr std::uint32_t kRecordVersion = 1;

        // What the plugin has put into the permanent CarryWeight modifier of the current game, and for the log
        // the race and sex it was counted for (specification, section 8).
        float         g_applied = 0.0f;
        std::string   g_appliedRace;
        std::uint32_t g_appliedSex = 0;  // 0 male, 1 female, 2 unknown

        // The Survival Mode penalty seen by the last recount (specification, section 3a); polling compares with it.
        // Not in the co-save: every load recounts with the penalty on the player at that moment.
        float g_lastGamePenalty = 0.0f;

        std::string RecordName(std::uint32_t a_type)
        {
            std::string name(4, ' ');
            for (int i = 0; i < 4; ++i) {
                name[i] = static_cast<char>((a_type >> (8 * (3 - i))) & 0xFF);
            }
            return name;
        }

        const char* SexText(core::Sex a_sex)
        {
            return a_sex == core::Sex::Male ? "male" : a_sex == core::Sex::Female ? "female" : "unknown";
        }

        void OnRevert(SKSE::SerializationInterface*)
        {
            spdlog::debug("co-save: revert, applied contribution {} reset to 0", g_applied);
            g_applied = 0.0f;
            g_appliedRace.clear();
            g_appliedSex = 2;
        }

        void OnSave(SKSE::SerializationInterface* a_intfc)
        {
            const auto length = static_cast<std::uint32_t>(g_appliedRace.size());
            if (!a_intfc->OpenRecord(kRecordType, kRecordVersion) || !a_intfc->WriteRecordData(g_applied) ||
                !a_intfc->WriteRecordData(g_appliedSex) || !a_intfc->WriteRecordData(length) ||
                (length && !a_intfc->WriteRecordData(g_appliedRace.data(), length))) {
                spdlog::error("co-save: could not write record {}", RecordName(kRecordType));
                return;
            }
            spdlog::debug("co-save: saved applied contribution {} ({}, sex {})", g_applied, g_appliedRace, g_appliedSex);
        }

        void OnLoad(SKSE::SerializationInterface* a_intfc)
        {
            std::uint32_t type = 0;
            std::uint32_t version = 0;
            std::uint32_t length = 0;
            bool          found = false;
            while (a_intfc->GetNextRecordInfo(type, version, length)) {
                if (type != kRecordType || version != kRecordVersion) {
                    spdlog::warn("co-save: skipped record {} version {} ({} bytes)", RecordName(type), version, length);
                    continue;
                }
                float         applied = 0.0f;
                std::uint32_t sex = 2;
                std::uint32_t size = 0;
                std::string   race;
                bool          ok = a_intfc->ReadRecordData(applied) == sizeof(applied) && a_intfc->ReadRecordData(sex) == sizeof(sex) &&
                          a_intfc->ReadRecordData(size) == sizeof(size) && size <= 512;
                if (ok && size) {
                    race.resize(size);
                    ok = a_intfc->ReadRecordData(race.data(), size) == size;
                }
                if (!ok) {
                    spdlog::error("co-save: record {} is damaged, applied contribution taken as 0", RecordName(type));
                    continue;
                }
                g_applied = applied;
                g_appliedSex = sex;
                g_appliedRace = std::move(race);
                found = true;
            }
            if (found) {
                spdlog::info("co-save: applied contribution {} (counted for {}, sex {})", g_applied,
                    g_appliedRace.empty() ? "?"s : g_appliedRace, g_appliedSex == 0 ? "male" : g_appliedSex == 1 ? "female" : "unknown");
            } else {
                spdlog::info("co-save: no record of this plugin in the save, applied contribution 0");
            }
        }

        void RecountTask(std::string a_reason)
        {
            SKSE::GetTaskInterface()->AddTask([reason = std::move(a_reason)] { Recount(reason); });
        }

        class RaceSexMenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(
                const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                // Sex can be changed only in this menu (specification, section 7).
                if (a_event && !a_event->opening && a_event->menuName == RE::RaceSexMenu::MENU_NAME) {
                    RecountTask("RaceSex Menu closed");
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };
        RaceSexMenuSink g_raceSexMenuSink;

        class RaceSwitchSink final : public RE::BSTEventSink<RE::TESSwitchRaceCompleteEvent>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESSwitchRaceCompleteEvent* a_event, RE::BSTEventSource<RE::TESSwitchRaceCompleteEvent>*) override
            {
                if (a_event && a_event->subject && a_event->subject.get() == RE::PlayerCharacter::GetSingleton()) {
                    RecountTask("race switch");
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };
        RaceSwitchSink g_raceSwitchSink;

        void RecountNow(std::string_view a_reason)
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            const auto* race = player ? player->GetRace() : nullptr;
            const auto* base = player ? player->GetActorBase() : nullptr;
            if (!player || !race || !base) {
                spdlog::warn("recount ({}): the player or the race is not available yet, nothing changed", a_reason);
                return;
            }

            const auto& config = config::Get();
            const auto  sex = base->GetSex();
            core::RecountInput input;
            input.race = races::Find(race);
            input.beast = races::IsBeast(race);
            input.sex = sex == RE::SEX::kMale ? core::Sex::Male : sex == RE::SEX::kFemale ? core::Sex::Female : core::Sex::Unknown;
            input.raceBase = race->data.baseCarryWeight;
            input.applied = g_applied;
            input.enabled = config.general.enabled;
            input.minTotal = config.general.minTotal;
            input.gamePenalty = survival::GamePenalty();
            input.survivalPenalty = config.general.survivalPenalty;
            g_lastGamePenalty = input.gamePenalty;
            const auto result = core::Recount(input);

            const auto raceText = std::format("{} [{}]", k7::skse::FormText(*race), races::Describe(race));
            auto*      values = player->AsActorValueOwner();
            if (result.kind == core::RecountKind::SkipBeast) {
                spdlog::info("recount ({}): race {}, no recount, contribution {} kept", a_reason, raceText, g_applied);
                return;
            }
            if (result.kind == core::RecountKind::SkipUnknown) {
                spdlog::warn("recount ({}): race {}, the sex cannot be read, nothing changed", a_reason, raceText);
                return;
            }

            if (result.delta != 0.0f) {
                values->ModActorValue(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kCarryWeight, result.delta);
            }
            const auto before = g_applied;
            g_applied = result.contribution;
            g_appliedRace = k7::skse::FormText(*race);
            g_appliedSex = input.sex == core::Sex::Male ? 0 : input.sex == core::Sex::Female ? 1 : 2;

            // The penalty is cancelled only where the plugin applies a value (section 3a).
            const auto survivalText = input.gamePenalty <= 0.0f ? ""s :
                                      result.value ? std::format(", survival penalty {} cancelled, own {}", input.gamePenalty, input.survivalPenalty) :
                                                     std::format(", survival penalty {} left as it is", input.gamePenalty);
            const auto line = std::format("recount ({}): race {}, sex {}, base {}, value {}, enabled {}{}, contribution {} -> {}, delta {}; "
                                          "carry weight {} (base {}, permanent modifier {})",
                a_reason, raceText, SexText(input.sex), input.raceBase, result.value ? std::to_string(*result.value) : "none"s,
                input.enabled, survivalText, before, result.contribution, result.delta, values->GetActorValue(RE::ActorValue::kCarryWeight),
                values->GetBaseActorValue(RE::ActorValue::kCarryWeight),
                player->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kPermanent, RE::ActorValue::kCarryWeight));
            if (result.delta != 0.0f || a_reason == "save loaded" || a_reason == "survival penalty") {
                spdlog::info("{}", line);
            } else {
                spdlog::debug("{}", line);
            }

            // Specification, section 8: a contribution applied twice after the DLL was removed and put back
            // shows as a carry weight below what the plugin ever allows.
            if (result.delta != 0.0f && values->GetActorValue(RE::ActorValue::kCarryWeight) < static_cast<float>(config.general.minTotal)) {
                spdlog::warn("recount ({}): carry weight {} is below min_total {}. If the plugin was removed without "
                             "enabled = false and later installed again, its contribution may be in the save twice; "
                             "see the removal instructions in the README",
                    a_reason, values->GetActorValue(RE::ActorValue::kCarryWeight), config.general.minTotal);
            }
        }
    }

    void RegisterSerialization()
    {
        const auto* serialization = SKSE::GetSerializationInterface();
        serialization->SetUniqueID(kUniqueId);
        serialization->SetRevertCallback(OnRevert);
        serialization->SetSaveCallback(OnSave);
        serialization->SetLoadCallback(OnLoad);
    }

    void RegisterEvents()
    {
        RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&g_raceSexMenuSink);
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESSwitchRaceCompleteEvent>(&g_raceSwitchSink);
    }

    void OnNewGame()
    {
        g_applied = 0.0f;
        g_appliedRace.clear();
        g_appliedSex = 2;
        g_lastGamePenalty = 0.0f;
        spdlog::info("new game: applied contribution 0, the first recount comes when the character creation menu closes");
    }

    float Applied() noexcept
    {
        return g_applied;
    }

    float LastGamePenalty() noexcept
    {
        return g_lastGamePenalty;
    }

    void Recount(std::string_view a_reason)
    {
        try {
            RecountNow(a_reason);
        } catch (const std::exception& error) {
            // Specification, section 10: a failure must not crash the game; carry weight stays as it was.
            spdlog::error("recount ({}): unexpected failure, nothing changed: {}", a_reason, error.what());
        }
    }
}
