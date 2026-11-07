#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <set>
#include <string>

#include "k7/common/diagnostics.h"
#include "k7/common/file_io.h"
#include "k7/common/lang.h"
#include "ui/keys.h"

// AGENTS.md, "Локализация": english.toml and russian.toml exist and have exactly the keys the menu uses.
TEST_CASE("english.toml and russian.toml have exactly the keys of the menu", "[lang]")
{
    std::set<std::string> expected;
    for (const auto key : k7::rscw::ui::keys::kAll) {
        CHECK(expected.insert(std::string(key)).second);  // no key twice in keys.h
    }

    for (const auto* language : { "english", "russian" }) {
        INFO(language);
        const auto path = std::filesystem::path(K7RS_LANG_DIR) / (std::string(language) + ".toml");
        const auto text = k7::common::io::ReadFile(path);
        REQUIRE(text);
        k7::common::Diagnostics diagnostics;
        const auto              table = k7::common::lang::ParseTable(*text, path.filename().string(), diagnostics);
        REQUIRE(table);
        CHECK(diagnostics.empty());

        std::set<std::string> actual;
        for (const auto& [key, value] : *table) {
            actual.insert(key);
            INFO(key);
            CHECK_FALSE(value.empty());
            // The menu's font (stand 16: LegendsOfTheNorth of "SKSE Menu Framework Rus") draws ASCII and Cyrillic;
            // a typographic dash or minus came out as "?". UTF-8 of U+0400–U+04FF starts with 0xD0 or 0xD1.
            for (std::size_t i = 0; i < value.size(); ++i) {
                const auto c = static_cast<unsigned char>(value[i]);
                if (c < 0x80) {
                    continue;
                }
                const bool cyrillic = (c == 0xD0 || c == 0xD1) && i + 1 < value.size();
                CHECK(cyrillic);
                ++i;
            }
        }
        CHECK(actual == expected);
    }
}
