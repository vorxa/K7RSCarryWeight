#pragma once

#include <filesystem>
#include <initializer_list>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "k7/common/diagnostics.h"

// Localization tables (FR-11 of the K7InvVolLimits specification; AGENTS.md, "Локализация"): every string the
// player sees is a key of Data/SKSE/Plugins/<plugin>/lang/<language>.toml; nothing the player sees is built
// into the code. Every plugin ships english.toml and russian.toml with the same keys.
namespace k7::common::lang
{
    inline constexpr std::string_view kLangDir         = "lang";
    inline constexpr std::string_view kFallbackLanguage = "english";

    // One table: key -> text. A file gives a key in the short form `KEY = "text"` or the full one
    // `[KEY]` with `text = "..."` (and `voice = [...]`, read by plugins that play sounds, ignored here).
    using Table = std::unordered_map<std::string, std::string>;

    // Parses one table. A file that does not parse gives nothing and an error with the line; a key of another
    // type, or a full form without `text`, is an error with its line and is left out, the other keys stay.
    std::optional<Table> ParseTable(std::string_view text, std::string_view display, Diagnostics& diagnostics);

    // "RUSSIAN", " Russian " -> "russian": the language name as the game gives it (sLanguage:General), lowered.
    std::string NormalizeLanguage(std::string_view language);

    // The tables of one plugin: the game's language and English to fall back on.
    class Localization
    {
    public:
        // Reads <dir>/lang/<language>.toml and <dir>/lang/english.toml (once if the language is English). A
        // missing table of the language is a warning, a missing English table an error; problems go to
        // `diagnostics`. Replaces whatever was loaded before.
        void Load(const std::filesystem::path& dir, std::string_view language, Diagnostics& diagnostics);

        // The text of `key` (a leading `$` is ignored): from the language, else from English, else the key
        // itself, so that a missing string is visible rather than empty. A key missing in both is remembered
        // for TakeMissing().
        std::string Text(std::string_view key) const;

        // Text(key) with `{name}` replaced by the value of that name. A name that is not given stays as it is.
        std::string Format(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string_view>> values) const;

        // Keys asked for since the last call that are in neither table, each once (for one log line per key).
        std::set<std::string> TakeMissing() const;

        const std::string& Language() const noexcept { return language_; }
        std::size_t        LanguageKeys() const noexcept { return primary_.size(); }
        std::size_t        FallbackKeys() const noexcept { return fallback_.size(); }
        bool               Has(std::string_view key) const;

    private:
        std::string                   language_{ kFallbackLanguage };
        Table                         primary_;
        Table                         fallback_;
        mutable std::set<std::string> missing_;
        mutable std::set<std::string> reported_;
    };

    // Replaces `{name}` in `text` with the value of that name. A name that is not given stays as it is.
    std::string Substitute(std::string_view text, std::initializer_list<std::pair<std::string_view, std::string_view>> values);
}
