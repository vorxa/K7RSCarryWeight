#include "core/serialize.h"

#include <algorithm>
#include <format>

namespace k7::rscw::core
{
    std::string QuoteToml(std::string_view text)
    {
        std::string out = "\"";
        for (const char c : text) {
            if (c == '"' || c == '\\') {
                out += '\\';
            }
            out += c;
        }
        return out + '"';
    }

    std::string FormsToToml(const std::vector<common::FormKey>& forms)
    {
        std::string out = "[";
        for (std::size_t i = 0; i < forms.size(); ++i) {
            out += (i ? ", " : "") + QuoteToml(common::ToString(forms[i]));
        }
        return out + "]";
    }

    std::string TomlKey(std::string_view key)
    {
        const bool bare = !key.empty() && std::ranges::all_of(key, [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        });
        return bare ? std::string(key) : QuoteToml(key);
    }

    namespace
    {
        std::string Quote(std::string_view text)
        {
            return QuoteToml(text);
        }

        std::string Forms(const std::vector<common::FormKey>& forms)
        {
            return FormsToToml(forms);
        }
    }

    std::string ToConfigToml(const Config& config, std::string_view pluginVersion)
    {
        std::string out;
        const auto  line = [&](std::string_view text) {
            out += text;
            out += '\n';
        };

        line("# K7 - Race&Sex Carry Weight: the built-in settings, as text.");
        line("# This file is rewritten whenever the plugin is updated: put your changes into config.user.toml");
        line("# in the same folder. A value there replaces the one here; give only what you change, e.g.");
        line("#   [races.nord]");
        line("#   male = 120");
        line("# Changes take effect after the game is restarted or a save is loaded.");
        line("");
        line("[meta]");
        out += std::format("config_version = {}  # schema of this file, do not change\n", config.configVersion);
        out += std::format("plugin_version = {}  # the plugin version that wrote this file\n", Quote(pluginVersion));
        line("");

        line("[general]");
        out += std::format("enabled   = {}  # false: the plugin takes back its contribution when a save is loaded (before removing it)\n",
            config.general.enabled ? "true" : "false");
        out += std::format("min_total = {}  # the lowest base carry weight a race can get, {} to {}\n", config.general.minTotal,
            kMinTotalLow, kMinTotalHigh);
        out += std::format("log_level = {}  # trace | debug | info | warn | error\n", Quote(common::ToString(config.general.logLevel)));
        out += std::format("survival_penalty = {}  # while the Survival Mode carry weight penalty is on, the plugin cancels it and\n",
            config.general.survivalPenalty);
        out += std::format("                      # takes this many points off the value instead, {} to {}\n", kSurvivalPenaltyLow,
            kSurvivalPenaltyHigh);
        line("");

        line("# Base carry weight of a man and a woman of each race, whole numbers from 1 to 1000. It replaces the");
        line("# base of the race (300 for every vanilla race); stamina bonuses, items, spells and other mods add to it.");
        line("# forms: the race records that count as this race, \"Plugin.esp|0x123456\" (local form id). The vanilla");
        line("# races list their ordinary and vampire record. A race of a mod can be added to a race here or get");
        line("# its own [races.<name>] in config.user.toml; a race that is not listed anywhere is left alone.");
        for (const auto& race : config.races) {
            line("");
            out += std::format("[races.{}]\n", TomlKey(race.id));
            out += std::format("forms  = {}\n", Forms(race.forms));
            out += std::format("male   = {}\n", race.male);
            out += std::format("female = {}\n", race.female);
        }
        line("");

        line("# Beast forms (werewolf, vampire lord): while the player has one, the plugin does not recount and keeps");
        line("# what it applied before the transformation. Add the transformations of mods here.");
        line("[beast]");
        out += std::format("forms = {}\n", Forms(config.beastForms));
        line("");

        line("# Magic effects of the Survival Mode carry weight penalty (Survival_AbLowerCarryWeight). While the player");
        line("# has one of them, the plugin cancels its amount and applies survival_penalty instead. Add the penalty");
        line("# effects of other survival mods here.");
        line("[survival]");
        out += std::format("effects = {}\n", Forms(config.survivalEffects));
        return out;
    }
}
