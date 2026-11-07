#include "pch.h"

#include "race_lookup.h"

#include <unordered_map>
#include <unordered_set>

#include "k7/common/form_key.h"
#include "k7/skse/form_lookup.h"

namespace k7::rscw::races
{
    namespace
    {
        // Copies of the entries, so that a later reload of the configuration cannot leave dangling pointers.
        std::unordered_map<const RE::TESRace*, core::RaceEntry> g_races;
        std::unordered_set<const RE::TESRace*>                  g_beasts;

        bool IsDawnguard(const common::FormKey& key)
        {
            return common::Equivalent(key, common::FormKey{ "Dawnguard.esm", key.id });
        }

        // The race a record names, or null with a log line saying why.
        const RE::TESRace* ResolveOne(RE::TESDataHandler& handler, const common::FormKey& key, const core::FormSource* source,
            std::string_view owner)
        {
            const auto where = source ? std::format("{}:{}: ", source->file, source->line) : "built-in: "s;
            const auto* form = k7::skse::LookupForm(handler, key);
            if (!form) {
                if (!handler.LookupModByName(key.plugin) && IsDawnguard(key)) {
                    spdlog::debug("races: {}{}: {} not loaded (Dawnguard is not installed)", where, owner, common::ToString(key));
                } else {
                    spdlog::warn("races: {}{}: {} is not loaded, ignored", where, owner, common::ToString(key));
                }
                return nullptr;
            }
            const auto* race = form->As<RE::TESRace>();
            if (!race) {
                spdlog::warn("races: {}{}: {} is not a race, ignored", where, owner, common::ToString(key));
            }
            return race;
        }
    }

    void Resolve(const core::Config& config, const core::Sources& sources, std::string_view reason)
    {
        g_races.clear();
        g_beasts.clear();
        auto* handler = RE::TESDataHandler::GetSingleton();
        if (!handler) {
            spdlog::error("races ({}): no data handler, nothing resolved", reason);
            return;
        }

        std::size_t records = 0;
        std::size_t found = 0;
        for (const auto& race : config.races) {
            const auto owner = std::format("races.{}", race.id);
            for (const auto& key : race.forms) {
                ++records;
                if (const auto* resolved = ResolveOne(*handler, key, sources.Find(owner, key), owner)) {
                    ++found;
                    g_races.insert_or_assign(resolved, race);
                    spdlog::debug("races: {} {} -> {} (base carry weight {})", owner, common::ToString(key),
                        resolved->GetFormEditorID(), resolved->data.baseCarryWeight);
                }
            }
        }
        std::size_t beastFound = 0;
        for (const auto& key : config.beastForms) {
            if (const auto* resolved = ResolveOne(*handler, key, sources.Find("beast", key), "beast")) {
                ++beastFound;
                g_beasts.insert(resolved);
                spdlog::debug("races: beast {} (base carry weight {})", common::ToString(key), resolved->data.baseCarryWeight);
            }
        }
        spdlog::info("races ({}): {} of {} race records and {} of {} beast forms found; {} configured races", reason,
            found, records, beastFound, config.beastForms.size(), config.races.size());
    }

    const core::RaceEntry* Find(const RE::TESRace* race) noexcept
    {
        const auto it = g_races.find(race);
        return it == g_races.end() ? nullptr : &it->second;
    }

    bool IsBeast(const RE::TESRace* race) noexcept
    {
        return g_beasts.contains(race);
    }

    std::string Describe(const RE::TESRace* race)
    {
        if (IsBeast(race)) {
            return "beast form";
        }
        const auto* entry = Find(race);
        return entry ? std::format("races.{}", entry->id) : "not in configuration"s;
    }
}
