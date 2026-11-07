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
