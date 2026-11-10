#include "k7/common/lang.h"

#include <cctype>
#include <format>
#include <random>
#include <tuple>

#include <toml++/toml.hpp>

#include "k7/common/config_layers.h"
#include "k7/common/file_io.h"
#include "k7/common/toml_reader.h"

namespace k7::common::lang
{
    namespace
    {
        bool ReadVoices(const toml::node& value, const std::string& name, std::string_view display,
            Parsed& out, Diagnostics& diagnostics)
        {
            const auto* list = value.as_array();
            if (!list) {
                diagnostics.push_back(Diagnostic{ Severity::Error, std::string(display), LineOf(value),
                    std::format("{}: voice must be a list of texts", name) });
                return false;
            }
            std::vector<std::string> voices;
            for (const auto& item : *list) {
                const auto* text = item.as_string();
                if (!text) {
                    diagnostics.push_back(Diagnostic{ Severity::Error, std::string(display), LineOf(item),
                        std::format("{}: every voice entry must be a text", name) });
                    return false;
                }
                voices.push_back(text->get());
            }
            if (voices.empty()) {
                diagnostics.push_back(Diagnostic{ Severity::Error, std::string(display), LineOf(value),
                    std::format("{}: the voice list is empty", name) });
                return false;
            }
            out.voices[name] = std::move(voices);
            return true;
        }

        std::string_view StripDollar(std::string_view key)
        {
            return !key.empty() && key.front() == '$' ? key.substr(1) : key;
        }

        std::optional<Parsed> ReadTable(const std::filesystem::path& path, std::string_view display, Diagnostics& diagnostics,
            Severity missing)
        {
            const auto text = io::ReadFile(path);
            if (!text) {
                diagnostics.push_back(Diagnostic{ missing, std::string(display), 0, "not found" });
                return std::nullopt;
            }
            return ParseTables(*text, display, diagnostics);
        }
    }

    namespace
    {
        // Reads the phrases of a `text` value: a string, or an array of strings (several phrases of one key). An
        // empty array, or an element that is not a string, is an error with its line.
        bool ReadPhrases(const toml::node& value, const std::string& name, std::string_view display, Parsed& out,
            Diagnostics& diagnostics)
        {
            if (const auto* text = value.as_string()) {
                out.texts[name] = text->get();
                return true;
            }
            if (const auto* list = value.as_array()) {
                std::vector<std::string> phrases;
                for (const auto& element : *list) {
                    const auto* phrase = element.as_string();
                    if (!phrase) {
                        diagnostics.push_back(Diagnostic{ Severity::Error, std::string(display), LineOf(element),
                            std::format("{}: every phrase of the list must be a text \"...\"", name) });
                        continue;
                    }
                    phrases.push_back(phrase->get());
                }
                if (phrases.empty()) {
                    diagnostics.push_back(Diagnostic{ Severity::Error, std::string(display), LineOf(value),
                        std::format("{}: the list of phrases is empty", name) });
                    return false;
                }
                out.texts[name] = phrases.front();
                if (phrases.size() > 1) {
                    out.variants[name] = std::move(phrases);
                }
                return true;
            }
            diagnostics.push_back(Diagnostic{ Severity::Error, std::string(display), LineOf(value),
                std::format("{}: expected a text \"...\" or a list of texts", name) });
            return false;
        }
    }

    std::optional<Parsed> ParseTables(std::string_view text, std::string_view display, Diagnostics& diagnostics)
    {
        const auto parsed = config::Parse(std::string(text), std::string(display), diagnostics);
        if (!parsed) {
            return std::nullopt;
        }
        Parsed out;
        for (const auto& [key, node] : *parsed) {
            const std::string name(key.str());
            if (node.is_string() || node.is_array()) {
                ReadPhrases(node, name, display, out, diagnostics);
                continue;
            }
            if (const auto* full = node.as_table()) {
                bool any = false;
                if (const auto* value = full->get("text")) {
                    any = ReadPhrases(*value, name, display, out, diagnostics);
                }
                if (const auto* value = full->get("voice")) {
                    any = ReadVoices(*value, name, display, out, diagnostics) || any;
                }
                for (const auto* sex : { "male", "female" }) {
                    if (const auto* variant = full->get_as<toml::table>(sex)) {
                        if (const auto* value = variant->get("text")) {
                            any = ReadPhrases(*value, name + "." + sex, display, out, diagnostics) || any;
                        }
                        if (const auto* value = variant->get("voice")) {
                            any = ReadVoices(*value, name + "." + sex, display, out, diagnostics) || any;
                        }
                    }
                }
                if (!any && !full->contains("text")) {
                    diagnostics.push_back(Diagnostic{ Severity::Error, std::string(display), LineOf(node),
                        std::format("{}: the table has no text = \"...\"", name) });
                }
                continue;
            }
            diagnostics.push_back(Diagnostic{ Severity::Error, std::string(display), LineOf(node),
                std::format("{}: expected a text \"...\" or a table with text = \"...\"", name) });
        }
        for (const auto& [name, voices] : out.voices) {
            if (!out.texts.contains(name)) {
                out.texts[name] = "";
                if (voices.size() > 1) { out.variants[name] = std::vector<std::string>(voices.size()); }
            } else {
                const auto variants = out.variants.find(name);
                const auto count = variants == out.variants.end() ? 1 : variants->second.size();
                if (voices.size() > count) {
                    diagnostics.push_back(Diagnostic{ Severity::Warning, std::string(display), 0,
                        std::format("{}: extra voice entries are ignored", name) });
                }
            }
        }
        return out;
    }

    std::optional<Table> ParseTable(std::string_view text, std::string_view display, Diagnostics& diagnostics)
    {
        auto parsed = ParseTables(text, display, diagnostics);
        if (!parsed) {
            return std::nullopt;
        }
        return std::move(parsed->texts);
    }

    std::string NormalizeLanguage(std::string_view language)
    {
        std::string out;
        for (const char c : language) {
            if (!std::isspace(static_cast<unsigned char>(c))) {
                out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
        }
        return out.empty() ? std::string(kFallbackLanguage) : out;
    }

    void Localization::Load(const std::filesystem::path& dir, std::string_view language, Diagnostics& diagnostics)
    {
        language_ = NormalizeLanguage(language);
        primary_.clear();
        fallback_.clear();
        primaryVariants_.clear();
        fallbackVariants_.clear();
        primaryVoices_.clear();
        fallbackVoices_.clear();
        last_.clear();
        missing_.clear();
        reported_.clear();

        const auto folder = dir / kLangDir;
        const auto display = [](std::string_view name) { return std::format("{}/{}.toml", kLangDir, name); };
        if (auto english = ReadTable(folder / std::format("{}.toml", kFallbackLanguage), display(kFallbackLanguage), diagnostics,
                Severity::Error)) {
            fallback_         = std::move(english->texts);
            fallbackVariants_ = std::move(english->variants);
            fallbackVoices_ = std::move(english->voices);
        }
        if (language_ != kFallbackLanguage) {
            if (auto table = ReadTable(folder / std::format("{}.toml", language_), display(language_), diagnostics, Severity::Warning)) {
                primary_         = std::move(table->texts);
                primaryVariants_ = std::move(table->variants);
                primaryVoices_ = std::move(table->voices);
            }
        }
    }

    bool Localization::Has(std::string_view key) const
    {
        const std::string name(StripDollar(key));
        return primary_.contains(name) || fallback_.contains(name);
    }

    const std::string* Localization::Pick(const std::string& name, const Table& table, const Variants& variants,
        const char* which, std::size_t* selectedIndex) const
    {
        if (const auto list = variants.find(name); list != variants.end() && !list->second.empty()) {
            const auto  count = list->second.size();
            const auto  state = std::string(which) + ":" + name;
            std::size_t index = 0;
            if (count > 1) {
                static thread_local std::mt19937_64 engine{ std::random_device{}() };
                const auto random = [&](std::size_t n) {
                    return random_ ? random_(n) : static_cast<std::size_t>(engine() % n);
                };
                const auto previous = last_.find(state);
                if (previous == last_.end()) {
                    index = random(count);
                } else {
                    // One of the other phrases: draw among count - 1 and skip over the one shown last.
                    index = random(count - 1);
                    if (index >= previous->second) {
                        ++index;
                    }
                }
            }
            last_[state] = index;
            if (selectedIndex) { *selectedIndex = index; }
            return &list->second[index];
        }
        if (const auto it = table.find(name); it != table.end()) {
            if (selectedIndex) { *selectedIndex = 0; }
            return &it->second;
        }
        return nullptr;
    }

    Selected Localization::Select(std::string_view key, Gender gender) const
    {
        const std::string name(StripDollar(key));
        const auto variant = name + (gender == Gender::Female ? ".female" : ".male");
        const bool sexSpecific = primary_.contains(variant) || fallback_.contains(variant) ||
            primaryVoices_.contains(variant) || fallbackVoices_.contains(variant);
        const auto tables = { std::tuple{ &primary_, &primaryVariants_, &primaryVoices_, "p" },
            std::tuple{ &fallback_, &fallbackVariants_, &fallbackVoices_, "f" } };
        for (const auto& [table, variants, voices, which] : tables) {
            for (const auto* candidate : { &variant, &name }) {
                std::size_t index = 0;
                if (const auto* text = Pick(*candidate, *table, *variants, which, &index)) {
                    Selected selected{ *text, {} };
                    const auto voiceName = sexSpecific ? variant : *candidate;
                    if (const auto it = voices->find(voiceName); it != voices->end() && index < it->second.size()) {
                        selected.voice = it->second[index];
                    }
                    return selected;
                }
            }
        }
        return { Text(key), {} };
    }

    std::string Localization::Text(std::string_view key) const
    {
        const std::string name(StripDollar(key));
        if (const auto* text = Pick(name, primary_, primaryVariants_, "p")) {
            return *text;
        }
        if (const auto* text = Pick(name, fallback_, fallbackVariants_, "f")) {
            return *text;
        }
        if (!reported_.contains(name)) {
            missing_.insert(name);
        }
        return name;
    }

    std::string Localization::Text(std::string_view key, Gender gender) const
    {
        const std::string name(StripDollar(key));
        const std::string variant = name + (gender == Gender::Female ? ".female" : ".male");
        const auto        tables  = { std::tuple{ &primary_, &primaryVariants_, "p" }, std::tuple{ &fallback_, &fallbackVariants_, "f" } };
        for (const auto& [table, variants, which] : tables) {
            if (const auto* text = Pick(variant, *table, *variants, which)) {
                return *text;
            }
            if (const auto* text = Pick(name, *table, *variants, which)) {
                return *text;
            }
        }
        return Text(key);
    }

    std::string Localization::Format(std::string_view key, Gender gender,
        std::initializer_list<std::pair<std::string_view, std::string_view>> values) const
    {
        return Substitute(Text(key, gender), values);
    }

    std::string Localization::Format(std::string_view key,
        std::initializer_list<std::pair<std::string_view, std::string_view>> values) const
    {
        return Substitute(Text(key), values);
    }

    std::set<std::string> Localization::TakeMissing() const
    {
        auto taken = std::move(missing_);
        missing_.clear();
        reported_.insert(taken.begin(), taken.end());
        return taken;
    }

    std::string Substitute(std::string_view text, std::initializer_list<std::pair<std::string_view, std::string_view>> values)
    {
        std::string out;
        std::size_t pos = 0;
        while (pos < text.size()) {
            const auto open = text.find('{', pos);
            if (open == std::string_view::npos) {
                break;
            }
            const auto close = text.find('}', open + 1);
            if (close == std::string_view::npos) {
                break;
            }
            out.append(text.substr(pos, open - pos));
            const auto name = text.substr(open + 1, close - open - 1);
            bool       replaced = false;
            for (const auto& [n, v] : values) {
                if (n == name) {
                    out.append(v);
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                out.append(text.substr(open, close - open + 1));
            }
            pos = close + 1;
        }
        out.append(text.substr(pos));
        return out;
    }
}
