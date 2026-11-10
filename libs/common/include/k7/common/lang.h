#pragma once

#include <filesystem>
#include <functional>
#include <initializer_list>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "k7/common/diagnostics.h"

// Localization tables (FR-11 of the K7InvVolLimits specification; AGENTS.md, "Локализация"): every string the
// player sees is a key of Data/SKSE/Plugins/<plugin>/lang/<language>.toml; nothing the player sees is built
// into the code. Every plugin ships english.toml and russian.toml with the same keys.
namespace k7::common::lang
{
    inline constexpr std::string_view kLangDir         = "lang";
    inline constexpr std::string_view kFallbackLanguage = "english";

    // The sex of the player character for the variants of FR-11.6.
    enum class Gender
    {
        Male,
        Female,
    };

    // One table: key -> text. A variant for one sex is stored under "KEY.male" or "KEY.female" (a real key has no dot).
    // A file gives a key in the short form `KEY = "text"` or the full one `[KEY]` with `text = "..."` (and
    // `voice = [...]`, read by plugins that play sounds, ignored here), and optionally the sub-tables `[KEY.male]`
    // and `[KEY.female]` with their own `text`. Several phrases of one key are a list: `text = ["a", "b"]` (or
    // `KEY = ["a", "b"]`); the table then holds the first of them and `Variants` all.
    using Table = std::unordered_map<std::string, std::string>;
    using Variants = std::unordered_map<std::string, std::vector<std::string>>;

    struct Parsed
    {
        Table    texts;
        Variants variants; // only keys that give a list of several phrases
        Variants voices;   // positional sound references, including empty entries
    };

    struct Selected
    {
        std::string text;
        std::string voice;
    };

    // Parses one table. A file that does not parse gives nothing and an error with the line; a key of another
    // type, a full form without `text`, or an empty list or one with a non-text element is an error with its line and
    // is left out, the other keys stay.
    std::optional<Parsed> ParseTables(std::string_view text, std::string_view display, Diagnostics& diagnostics);

    // ParseTables() without the variant lists (the first phrase of each key).
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

        // Chooses the number in [0, count) for a key with several phrases; the default is random. Tests set their own.
        void SetRandom(std::function<std::size_t(std::size_t count)> random) { random_ = std::move(random); }

        // The text of `key` (a leading `$` is ignored): from the language, else from English, else the key
        // itself, so that a missing string is visible rather than empty. A key missing in both is remembered
        // for TakeMissing().
        std::string Text(std::string_view key) const;

        // The text of `key` for one sex (FR-11.6): the sex's variant in the language, else the general text of the
        // language, else the sex's variant in English, else the general English text, else the key itself.
        std::string Text(std::string_view key, Gender gender) const;
        Selected Select(std::string_view key, Gender gender) const;

        // Text(key) with `{name}` replaced by the value of that name. A name that is not given stays as it is.
        std::string Format(std::string_view key, std::initializer_list<std::pair<std::string_view, std::string_view>> values) const;

        // Keys asked for since the last call that are in neither table, each once (for one log line per key).
        std::set<std::string> TakeMissing() const;

        const std::string& Language() const noexcept { return language_; }
        std::size_t        LanguageKeys() const noexcept { return primary_.size(); }
        std::size_t        FallbackKeys() const noexcept { return fallback_.size(); }
        bool               Has(std::string_view key) const;

        // Text(key, gender) with `{name}` replaced.
        std::string Format(std::string_view key, Gender gender,
            std::initializer_list<std::pair<std::string_view, std::string_view>> values) const;

    private:
        std::string                   language_{ kFallbackLanguage };
        // The phrase of `name` in one table: one of its variants, never the one shown last for this `state`
        // (so the same phrase does not come twice in a row); else its text.
        const std::string* Pick(const std::string& name, const Table& table, const Variants& variants, const char* which,
            std::size_t* selectedIndex = nullptr) const;

        Table                         primary_;
        Table                         fallback_;
        Variants                      primaryVariants_;
        Variants                      fallbackVariants_;
        Variants                      primaryVoices_;
        Variants                      fallbackVoices_;
        std::function<std::size_t(std::size_t)> random_;
        mutable std::unordered_map<std::string, std::size_t> last_;
        mutable std::set<std::string> missing_;
        mutable std::set<std::string> reported_;
    };

    // Replaces `{name}` in `text` with the value of that name. A name that is not given stays as it is.
    std::string Substitute(std::string_view text, std::initializer_list<std::pair<std::string_view, std::string_view>> values);
}
