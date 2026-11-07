#include "core/apply.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <utility>
#include <vector>

#include "k7/common/toml_reader.h"

namespace k7::rscw::core
{
    namespace
    {
        std::string LowerAscii(std::string_view text)
        {
            std::string result(text);
            for (auto& c : result) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return result;
        }

        // An integer in [low, high]; outside the range it is an error and the value stays.
        void IntInRange(common::TomlReader& r, std::string_view key, int& out, int low, int high)
        {
            int value = out;
            if (!r.Int(key, value)) {
                return;
            }
            if (value < low || value > high) {
                r.Error(*r.Table().get(key), key, std::format("must be between {} and {}", low, high));
                return;
            }
            out = value;
        }

        // A list of forms "Plugin.esp|0x123456". Returns false if the key is absent or not an array; bad
        // elements are reported and left out.
        bool ReadForms(common::TomlReader& r, std::string_view key, std::vector<std::pair<common::FormKey, int>>& out)
        {
            const auto* array = r.Array(key);
            if (!array) {
                return false;
            }
            for (const auto& element : *array) {
                const auto line = common::LineOf(element);
                const auto* text = element.as_string();
                if (!text) {
                    r.Report(common::Severity::Error, line, std::format("{}: expected a form as \"Plugin.esp|0x123456\"", key));
                    continue;
                }
                const auto form = common::ParseFormKey(text->get());
                if (!form) {
                    r.Report(common::Severity::Error, line, std::format("{}: bad form '{}', expected \"Plugin.esp|0x123456\"", key, text->get()));
                    continue;
                }
                out.emplace_back(*form, line);
            }
            return true;
        }

        void Record(Sources* sources, std::string_view owner, const std::vector<std::pair<common::FormKey, int>>& forms,
            std::string_view file)
        {
            if (!sources) {
                return;
            }
            for (const auto& [form, line] : forms) {
                sources->forms[Sources::Key(owner, form)] = FormSource{ std::string(file), line, sources->nextOrder++ };
            }
        }

        void Forget(Sources* sources, std::string_view owner, const std::vector<common::FormKey>& forms)
        {
            if (!sources) {
                return;
            }
            for (const auto& form : forms) {
                sources->forms.erase(Sources::Key(owner, form));
            }
        }

        std::vector<common::FormKey> KeysOf(const std::vector<std::pair<common::FormKey, int>>& forms)
        {
            std::vector<common::FormKey> keys;
            for (const auto& [form, line] : forms) {
                if (std::ranges::none_of(keys, [&](const common::FormKey& k) { return common::Equivalent(k, form); })) {
                    keys.push_back(form);
                }
            }
            return keys;
        }

        void ApplyRace(Config& config, const toml::table& table, std::string_view id, std::string_view file,
            common::Diagnostics& diagnostics, Sources* sources)
        {
            const auto         path = std::format("races.{}", id);
            common::TomlReader r(table, file, path, diagnostics);

            auto* race = FindRace(config, id);
            if (!race) {
                config.races.push_back(RaceEntry{ std::string(id), {}, 0, 0 });
                race = &config.races.back();
            }
            IntInRange(r, "male", race->male, kValueLow, kValueHigh);
            IntInRange(r, "female", race->female, kValueLow, kValueHigh);
            std::vector<std::pair<common::FormKey, int>> forms;
            if (ReadForms(r, "forms", forms)) {
                Forget(sources, path, race->forms);
                race->forms = KeysOf(forms);
                Record(sources, path, forms, file);
            }
            r.ReportUnknown();
        }
    }

    std::string Sources::Key(std::string_view owner, const common::FormKey& form)
    {
        return std::format("{}|{}|0x{:06x}", owner, LowerAscii(form.plugin), form.id);
    }

    const FormSource* Sources::Find(std::string_view owner, const common::FormKey& form) const
    {
        const auto it = forms.find(Key(owner, form));
        return it == forms.end() ? nullptr : &it->second;
    }

    void ApplyLayer(Config& config, const toml::table& layer, std::string_view file, common::Diagnostics& diagnostics,
        Sources* sources)
    {
        common::TomlReader root(layer, file, "", diagnostics);

        if (const auto* meta = root.SubTable("meta")) {
            common::TomlReader r(*meta, file, "meta", diagnostics);
            // Read by the loader (migration) and by EnsureConfigFile (the stamp).
            r.Known("config_version");
            r.Known("plugin_version");
            r.ReportUnknown();
        }

        if (const auto* general = root.SubTable("general")) {
            common::TomlReader r(*general, file, "general", diagnostics);
            r.Bool("enabled", config.general.enabled);
            IntInRange(r, "min_total", config.general.minTotal, kMinTotalLow, kMinTotalHigh);
            r.Enum("log_level", config.general.logLevel, common::ParseLogLevel, "trace, debug, info, warn, error");
            IntInRange(r, "survival_penalty", config.general.survivalPenalty, kSurvivalPenaltyLow, kSurvivalPenaltyHigh);
            r.ReportUnknown();
        }

        if (const auto* races = root.SubTable("races")) {
            // TOML tables are unordered: new races are added in alphabetical order of their ids.
            std::vector<std::pair<std::string, const toml::node*>> entries;
            for (const auto& [key, node] : *races) {
                entries.emplace_back(std::string(key.str()), &node);
            }
            std::ranges::sort(entries, {}, &std::pair<std::string, const toml::node*>::first);
            for (const auto& [id, node] : entries) {
                if (const auto* table = node->as_table()) {
                    ApplyRace(config, *table, id, file, diagnostics, sources);
                } else {
                    diagnostics.push_back(common::Diagnostic{ common::Severity::Error, std::string(file), common::LineOf(*node),
                        std::format("races.{}: expected a table [races.{}]", id, id) });
                }
            }
        }

        if (const auto* beast = root.SubTable("beast")) {
            common::TomlReader r(*beast, file, "beast", diagnostics);
            std::vector<std::pair<common::FormKey, int>> forms;
            if (ReadForms(r, "forms", forms)) {
                Forget(sources, "beast", config.beastForms);
                config.beastForms = KeysOf(forms);
                Record(sources, "beast", forms, file);
            }
            r.ReportUnknown();
        }

        if (const auto* survival = root.SubTable("survival")) {
            common::TomlReader r(*survival, file, "survival", diagnostics);
            std::vector<std::pair<common::FormKey, int>> forms;
            if (ReadForms(r, "effects", forms)) {
                Forget(sources, "survival", config.survivalEffects);
                config.survivalEffects = KeysOf(forms);
                Record(sources, "survival", forms, file);
            }
            r.ReportUnknown();
        }

        root.ReportUnknown();
    }
}
