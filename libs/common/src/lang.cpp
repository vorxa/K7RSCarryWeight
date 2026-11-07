#include "k7/common/lang.h"

#include <cctype>
#include <format>

#include <toml++/toml.hpp>

#include "k7/common/config_layers.h"
#include "k7/common/file_io.h"
#include "k7/common/toml_reader.h"

namespace k7::common::lang
{
    namespace
    {
        std::string_view StripDollar(std::string_view key)
        {
            return !key.empty() && key.front() == '$' ? key.substr(1) : key;
        }

        std::optional<Table> ReadTable(const std::filesystem::path& path, std::string_view display, Diagnostics& diagnostics,
            Severity missing)
        {
            const auto text = io::ReadFile(path);
            if (!text) {
                diagnostics.push_back(Diagnostic{ missing, std::string(display), 0, "not found" });
                return std::nullopt;
            }
            return ParseTable(*text, display, diagnostics);
        }
    }

    std::optional<Table> ParseTable(std::string_view text, std::string_view display, Diagnostics& diagnostics)
    {
        const auto parsed = config::Parse(std::string(text), std::string(display), diagnostics);
        if (!parsed) {
            return std::nullopt;
        }
        Table table;
        for (const auto& [key, node] : *parsed) {
            const std::string name(key.str());
            if (const auto* value = node.as_string()) {
                table[name] = value->get();
                continue;
            }
            if (const auto* full = node.as_table()) {
                if (const auto* value = full->get_as<std::string>("text")) {
                    table[name] = value->get();
                    continue;
                }
                diagnostics.push_back(Diagnostic{ Severity::Error, std::string(display), LineOf(node),
                    std::format("{}: the table has no text = \"...\"", name) });
                continue;
            }
            diagnostics.push_back(Diagnostic{ Severity::Error, std::string(display), LineOf(node),
                std::format("{}: expected a text \"...\" or a table with text = \"...\"", name) });
        }
        return table;
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
        missing_.clear();
        reported_.clear();

        const auto folder = dir / kLangDir;
        const auto display = [](std::string_view name) { return std::format("{}/{}.toml", kLangDir, name); };
        if (auto english = ReadTable(folder / std::format("{}.toml", kFallbackLanguage), display(kFallbackLanguage), diagnostics,
                Severity::Error)) {
            fallback_ = std::move(*english);
        }
        if (language_ != kFallbackLanguage) {
            if (auto table = ReadTable(folder / std::format("{}.toml", language_), display(language_), diagnostics, Severity::Warning)) {
                primary_ = std::move(*table);
            }
        }
    }

    bool Localization::Has(std::string_view key) const
    {
        const std::string name(StripDollar(key));
        return primary_.contains(name) || fallback_.contains(name);
    }

    std::string Localization::Text(std::string_view key) const
    {
        const std::string name(StripDollar(key));
        if (const auto it = primary_.find(name); it != primary_.end()) {
            return it->second;
        }
        if (const auto it = fallback_.find(name); it != fallback_.end()) {
            return it->second;
        }
        if (!reported_.contains(name)) {
            missing_.insert(name);
        }
        return name;
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
