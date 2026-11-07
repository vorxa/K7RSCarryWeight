#include <catch2/catch_test_macros.hpp>

#include "k7/common/form_key.h"

using namespace k7::common;

TEST_CASE("ParseFormKey", "[form_key]")
{
    SECTION("accepted spellings")
    {
        const auto key = ParseFormKey("Skyrim.esm|0x013948");
        REQUIRE(key.has_value());
        CHECK(key->plugin == "Skyrim.esm");
        CHECK(key->id == 0x13948);

        CHECK(ParseFormKey("My Mod.esp|13948")->id == 0x13948);
        CHECK(ParseFormKey("A.esp|0XFFFFFF")->id == 0xFFFFFF);
    }
    SECTION("rejected spellings")
    {
        CHECK_FALSE(ParseFormKey("").has_value());
        CHECK_FALSE(ParseFormKey("Skyrim.esm").has_value());
        CHECK_FALSE(ParseFormKey("|0x1").has_value());
        CHECK_FALSE(ParseFormKey("Skyrim.esm|").has_value());
        CHECK_FALSE(ParseFormKey("Skyrim.esm|0x").has_value());
        CHECK_FALSE(ParseFormKey("Skyrim.esm|0xZZ").has_value());
        CHECK_FALSE(ParseFormKey("Skyrim.esm|0x1000000").has_value()); // more than 24 bits
        CHECK_FALSE(ParseFormKey("Skyrim.esm|0x12 ").has_value());
    }
    SECTION("round trip and equivalence")
    {
        const auto key = *ParseFormKey("Skyrim.esm|0x1396B");
        CHECK(ToString(key) == "Skyrim.esm|0x01396B");
        CHECK(*ParseFormKey(ToString(key)) == key);
        CHECK(Equivalent(key, *ParseFormKey("SKYRIM.ESM|0x01396b")));
        CHECK_FALSE(Equivalent(key, *ParseFormKey("Skyrim.esm|0x1396C")));
    }
}
