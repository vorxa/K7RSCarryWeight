#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <format>

#include "k7/common/file_io.h"
#include "k7/common/lang.h"

using namespace k7::common;
using namespace k7::common::lang;

namespace
{
    namespace fs = std::filesystem;

    class LangDir
    {
    public:
        LangDir()
        {
            path_ = fs::temp_directory_path() / std::format("k7_lang_tests_{}", std::chrono::steady_clock::now().time_since_epoch().count());
            fs::create_directories(path_ / kLangDir);
        }
        ~LangDir()
        {
            std::error_code ec;
            fs::remove_all(path_, ec);
        }
        void Write(std::string_view language, std::string_view text) const
        {
            REQUIRE(io::WriteFile(path_ / kLangDir / std::format("{}.toml", language), text));
        }
        const fs::path& Path() const noexcept { return path_; }

    private:
        fs::path path_;
    };

    bool Mentions(const Diagnostics& diagnostics, std::string_view text)
    {
        for (const auto& d : diagnostics) {
            if (Format(d).find(text) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
}

TEST_CASE("a table reads the short and the full form", "[lang]")
{
    Diagnostics diagnostics;
    const auto  table = ParseTable("K7_A = \"Short\"\n\n[K7_B]\ntext = \"Full\"\nvoice = [\"a.wav\"]\n", "lang/english.toml", diagnostics);
    REQUIRE(table);
    CHECK(table->at("K7_A") == "Short");
    CHECK(table->at("K7_B") == "Full");
    CHECK(diagnostics.empty());
}

TEST_CASE("voice follows the selected phrase and gender", "[lang]")
{
    LangDir dir;
    dir.Write("english", R"(
[K7_A]
text = ["first", "second"]
voice = ["one.wav", "two.wav"]
[K7_A.female]
text = ["female first", "female second"]
)");
    Diagnostics diagnostics;
    Localization localization;
    localization.Load(dir.Path(), "english", diagnostics);
    localization.SetRandom([](std::size_t) { return 0; });
    const auto first = localization.Select("K7_A", Gender::Male);
    const auto second = localization.Select("K7_A", Gender::Male);
    CHECK(first.text == "first");
    CHECK(first.voice == "one.wav");
    CHECK(second.text == "second");
    CHECK(second.voice == "two.wav");
    CHECK(localization.Select("K7_A", Gender::Female).voice.empty());
}

TEST_CASE("voice-only entries select sounds without visible text", "[lang]")
{
    LangDir dir;
    dir.Write("english", "[K7_SOUND]\nvoice = [\"first.wav\", \"second.wav\"]\n");
    Localization localization;
    Diagnostics diagnostics;
    localization.Load(dir.Path(), "english", diagnostics);
    localization.SetRandom([](std::size_t) { return 0; });
    const auto first = localization.Select("K7_SOUND", Gender::Male);
    const auto second = localization.Select("K7_SOUND", Gender::Male);
    CHECK(first.text.empty());
    CHECK(first.voice == "first.wav");
    CHECK(second.text.empty());
    CHECK(second.voice == "second.wav");
    CHECK(diagnostics.empty());
}

TEST_CASE("a key of another type or a full form without text is an error with its line, the rest stays", "[lang]")
{
    Diagnostics diagnostics;
    const auto  table = ParseTable("K7_A = \"ok\"\nK7_N = 5\n[K7_T]\nvoice = []\n", "lang/russian.toml", diagnostics);
    REQUIRE(table);
    CHECK(table->size() == 1);
    CHECK(Mentions(diagnostics, "lang/russian.toml:2: K7_N: expected a text"));
    CHECK(Mentions(diagnostics, "lang/russian.toml:3: K7_T: the table has no text"));
}

TEST_CASE("a table that does not parse gives nothing", "[lang]")
{
    Diagnostics diagnostics;
    CHECK_FALSE(ParseTable("K7_A = \"unterminated\n", "lang/english.toml", diagnostics));
    CHECK(Mentions(diagnostics, "lang/english.toml:1"));
}

TEST_CASE("the language of the game is lowered, empty means English", "[lang]")
{
    CHECK(NormalizeLanguage("RUSSIAN") == "russian");
    CHECK(NormalizeLanguage(" English ") == "english");
    CHECK(NormalizeLanguage("") == "english");
}

TEST_CASE("text comes from the language, then from English, then is the key itself", "[lang]")
{
    LangDir dir;
    dir.Write("english", "K7_A = \"Apple\"\nK7_B = \"Bread\"\n");
    dir.Write("russian", "K7_A = \"Яблоко\"\n");
    Localization lang;
    Diagnostics  diagnostics;
    lang.Load(dir.Path(), "RUSSIAN", diagnostics);
    CHECK(diagnostics.empty());
    CHECK(lang.Language() == "russian");
    CHECK(lang.Text("K7_A") == "Яблоко");
    CHECK(lang.Text("$K7_A") == "Яблоко");
    CHECK(lang.Text("K7_B") == "Bread");
    CHECK(lang.Text("K7_NONE") == "K7_NONE");
    CHECK(lang.Has("K7_B"));
    CHECK_FALSE(lang.Has("K7_NONE"));
}

TEST_CASE("a missing key is reported once", "[lang]")
{
    LangDir dir;
    dir.Write("english", "K7_A = \"Apple\"\n");
    Localization lang;
    Diagnostics  diagnostics;
    lang.Load(dir.Path(), "english", diagnostics);
    (void)lang.Text("K7_X");
    (void)lang.Text("K7_X");
    CHECK(lang.TakeMissing() == std::set<std::string>{ "K7_X" });
    (void)lang.Text("K7_X");
    CHECK(lang.TakeMissing().empty());
}

TEST_CASE("a missing table of the language is a warning, a missing English one an error", "[lang]")
{
    LangDir dir;
    dir.Write("english", "K7_A = \"Apple\"\n");
    Localization lang;
    Diagnostics  diagnostics;
    lang.Load(dir.Path(), "german", diagnostics);
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics.front().severity == Severity::Warning);
    CHECK(Mentions(diagnostics, "lang/german.toml: not found"));
    CHECK(lang.Text("K7_A") == "Apple");

    LangDir empty;
    diagnostics.clear();
    lang.Load(empty.Path(), "english", diagnostics);
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics.front().severity == Severity::Error);
}

TEST_CASE("{name} is replaced by its value, an unknown name stays", "[lang]")
{
    CHECK(Substitute("Taken {taken} of {total}", { { "taken", "3" }, { "total", "5" } }) == "Taken 3 of 5");
    CHECK(Substitute("{who} and {unknown}", { { "who", "I" } }) == "I and {unknown}");
    CHECK(Substitute("no braces", {}) == "no braces");
    CHECK(Substitute("open { only", {}) == "open { only");

    LangDir dir;
    dir.Write("english", "K7_RACE = \"Race: {race}\"\n");
    Localization lang;
    Diagnostics  diagnostics;
    lang.Load(dir.Path(), "english", diagnostics);
    CHECK(lang.Format("K7_RACE", { { "race", "Nord" } }) == "Race: Nord");
}

TEST_CASE("a text by the sex of the character: sex of the language, language, sex of English, English", "[lang]")
{
    LangDir dir;
    dir.Write("english", "K7_ALL = \"All\"\nK7_EN_ONLY = \"En only\"\n"
                         "[K7_ONLY_F]\ntext = \"En general\"\n[K7_ONLY_F.female]\ntext = \"En female\"\n");
    dir.Write("russian", "K7_ONLY_F = \"Ru general\"\n"
                         "[K7_ALL]\ntext = \"Vse\"\n[K7_ALL.male]\ntext = \"Vse m\"\n[K7_ALL.female]\ntext = \"Vse f\"\n");
    Localization lang;
    Diagnostics  diagnostics;
    lang.Load(dir.Path(), "russian", diagnostics);
    CHECK(diagnostics.empty());
    CHECK(lang.Text("$K7_ALL", Gender::Male) == "Vse m");
    CHECK(lang.Text("$K7_ALL", Gender::Female) == "Vse f");
    CHECK(lang.Text("K7_ALL") == "Vse");
    // The language has only a general text: it wins over the English variant of the sex.
    CHECK(lang.Text("K7_ONLY_F", Gender::Female) == "Ru general");
    CHECK(lang.Text("K7_EN_ONLY", Gender::Female) == "En only");
    CHECK(lang.Text("K7_NO_SUCH", Gender::Male) == "K7_NO_SUCH");

    lang.Load(dir.Path(), "english", diagnostics);
    CHECK(lang.Text("K7_ONLY_F", Gender::Female) == "En female");
    CHECK(lang.Text("K7_ONLY_F", Gender::Male) == "En general");
    CHECK(lang.Format("K7_ALL", Gender::Male, {}) == "All");
}

TEST_CASE("several phrases of one key: the same phrase does not come twice in a row", "[lang]")
{
    LangDir dir;
    dir.Write("english", "K7_ONE = \"One\"\n"
                         "[K7_MANY]\ntext = [\"A\", \"B\", \"C\"]\n"
                         "[K7_MANY.female]\ntext = [\"Fa\", \"Fb\"]\n");
    Localization lang;
    Diagnostics  diagnostics;
    lang.Load(dir.Path(), "english", diagnostics);
    CHECK(diagnostics.empty());

    // Any random source: never the phrase shown last, and all of the others do come.
    std::size_t seed = 0;
    lang.SetRandom([&seed](std::size_t count) { return (seed++ * 7 + 3) % count; });
    std::set<std::string> seen;
    std::string           previous;
    for (int i = 0; i < 60; ++i) {
        const auto text = lang.Text("K7_MANY");
        CHECK(text != previous);
        seen.insert(text);
        previous = text;
    }
    CHECK(seen == std::set<std::string>{ "A", "B", "C" });

    // The sex's own list is its own state; two phrases alternate.
    previous.clear();
    seen.clear();
    for (int i = 0; i < 20; ++i) {
        const auto text = lang.Text("K7_MANY", Gender::Female);
        CHECK(text != previous);
        seen.insert(text);
        previous = text;
    }
    CHECK(seen == std::set<std::string>{ "Fa", "Fb" });
    // The male has no list of its own: the general list.
    CHECK(std::set<std::string>{ "A", "B", "C" }.contains(lang.Text("K7_MANY", Gender::Male)));
    // A single phrase is always that phrase.
    for (int i = 0; i < 3; ++i) {
        CHECK(lang.Text("K7_ONE") == "One");
    }
}

TEST_CASE("a list of phrases: the full form too; an empty list or a non-text element is an error", "[lang]")
{
    Diagnostics diagnostics;
    const auto  parsed = ParseTables("K_B = []\nK_C = [\"c\", 5]\nK_D = [\"only\"]\n"
                                     "[K_A]\ntext = [\"a1\", \"a2\"]\n[K_A.female]\ntext = [\"f1\", \"f2\", \"f3\"]\n",
        "t.toml", diagnostics);
    REQUIRE(parsed);
    CHECK(parsed->variants.at("K_A") == std::vector<std::string>{ "a1", "a2" });
    CHECK(parsed->variants.at("K_A.female").size() == 3);
    CHECK(parsed->texts.at("K_A") == "a1");
    CHECK(Mentions(diagnostics, "K_B: the list of phrases is empty"));
    CHECK(Mentions(diagnostics, "K_C: every phrase of the list must be a text"));
    CHECK(parsed->texts.at("K_C") == "c");
    // A list of one phrase is that phrase, not a list.
    CHECK(parsed->texts.at("K_D") == "only");
    CHECK_FALSE(parsed->variants.contains("K_D"));
}
