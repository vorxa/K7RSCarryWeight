#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <string_view>

#include "k7/common/toml_reader.h"

using namespace k7::common;

namespace
{
    enum class Colour { Red, Green };

    std::optional<Colour> ParseColour(std::string_view text)
    {
        if (text == "red") {
            return Colour::Red;
        }
        if (text == "green") {
            return Colour::Green;
        }
        return std::nullopt;
    }
}

TEST_CASE("values of the right type are read", "[toml_reader]")
{
    const auto  table = toml::parse("flag = true\nsize = 5\nratio = 0.5\ncount = 3\nname = \"x\"\ncolour = \"green\"\n");
    Diagnostics diagnostics;
    TomlReader  r(table, "config.toml", "", diagnostics);

    bool        flag{};
    float       size{};
    float       ratio{};
    int         count{};
    std::string name;
    Colour      colour{ Colour::Red };
    CHECK(r.Bool("flag", flag));
    CHECK(r.Float("size", size)); // an integer is accepted for a float
    CHECK(r.Float("ratio", ratio));
    CHECK(r.Int("count", count));
    CHECK(r.String("name", name));
    CHECK(r.Enum("colour", colour, ParseColour, "red, green"));
    r.ReportUnknown();

    CHECK(flag);
    CHECK(size == 5.0f);
    CHECK(ratio == 0.5f);
    CHECK(count == 3);
    CHECK(name == "x");
    CHECK(colour == Colour::Green);
    CHECK(diagnostics.empty());
}

TEST_CASE("a missing key is not an error and leaves the value alone", "[toml_reader]")
{
    const auto  table = toml::parse("");
    Diagnostics diagnostics;
    TomlReader  r(table, "config.toml", "", diagnostics);

    int value = 7;
    CHECK_FALSE(r.Int("count", value));
    CHECK(value == 7);
    CHECK(diagnostics.empty());
}

TEST_CASE("a wrong type is an error with file, line and dotted key", "[toml_reader]")
{
    const auto  table = toml::parse("a = 1\nsize = \"big\"\n");
    Diagnostics diagnostics;
    TomlReader  r(table, "overrides/x.toml", "slots.general", diagnostics);

    float size = 1.0f;
    CHECK_FALSE(r.Float("size", size));
    CHECK(size == 1.0f);
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics[0].severity == Severity::Error);
    CHECK(Format(diagnostics[0]) == "overrides/x.toml:2: slots.general.size: expected a number");
}

TEST_CASE("values outside the allowed range are errors", "[toml_reader]")
{
    const auto  table = toml::parse("low = -1\nhigh = 11.0\nbig = 9999999999\n");
    Diagnostics diagnostics;
    TomlReader  r(table, "config.toml", "", diagnostics);

    int   low = 0;
    float high = 0.0f;
    int   big = 0;
    CHECK_FALSE(r.Int("low", low, 0));
    CHECK_FALSE(r.Float("high", high, 0.0f, 10.0f));
    CHECK_FALSE(r.Int("big", big));
    REQUIRE(diagnostics.size() == 3);
    CHECK(diagnostics[0].message == "low: must not be less than 0");
    CHECK(diagnostics[1].message == "high: must be between 0 and 10");
    CHECK(diagnostics[2].message == "big: number out of range");
}

TEST_CASE("an unknown enum spelling lists the allowed values", "[toml_reader]")
{
    const auto  table = toml::parse("colour = \"blue\"\n");
    Diagnostics diagnostics;
    TomlReader  r(table, "config.toml", "", diagnostics);

    Colour colour{ Colour::Red };
    CHECK_FALSE(r.Enum("colour", colour, ParseColour, "red, green"));
    CHECK(colour == Colour::Red);
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics[0].message == "colour: unknown value 'blue', expected one of: red, green");
}

TEST_CASE("keys that were not asked for are reported as unknown warnings", "[toml_reader]")
{
    const auto  table = toml::parse("size = 1\ncolour = \"red\"\nnested = { a = 1 }\n");
    Diagnostics diagnostics;
    TomlReader  r(table, "config.toml", "slots", diagnostics);

    float size{};
    r.Float("size", size);
    r.Known("nested");
    r.ReportUnknown();
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics[0].severity == Severity::Warning);
    CHECK(Format(diagnostics[0]) == "config.toml:2: unknown key 'slots.colour'");
}

TEST_CASE("tables and arrays are returned only when the type matches", "[toml_reader]")
{
    const auto  table = toml::parse("[sub]\nx = 1\n[[list]]\ny = 2\n");
    Diagnostics diagnostics;
    TomlReader  r(table, "config.toml", "", diagnostics);

    CHECK(r.SubTable("sub") != nullptr);
    CHECK(r.Array("list") != nullptr);
    CHECK(r.SubTable("list") == nullptr); // an array is not a table
    CHECK(r.Array("missing") == nullptr);
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics[0].message == "list: expected a table");
}
