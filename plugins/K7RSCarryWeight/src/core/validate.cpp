#include "core/validate.h"

#include <algorithm>
#include <format>
#include <string>

namespace k7::rscw::core
{
    namespace
    {
        void Warn(common::Diagnostics& diagnostics, const FormSource* where, std::string message)
        {
            diagnostics.push_back(where ? common::Diagnostic{ common::Severity::Warning, where->file, where->line, std::move(message) } :
                                          common::Diagnostic{ common::Severity::Warning, std::string(common::kMergedConfig), 0, std::move(message) });
        }

        std::string Owner(const RaceEntry& race)
        {
            return std::format("races.{}", race.id);
        }

        int OrderOf(const Sources& sources, const RaceEntry& race, const common::FormKey& form)
        {
            const auto* source = sources.Find(Owner(race), form);
            return source ? source->order : 0;
        }
    }

    void ValidateConfig(Config& config, const Sources& sources, common::Diagnostics& diagnostics)
    {
        // Incomplete races: only a race a file added can be incomplete, the built-in ones have every field.
        std::erase_if(config.races, [&](const RaceEntry& race) {
            std::string missing;
            if (race.forms.empty()) {
                missing += " forms";
            }
            if (race.male == 0) {
                missing += " male";
            }
            if (race.female == 0) {
                missing += " female";
            }
            if (missing.empty()) {
                return false;
            }
            diagnostics.push_back(common::Diagnostic{ common::Severity::Warning, std::string(common::kMergedConfig), 0,
                std::format("races.{}: missing{}, the race is ignored", race.id, missing) });
            return true;
        });

        // A beast form is never a race of the configuration.
        for (auto& race : config.races) {
            std::erase_if(race.forms, [&](const common::FormKey& form) {
                const bool beast = std::ranges::any_of(config.beastForms, [&](const common::FormKey& b) { return common::Equivalent(b, form); });
                if (beast) {
                    Warn(diagnostics, sources.Find(Owner(race), form),
                        std::format("races.{}: {} is also a beast form, it is treated as a beast form only", race.id, common::ToString(form)));
                }
                return beast;
            });
        }

        // A record named by two races stays with the one that named it last.
        for (std::size_t i = 0; i < config.races.size(); ++i) {
            for (const auto& form : std::vector(config.races[i].forms)) {
                std::size_t winner = i;
                for (std::size_t j = 0; j < config.races.size(); ++j) {
                    if (j == winner || std::ranges::none_of(config.races[j].forms, [&](const common::FormKey& f) { return common::Equivalent(f, form); })) {
                        continue;
                    }
                    const auto a = OrderOf(sources, config.races[winner], form);
                    const auto b = OrderOf(sources, config.races[j], form);
                    if (b > a || (b == a && j > winner)) {
                        winner = j;
                    }
                }
                for (std::size_t j = 0; j < config.races.size(); ++j) {
                    if (j == winner) {
                        continue;
                    }
                    auto& forms = config.races[j].forms;
                    const auto before = forms.size();
                    std::erase_if(forms, [&](const common::FormKey& f) { return common::Equivalent(f, form); });
                    if (forms.size() != before) {
                        Warn(diagnostics, sources.Find(Owner(config.races[winner]), form),
                            std::format("{} is named by races.{} and races.{}; the later one, races.{}, keeps it",
                                common::ToString(form), config.races[j].id, config.races[winner].id, config.races[winner].id));
                    }
                }
            }
        }
    }
}
