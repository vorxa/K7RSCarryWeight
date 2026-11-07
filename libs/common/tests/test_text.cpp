#include <catch2/catch_test_macros.hpp>

#include "k7/common/text.h"

using namespace k7::common;

TEST_CASE("names compare ASCII case-insensitively, the way the game does", "[text]")
{
    CHECK(EqualsIgnoreCase("Skyrim.esm", "SKYRIM.ESM"));
    CHECK(EqualsIgnoreCase("", ""));
    CHECK_FALSE(EqualsIgnoreCase("Skyrim.esm", "Skyrim.esp"));
    CHECK_FALSE(EqualsIgnoreCase("Skyrim", "Skyrim.esm"));
}

TEST_CASE("a prefix matches case-insensitively", "[text]")
{
    CHECK(StartsWithIgnoreCase("ClothingRich", "clothing"));
    CHECK(StartsWithIgnoreCase("Clothing", ""));
    CHECK_FALSE(StartsWithIgnoreCase("Cloth", "clothing"));
    CHECK_FALSE(StartsWithIgnoreCase("ArmorClothing", "clothing"));
}
