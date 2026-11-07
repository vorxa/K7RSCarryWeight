#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace k7::common
{
    // A form as a plugin name and a local id: how configuration refers to records of the game, without
    // game types. Resolved to a pointer only by the engine layer.
    struct FormKey
    {
        std::string   plugin; // "Skyrim.esm"
        std::uint32_t id{};   // local form id, without the load-order byte

        bool operator==(const FormKey&) const = default;
    };

    // "Skyrim.esm|0x013948" (the 0x prefix is optional, hex digits, at most 24 bits).
    // Empty result: no plugin name, no id, not hex, or the id does not fit.
    std::optional<FormKey> ParseFormKey(std::string_view text) noexcept;

    // Canonical spelling: "Skyrim.esm|0x013948".
    std::string ToString(const FormKey& key);

    // Plugin file names are case-insensitive in Skyrim, so "SKYRIM.ESM|0x1" is the form "Skyrim.esm|0x1".
    bool Equivalent(const FormKey& a, const FormKey& b) noexcept;
}
